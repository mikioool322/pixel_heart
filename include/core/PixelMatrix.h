#pragma once

#include <stdint.h>

#include <Adafruit_NeoPixel.h>

#include "config/HardwareConfig.h"
#include "config/LedPowerBudget.h"
#include "core/Color.h"
#include "core/DisplayRotation.h"

class PixelMatrix {
public:
    static constexpr uint8_t WIDTH = 16;
    static constexpr uint8_t HEIGHT = 16;
    static constexpr uint16_t LED_COUNT = WIDTH * HEIGHT;

    PixelMatrix();

    void begin();
    void clear();
    void setPixel(uint8_t x, uint8_t y, Color color);
    void fill(Color color);
    void setBrightness(uint8_t brightness);
    void show();
    void showIfDirty();
    bool isDirty() const { return dirty_; }

    // Read-back of the logical framebuffer, for tests and diagnostics.
    //
    // This exists because the quiet-hours bug was invisible to manager-only
    // assertions: the manager reported NONE while the framebuffer still held a
    // full sun picture. Exposing the actual pixel values is what lets a test
    // assert "logically off" instead of "logically claims to be off".
    uint32_t pixelColorAt(uint16_t index) const;

    // True when every LED is black. The single assertion that expresses
    // "the physical display agrees with logical NONE".
    bool isBlack() const;

    // Brightness actually handed to the strip, after the safety ceiling has
    // been applied. Exposed for tests and diagnostics; never above
    // LedPowerBudget::kMaxSafeBrightnessRaw.
    uint8_t effectiveBrightnessRaw() const { return brightnessRaw_; }

    // Global display orientation. Defaults to DEG_0, which is the original
    // behaviour, so the panel looks identical until the user changes it.
    //
    // PixelMatrix only receives the setting; it never reads storage or serves
    // HTTP. The owner is App, which is also what persists it.
    void setRotation(DisplayRotation rotation);
    DisplayRotation rotation() const { return rotation_; }

    static constexpr uint16_t xyToIndexStatic(uint8_t x, uint8_t y) {
        return (x >= WIDTH || y >= HEIGHT)
            ? 0u
            : ((y & 1u) == 0u
                   ? (static_cast<uint16_t>(y) * WIDTH + x)
                   : (static_cast<uint16_t>(y) * WIDTH + (WIDTH - 1u - x)));
    }

    constexpr uint16_t xyToIndex(uint8_t x, uint8_t y) const {
        return xyToIndexStatic(x, y);
    }

private:
    // The ONE place logical coordinates are adjusted for orientation.
    //
    // Called from setPixel() and nowhere else, so anything drawn through
    // logical coordinates rotates exactly once. In particular drawMask() must
    // NOT call this: it already routes through setPixel(), and rotating in both
    // places would double-rotate every mask.
    //
    // `x`/`y` must already be validated as 0..15. Dimension-1 arithmetic on an
    // unvalidated uint8_t would underflow.
    void transformCoordinates(uint8_t x, uint8_t y, uint8_t& outX, uint8_t& outY) const;

    Adafruit_NeoPixel pixels;
    bool dirty_;
    DisplayRotation rotation_;

    // Last brightness actually applied to the strip, after clamping.
    uint8_t brightnessRaw_;
};

static_assert(PixelMatrix::xyToIndexStatic(0, 0) == 0, "mapping row0 col0");
static_assert(PixelMatrix::xyToIndexStatic(15, 0) == 15, "mapping row0 col15");
static_assert(PixelMatrix::xyToIndexStatic(0, 1) == 31, "mapping row1 col0");
static_assert(PixelMatrix::xyToIndexStatic(15, 1) == 16, "mapping row1 col15");
static_assert(PixelMatrix::xyToIndexStatic(0, 2) == 32, "mapping row2 col0");
static_assert(PixelMatrix::xyToIndexStatic(15, 2) == 47, "mapping row2 col15");
static_assert(PixelMatrix::xyToIndexStatic(0, 15) == 255, "mapping last row start");
static_assert(PixelMatrix::xyToIndexStatic(15, 15) == 240, "mapping last row end");
static_assert(PixelMatrix::xyToIndexStatic(16, 0) == 0, "mapping invalid x");
static_assert(PixelMatrix::xyToIndexStatic(0, 16) == 0, "mapping invalid y");
