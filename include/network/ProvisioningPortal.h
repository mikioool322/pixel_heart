#pragma once

#include <stdint.h>

#include "network/NetworkCredentials.h"

// Result of a portal submission, reported back so the state machine can decide
// what to do next without knowing anything about HTTP.
enum class PortalSubmitResult : uint8_t {
    ACCEPTED,        // syntactically valid, now being tested
    REJECTED_FORMAT  // missing SSID or over-long field
};

// Captive-portal surface, reduced to what the state machine consumes.
//
// The ESP32 implementation owns WebServer + DNSServer. A portal instance is
// attached to a NetworkManager by pointer; the manager does not own the
// lifetime of the portal, it only drives it.
class ProvisioningPortal {
public:
    virtual ~ProvisioningPortal() = default;

    // Callback used by the portal to hand a validated submission upwards.
    using SubmitHandler = void (*)(const NetworkCredentials& credentials, void* context);

    // Called by the state machine to bring the portal up / tear it down.
    virtual void start(SubmitHandler handler, void* context) = 0;
    virtual void stop() = 0;
    virtual bool active() const = 0;

    // Services any pending HTTP / DNS work. Must not block for long.
    virtual void tick() = 0;

    // Tells the portal whether a connection attempt is in flight, so it can
    // report "testing..." instead of accepting a second submission.
    virtual void setPendingConnection(bool pending) = 0;

    // Last human-readable status line shown on the portal page.
    virtual void setStatusMessage(const char* message) = 0;

    // The SSID currently being tested, if any. Used to prefill the page.
    // Never includes a password.
    virtual void setPendingSsid(const char* ssid) = 0;
};
