#include "App.h"

#include <stdio.h>

#include "config/HardwareConfig.h"
#include "control/BrightnessLevels.h"
#include "core/AnimationNames.h"
#include "core/BootDiagnostics.h"

#if defined(ARDUINO_ARCH_ESP32)

// There is deliberately no initial civil time on ESP32. NtpTimeProvider
// reports valid == false until SNTP has produced a credible clock, and the
// Scheduler's existing invalid-time fallback covers the gap. No development
// clock is substituted here.

App::App()
    : settings_(UserSettingsDefaults::make()),
      matrix_(),
      animationManager_(),
      networkDriver_(),
      provisioningPortal_(),
      credentialStore_(),
      networkManager_(networkDriver_, credentialStore_, &provisioningPortal_),
      clock_(),
      lanService_(),
      settingsStore_(esp32SharedNvs()),
      manualOverride_(),
      scheduler_(clock_, manualOverride_, SchedulerConfig()),
      playbackPolicy_(scheduler_, animationManager_, matrix_),
      controlPipeline_(settings_, manualOverride_, playbackPolicy_, animationManager_),
      appTickCount_(0u),
      lastAppHeartbeatMs_(0u),
      lastStatusPublishMs_(0u) {
    provisioningPortal_.setScanDriver(&networkDriver_);

    // Guard rail: the ESP32 build must be wired to the NTP-backed clock. If
    // someone swaps it for the development clock, this fails to compile
    // rather than shipping a device that displays a fake date.
    static_assert(
        sizeof(clock_) == sizeof(NtpTimeProvider),
        "ESP32 must use the NTP-backed time source");
    static_assert(
        sizeof(scheduler_) > 0u,
        "scheduler must be constructed from a TimeProvider");
}

void App::begin() {
    // Printed before anything else so a genuine reboot is visible in the log
    // and distinguishable from a host-side USB serial reconnect.
    BootDiagnostics::begin();

    // Load persisted settings BEFORE the matrix or the scheduler is armed, so
    // there is exactly one point where defaults can be replaced.
    // Wi-Fi credentials are handled separately by their own store.
    UserSettings stored = settings_;
    if (settingsStore_.load(stored)) {
        settings_ = stored;
        Serial.printf(
            "[SET] loaded mode=%s animation=%s brightness=%u%%\n",
            (settings_.mode == ControlMode::MANUAL) ? "MANUAL" : "AUTO",
            animationNameOf(settings_.animation),
            static_cast<unsigned>(settings_.brightnessPercent));
    } else {
        Serial.println(F("[SET] no stored settings, using defaults"));
    }

    matrix_.begin();
    // Resolved through the level table, not straight from the stored percent:
    // the levels deliberately target raw values (1/3/7/16/33) that the coarse
    // percent scale cannot express. No migration is needed - the stored byte is
    // still a percentage, and a legacy value simply resolves to the nearest
    // level that is no brighter than before.
    matrix_.setBrightness(BrightnessLevels::rawForPercent(settings_.brightnessPercent));
    matrix_.setRotation(settings_.rotation);
    matrix_.clear();
    matrix_.show();

    // Start the network. Until NTP has produced a credible clock,
    // NtpTimeProvider::now().valid is false and the Scheduler uses its
    // existing invalid-time fallback. Boot is never blocked waiting for time.
    networkManager_.begin(millis());

    scheduler_.reset();
    playbackPolicy_.reset();

    // Restore the persisted mode. MANUAL re-arms the override so the scheduler
    // cannot steal the panel back on boot.
    if (settings_.mode == ControlMode::MANUAL) {
        manualOverride_.activateUntilCancelled(settings_.animation, PlayMode::LOOP);
    }
}

