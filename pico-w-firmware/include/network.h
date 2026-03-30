#ifndef NETWORK_H
#define NETWORK_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  NETWORK_STATE_DISCONNECTED = 0,
  NETWORK_STATE_CONNECTING = 1,
  NETWORK_STATE_CONNECTED = 2,
  NETWORK_STATE_BACKOFF = 3
} network_state_t;

typedef struct {
  network_state_t state;
  uint8_t attempts;
  uint64_t next_action_ms;
} network_manager_t;

bool network_init(network_manager_t *manager);
void network_poll(network_manager_t *manager, uint64_t now_ms);
bool network_is_ready(const network_manager_t *manager);
const char *network_state_name(network_state_t state);
bool network_http_post_json(
  const char *host,
  uint16_t port,
  const char *path,
  const char *json_payload,
  uint32_t timeout_ms,
  int *http_status
);
void network_deinit(void);

#endif