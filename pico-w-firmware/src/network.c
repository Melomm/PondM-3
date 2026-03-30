#include "network.h"

#include "config.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "lwip/err.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "pico/cyw43_arch.h"
#include "pico/time.h"

typedef struct {
  struct tcp_pcb *pcb;
  bool done;
  bool success;
  int http_status;
  bool status_seen;
  char request[768];
} http_raw_ctx_t;

static uint64_t compute_backoff_ms(uint8_t attempt, uint32_t base_ms, uint32_t max_ms) {
  uint8_t shift = attempt;
  if (shift > 5u) {
    shift = 5u;
  }

  uint64_t delay = ((uint64_t)base_ms) << shift;
  if (delay > max_ms) {
    delay = max_ms;
  }
  return delay;
}

bool network_init(network_manager_t *manager) {
  if (manager == NULL) {
    return false;
  }

  memset(manager, 0, sizeof(*manager));
  manager->state = NETWORK_STATE_DISCONNECTED;
  manager->next_action_ms = 0u;

  if (cyw43_arch_init() != 0) {
    return false;
  }

  cyw43_arch_enable_sta_mode();
  return true;
}

void network_poll(network_manager_t *manager, uint64_t now_ms) {
  if (manager == NULL) {
    return;
  }

  if ((manager->state == NETWORK_STATE_DISCONNECTED || manager->state == NETWORK_STATE_BACKOFF) &&
      now_ms >= manager->next_action_ms) {
    manager->state = NETWORK_STATE_CONNECTING;
    const int rc = cyw43_arch_wifi_connect_timeout_ms(
      WIFI_SSID,
      WIFI_PASSWORD,
      CYW43_AUTH_WPA2_AES_PSK,
      WIFI_CONNECT_TIMEOUT_MS
    );

    if (rc == 0) {
      manager->state = NETWORK_STATE_CONNECTED;
      manager->attempts = 0u;
      manager->next_action_ms = now_ms + 2000u;
      return;
    }

    manager->state = NETWORK_STATE_BACKOFF;
    manager->attempts++;
    manager->next_action_ms = now_ms + compute_backoff_ms(manager->attempts, WIFI_RETRY_BASE_MS, WIFI_RETRY_MAX_MS);
    return;
  }

  if (manager->state == NETWORK_STATE_CONNECTED && now_ms >= manager->next_action_ms) {
    const int link = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    if (link < CYW43_LINK_UP) {
      manager->state = NETWORK_STATE_BACKOFF;
      manager->attempts++;
      manager->next_action_ms = now_ms + compute_backoff_ms(manager->attempts, WIFI_RETRY_BASE_MS, WIFI_RETRY_MAX_MS);
    } else {
      manager->next_action_ms = now_ms + 2000u;
    }
  }
}

bool network_is_ready(const network_manager_t *manager) {
  return manager != NULL && manager->state == NETWORK_STATE_CONNECTED;
}

const char *network_state_name(network_state_t state) {
  switch (state) {
    case NETWORK_STATE_DISCONNECTED:
      return "disconnected";
    case NETWORK_STATE_CONNECTING:
      return "connecting";
    case NETWORK_STATE_CONNECTED:
      return "connected";
    case NETWORK_STATE_BACKOFF:
      return "backoff";
    default:
      return "unknown";
  }
}

static int parse_status_from_payload(const struct pbuf *p) {
  if (p == NULL || p->len == 0u) {
    return 0;
  }

  char line[64];
  const uint16_t copy_len = p->len < (sizeof(line) - 1u) ? p->len : (uint16_t)(sizeof(line) - 1u);
  memcpy(line, p->payload, copy_len);
  line[copy_len] = '\0';

  int status = 0;
  if (sscanf(line, "HTTP/%*d.%*d %d", &status) == 1) {
    return status;
  }

  return 0;
}

static err_t http_raw_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
  http_raw_ctx_t *ctx = (http_raw_ctx_t *)arg;
  if (ctx == NULL) {
    return ERR_ARG;
  }

  if (err != ERR_OK) {
    ctx->done = true;
    ctx->success = false;
    return err;
  }

  const size_t request_len = strlen(ctx->request);
  err_t wr = tcp_write(tpcb, ctx->request, request_len, TCP_WRITE_FLAG_COPY);
  if (wr != ERR_OK) {
    ctx->done = true;
    ctx->success = false;
    return wr;
  }

  err_t fl = tcp_output(tpcb);
  if (fl != ERR_OK) {
    ctx->done = true;
    ctx->success = false;
    return fl;
  }

  return ERR_OK;
}

