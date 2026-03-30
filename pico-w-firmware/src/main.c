#include "config.h"
#include "network.h"
#include "sensors.h"
#include "telemetry.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/time.h"

int main(void) {
  stdio_init_all();
  sleep_ms(1500);

  printf("Pico W telemetry firmware starting\n");
  printf("device_id=%s backend=%s:%u%s\n", TELEMETRY_DEVICE_ID, BACKEND_HOST, (unsigned int)BACKEND_PORT, BACKEND_PATH);
  printf("analog_gpio=%u digital_gpio=%u sample_period_ms=%u\n",
         (unsigned int)ANALOG_SENSOR_GPIO,
         (unsigned int)DIGITAL_SENSOR_GPIO,
         (unsigned int)SENSOR_SAMPLE_PERIOD_MS);

  printf("[boot] init sensors...\n");
  if (!sensors_init()) {
    printf("fatal: sensors_init failed\n");
    while (true) {
      sleep_ms(1000);
    }
  }
  printf("[boot] sensors ok\n");

  telemetry_queue_t queue;
  telemetry_queue_init(&queue);

  network_manager_t network;
  printf("[boot] init wifi stack...\n");
  if (!network_init(&network)) {
    printf("fatal: network_init failed\n");
    while (true) {
      sleep_ms(1000);
    }
  }
  printf("[boot] wifi stack ok\n");

  network_state_t previous_state = network.state;

  while (true) {
    const uint64_t now_ms = to_ms_since_boot(get_absolute_time());

    network_poll(&network, now_ms);
    if (network.state != previous_state) {
      printf("[wifi] state=%s attempts=%u\n", network_state_name(network.state), network.attempts);
      previous_state = network.state;
    }

    if (sensors_consume_periodic_tick()) {
      sensor_snapshot_t snapshot;
      sensors_capture_snapshot(&snapshot);

      const bool pushed_analog = telemetry_queue_push_analog(
        &queue,
        "tank_level",
        "%",
        snapshot.analog_physical,
        ANALOG_SENSOR_GPIO,
        snapshot.sampled_ms
      );

      const bool pushed_discrete = telemetry_queue_push_discrete(
        &queue,
        "presence",
        (!snapshot.digital_state) ? "present" : "absent",
        DIGITAL_SENSOR_GPIO,
        snapshot.sampled_ms
      );

      printf(
        "[sample] ms=%" PRIu64 " adc_raw=%u adc_v=%.3f level=%.2f%% presence=%s queue=%u push_a=%d push_d=%d\n",
        snapshot.sampled_ms,
        snapshot.adc_raw,
        snapshot.adc_voltage,
        snapshot.analog_physical,
        (!snapshot.digital_state) ? "present" : "absent",
        (unsigned int)telemetry_queue_count(&queue),
        pushed_analog ? 1 : 0,
        pushed_discrete ? 1 : 0
      );
    }

    bool changed_state = false;
    uint64_t changed_at = 0;
    if (sensors_consume_digital_change(&changed_state, &changed_at)) {
      const bool pushed_edge = telemetry_queue_push_discrete(
        &queue,
        "presence",
        (!changed_state) ? "present" : "absent",
        DIGITAL_SENSOR_GPIO,
        changed_at
      );
      printf(
        "[irq] ms=%" PRIu64 " presence=%s queue=%u push=%d\n",
        changed_at,
        (!changed_state) ? "present" : "absent",
        (unsigned int)telemetry_queue_count(&queue),
        pushed_edge ? 1 : 0
      );
    }

    if (network_is_ready(&network)) {
      telemetry_event_t event;
      if (telemetry_queue_peek_due(&queue, now_ms, &event)) {
        char json[384];
        const size_t json_size = telemetry_build_payload_json(
          &event,
          TELEMETRY_DEVICE_ID,
          TELEMETRY_EPOCH_UNIX,
          json,
          sizeof(json)
        );

        if (json_size == 0u) {
          telemetry_queue_drop_front(&queue);
          printf("[http] drop reason=payload_overflow queue=%u\n", (unsigned int)telemetry_queue_count(&queue));
        } else {
          int http_status = 0;
          const bool sent = network_http_post_json(
            BACKEND_HOST,
            BACKEND_PORT,
            BACKEND_PATH,
            json,
            HTTP_TIMEOUT_MS,
            &http_status
          );

          if (sent) {
            telemetry_queue_drop_front(&queue);
            printf(
              "[http] ok status=%d sensor=%s attempt=%u remaining=%u\n",
              http_status,
              event.sensor_type,
              event.attempts,
              (unsigned int)telemetry_queue_count(&queue)
            );
          } else {
            uint8_t attempts_after = 0;
            const bool requeued = telemetry_queue_retry_front(&queue, now_ms, &attempts_after);
            printf(
              "[http] fail status=%d sensor=%s retry=%d attempts=%u queued=%u\n",
              http_status,
              event.sensor_type,
              requeued ? 1 : 0,
              attempts_after,
              (unsigned int)telemetry_queue_count(&queue)
            );
          }
        }
      }
    }

    sleep_ms(MAIN_LOOP_DELAY_MS);
  }

  network_deinit();
  return 0;
}