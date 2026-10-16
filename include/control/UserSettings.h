#pragma once

#include <stdint.h>

#include "config/HardwareConfig.h"
#include "control/AnimationCatalog.h"
#include "core/AnimationId.h"
#include "core/DisplayRotation.h"

// AUTO   = the schedule decides what plays.
// MANUAL = the user's selected animation plays, ignoring the schedule.
enum class ControlMode : uint8_t {
    AUTO = 0,
    MANUAL = 1
};

// A single control change requested from the LAN panel.
enum class ControlAction : uint8_t {
    NONE = 0,
    SET_MODE,
    SELECT_ANIMATION,
    SET_BRIGHTNESS,
    SET_ROTATION,
    PLAY
};

// Deferred control request. Produced by the LAN panel's HTTP handlers and
// applied by App on the next tick, so no state is mutated from inside
// handleClient(). Trivially copyable, no dynamic storage.
struct ControlRequest {
    ControlAction action;
    ControlMode mode;
    AnimationId animation;
    uint8_t brightnessPercent;  // absolute, for SET_BRIGHTNESS
    DisplayRotation rotation;
};

inline ControlRequest makeIdleControlRequest() {
    ControlRequest request;
    request.action = ControlAction::NONE;
    request.mode = ControlMode::AUTO;
    request.animation = AnimationId::NONE;
    request.brightnessPercent = 0u;
    request.rotation = DisplayRotation::DEG_0;
    return request;
}

// Persisted user-facing settings. One brightness value for the whole matrix.
struct UserSettings {
    ControlMode mode;
    AnimationId animation;
    uint8_t brightnessPercent;  // 0..100
    DisplayRotation rotation;
};

namespace UserSettingsDefaults {

// Derived from the board's existing default so there is exactly ONE source of
// truth for how bright the panel starts. The +127 rounding makes
// percent -> raw -> percent round-trip back to the same value.
constexpr uint8_t kBrightnessPercent = static_cast<uint8_t>(
    (static_cast<uint16_t>(HardwareConfig::DEFAULT_BRIGHTNESS) * 100u + 127u) / 255u);

// The scheduler's invalid-time fallback is AURORA, so MANUAL starts on the same
// animation the device already shows before NTP is available.
constexpr AnimationId kAnimation = AnimationId::AURORA;

constexpr ControlMode kMode = ControlMode::AUTO;

// DEG_0 is the original orientation, so an existing installation keeps looking
// exactly as it did before rotation existed.
constexpr DisplayRotation kRotation = DisplayRotation::DEG_0;

inline UserSettings make() {
    UserSettings settings;
    settings.mode = kMode;
    settings.animation = kAnimation;
    settings.brightnessPercent = kBrightnessPercent;
    settings.rotation = kRotation;
    return settings;
}

}  // namespace UserSettingsDefaults

inline uint8_t clampBrightnessPercent(int value) {
    if (value < 0) {
        return 0u;
    }
    if (value > 100) {
        return 100u;
    }
    return static_cast<uint8_t>(value);
}

// 0..100 -> 0..255 for Adafruit_NeoPixel's global brightness. This is the only
// place the two scales meet; nothing else in the firmware deals in raw values.
constexpr uint8_t brightnessPercentToRaw(uint8_t percent) {
    return static_cast<uint8_t>((static_cast<uint16_t>(percent) * 255u) / 100u);
}

// Rejects anything the panel could not have produced, so a corrupt stored value
// can never reach the matrix or the scheduler.
inline bool isUserSettingsValid(const UserSettings& settings) {
    if (settings.mode != ControlMode::AUTO && settings.mode != ControlMode::MANUAL) {
        return false;
    }
    if (!AnimationCatalog::isSelectable(settings.animation)) {
        return false;
    }
    if (!isDisplayRotationValid(settings.rotation)) {
        return false;
    }
    return settings.brightnessPercent <= 100u;
}