static err_t http_raw_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
  http_raw_ctx_t *ctx = (http_raw_ctx_t *)arg;
  if (ctx == NULL) {
    if (p != NULL) {
      pbuf_free(p);
    }
    return ERR_ARG;
  }

  if (err != ERR_OK) {
    if (p != NULL) {
      pbuf_free(p);
    }
    ctx->done = true;
    ctx->success = false;
    return err;
  }

  if (p == NULL) {
    if (!ctx->status_seen) {
      ctx->success = false;
    }
    ctx->done = true;
    return ERR_OK;
  }

  if (!ctx->status_seen) {
    const int status = parse_status_from_payload(p);
    if (status > 0) {
      ctx->http_status = status;
      ctx->status_seen = true;
      ctx->success = (status >= 200 && status < 300);
      ctx->done = true;
    }
  }

  tcp_recved(tpcb, p->tot_len);
  pbuf_free(p);
  return ERR_OK;
}

static void http_raw_error(void *arg, err_t err) {
  (void)err;
  http_raw_ctx_t *ctx = (http_raw_ctx_t *)arg;
  if (ctx == NULL) {
    return;
  }

  ctx->pcb = NULL;
  ctx->done = true;
  ctx->success = false;
}

bool network_http_post_json(
  const char *host,
  uint16_t port,
  const char *path,
  const char *json_payload,
  uint32_t timeout_ms,
  int *http_status
) {
  if (host == NULL || path == NULL || json_payload == NULL) {
    return false;
  }

  if (http_status != NULL) {
    *http_status = 0;
  }

  ip_addr_t remote_addr;
  if (!ipaddr_aton(host, &remote_addr)) {
    // This raw implementation expects BACKEND_HOST as literal IPv4/IPv6 address.
    return false;
  }

  http_raw_ctx_t ctx;
  memset(&ctx, 0, sizeof(ctx));

  const size_t payload_len = strlen(json_payload);
  const int req_len = snprintf(
    ctx.request,
    sizeof(ctx.request),
    "POST %s HTTP/1.1\r\n"
    "Host: %s:%u\r\n"
    "Content-Type: application/json\r\n"
    "Connection: close\r\n"
    "Content-Length: %u\r\n\r\n"
    "%s",
    path,
    host,
    (unsigned int)port,
    (unsigned int)payload_len,
    json_payload
  );

  if (req_len <= 0 || (size_t)req_len >= sizeof(ctx.request)) {
    return false;
  }

  cyw43_arch_lwip_begin();
  ctx.pcb = tcp_new_ip_type(IP_GET_TYPE(&remote_addr));
  if (ctx.pcb == NULL) {
    cyw43_arch_lwip_end();
    return false;
  }

  tcp_arg(ctx.pcb, &ctx);
  tcp_err(ctx.pcb, http_raw_error);
  tcp_recv(ctx.pcb, http_raw_recv);

  const err_t conn_err = tcp_connect(ctx.pcb, &remote_addr, port, http_raw_connected);
  if (conn_err != ERR_OK) {
    tcp_abort(ctx.pcb);
    ctx.pcb = NULL;
    cyw43_arch_lwip_end();
    return false;
  }
  cyw43_arch_lwip_end();

  const absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
  while (!ctx.done && !time_reached(deadline)) {
    sleep_ms(10);
  }

  cyw43_arch_lwip_begin();
  if (ctx.pcb != NULL) {
    tcp_arg(ctx.pcb, NULL);
    tcp_err(ctx.pcb, NULL);
    tcp_recv(ctx.pcb, NULL);
    if (ctx.done) {
      (void)tcp_close(ctx.pcb);
    } else {
      tcp_abort(ctx.pcb);
    }
    ctx.pcb = NULL;
  }
  cyw43_arch_lwip_end();

  if (!ctx.done) {
    return false;
  }

  if (http_status != NULL) {
    *http_status = ctx.http_status;
  }
  return ctx.success;
}

void network_deinit(void) {
  cyw43_arch_deinit();
}