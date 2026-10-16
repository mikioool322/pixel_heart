#include "network/Esp32NetworkDriver.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <WiFi.h>
#include <esp_wifi.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "network/NetworkDefaults.h"

namespace {
// Cap on a single scan so a congested band cannot stall the animation tick.
constexpr uint32_t kScanMaxMsPerChannel = 120UL;
}  // namespace

Esp32NetworkDriver::Esp32NetworkDriver()
    : apActive_(false) {
}

void Esp32NetworkDriver::beginStation(const NetworkCredentials& credentials) {
    // Keep the setup AP alive while probing, so a failed attempt leaves the
    // user a portal instead of a blank screen.
    if (apActive_) {
        WiFi.mode(WIFI_AP_STA);
    } else {
        WiFi.mode(WIFI_STA);
    }

    // No eraseap: a failed attempt must not wipe the stored configuration.
    WiFi.begin(credentials.ssid, credentials.password);
}

void Esp32NetworkDriver::startAccessPoint(const char* ssid, const char* password) {
    char ssidBuffer[40];

    if (ssid == nullptr) {
        buildSetupApSsid(ssidBuffer, sizeof(ssidBuffer));
    } else {
        strncpy(ssidBuffer, ssid, sizeof(ssidBuffer) - 1u);
        ssidBuffer[sizeof(ssidBuffer) - 1u] = '\0';
    }

    const char* passphrase =
        (password == nullptr) ? NetworkDefaults::kSetupApPassword : password;

    // WPA2: an open setup network would let anyone nearby reconfigure the
    // device. softAP() with a passphrase selects WPA2-PSK.
    const bool ok = WiFi.softAP(
        ssidBuffer,
        passphrase,
        1,        // channel
        false,    // not hidden
        2,        // max simultaneous clients
        false);   // no FTM responder

    apActive_ = ok;

    if (ok) {
        WiFi.softAPIP();
    }
}

void Esp32NetworkDriver::stopAccessPoint() {
    if (!apActive_) {
        return;
    }

    WiFi.softAPdisconnect(true);
    apActive_ = false;

    // Drop back to station-only so the radio is not left in AP mode.
    WiFi.mode(WIFI_STA);
}

bool Esp32NetworkDriver::accessPointActive() const {
    return apActive_;
}

bool Esp32NetworkDriver::stationConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

void Esp32NetworkDriver::disconnectStation() {
    // wifioff = false, eraseap = false: keep the stored configuration.
    WiFi.disconnect(false, false);
}

uint32_t Esp32NetworkDriver::localAddress() const {
    const IPAddress address = WiFi.localIP();
    if (address == IPAddress(0u, 0u, 0u, 0u)) {
        return 0u;
    }

    return (static_cast<uint32_t>(address[0]) << 24) |
           (static_cast<uint32_t>(address[1]) << 16) |
           (static_cast<uint32_t>(address[2]) << 8) |
           static_cast<uint32_t>(address[3]);
}

void Esp32NetworkDriver::currentSsid(char* out, uint8_t capacity) const {
    if (out == nullptr || capacity == 0u) {
        return;
    }

    out[0] = '\0';

    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    // Read the SSID straight from the driver into the caller's buffer.
    // WiFi.SSID() returns an Arduino String, i.e. a heap allocation; this
    // method is called from the main loop, so allocating there is avoidable.
    wifi_ap_record_t info;
    if (esp_wifi_sta_get_ap_info(&info) != ESP_OK) {
        return;
    }

    strncpy(
        out,
        reinterpret_cast<const char*>(info.ssid),
        static_cast<size_t>(capacity) - 1u);
    out[static_cast<size_t>(capacity) - 1u] = '\0';
}

uint8_t Esp32NetworkDriver::scanNetworks(char ssids[][33], uint8_t maxResults) {
    if (ssids == nullptr || maxResults == 0u) {
        return 0u;
    }

    const int16_t found = WiFi.scanNetworks(false, false, true, kScanMaxMsPerChannel);

    if (found <= 0) {
        WiFi.scanDelete();
        return 0u;
    }

    const int16_t wanted = (found < static_cast<int16_t>(maxResults))
        ? found
        : static_cast<int16_t>(maxResults);

    uint8_t written = 0u;
    for (int16_t i = 0; i < wanted; ++i) {
        strncpy(ssids[written], WiFi.SSID(i).c_str(), 32u);
        ssids[written][32] = '\0';
        ++written;
    }

    WiFi.scanDelete();
    return written;
}

void Esp32NetworkDriver::buildSetupApSsid(char* out, uint8_t capacity) const {
    if (out == nullptr || capacity == 0u) {
        return;
    }

    // Only a 4 hex-digit suffix derived from the chip MAC is exposed, purely
    // to tell two units apart on a phone's network list.
    const uint64_t mac = ESP.getEfuseMac();
    const uint8_t suffix0 = static_cast<uint8_t>((mac >> 8) & 0xFFu);
    const uint8_t suffix1 = static_cast<uint8_t>(mac & 0xFFu);

    const int written = snprintf(
        out,
        static_cast<size_t>(capacity),
        "%s-%02X%02X",
        NetworkDefaults::kSetupApSsidPrefix,
        suffix0,
        suffix1);

    if (written < 0) {
        out[0] = '\0';
    } else if (static_cast<size_t>(written) >= static_cast<size_t>(capacity)) {
        out[static_cast<size_t>(capacity) - 1u] = '\0';
    }
}

#endif  // defined(ARDUINO_ARCH_ESP32)
