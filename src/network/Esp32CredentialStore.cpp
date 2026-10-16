#include "network/Esp32CredentialStore.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <Arduino.h>
#include <Preferences.h>
#include <stdint.h>
#include <string.h>

namespace {
// Single backend instance: exactly one open NVS handle at a time.
Preferences gPreferences;
}  // namespace

Esp32PreferencesNvs::Esp32PreferencesNvs()
    : open_(false) {
}

bool Esp32PreferencesNvs::begin(const char* name, bool readOnly) {
    if (open_) {
        // Mirrors the core: a second begin() on a live handle fails.
        return false;
    }

    open_ = gPreferences.begin(name, readOnly);
    if (!open_) {
        Serial.println(F("[NVS] opening namespace failed"));
        return false;
    }

    Serial.print(F("[NVS] opening namespace "));
    Serial.println(name);
    return true;
}

void Esp32PreferencesNvs::end() {
    if (!open_) {
        return;
    }

    // Flushes the pending commit and releases the handle.
    gPreferences.end();
    open_ = false;
}

bool Esp32PreferencesNvs::isKey(const char* key) {
    return open_ && gPreferences.isKey(key);
}

size_t Esp32PreferencesNvs::getBytesLength(const char* key) {
    if (!open_) {
        return 0u;
    }

    // Use the core's own length query rather than trusting a fixed size.
    return gPreferences.getBytesLength(key);
}

size_t Esp32PreferencesNvs::getBytes(const char* key, void* buffer, size_t maxLen) {
    if (!open_) {
        return 0u;
    }

    return gPreferences.getBytes(key, buffer, maxLen);
}

size_t Esp32PreferencesNvs::putBytes(const char* key, const void* value, size_t len) {
    if (!open_) {
        return 0u;
    }

    return gPreferences.putBytes(key, value, len);
}

bool Esp32PreferencesNvs::getBool(const char* key, bool defaultValue) {
    if (!open_) {
        return defaultValue;
    }

    return gPreferences.getBool(key, defaultValue);
}

bool Esp32PreferencesNvs::putBool(const char* key, bool value) {
    if (!open_) {
        return false;
    }

    return gPreferences.putBool(key, value);
}

bool Esp32PreferencesNvs::remove(const char* key) {
    if (!open_) {
        return false;
    }

    return gPreferences.remove(key);
}

Esp32CredentialStore::Esp32CredentialStore()
    : logic_(esp32SharedNvs(), true) {
}

NvsKeyValueStore& esp32SharedNvs() {
    static Esp32PreferencesNvs instance;
    return instance;
}

bool Esp32CredentialStore::load(NetworkCredentials& out) {
    return logic_.load(out);
}

bool Esp32CredentialStore::save(const NetworkCredentials& credentials) {
    return logic_.save(credentials);
}

bool Esp32CredentialStore::clear() {
    return logic_.clear();
}

#endif  // defined(ARDUINO_ARCH_ESP32)
