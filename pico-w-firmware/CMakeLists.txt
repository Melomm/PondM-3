cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED ENV{PICO_SDK_PATH})
  message(FATAL_ERROR "PICO_SDK_PATH nao definido. Exporte o caminho para o Pico SDK antes de compilar.")
endif()

set(PICO_BOARD pico_w CACHE STRING "Target board")
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico_w_telemetry_firmware C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

if(NOT TARGET pico_lwip)
  message(FATAL_ERROR "lwIP indisponivel no Pico SDK. Execute: git -C $ENV{PICO_SDK_PATH} submodule update --init lib/lwip")
endif()

if(NOT TARGET pico_cyw43_arch_lwip_threadsafe_background)
  message(FATAL_ERROR "Suporte Wi-Fi Pico W indisponivel no Pico SDK. Verifique submodulos do cyw43-driver/lwip.")
endif()

add_executable(pico_w_telemetry
  src/main.c
  src/network.c
  src/sensors.c
  src/telemetry.c
)

target_include_directories(pico_w_telemetry PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(pico_w_telemetry PRIVATE
  pico_stdlib
  hardware_adc
  hardware_irq
  hardware_timer
  hardware_sync
  pico_lwip
  pico_cyw43_arch_lwip_threadsafe_background
)

pico_enable_stdio_uart(pico_w_telemetry 1)
pico_enable_stdio_usb(pico_w_telemetry 0)
pico_add_extra_outputs(pico_w_telemetry)