#pragma once

#include <stdint.h>

#if defined(ARDUINO_ARCH_AVR)
#include <avr/pgmspace.h>
#endif

#include "core/Color.h"
#include "core/PixelMatrix.h"

using PixelMaskRow = uint16_t;

struct PixelMask16 {
    PixelMaskRow rows[16];
};

inline bool maskHasPixel(const PixelMask16& mask, uint8_t x, uint8_t y) {
    if (x >= 16u || y >= 16u) {
        return false;
    }

    const PixelMaskRow row = mask.rows[y];
    return (row & (static_cast<PixelMaskRow>(1u) << (15u - x))) != 0u;
}

inline bool maskHasPixel(const PixelMask16* mask, uint8_t x, uint8_t y) {
    if (mask == nullptr || x >= 16u || y >= 16u) {
        return false;
    }

#if defined(ARDUINO_ARCH_AVR)
    const PixelMaskRow row = pgm_read_word(&mask->rows[y]);
#else
    const PixelMaskRow row = mask->rows[y];
#endif

    return (row & (static_cast<PixelMaskRow>(1u) << (15u - x))) != 0u;
}

inline void drawMask(const PixelMask16& mask, int16_t xOffset, int16_t yOffset, Color color, PixelMatrix& matrix) {
    for (uint8_t y = 0; y < 16u; ++y) {
        const PixelMaskRow row = mask.rows[y];
        for (uint8_t x = 0; x < 16u; ++x) {
            if ((row & (static_cast<PixelMaskRow>(1u) << (15u - x))) == 0u) {
                continue;
            }

            const int16_t px = xOffset + static_cast<int16_t>(x);
            const int16_t py = yOffset + static_cast<int16_t>(y);
            if (px < 0 || py < 0 || px >= PixelMatrix::WIDTH || py >= PixelMatrix::HEIGHT) {
                continue;
            }

            matrix.setPixel(static_cast<uint8_t>(px), static_cast<uint8_t>(py), color);
        }
    }
}

inline void drawMask(const PixelMask16* mask, int16_t xOffset, int16_t yOffset, Color color, PixelMatrix& matrix) {
    if (mask == nullptr) {
        return;
    }

#if defined(ARDUINO_ARCH_AVR)
    for (uint8_t y = 0; y < 16u; ++y) {
        const PixelMaskRow row = pgm_read_word(&mask->rows[y]);
        for (uint8_t x = 0; x < 16u; ++x) {
            if ((row & (static_cast<PixelMaskRow>(1u) << (15u - x))) == 0u) {
                continue;
            }

            const int16_t px = xOffset + static_cast<int16_t>(x);
            const int16_t py = yOffset + static_cast<int16_t>(y);
            if (px < 0 || py < 0 || px >= PixelMatrix::WIDTH || py >= PixelMatrix::HEIGHT) {
                continue;
            }

            matrix.setPixel(static_cast<uint8_t>(px), static_cast<uint8_t>(py), color);
        }
    }
#else
    drawMask(*mask, xOffset, yOffset, color, matrix);
#endif
}
