#pragma once

#include <stddef.h>
#include <stdint.h>

// Bounded POD Wi-Fi credential container.
//
// 802.11 limits: SSID <= 32 bytes, WPA2 passphrase <= 63 bytes.
// Buffers are sized +1 so a value may always be stored NUL-terminated.
//
// This header is intentionally free of Arduino and ESP headers so the
// validation rules can be unit tested in the native environment.
struct NetworkCredentials {
    static constexpr uint8_t kMaxSsidLength = 32u;
    static constexpr uint8_t kMaxPasswordLength = 63u;

    char ssid[kMaxSsidLength + 1u];
    char password[kMaxPasswordLength + 1u];
    bool valid;
};

// Zeroes the whole record, guaranteeing empty NUL-terminated strings.
inline void clearNetworkCredentials(NetworkCredentials& credentials) {
    for (uint8_t i = 0u; i <= NetworkCredentials::kMaxSsidLength; ++i) {
        credentials.ssid[i] = '\0';
    }
    for (uint8_t i = 0u; i <= NetworkCredentials::kMaxPasswordLength; ++i) {
        credentials.password[i] = '\0';
    }
    credentials.valid = false;
}

inline bool isValidSsidLength(size_t length) {
    return length > 0u && length <= NetworkCredentials::kMaxSsidLength;
}

inline bool isValidPasswordLength(size_t length) {
    return length <= NetworkCredentials::kMaxPasswordLength;
}

// Copies an SSID. Returns false (and leaves `out` untouched) for an empty or
// over-long value, so a malformed submission can never be persisted.
inline bool assignNetworkSsid(NetworkCredentials& out, const char* value) {
    if (value == nullptr) {
        return false;
    }

    // Note the strict bound: the buffer is kMaxSsidLength + 1 bytes, so
    // index kMaxSsidLength is the terminator slot and must not be read as
    // part of the payload.
    size_t length = 0u;
    while (length < NetworkCredentials::kMaxSsidLength && value[length] != '\0') {
        ++length;
    }

    // Reject an over-long value rather than silently truncating it.
    if (value[length] != '\0') {
        return false;
    }

    if (!isValidSsidLength(length)) {
        return false;
    }

    clearNetworkCredentials(out);
    for (size_t i = 0u; i < length; ++i) {
        out.ssid[i] = value[i];
    }
    out.ssid[length] = '\0';
    out.valid = true;
    return true;
}

// Copies a passphrase. An empty passphrase is accepted: open networks exist
// and rejecting them would strand such users in a retry loop.
inline bool assignNetworkPassword(NetworkCredentials& out, const char* value) {
    if (value == nullptr) {
        value = "";
    }

    size_t length = 0u;
    while (length < NetworkCredentials::kMaxPasswordLength && value[length] != '\0') {
        ++length;
    }

    // Reject an over-long value rather than silently truncating it.
    if (value[length] != '\0') {
        return false;
    }

    for (size_t i = 0u; i < length; ++i) {
        out.password[i] = value[i];
    }
    out.password[length] = '\0';
    return true;
}

// Full validation of a record, e.g. one freshly read back from storage.
inline bool areNetworkCredentialsValid(const NetworkCredentials& credentials) {
    if (!credentials.valid) {
        return false;
    }

    size_t ssidLength = 0u;
    while (ssidLength < NetworkCredentials::kMaxSsidLength &&
           credentials.ssid[ssidLength] != '\0') {
        ++ssidLength;
    }

    if (!isValidSsidLength(ssidLength)) {
        return false;
    }

    // A record with no terminator inside the buffer is treated as invalid
    // rather than trusted, so a corrupt NVS blob cannot be acted on.
    if (credentials.ssid[ssidLength] != '\0') {
        return false;
    }

    size_t passwordLength = 0u;
    while (passwordLength < NetworkCredentials::kMaxPasswordLength &&
           credentials.password[passwordLength] != '\0') {
        ++passwordLength;
    }

    if (credentials.password[passwordLength] != '\0') {
        return false;
    }

    return isValidPasswordLength(passwordLength);
}

inline bool isOpenNetwork(const NetworkCredentials& credentials) {
    return credentials.password[0] == '\0';
}
