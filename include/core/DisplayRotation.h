#pragma once

#include <stdint.h>

// Global display orientation.
//
// The physical serpentine wiring of the panel never changes; this is a purely
// logical transformation applied ABOVE that mapping. Animations always draw in
// normal upright 16x16 logical coordinates and are unaware of it.
enum class DisplayRotation : uint8_t {
    DEG_0 = 0,
    DEG_90 = 1,
    DEG_180 = 2,
    DEG_270 = 3
};

constexpr uint8_t kDisplayRotationCount = 4u;

// True for the four values above. Used to reject a corrupt stored value.
inline bool isDisplayRotationValid(DisplayRotation rotation) {
    return static_cast<uint8_t>(rotation) < kDisplayRotationCount;
}

// Compact 0..3 form, for persistence and for the control panel.
inline uint8_t displayRotationToOrdinal(DisplayRotation rotation) {
    return static_cast<uint8_t>(rotation);
}

// Maps 0..3 back to a rotation. Anything out of range becomes DEG_0, so an
// unexpected value can never leave the panel in an undefined orientation.
inline DisplayRotation displayRotationFromOrdinal(uint8_t ordinal) {
    if (ordinal >= kDisplayRotationCount) {
        return DisplayRotation::DEG_0;
    }

    return static_cast<DisplayRotation>(ordinal);
}

// Degrees for display, e.g. 0 / 90 / 180 / 270.
inline uint16_t displayRotationDegrees(DisplayRotation rotation) {
    return static_cast<uint16_t>(displayRotationToOrdinal(rotation)) * 90u;
}

// Maps degrees as submitted by the panel to a rotation. Only the four exact
// values are accepted; anything else yields DEG_0.
inline DisplayRotation displayRotationFromDegrees(uint16_t degrees) {
    switch (degrees) {
        case 90u:
            return DisplayRotation::DEG_90;
        case 180u:
            return DisplayRotation::DEG_180;
        case 270u:
            return DisplayRotation::DEG_270;
        default:
            return DisplayRotation::DEG_0;
    }
}
