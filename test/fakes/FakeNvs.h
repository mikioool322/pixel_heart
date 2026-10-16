#pragma once

#include <stdint.h>
#include <string.h>

#include "network/CredentialStoreLogic.h"

// In-memory NvsKeyValueStore that reproduces the ESP32 Preferences semantics
// that matter, so the portable persistence logic is exercised under the same
// constraints it meets on the device:
//
//   * begin() fails if the backend is already open
//   * putBytes() FAILS for a zero-length payload
//   * getBytes() returns 0 when the stored length exceeds maxLen
//   * getBytes() does not NUL-terminate
//   * remove() erases a key
//
// Crucially this backend OUTLIVES any store or manager object built on top of
// it, so a test can destroy one store and construct another over the same
// bytes: the native equivalent of a power cycle.
class FakeNvs final : public NvsKeyValueStore {
public:
    static constexpr uint8_t kMaxKeys = 8u;
    static constexpr uint8_t kMaxValueBytes = 128u;

    FakeNvs() {
        open_ = false;
        readOnly_ = false;
        keyCount_ = 0u;
        failNextPutBytes_ = false;
        writeCount_ = 0u;
        memset(keys_, 0, sizeof(keys_));
        memset(values_, 0, sizeof(values_));
        memset(valueLengths_, 0, sizeof(valueLengths_));
    }

    bool begin(const char* name, bool readOnly) override {
        if (open_) {
            return false;  // mirrors Preferences::begin on a live handle
        }

        (void)name;
        open_ = true;
        readOnly_ = readOnly;
        return true;
    }

    void end() override {
        open_ = false;
    }

    bool isKey(const char* key) override {
        return find(key) >= 0;
    }

    size_t getBytesLength(const char* key) override {
        const int index = find(key);
        if (index < 0) {
            return 0u;
        }
        return valueLengths_[index];
    }

    size_t getBytes(const char* key, void* buffer, size_t maxLen) override {
        const size_t length = getBytesLength(key);
        if (length == 0u || buffer == nullptr || maxLen == 0u) {
            return length;
        }
        if (length > maxLen) {
            return 0u;  // mirrors the core's "not enough space" behaviour
        }

        memcpy(buffer, values_[find(key)], length);
        return length;
    }

    size_t putBytes(const char* key, const void* value, size_t len) override {
        // A zero-length blob write is rejected by the real Preferences API.
        if (!open_ || readOnly_ || key == nullptr || value == nullptr || len == 0u) {
            return 0u;
        }
        if (len > kMaxValueBytes) {
            return 0u;
        }
        if (failNextPutBytes_) {
            failNextPutBytes_ = false;
            return 0u;
        }

        const int index = ensure(key);
        if (index < 0) {
            return 0u;
        }

        memcpy(values_[index], value, len);
        valueLengths_[index] = static_cast<uint8_t>(len);
        ++writeCount_;
        return len;
    }

    bool getBool(const char* key, bool defaultValue) override {
        const int index = find(key);
        if (index < 0 || valueLengths_[index] != 1u) {
            return defaultValue;
        }
        return values_[index][0] != 0u;
    }

    bool putBool(const char* key, bool value) override {
        if (!open_ || readOnly_ || key == nullptr) {
            return false;
        }

        const int index = ensure(key);
        if (index < 0) {
            return false;
        }

        values_[index][0] = value ? 1u : 0u;
        valueLengths_[index] = 1u;
        ++writeCount_;
        return true;
    }

    bool remove(const char* key) override {
        const int index = find(key);
        if (index < 0) {
            return false;
        }

        for (uint8_t i = static_cast<uint8_t>(index); i + 1u < keyCount_; ++i) {
            memcpy(keys_[i], keys_[i + 1u], sizeof(keys_[i]));
            memcpy(values_[i], values_[i + 1u], sizeof(values_[i]));
            valueLengths_[i] = valueLengths_[i + 1u];
        }
        --keyCount_;
        return true;
    }

    // Test control: make the next putBytes fail, simulating a full or
    // damaged partition.
    void failNextPutBytes() {
        failNextPutBytes_ = true;
    }

    uint16_t writeCount() const {
        return writeCount_;
    }

private:
    int find(const char* key) const {
        if (key == nullptr) {
            return -1;
        }

        for (uint8_t i = 0u; i < keyCount_; ++i) {
            if (strcmp(keys_[i], key) == 0) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    int ensure(const char* key) {
        const int existing = find(key);
        if (existing >= 0) {
            return existing;
        }
        if (keyCount_ >= kMaxKeys) {
            return -1;
        }
        if (strlen(key) >= sizeof(keys_[0])) {
            return -1;
        }

        strncpy(keys_[keyCount_], key, sizeof(keys_[0]) - 1u);
        keys_[keyCount_][sizeof(keys_[0]) - 1u] = '\0';
        valueLengths_[keyCount_] = 0u;

        const int index = static_cast<int>(keyCount_);
        ++keyCount_;
        return index;
    }

    bool open_;
    bool readOnly_;
    bool failNextPutBytes_;
    uint16_t writeCount_;

    uint8_t keyCount_;
    char keys_[kMaxKeys][17];
    uint8_t values_[kMaxKeys][kMaxValueBytes];
    uint8_t valueLengths_[kMaxKeys];
};
