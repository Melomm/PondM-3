#include "sensors.h"

#include "config.h"

#include <stdbool.h>
#include <stdint.h>

#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/structs/io_bank0.h"
#include "hardware/structs/sio.h"
#include "hardware/sync.h"
#include "pico/time.h"

enum {
  GPIO_EVT_EDGE_LOW_BIT = 2,
  GPIO_EVT_EDGE_HIGH_BIT = 3
};

static volatile bool g_periodic_tick = false;
static volatile bool g_digital_changed = false;
static volatile bool g_digital_state = false;
static volatile uint64_t g_last_irq_ms = 0;
static volatile uint64_t g_digital_change_ms = 0;
static uint32_t g_irq_bank = 0;
static uint32_t g_irq_mask = 0;
static struct repeating_timer g_sampling_timer;
static bool g_irq_handler_registered = false;

static inline uint32_t gpio_event_mask_for_pin(uint gpio) {
  const uint32_t shift = (gpio % 8u) * 4u;
  return (1u << (shift + GPIO_EVT_EDGE_LOW_BIT)) | (1u << (shift + GPIO_EVT_EDGE_HIGH_BIT));
}

static bool sampling_timer_callback(__unused struct repeating_timer *timer) {
  g_periodic_tick = true;
  return true;
}

static void gpio_bank0_irq_handler(void) {
  const uint64_t now_ms = to_ms_since_boot(get_absolute_time());

  for (uint32_t bank = 0; bank < 4u; ++bank) {
    const uint32_t pending = io_bank0_hw->proc0_irq_ctrl.ints[bank];
    if (pending == 0u) {
      continue;
    }

    if (bank == g_irq_bank && (pending & g_irq_mask) != 0u) {
      if ((now_ms - g_last_irq_ms) >= DIGITAL_DEBOUNCE_MS) {
        const bool level = ((sio_hw->gpio_in >> DIGITAL_SENSOR_GPIO) & 0x1u) != 0u;
        if (level != g_digital_state) {
          g_digital_state = level;
          g_digital_changed = true;
          g_digital_change_ms = now_ms;
        }
      }
      g_last_irq_ms = now_ms;
    }

    // W1C register for latched events.
    io_bank0_hw->intr[bank] = pending;
  }
}

bool sensors_init(void) {
  adc_init();
  adc_gpio_init(ANALOG_SENSOR_GPIO);
  adc_select_input(ANALOG_SENSOR_ADC_INPUT);

  gpio_init(DIGITAL_SENSOR_GPIO);
  gpio_set_dir(DIGITAL_SENSOR_GPIO, GPIO_IN);
  gpio_pull_up(DIGITAL_SENSOR_GPIO);
  g_digital_state = ((sio_hw->gpio_in >> DIGITAL_SENSOR_GPIO) & 0x1u) != 0u;

  g_irq_bank = DIGITAL_SENSOR_GPIO / 8u;
  g_irq_mask = gpio_event_mask_for_pin(DIGITAL_SENSOR_GPIO);

  io_bank0_hw->intr[g_irq_bank] = g_irq_mask;
  io_bank0_hw->proc0_irq_ctrl.inte[g_irq_bank] |= g_irq_mask;

  if (!g_irq_handler_registered) {
    irq_add_shared_handler(IO_IRQ_BANK0, gpio_bank0_irq_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    g_irq_handler_registered = true;
  }
  irq_set_enabled(IO_IRQ_BANK0, true);

  return add_repeating_timer_ms((int32_t)SENSOR_SAMPLE_PERIOD_MS, sampling_timer_callback, NULL, &g_sampling_timer);
}

bool sensors_consume_periodic_tick(void) {
  const uint32_t irq_state = save_and_disable_interrupts();
  const bool had_tick = g_periodic_tick;
  g_periodic_tick = false;
  restore_interrupts(irq_state);
  return had_tick;
}

bool sensors_consume_digital_change(bool *new_state, uint64_t *change_ms) {
  if (new_state == NULL || change_ms == NULL) {
    return false;
  }

  const uint32_t irq_state = save_and_disable_interrupts();
  if (!g_digital_changed) {
    restore_interrupts(irq_state);
    return false;
  }

  *new_state = g_digital_state;
  *change_ms = g_digital_change_ms;
  g_digital_changed = false;
  restore_interrupts(irq_state);
  return true;
}

void sensors_capture_snapshot(sensor_snapshot_t *out) {
  if (out == NULL) {
    return;
  }

  const uint16_t raw = adc_read();
  const float voltage = ((float)raw * 3.3f) / 4095.0f;
  float scaled = ANALOG_PHYSICAL_MIN + (voltage / 3.3f) * (ANALOG_PHYSICAL_MAX - ANALOG_PHYSICAL_MIN);
  if (scaled < ANALOG_PHYSICAL_MIN) {
    scaled = ANALOG_PHYSICAL_MIN;
  }
  if (scaled > ANALOG_PHYSICAL_MAX) {
    scaled = ANALOG_PHYSICAL_MAX;
  }

  out->adc_raw = raw;
  out->adc_voltage = voltage;
  out->analog_physical = scaled;
  out->digital_state = ((sio_hw->gpio_in >> DIGITAL_SENSOR_GPIO) & 0x1u) != 0u;
  out->sampled_ms = to_ms_since_boot(get_absolute_time());
}