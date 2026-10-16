#pragma once

#include <Arduino.h>

namespace HardwareConfig {
#if defined(ARDUINO_AVR_UNO)
constexpr uint8_t LED_DATA_PIN = 6;
constexpr uint8_t DEFAULT_BRIGHTNESS = 5;
#elif defined(ARDUINO_ARCH_ESP32)
constexpr uint8_t LED_DATA_PIN = D6;
constexpr uint8_t DEFAULT_BRIGHTNESS = 30;
#else
#error "Unsupported board for Pixel Heart Phase 1"
#endif
}  // namespace HardwareConfig
