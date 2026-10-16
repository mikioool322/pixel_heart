#include "network/CredentialStoreLogic.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "network/NetworkDefaults.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <Arduino.h>
#endif

namespace {

// Field sizes including the terminating NUL. SSID 32 chars + NUL, password
// 63 chars + NUL. These mirror NetworkCredentials' own buffers exactly.
constexpr uint8_t kSsidFieldSize = NetworkCredentials::kMaxSsidLength + 1u;      // 33
constexpr uint8_t kPasswordFieldSize = NetworkCredentials::kMaxPasswordLength + 1u; // 64

// Copies `value` plus its terminator into `out`, returning the byte count.
// Returns 0 when it does not fit.
uint8_t encodeField(const char* value, char* out, uint8_t capacity) {
    if (value == nullptr || out == nullptr || capacity == 0u) {
        return 0u;
    }

    size_t length = 0u;
    while (length + 1u < capacity && value[length] != '\0') {
        ++length;
    }

    // A string with no terminator inside the capacity is not encodable.
    if (value[length] != '\0') {
        return 0u;
    }

    for (size_t i = 0u; i < length; ++i) {
        out[i] = value[i];
    }
    out[length] = '\0';

    return static_cast<uint8_t>(length + 1u);
}

}  // namespace

CredentialStoreLogic::CredentialStoreLogic(NvsKeyValueStore& nvs, bool verbose)
    : nvs_(nvs),
      verbose_(verbose) {
}