void App::publishLanStatus(uint32_t now) {
    if (!lanService_.started()) {
        return;
    }

    // Control state is republished on EVERY tick, deliberately.
    //
    // It is a handful of scalar copies, and it must never lag a click: the
    // panel is rendered on the GET that follows the POST's 303 redirect, so a
    // stale snapshot here is exactly what made the first click look like it did
    // nothing. applyControl() has already settled playback, so this reads the
    // state the click just produced.
    lanService_.setStatusAnimation(controlPipeline_.visibleAnimation());
    publishPanelState();

    // Ambient telemetry is rate-limited. It is not click-driven, and gathering
    // it used to call WiFi.SSID() (which returns a heap String) on every single
    // loop iteration - a large, sustained allocation rate for data that is
    // displayed to a human at most once every few seconds.
    if (lastStatusPublishMs_ != 0u &&
        now - lastStatusPublishMs_ < 1000u) {
        return;
    }
    lastStatusPublishMs_ = now;

    char ssid[33];
    ssid[0] = '\0';
    networkDriver_.currentSsid(ssid, sizeof(ssid));
    lanService_.setStatusSsid(ssid);
    lanService_.setStatusAddress(networkManager_.localAddress());
    lanService_.setStatusConnected(networkManager_.isConnected());

    // Read-only rendering of the current civil time.
    const ClockSnapshot snapshot = clock_.now();
    if (snapshot.valid) {
        char text[24];
        snprintf(
            text,
            sizeof(text),
            "%04u-%02u-%02u %02u:%02u:%02u",
            static_cast<unsigned>(snapshot.year),
            static_cast<unsigned>(snapshot.month),
            static_cast<unsigned>(snapshot.day),
            static_cast<unsigned>(snapshot.hour),
            static_cast<unsigned>(snapshot.minute),
            static_cast<unsigned>(snapshot.second));
        lanService_.setStatusTime(text);
    } else {
        lanService_.setStatusTime("not synchronized");
    }
}

void App::publishPanelState() {
    lanService_.setStatusMode(settings_.mode);
    lanService_.setStatusBrightness(settings_.brightnessPercent);
    lanService_.setStatusRotation(settings_.rotation);
}

void App::applyControl(const ControlRequest& request, uint32_t now) {
    // The ordered decision core lives in ControlPipeline, which applies the
    // request and settles playback in one step. App only performs the side
    // effects it asks for: persistence and the two matrix settings.
    const ControlPipeline::Outcome outcome = controlPipeline_.apply(request, now);

    // Report an unrenderable scheduler decision ONCE, when playback first
    // falls back. Quiet hours producing NONE is normal operation and is
    // deliberately not logged here.
    AnimationId requested = AnimationId::NONE;
    AnimationId applied = AnimationId::NONE;
    if (playbackPolicy_.consumeUnavailableNotice(requested, applied)) {
        Serial.printf(
            "[CTL] playback unavailable requested=%s applied=%s\n",
            animationNameOf(requested),
            animationNameOf(applied));
    }

    if (outcome.applyBrightness) {
        // Bounded by two independent layers: the pipeline clamps the request,
        // and PixelMatrix clamps again inside setBrightness().
        matrix_.setBrightness(BrightnessLevels::rawForPercent(outcome.brightnessPercent));
    }

    if (outcome.applyRotation) {
        // Takes effect immediately. setRotation() marks the frame dirty, so the
        // next animation update repaints in the new orientation and
        // showIfDirty() transmits it. No framebuffer, no forced show().
        matrix_.setRotation(outcome.rotation);
    }

    if (outcome.persist) {
        // One write per accepted user action. Never from the main loop.
        if (!settingsStore_.save(settings_)) {
            Serial.println(F("[SET] WARNING: settings were NOT persisted"));
        }
    }

    switch (request.action) {
        case ControlAction::SET_MODE:
            Serial.printf("[CTL] mode=%s\n", (settings_.mode == ControlMode::MANUAL) ? "MANUAL" : "AUTO");
            break;

        case ControlAction::SELECT_ANIMATION:
            if (outcome.accepted) {
                Serial.printf(
                    outcome.restarted ? "[CTL] restart %s\n" : "[CTL] animation=%s\n",
                    animationNameOf(settings_.animation));
            }
            break;

        case ControlAction::SET_BRIGHTNESS:
            if (outcome.applyBrightness) {
                Serial.printf("[CTL] brightness=%u%%\n", static_cast<unsigned>(settings_.brightnessPercent));
            }
            break;

        case ControlAction::SET_ROTATION:
            if (outcome.applyRotation) {
                Serial.printf(
                    "[CTL] rotation=%u\n",
                    static_cast<unsigned>(displayRotationDegrees(settings_.rotation)));
            }
            break;

        case ControlAction::PLAY:
            Serial.println(F("[CTL] play"));
            break;

        case ControlAction::NONE:
        default:
            break;
    }
}

