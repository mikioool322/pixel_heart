#pragma once

#include <stdint.h>

#include "core/AnimationManager.h"
#include "core/PixelMatrix.h"
#include "control/ControlPipeline.h"
#include "control/SettingsStore.h"
#include "control/UserSettings.h"
#include "scheduling/ManualOverride.h"
#include "scheduling/ManualTimeProvider.h"
#include "scheduling/PlaybackPolicy.h"
#include "scheduling/Scheduler.h"

#if defined(ARDUINO_ARCH_ESP32)
#include "network/Esp32CredentialStore.h"
#include "network/Esp32LanService.h"
#include "network/Esp32NetworkDriver.h"
#include "network/Esp32ProvisioningPortal.h"
#include "network/NetworkManager.h"
#include "time/NtpTimeProvider.h"
#endif

// Composition root.
//
// Time source, network stack and scheduling are wired here and nowhere else.
// On ESP32 the Scheduler sees an NtpTimeProvider; on Uno it falls back to a
// fixed development clock. The Scheduler is unaware of the difference.
class App {
public:
    App();

    void begin();
    void tick(uint32_t now);

private:
    void tickNetwork(uint32_t now);

    // Applies one deferred control change from the LAN panel and settles
    // playback, so the panel is only ever published from a settled state.
    // Called from the main loop, never from an HTTP handler.
    void applyControl(const ControlRequest& request, uint32_t now);

    // Pushes mode / brightness / selected-animation state to the panel.
    void publishPanelState();

#if defined(ARDUINO_ARCH_ESP32)
    void publishLanStatus(uint32_t now);
#endif

    // AUTO vs MANUAL, selected animation, global brightness.
    UserSettings settings_;

    PixelMatrix matrix_;
    AnimationManager animationManager_;

#if defined(ARDUINO_ARCH_ESP32)
    // Declaration order matters: the portal and store must outlive the
    // manager, and the driver must outlive the portal.
    Esp32NetworkDriver networkDriver_;
    Esp32ProvisioningPortal provisioningPortal_;
    Esp32CredentialStore credentialStore_;
    NetworkManager networkManager_;
    NtpTimeProvider clock_;
    Esp32LanService lanService_;
    SettingsStoreLogic settingsStore_;
#else
    // DEV-ONLY development clock for the AVR build. The Uno has no radio and
    // no NTP path, so a fixed civil time is used to exercise the scheduler.
    // This is not a fallback for ESP32 and must never be used as one.
    ManualTimeProvider clock_;
#endif

    // Liveness instrumentation. Declared after the objects it observes so
    // member-initialisation order stays unambiguous.
    uint32_t appTickCount_;
    uint32_t lastAppHeartbeatMs_;
#if defined(ARDUINO_ARCH_ESP32)
    uint32_t lastStatusPublishMs_;
#endif

    ManualOverride manualOverride_;
    Scheduler scheduler_;
    PlaybackPolicy playbackPolicy_;
#if defined(ARDUINO_ARCH_ESP32)
    // The single ordered step that turns a queued panel request into settled
    // playback. Declared after the objects it references.
    ControlPipeline controlPipeline_;
#endif
};