void CredentialStoreLogic::log(const char* fmt, ...) const {
    if (!verbose_) {
        return;
    }

#if defined(ARDUINO_ARCH_ESP32)
    char buffer[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    Serial.println(buffer);
#else
    // Native tests silence this path; keeping it inert avoids polluting
    // test output while still compiling the same code.
    (void)fmt;
#endif
}

uint32_t CredentialStoreLogic::fingerprint(const NetworkCredentials& credentials) {
    // FNV-1a. Non-secret: enough to prove that the record loaded on a later
    // boot is byte-identical to the one saved, without revealing the password.
    uint32_t hash = 2166136261u;

    for (const char* p = credentials.ssid; *p != '\0'; ++p) {
        hash ^= static_cast<uint32_t>(static_cast<uint8_t>(*p));
        hash *= 16777619u;
    }

    // Separator so that {"ab","c"} and {"a","bc"} cannot collide.
    hash ^= 0xFFu;
    hash *= 16777619u;

    for (const char* p = credentials.password; *p != '\0'; ++p) {
        hash ^= static_cast<uint32_t>(static_cast<uint8_t>(*p));
        hash *= 16777619u;
    }

    return hash;
}

bool CredentialStoreLogic::save(const NetworkCredentials& credentials) {
    if (!areNetworkCredentialsValid(credentials)) {
        log("[NVS] Save refused: credentials invalid");
        return false;
    }

    if (!nvs_.begin(NetworkDefaults::kNvsNamespace, false)) {
        log("[NVS] Save failed: could not open namespace");
        return false;
    }

    log("[NVS] Saving Wi-Fi credentials");

    // Step 1: invalidate the commit marker BEFORE touching the payload, so an
    // interrupted write can never leave a valid marker over partial data.
    if (!nvs_.putBool(NetworkDefaults::kKeyValid, false)) {
        log("[NVS] Save failed: could not clear valid marker");
        nvs_.end();
        return false;
    }

    char ssidField[kSsidFieldSize];
    char passwordField[kPasswordFieldSize];

    const uint8_t ssidLength = encodeField(
        credentials.ssid, ssidField, kSsidFieldSize);
    const uint8_t passwordLength = encodeField(
        credentials.password, passwordField, kPasswordFieldSize);

    if (ssidLength == 0u || passwordLength == 0u) {
        log("[NVS] Save failed: field encoding error");
        nvs_.end();
        return false;
    }

    log("[NVS] SSID length=%u", static_cast<unsigned>(ssidLength));
    log("[NVS] Password length=%u", static_cast<unsigned>(passwordLength));

    // Step 2 + 3: payload, with the written byte count verified rather than
    // merely checked for non-zero.
    const size_t ssidWritten = nvs_.putBytes(
        NetworkDefaults::kKeySsid, ssidField, ssidLength);
    log("[NVS] put ssid result=%u", static_cast<unsigned>(ssidWritten));

    const size_t passwordWritten = nvs_.putBytes(
        NetworkDefaults::kKeyPassword, passwordField, passwordLength);
    log("[NVS] put password result=%u", static_cast<unsigned>(passwordWritten));

    const bool payloadOk =
        ssidWritten == ssidLength && passwordWritten == passwordLength;

    bool committed = false;
    if (payloadOk) {
        // Step 4: the marker becomes true only once the payload is fully
        // written and verified.
        committed = nvs_.putBool(NetworkDefaults::kKeyValid, true);
        log("[NVS] put valid result=%d", committed ? 1 : 0);
    } else {
        log("[NVS] Save failed: payload write incomplete");
    }

    // Explicit close: releases the backend handle and flushes any pending
    // commit. Leaving it open is what the previous revision did.
    nvs_.end();

    if (payloadOk && committed) {
        log(
            "[NVS] Save complete fingerprint=%08lX",
            static_cast<unsigned long>(fingerprint(credentials)));
    }

    return payloadOk && committed;
}

bool CredentialStoreLogic::load(NetworkCredentials& out) {
    clearNetworkCredentials(out);

    // Read-only: loading must never be able to mutate the record.
    if (!nvs_.begin(NetworkDefaults::kNvsNamespace, true)) {
        log("[NVS] Load failed: could not open namespace");
        return false;
    }

    bool ok = false;

    do {
        if (!nvs_.isKey(NetworkDefaults::kKeyValid)) {
            log("[NVS] wifi_valid=missing");
            break;
        }

        const bool valid = nvs_.getBool(NetworkDefaults::kKeyValid, false);
        log("[NVS] wifi_valid=%s", valid ? "true" : "false");
        if (!valid) {
            break;
        }

        const size_t ssidLength = nvs_.getBytesLength(NetworkDefaults::kKeySsid);
        const size_t passwordLength = nvs_.getBytesLength(NetworkDefaults::kKeyPassword);

        log("[NVS] ssid_present=%s", ssidLength > 0u ? "true" : "false");
        log("[NVS] ssid_length=%u", static_cast<unsigned>(ssidLength));
        log("[NVS] password_present=%s", passwordLength > 0u ? "true" : "false");
        log("[NVS] password_length=%u", static_cast<unsigned>(passwordLength));

        // Reject impossible lengths BEFORE reading, so a corrupt or
        // truncated record is refused rather than trusted.
        if (ssidLength < 1u || ssidLength > kSsidFieldSize) {
            log("[NVS] Load failed: implausible SSID length");
            break;
        }
        if (passwordLength < 1u || passwordLength > kPasswordFieldSize) {
            log("[NVS] Load failed: implausible password length");
            break;
        }

        char ssidField[kSsidFieldSize];
        char passwordField[kPasswordFieldSize];
        memset(ssidField, 0, sizeof(ssidField));
        memset(passwordField, 0, sizeof(passwordField));

        if (nvs_.getBytes(NetworkDefaults::kKeySsid, ssidField, sizeof(ssidField)) != ssidLength) {
            log("[NVS] Load failed: SSID read error");
            break;
        }
        if (nvs_.getBytes(
                NetworkDefaults::kKeyPassword, passwordField, sizeof(passwordField)) != passwordLength) {
            log("[NVS] Load failed: password read error");
            break;
        }

        // Defence in depth. The stored blob carries its own terminator, but
        // force one at the last legal index so a malformed record can never
        // be read past the buffer.
        ssidField[sizeof(ssidField) - 1u] = '\0';
        passwordField[sizeof(passwordField) - 1u] = '\0';

        // Shared validation rules, identical to the ones used on submission.
        if (!assignNetworkSsid(out, ssidField)) {
            log("[NVS] Load failed: SSID rejected by validation");
            break;
        }
        if (!assignNetworkPassword(out, passwordField)) {
            log("[NVS] Load failed: password rejected by validation");
            break;
        }

        out.valid = true;
        ok = true;
        log("[NVS] loaded_credentials_valid=true");
        log(
            "[NVS] credential fingerprint=%08lX",
            static_cast<unsigned long>(fingerprint(out)));
    } while (false);

    nvs_.end();
    return ok;
}

bool CredentialStoreLogic::clear() {
    if (!nvs_.begin(NetworkDefaults::kNvsNamespace, false)) {
        return false;
    }

    // Order: drop the commit marker first so a partial erase cannot leave a
    // marker pointing at missing payload.
    bool ok = nvs_.remove(NetworkDefaults::kKeyValid);
    ok = nvs_.remove(NetworkDefaults::kKeySsid) && ok;
    ok = nvs_.remove(NetworkDefaults::kKeyPassword) && ok;

    nvs_.end();
    return ok;
}
