#ifndef CONFIG_H
#define CONFIG_H

// Device identity sent to backend payload.
#ifndef TELEMETRY_DEVICE_ID
#define TELEMETRY_DEVICE_ID "pico-w-01"
#endif

// Wi-Fi credentials (WPA2-PSK).
#ifndef WIFI_SSID
#define WIFI_SSID "NOME DA SUA REDE"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "SENHA DA SUA REDE"
#endif

// Backend endpoint from part 1.
#ifndef BACKEND_HOST
#define BACKEND_HOST "SEU IPV4 DA SUA REDE"
#endif

#ifndef BACKEND_PORT
#define BACKEND_PORT 3000
#endif

#ifndef BACKEND_PATH
#define BACKEND_PATH "/telemetry"
#endif

#ifndef TELEMETRY_EPOCH_UNIX
#define TELEMETRY_EPOCH_UNIX 1774569600u
#endif

#ifndef SENSOR_SAMPLE_PERIOD_MS
#define SENSOR_SAMPLE_PERIOD_MS 1000u
#endif

#ifndef DIGITAL_DEBOUNCE_MS
#define DIGITAL_DEBOUNCE_MS 60u
#endif

#ifndef MAIN_LOOP_DELAY_MS
#define MAIN_LOOP_DELAY_MS 25u
#endif

#ifndef TELEMETRY_QUEUE_CAPACITY
#define TELEMETRY_QUEUE_CAPACITY 64u
#endif

#ifndef TELEMETRY_MAX_RETRIES
#define TELEMETRY_MAX_RETRIES 8u
#endif

#ifndef TELEMETRY_RETRY_BASE_MS
#define TELEMETRY_RETRY_BASE_MS 1000u
#endif

#ifndef TELEMETRY_RETRY_MAX_MS
#define TELEMETRY_RETRY_MAX_MS 30000u
#endif

#ifndef WIFI_RETRY_BASE_MS
#define WIFI_RETRY_BASE_MS 2000u
#endif

#ifndef WIFI_RETRY_MAX_MS
#define WIFI_RETRY_MAX_MS 60000u
#endif

#ifndef WIFI_CONNECT_TIMEOUT_MS
#define WIFI_CONNECT_TIMEOUT_MS 10000u
#endif

#ifndef HTTP_TIMEOUT_MS
#define HTTP_TIMEOUT_MS 4000u
#endif

// Sensor pins.
#ifndef ANALOG_SENSOR_GPIO
#define ANALOG_SENSOR_GPIO 26u
#endif

#ifndef ANALOG_SENSOR_ADC_INPUT
#define ANALOG_SENSOR_ADC_INPUT 0u
#endif

#ifndef DIGITAL_SENSOR_GPIO
#define DIGITAL_SENSOR_GPIO 15u
#endif

// Physical mapping for analog sensor value (0-3.3V -> 0-100%).
#ifndef ANALOG_PHYSICAL_MIN
#define ANALOG_PHYSICAL_MIN 0.0f
#endif

#ifndef ANALOG_PHYSICAL_MAX
#define ANALOG_PHYSICAL_MAX 100.0f
#endif

#endif