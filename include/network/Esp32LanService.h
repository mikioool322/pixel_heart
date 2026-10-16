#pragma once

#include <stdint.h>

#if defined(ARDUINO_ARCH_ESP32)

// Included inside the guard on purpose: control/UserSettings.h pulls in
// config/HardwareConfig.h, which pulls in <Arduino.h>. Natively the test fake
// defines ARDUINO_ARCH_ESP32, so including it earlier would make this
// translation unit take ESP-only branches and look for <ESPmDNS.h>.
#include "control/PanelRenderer.h"
#include "control/UserSettings.h"

// Post-connection LAN services: mDNS advertisement and a minimal, read-only
// status page at http://pixelheart.local/.
//
// This is deliberately NOT the control panel. There is no animation picker,
// no brightness control, no schedule editor and no manual override UI here;
// those belong to a later phase. Nothing on this page is capable of changing
// stored configuration.
class Esp32LanService {
public:
    Esp32LanService();

    // Starts mDNS and the status server. Safe to call repeatedly; only the
    // first call has an effect.
    void begin();

    bool started() const;

    // Services pending LAN HTTP work. Must not block.
    void tick();

    // Stops the listener and releases port 80, allowing another component to
    // claim it.
    void stop();

    // Data rendered on the status page. All are plain scalars; no secrets.
    void setStatusSsid(const char* ssid);
    void setStatusAddress(uint32_t address);
    void setStatusTime(const char* formattedTime);
    void setStatusAnimation(AnimationId animation);
    void setStatusConnected(bool connected);

    // Liveness / diagnostics, used by the serial heartbeat.
    bool routesRegistered() const;
    uint32_t tickCount() const;
    uint32_t handleClientCount() const;

    // Set by POST /wifi/change and consumed by the owner, so the transition is
    // sequenced OUTSIDE the HTTP handler. The handler must never tear down the
    // server it is currently running inside.
    bool consumeWifiChangeRequest();
    uint16_t wifiChangeRequests() const;

    // Deferred control changes. All of them are POST-only and are applied by
    // App on the next tick, never inside an HTTP handler.
    ControlRequest consumeControlRequest();

    // Values rendered on the panel. Pushed by App each status refresh.
    void setStatusMode(ControlMode mode);
    void setStatusBrightness(uint8_t percent);
    void setStatusRotation(DisplayRotation rotation);

private:
    void sendHealth();
    void sendStatusPage();
    void sendNotFoundPage();
    void handleWifiChange();
    void handleMode();
    void handleAnimation();
    void handleBrightness();
    void handleRotation();
    void handlePlay();

    // Returns true when the request may proceed. Read-only methods are always
    // allowed; anything else must present valid Basic credentials, and a
    // failure sends 401 + WWW-Authenticate and returns false. Callers MUST
    // check the result before touching any state.
    bool authorize();
    // Records a deferred request and redirects back to the panel, so a browser
    // refresh shows the applied state with no JavaScript.
    void redirectToPanel();
    void queueControl(ControlAction action);

    // True once a route has been registered. A separate flag from started_
    // because it is the precondition the HTTP bug turned on: a listening
    // server with no registered route answers nothing.
    bool routesRegistered_;

    bool started_;
    char ssid_[33];
    uint32_t address_;
    char time_[24];
    AnimationId currentAnimation_;
    bool connected_;

    // Liveness instrumentation. Counts are 32-bit and wrap harmlessly.
    uint32_t tickCount_;
    uint32_t handleClientCount_;
    uint32_t healthRequestCount_;
    uint32_t statusRequestCount_;
    uint32_t lastHeartbeatMs_;

    // Deferred "Change Wi-Fi" request.
    bool wifiChangeRequested_;
    uint16_t wifiChangeRequests_;

    // Deferred control change, applied by App on the next tick.
    ControlRequest pendingControl_;

    // Panel state pushed by App.
    ControlMode mode_;
    uint8_t brightnessPercent_;
    DisplayRotation rotation_;
};

#endif  // defined(ARDUINO_ARCH_ESP32)
