#pragma once

#include <stdint.h>
#include <stddef.h>

#define ARDUINO_ARCH_ESP32
#define D6 6

// Native test double. Provides only what the shared code under test touches.
// Network / NTP implementations are excluded from native tests by their
// ARDUINO_ARCH_ESP32 guards being handled per-file, so nothing here needs to
// emulate WiFi, NVS or SNTP.
inline uint32_t millis() {
    return 0u;
}
