#pragma once

#include "network/NetworkDriver.h"

#if defined(ARDUINO_ARCH_ESP32)

// NetworkDriver backed by the Arduino ESP32 WiFi library.
//
// This is the only place in the project that includes <WiFi.h>.
class Esp32NetworkDriver final : public NetworkDriver {
public:
    Esp32NetworkDriver();

    void beginStation(const NetworkCredentials& credentials) override;
    void startAccessPoint(const char* ssid, const char* password) override;
    void stopAccessPoint() override;
    bool accessPointActive() const override;
    bool stationConnected() const override;
    void disconnectStation() override;
    uint32_t localAddress() const override;
    void currentSsid(char* out, uint8_t capacity) const override;
    uint8_t scanNetworks(char ssids[][33], uint8_t maxResults) override;

    // Builds the setup AP SSID, appending a short stable chip-derived suffix.
    void buildSetupApSsid(char* out, uint8_t capacity) const;

private:
    bool apActive_;
};

#endif  // defined(ARDUINO_ARCH_ESP32)
