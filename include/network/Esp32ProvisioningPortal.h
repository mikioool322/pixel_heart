#pragma once

#include "network/NetworkDriver.h"
#include "network/ProvisioningPortal.h"

#if defined(ARDUINO_ARCH_ESP32)

// Captive portal built on DNSServer + WebServer.
//
// Arduino String is used here, and only here: the framework's HTML helpers
// expect it, and nothing in this class leaks into the scheduler, playback or
// animation layers.
//
// Security rules enforced by this class:
//   - credentials are accepted via POST only, never via GET query string
//   - the submitted password is never echoed back into any page
//   - the password field is rendered as type="password"
class Esp32ProvisioningPortal final : public ProvisioningPortal {
public:
    Esp32ProvisioningPortal();

    // Supplies the driver used for the explicit "Scan networks" action.
    // The portal does not take ownership; the pointer must outlive the portal.
    void setScanDriver(NetworkDriver* driver);

    void start(SubmitHandler handler, void* context) override;
    void stop() override;
    bool active() const override;
    void tick() override;
    void setPendingConnection(bool pending) override;
    void setStatusMessage(const char* message) override;
    void setPendingSsid(const char* ssid) override;

private:
    void handleRoot();
    void handleConnect();
    void handleScan();
    void handleNotFound();
    void sendPortalPage(int statusCode, const char* statusMessage, const char* pendingSsid);

    bool active_;
    // True once routes have been registered on the underlying WebServer.
    // WebServer::on() appends to a handler list that stop() never clears, so
    // re-registering on a later provisioning session would duplicate every
    // route and leak a handler object each time. With "Change Wi-Fi" the
    // portal can legitimately start more than once per boot.
    bool routesRegistered_;
    SubmitHandler handler_;
    void* context_;

    bool pendingConnection_;
    char statusMessage_[96];
    char pendingSsid_[33];
};

#endif  // defined(ARDUINO_ARCH_ESP32)
