#include "telemetry.h"

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static void safe_copy(char *dst, size_t dst_size, const char *src) {
  if (dst == NULL || dst_size == 0u) {
    return;
  }

  if (src == NULL) {
    dst[0] = '\0';
    return;
  }

  strncpy(dst, src, dst_size - 1u);
  dst[dst_size - 1u] = '\0';
}

static bool telemetry_queue_push_event(telemetry_queue_t *queue, const telemetry_event_t *event) {
  if (queue == NULL || event == NULL || queue->count >= TELEMETRY_QUEUE_CAPACITY) {
    return false;
  }

  queue->buffer[queue->tail] = *event;
  queue->tail = (uint16_t)((queue->tail + 1u) % TELEMETRY_QUEUE_CAPACITY);
  queue->count++;
  return true;
}

void telemetry_queue_init(telemetry_queue_t *queue) {
  if (queue == NULL) {
    return;
  }

  memset(queue, 0, sizeof(*queue));
}

bool telemetry_queue_push_analog(
  telemetry_queue_t *queue,
  const char *sensor_type,
  const char *unit,
  float analog_value,
  uint8_t gpio,
  uint64_t observed_ms
) {
  telemetry_event_t event;
  memset(&event, 0, sizeof(event));
  event.reading_type = TELEMETRY_READING_ANALOG;
  safe_copy(event.sensor_type, sizeof(event.sensor_type), sensor_type);
  safe_copy(event.unit, sizeof(event.unit), unit);
  event.analog_value = analog_value;
  event.gpio = gpio;
  event.observed_ms = observed_ms;
  event.next_attempt_ms = observed_ms;
  event.attempts = 0;
  return telemetry_queue_push_event(queue, &event);
}

bool telemetry_queue_push_discrete(
  telemetry_queue_t *queue,
  const char *sensor_type,
  const char *discrete_value,
  uint8_t gpio,
  uint64_t observed_ms
) {
  telemetry_event_t event;
  memset(&event, 0, sizeof(event));
  event.reading_type = TELEMETRY_READING_DISCRETE;
  safe_copy(event.sensor_type, sizeof(event.sensor_type), sensor_type);
  safe_copy(event.discrete_value, sizeof(event.discrete_value), discrete_value);
  event.gpio = gpio;
  event.observed_ms = observed_ms;
  event.next_attempt_ms = observed_ms;
  event.attempts = 0;
  return telemetry_queue_push_event(queue, &event);
}

bool telemetry_queue_peek_due(
  const telemetry_queue_t *queue,
  uint64_t now_ms,
  telemetry_event_t *out
) {
  if (queue == NULL || out == NULL || queue->count == 0u) {
    return false;
  }

  const telemetry_event_t *front = &queue->buffer[queue->head];
  if (front->next_attempt_ms > now_ms) {
    return false;
  }

  *out = *front;
  return true;
}

void telemetry_queue_drop_front(telemetry_queue_t *queue) {
  if (queue == NULL || queue->count == 0u) {
    return;
  }

  queue->head = (uint16_t)((queue->head + 1u) % TELEMETRY_QUEUE_CAPACITY);
  queue->count--;
}

bool telemetry_queue_retry_front(telemetry_queue_t *queue, uint64_t now_ms, uint8_t *attempts_after) {
  if (queue == NULL || queue->count == 0u) {
    return false;
  }

  telemetry_event_t event = queue->buffer[queue->head];
  telemetry_queue_drop_front(queue);

  if (event.attempts >= TELEMETRY_MAX_RETRIES) {
    return false;
  }

  event.attempts++;
  uint32_t shift = event.attempts - 1u;
  if (shift > 5u) {
    shift = 5u;
  }

  uint64_t delay_ms = ((uint64_t)TELEMETRY_RETRY_BASE_MS) << shift;
  if (delay_ms > TELEMETRY_RETRY_MAX_MS) {
    delay_ms = TELEMETRY_RETRY_MAX_MS;
  }

  event.next_attempt_ms = now_ms + delay_ms;
  if (!telemetry_queue_push_event(queue, &event)) {
    return false;
  }

  if (attempts_after != NULL) {
    *attempts_after = event.attempts;
  }

  return true;
}

uint16_t telemetry_queue_count(const telemetry_queue_t *queue) {
  return queue == NULL ? 0u : queue->count;
}

static size_t format_timestamp(uint32_t epoch_unix, uint64_t observed_ms, char *out, size_t out_size) {
  if (out == NULL || out_size == 0u) {
    return 0u;
  }

  const time_t timestamp = (time_t)(epoch_unix + (observed_ms / 1000u));
  struct tm *tm_ptr = gmtime(&timestamp);
  if (tm_ptr == NULL) {
    return 0u;
  }

  const int written = snprintf(
    out,
    out_size,
    "%04d-%02d-%02dT%02d:%02d:%02d.000Z",
    tm_ptr->tm_year + 1900,
    tm_ptr->tm_mon + 1,
    tm_ptr->tm_mday,
    tm_ptr->tm_hour,
    tm_ptr->tm_min,
    tm_ptr->tm_sec
  );

  return written > 0 ? (size_t)written : 0u;
}

size_t telemetry_build_payload_json(
  const telemetry_event_t *event,
  const char *device_id,
  uint32_t epoch_unix,
  char *out_json,
  size_t out_json_size
) {
  if (event == NULL || device_id == NULL || out_json == NULL || out_json_size == 0u) {
    return 0u;
  }

  char iso_time[32];
  if (format_timestamp(epoch_unix, event->observed_ms, iso_time, sizeof(iso_time)) == 0u) {
    return 0u;
  }

  int written = 0;
  if (event->reading_type == TELEMETRY_READING_ANALOG) {
    if (event->unit[0] != '\0') {
      written = snprintf(
        out_json,
        out_json_size,
        "{\"deviceId\":\"%s\",\"timestamp\":\"%s\",\"sensorType\":\"%s\",\"readingType\":\"analog\","
        "\"value\":%.3f,\"unit\":\"%s\",\"metadata\":{\"source\":\"pico-w\",\"gpio\":%u,\"attempt\":%u}}",
        device_id,
        iso_time,
        event->sensor_type,
        event->analog_value,
        event->unit,
        event->gpio,
        event->attempts
      );
    } else {
      written = snprintf(
        out_json,
        out_json_size,
        "{\"deviceId\":\"%s\",\"timestamp\":\"%s\",\"sensorType\":\"%s\",\"readingType\":\"analog\","
        "\"value\":%.3f,\"metadata\":{\"source\":\"pico-w\",\"gpio\":%u,\"attempt\":%u}}",
        device_id,
        iso_time,
        event->sensor_type,
        event->analog_value,
        event->gpio,
        event->attempts
      );
    }
  } else {
    written = snprintf(
      out_json,
      out_json_size,
      "{\"deviceId\":\"%s\",\"timestamp\":\"%s\",\"sensorType\":\"%s\",\"readingType\":\"discrete\","
      "\"value\":\"%s\",\"metadata\":{\"source\":\"pico-w\",\"gpio\":%u,\"attempt\":%u}}",
      device_id,
      iso_time,
      event->sensor_type,
      event->discrete_value,
      event->gpio,
      event->attempts
    );
  }

  if (written <= 0 || (size_t)written >= out_json_size) {
    return 0u;
  }

  return (size_t)written;
}