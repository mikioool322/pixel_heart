#pragma once

#include "network/CredentialStoreLogic.h"
#include "network/NetworkCredentialStore.h"
#include "network/NetworkCredentials.h"

#if defined(ARDUINO_ARCH_ESP32)

// Preferences / NVS backed NvsKeyValueStore.
//
// Mirrors the ESP32 core's Preferences semantics exactly, including the rule
// that a zero-length blob write fails. Deliberately one object, one handle.
class Esp32PreferencesNvs final : public NvsKeyValueStore {
public:
    Esp32PreferencesNvs();

    bool begin(const char* name, bool readOnly) override;
    void end() override;

    bool isKey(const char* key) override;
    size_t getBytesLength(const char* key) override;
    size_t getBytes(const char* key, void* buffer, size_t maxLen) override;
    size_t putBytes(const char* key, const void* value, size_t len) override;

    bool getBool(const char* key, bool defaultValue) override;
    bool putBool(const char* key, bool value) override;

    bool remove(const char* key) override;

private:
    bool open_;
};

// NetworkCredentialStore backed by NVS via CredentialStoreLogic.
//
// This class now contains no persistence policy of its own: the encoding,
// commit ordering and validation all live in the portable, unit-tested
// CredentialStoreLogic.
class Esp32CredentialStore final : public NetworkCredentialStore {
public:
    Esp32CredentialStore();

    bool load(NetworkCredentials& out) override;
    bool save(const NetworkCredentials& credentials) override;
    bool clear() override;

private:
    CredentialStoreLogic logic_;
};

// The single NVS key/value handle shared by everything that persists state.
//
// There must be exactly ONE of these. The ESP32 Preferences API refuses a
// second begin() on a handle that is still open, so two wrappers over the same
// backend would silently make whichever one ran second fail and fall back to
// defaults. All callers do begin -> work -> end, so one shared instance keeps
// the operations properly serialised.
NvsKeyValueStore& esp32SharedNvs();

#endif  // defined(ARDUINO_ARCH_ESP32)