void App::tickNetwork(uint32_t now) {
    networkManager_.tick(now);

    // Start NTP exactly once connectivity exists. The Scheduler is not
    // involved in this decision.
    if (networkManager_.isConnected() && !clock_.started()) {
        clock_.begin();
    }

    // A "Change Wi-Fi" request is serviced HERE, outside the HTTP handler that
    // raised it. The handler has already written its response and closed its
    // connection by the time handleClient() returned, so it is now safe to:
    //   1. stop the LAN server, which releases port 80
    //   2. ask the network manager to bring the setup portal up
    // The ordering is mandatory: the portal cannot claim port 80 until the LAN
    // server has released it.
    if (lanService_.started() && lanService_.consumeWifiChangeRequest()) {
        Serial.println(F("[HTTP] LAN stopping for Wi-Fi reconfiguration"));
        lanService_.stop();
        networkManager_.startProvisioning(now);
    }

    // The LAN status server may only claim port 80 once the provisioning
    // portal has fully released it. Checking isConnected() alone is not
    // enough: during the post-success grace period the device is already
    // connected while the provisioning server is still listening, and two
    // WebServer(80) instances would conflict. During a user-initiated
    // provisioning session the old station link may still be up, so
    // provisioningActive() is the only reliable signal.
    if (networkManager_.isConnected() &&
        !networkManager_.provisioningActive() &&
        !lanService_.started()) {
        lanService_.begin();
    }
}

void App::tick(uint32_t now) {
    tickNetwork(now);

    // ONE ordered step, and the fix for the double-click bug.
    //
    // The queued panel request is consumed, applied, and playback settled - all
    // before the panel snapshot is published and before any HTTP request is
    // served. The POST's 303 redirect is handled by lanService_.tick() below,
    // so the GET it triggers can only ever render state that already includes
    // the click. Previously the snapshot was published first and the redirect
    // could render the animation from *before* the click, which is why a second
    // click appeared to be required.
    applyControl(
        lanService_.started() ? lanService_.consumeControlRequest()
                              : makeIdleControlRequest(),
        now);

    if (lanService_.started()) {
        publishLanStatus(now);
        lanService_.tick();
    }

    animationManager_.tick(now, matrix_);
    matrix_.showIfDirty();

    // Rate-limited liveness proof for the outer loop, plus a one-shot note of
    // whether the HTTP server has routes and is being serviced.
    ++appTickCount_;
    if (now - lastAppHeartbeatMs_ >= 2000u || lastAppHeartbeatMs_ == 0u) {
        lastAppHeartbeatMs_ = now;
        Serial.printf(
            "[APP] loop alive count=%lu lanStarted=%d lanRoutes=%d netState=%u\n",
            static_cast<unsigned long>(appTickCount_),
            lanService_.started() ? 1 : 0,
            lanService_.routesRegistered() ? 1 : 0,
            static_cast<unsigned>(networkManager_.state()));
    }
}

#else  // !ARDUINO_ARCH_ESP32

namespace {
// DEV-ONLY fixed clock for the AVR build. The Uno has no Wi-Fi radio and no
// NTP path, so this fixed civil time stands in for a real one to exercise the
// scheduler. It is deliberately not used on ESP32.
constexpr uint16_t kDevYear = 2026u;
constexpr uint8_t kDevMonth = 9u;
constexpr uint8_t kDevDay = 30u;
constexpr uint8_t kDevHour = 20u;
constexpr uint8_t kDevMinute = 0u;
}  // namespace

App::App()
    : matrix_(),
      animationManager_(),
      clock_(),
      manualOverride_(),
      scheduler_(clock_, manualOverride_, SchedulerConfig()),
      playbackPolicy_(scheduler_, animationManager_, matrix_),
      appTickCount_(0u),
      lastAppHeartbeatMs_(0u) {
    // Guard rail: the AVR build may only ever use the dev-only fixed clock.
    // There is deliberately no network or NTP path on this board.
    static_assert(
        sizeof(clock_) == sizeof(ManualTimeProvider),
        "AVR builds may only use the dev-only fixed clock");
}

void App::begin() {
    matrix_.begin();
    matrix_.setBrightness(BrightnessLevels::rawForPercent(settings_.brightnessPercent));
    matrix_.setRotation(settings_.rotation);
    matrix_.clear();
    matrix_.show();

    clock_.set(makeClockSnapshot(kDevYear, kDevMonth, kDevDay, kDevHour, kDevMinute));
    scheduler_.reset();
    playbackPolicy_.reset();

    // Restore the persisted mode. MANUAL re-arms the override so the scheduler
    // cannot steal the panel back on boot.
    if (settings_.mode == ControlMode::MANUAL) {
        manualOverride_.activateUntilCancelled(settings_.animation, PlayMode::LOOP);
    }
}

void App::tickNetwork(uint32_t now) {
    // The Uno has no network stack. Nothing to service.
    (void)now;
}

void App::tick(uint32_t now) {
    playbackPolicy_.tick(now);
    animationManager_.tick(now, matrix_);
    matrix_.showIfDirty();
    (void)appTickCount_;
    (void)lastAppHeartbeatMs_;
}

#endif  // ARDUINO_ARCH_ESP32
