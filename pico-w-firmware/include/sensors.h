#ifndef SENSORS_H
#define SENSORS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint16_t adc_raw;
  float adc_voltage;
  float analog_physical;
  bool digital_state;
  uint64_t sampled_ms;
} sensor_snapshot_t;

bool sensors_init(void);
bool sensors_consume_periodic_tick(void);
void sensors_capture_snapshot(sensor_snapshot_t *out);
bool sensors_consume_digital_change(bool *new_state, uint64_t *change_ms);

#endif