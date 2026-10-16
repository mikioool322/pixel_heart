#pragma once

#include <stdint.h>

#include "assets/PixelMask.h"
#include "core/PixelMatrix.h"

namespace HeartAssets {

#if defined(ARDUINO_ARCH_AVR)
#define HEART_ASSET_STORAGE PROGMEM
#else
#define HEART_ASSET_STORAGE
#endif

static const PixelMask16 kHeartMask HEART_ASSET_STORAGE = {{
    0x781E,
    0xFC3F,
    0xFE7F,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0x7FFE,
    0x7FFE,
    0x3FFC,
    0x1FF8,
    0x1FF8,
    0x0FF0,
    0x07E0,
    0x07E0,
    0x03C0,
    0x0180
}};

#undef HEART_ASSET_STORAGE

inline bool contains(int16_t x, int16_t y) {
    if (x < 0 || y < 0 || x >= PixelMatrix::WIDTH || y >= PixelMatrix::HEIGHT) {
        return false;
    }

    return maskHasPixel(
        &kHeartMask,
        static_cast<uint8_t>(x),
        static_cast<uint8_t>(y)
    );
}

inline bool isOutlinePixel(uint8_t x, uint8_t y) {
    if (!contains(static_cast<int16_t>(x), static_cast<int16_t>(y))) {
        return false;
    }

    return
        !contains(static_cast<int16_t>(x) - 1, static_cast<int16_t>(y)) ||
        !contains(static_cast<int16_t>(x) + 1, static_cast<int16_t>(y)) ||
        !contains(static_cast<int16_t>(x), static_cast<int16_t>(y) - 1) ||
        !contains(static_cast<int16_t>(x), static_cast<int16_t>(y) + 1);
}

}  // namespace HeartAssets
