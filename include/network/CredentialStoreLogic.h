#pragma once

#include <stddef.h>
#include <stdint.h>

#include "network/NetworkCredentials.h"

// Platform-neutral key/value blob backend, shaped after the ESP32
// Preferences API so the real ESP32 adapter and the native test double can
// both implement it with identical semantics.
//
// The semantics that matter, and that a test double MUST reproduce:
//   * begin() fails if the backend is already open
//   * putBytes() FAILS (returns 0) when len == 0
//   * getBytes() returns 0 when the stored length exceeds maxLen
//   * getBytes() does NOT NUL-terminate
class NvsKeyValueStore {
public:
    virtual ~NvsKeyValueStore() = default;

    virtual bool begin(const char* name, bool readOnly) = 0;
    virtual void end() = 0;

    virtual bool isKey(const char* key) = 0;

    virtual size_t getBytesLength(const char* key) = 0;
    virtual size_t getBytes(const char* key, void* buffer, size_t maxLen) = 0;
    virtual size_t putBytes(const char* key, const void* value, size_t len) = 0;

    virtual bool getBool(const char* key, bool defaultValue) = 0;
    virtual bool putBool(const char* key, bool value) = 0;

    virtual bool remove(const char* key) = 0;
};

// Wi-Fi credential persistence algorithm, independent of the storage backend.
//
// Stored representation (one consistent model for both directions):
//   each field is a blob of exactly strlen(value) + 1 bytes, i.e. the string
//   characters INCLUDING the terminating NUL.
//
// Storing the terminator is deliberate: it makes an empty password (an open
// network) a legal 1-byte blob. The raw-byte representation used previously
// could never persist an open network, because the backend rejects a
// zero-length write.
//
// Commit protocol:
//   1. write the validity marker as false  (invalidate any previous record)
//   2. write the SSID,      verifying the byte count
//   3. write the password,  verifying the byte count
//   4. write the validity marker as true LAST
// A failure or power loss at any earlier step leaves valid == false, so a
// half-written record is never accepted as complete.
class CredentialStoreLogic {
public:
    explicit CredentialStoreLogic(NvsKeyValueStore& nvs, bool verbose = false);

    // Reads and validates the stored record. Never erases anything.
    bool load(NetworkCredentials& out);

    // Persists ONLY after the caller has proven the credentials work.
    bool save(const NetworkCredentials& credentials);

    // Explicit "forget Wi-Fi". Never called by the boot or reconnect paths.
    bool clear();

    static uint32_t fingerprint(const NetworkCredentials& credentials);

private:
    void log(const char* fmt, ...) const;

    NvsKeyValueStore& nvs_;
    bool verbose_;
};
