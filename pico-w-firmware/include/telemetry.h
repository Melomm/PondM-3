#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
  TELEMETRY_READING_ANALOG = 0,
  TELEMETRY_READING_DISCRETE = 1
} telemetry_reading_type_t;

typedef struct {
  telemetry_reading_type_t reading_type;
  char sensor_type[32];
  char unit[16];
  float analog_value;
  char discrete_value[16];
  uint8_t gpio;
  uint8_t attempts;
  uint64_t observed_ms;
  uint64_t next_attempt_ms;
} telemetry_event_t;

typedef struct {
  telemetry_event_t buffer[TELEMETRY_QUEUE_CAPACITY];
  uint16_t head;
  uint16_t tail;
  uint16_t count;
} telemetry_queue_t;

void telemetry_queue_init(telemetry_queue_t *queue);
bool telemetry_queue_push_analog(
  telemetry_queue_t *queue,
  const char *sensor_type,
  const char *unit,
  float analog_value,
  uint8_t gpio,
  uint64_t observed_ms
);
bool telemetry_queue_push_discrete(
  telemetry_queue_t *queue,
  const char *sensor_type,
  const char *discrete_value,
  uint8_t gpio,
  uint64_t observed_ms
);
bool telemetry_queue_peek_due(
  const telemetry_queue_t *queue,
  uint64_t now_ms,
  telemetry_event_t *out
);
void telemetry_queue_drop_front(telemetry_queue_t *queue);
bool telemetry_queue_retry_front(telemetry_queue_t *queue, uint64_t now_ms, uint8_t *attempts_after);
uint16_t telemetry_queue_count(const telemetry_queue_t *queue);
size_t telemetry_build_payload_json(
  const telemetry_event_t *event,
  const char *device_id,
  uint32_t epoch_unix,
  char *out_json,
  size_t out_json_size
);

#endif