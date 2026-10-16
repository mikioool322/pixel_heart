#pragma once

#include <stdint.h>

#include "control/UserSettings.h"
#include "network/CredentialStoreLogic.h"  // NvsKeyValueStore
#include "network/NetworkDefaults.h"

// Persistence for the user-facing settings: mode, selected animation and
// brightness.
//
// Uses the SAME NVS namespace as the Wi-Fi credentials but entirely separate
// keys, so a corrupt or missing settings record can never touch — let alone
// erase — a working Wi-Fi configuration.
//
// Each value is stored as a single byte, the same way the credentials store
// their payloads, so no new storage mechanism or interface is introduced.
//
// `set_version` is the commit marker and is written LAST: a power loss midway
// leaves a payload without a valid version, which is treated as "no settings"
// and falls back to defaults.
namespace SettingsStoreKeys {

constexpr const char* kVersion = "set_version";
constexpr const char* kMode = "set_mode";
constexpr const char* kAnimation = "set_anim";
constexpr const char* kBrightness = "set_bright";
constexpr const char* kRotation = "set_rot";

// Bump when the meaning of a stored value changes.
constexpr uint8_t kVersionValue = 1u;

}  // namespace SettingsStoreKeys

// Portable settings persistence, independent of the storage backend.
class SettingsStoreLogic {
public:
    explicit SettingsStoreLogic(NvsKeyValueStore& nvs)
        : nvs_(nvs) {}

    // Reads stored settings into `out`. Returns false (leaving `out`
    // untouched) when nothing usable is stored, so the caller keeps defaults.
    bool load(UserSettings& out);

    // Writes settings. Only called after a value has been accepted by a user
    // action, never from the main loop.
    bool save(const UserSettings& settings);

private:
    bool readByte(const char* key, uint8_t& out);
    bool writeByte(const char* key, uint8_t value);

    NvsKeyValueStore& nvs_;
};

inline bool SettingsStoreLogic::readByte(const char* key, uint8_t& out) {
    if (nvs_.getBytesLength(key) != 1u) {
        return false;
    }

    uint8_t value = 0u;
    if (nvs_.getBytes(key, &value, 1u) != 1u) {
        return false;
    }

    out = value;
    return true;
}

inline bool SettingsStoreLogic::writeByte(const char* key, uint8_t value) {
    return nvs_.putBytes(key, &value, 1u) == 1u;
}

inline bool SettingsStoreLogic::save(const UserSettings& settings) {
    if (!isUserSettingsValid(settings)) {
        return false;
    }

    if (!nvs_.begin(NetworkDefaults::kNvsNamespace, false)) {
        return false;
    }

    const bool payloadOk =
        writeByte(SettingsStoreKeys::kMode, static_cast<uint8_t>(settings.mode)) &&
        writeByte(SettingsStoreKeys::kAnimation, static_cast<uint8_t>(settings.animation)) &&
        writeByte(SettingsStoreKeys::kBrightness, settings.brightnessPercent) &&
        writeByte(SettingsStoreKeys::kRotation, displayRotationToOrdinal(settings.rotation));

    bool committed = false;
    if (payloadOk) {
        committed = writeByte(SettingsStoreKeys::kVersion, SettingsStoreKeys::kVersionValue);
    }

    // Release the handle so the credential store can use it next.
    nvs_.end();

    return payloadOk && committed;
}

inline bool SettingsStoreLogic::load(UserSettings& out) {
    if (!nvs_.begin(NetworkDefaults::kNvsNamespace, true)) {
        return false;
    }

    bool ok = false;
    UserSettings candidate = out;

    do {
        uint8_t version = 0u;
        if (!readByte(SettingsStoreKeys::kVersion, version)) {
            break;
        }
        if (version != SettingsStoreKeys::kVersionValue) {
            // Unknown layout: ignore it rather than misinterpreting the bytes.
            break;
        }

        uint8_t mode = 0u;
        uint8_t animation = 0u;
        uint8_t brightness = 0u;
        if (!readByte(SettingsStoreKeys::kMode, mode)) {
            break;
        }
        if (!readByte(SettingsStoreKeys::kAnimation, animation)) {
            break;
        }
        if (!readByte(SettingsStoreKeys::kBrightness, brightness)) {
            break;
        }

        candidate.mode = static_cast<ControlMode>(mode);
        candidate.animation = static_cast<AnimationId>(animation);
        candidate.brightnessPercent = brightness;

        // Rotation is read LENIENTLY, unlike the three fields above. Records
        // written before rotation existed have no set_rot key, and a corrupt
        // value must not throw away otherwise-good settings: both cases simply
        // mean "no rotation stored" -> DEG_0. This is what lets an old
        // installation keep booting unchanged.
        uint8_t rotation = 0u;
        if (readByte(SettingsStoreKeys::kRotation, rotation) &&
            rotation < kDisplayRotationCount) {
            candidate.rotation = displayRotationFromOrdinal(rotation);
        } else {
            candidate.rotation = DisplayRotation::DEG_0;
        }

        // The same validation the panel uses. Anything impossible falls back
        // to the caller's defaults instead of reaching the matrix.
        if (!isUserSettingsValid(candidate)) {
            break;
        }

        out = candidate;
        ok = true;
    } while (false);

    nvs_.end();
    return ok;
}
