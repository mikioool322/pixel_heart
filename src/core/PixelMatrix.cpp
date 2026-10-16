#include "core/PixelMatrix.h"

PixelMatrix::PixelMatrix()
    : pixels(LED_COUNT, HardwareConfig::LED_DATA_PIN, NEO_GRB + NEO_KHZ800),
      dirty_(false),
      rotation_(DisplayRotation::DEG_0),
      brightnessRaw_(LedPowerBudget::kMaxSafeBrightnessRaw) {
    // Adafruit_NeoPixel defaults to brightness 0, which means "no scaling" -
    // i.e. unlimited output. Arm the safety ceiling immediately so the strip
    // can never transmit an unbounded frame, even if a caller forgets to set
    // a brightness before the first show().
    pixels.setBrightness(brightnessRaw_);
}

void PixelMatrix::begin() {
    pixels.begin();
    dirty_ = false;
}

void PixelMatrix::clear() {
    pixels.clear();
    dirty_ = true;
}

void PixelMatrix::setRotation(DisplayRotation rotation) {
    // Reject anything unexpected rather than leaving the panel undefined.
    if (!isDisplayRotationValid(rotation) || rotation == rotation_) {
        return;
    }

    rotation_ = rotation;

    // Force a redraw: the pixels already in the strip are in the old
    // orientation, and animations only repaint on their own schedule.
    // clear() marks the frame dirty, so showIfDirty() will transmit once the
    // next animation update repaints. No extra framebuffer is involved.
    clear();
}

void PixelMatrix::transformCoordinates(
    uint8_t x,
    uint8_t y,
    uint8_t& outX,
    uint8_t& outY) const {
    switch (rotation_) {
        case DisplayRotation::DEG_90:
            // Clockwise: the old top-left corner moves to the top-right.
            outX = static_cast<uint8_t>(WIDTH - 1u - y);
            outY = x;
            break;

        case DisplayRotation::DEG_180:
            outX = static_cast<uint8_t>(WIDTH - 1u - x);
            outY = static_cast<uint8_t>(HEIGHT - 1u - y);
            break;

        case DisplayRotation::DEG_270:
            // Counter-clockwise.
            outX = y;
            outY = static_cast<uint8_t>(HEIGHT - 1u - x);
            break;

        case DisplayRotation::DEG_0:
        default:
            outX = x;
            outY = y;
            break;
    }
}

void PixelMatrix::setPixel(uint8_t x, uint8_t y, Color color) {
    // Validate in LOGICAL space, before any rotation arithmetic, so an
    // out-of-range coordinate can never underflow WIDTH - 1 - x.
    if (x >= WIDTH || y >= HEIGHT) {
        return;
    }

    uint8_t physicalX = 0u;
    uint8_t physicalY = 0u;
    transformCoordinates(x, y, physicalX, physicalY);

    // The existing serpentine mapping, unchanged: even rows left-to-right,
    // odd rows right-to-left.
    pixels.setPixelColor(xyToIndex(physicalX, physicalY), color.r, color.g, color.b);
    dirty_ = true;
}

void PixelMatrix::fill(Color color) {
    for (uint16_t i = 0; i < LED_COUNT; ++i) {
        pixels.setPixelColor(i, color.r, color.g, color.b);
    }
    dirty_ = true;
}

void PixelMatrix::setBrightness(uint8_t brightness) {
    // THE single LED safety authority.
    //
    // Every brightness request - from boot, from the web panel, from anywhere -
    // funnels through here, and this is the only place the strip's brightness
    // is ever written. The request is a *request*: it is clamped to the
    // firmware ceiling, so no user setting, animation or schedule can push the
    // output past the current budget.
    //
    // Clamping (rather than multiplying by a safety factor) means a request
    // at or below the ceiling is served exactly as before, so the shipped
    // default brightness is unaffected.
    const uint8_t safe = LedPowerBudget::clampBrightnessRaw(brightness);

    pixels.setBrightness(safe);
    brightnessRaw_ = safe;
    dirty_ = true;
}

void PixelMatrix::show() {
    pixels.show();
    dirty_ = false;
}

void PixelMatrix::showIfDirty() {
    if (!dirty_) {
        return;
    }

    pixels.show();
    dirty_ = false;
}

uint32_t PixelMatrix::pixelColorAt(uint16_t index) const {
    return pixels.getPixelColor(index);
}

bool PixelMatrix::isBlack() const {
    for (uint16_t i = 0u; i < LED_COUNT; ++i) {
        if (pixels.getPixelColor(i) != 0u) {
            return false;
        }
    }
    return true;
}
