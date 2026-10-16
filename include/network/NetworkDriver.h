#pragma once

#include <stdint.h>

#include "network/NetworkCredentials.h"

// Radio operations the state machine needs, expressed without any WiFi / ESP
// types. The ESP32 implementation wraps the Arduino WiFi library; native tests
// substitute an in-memory double.
class NetworkDriver {
public:
    virtual ~NetworkDriver() = default;

    // Brings up station mode and begins connecting with `credentials`.
    virtual void beginStation(const NetworkCredentials& credentials) = 0;

    // Starts or stops the setup access point.
    virtual void startAccessPoint(const char* ssid, const char* password) = 0;
    virtual void stopAccessPoint() = 0;
    virtual bool accessPointActive() const = 0;

    // Non-blocking association status.
    virtual bool stationConnected() const = 0;

    // Drops the current association without erasing the stored configuration.
    virtual void disconnectStation() = 0;

    // Packed IPv4 address in host byte order; 0 when not associated.
    virtual uint32_t localAddress() const = 0;

    // Human-readable current SSID. Never returns a password.
    virtual void currentSsid(char* out, uint8_t capacity) const = 0;

    // Scans for nearby networks. Returns the number of results written.
    // Results are SSID-only.
    virtual uint8_t scanNetworks(char ssids[][33], uint8_t maxResults) = 0;
};
