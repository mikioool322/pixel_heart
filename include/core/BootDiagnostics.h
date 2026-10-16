#pragma once

#include <stdint.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_system.h>
#endif

// Startup / liveness diagnostics.
//
// Purpose: distinguish a genuine device reset from a host-side USB serial
// reconnect, and expose heap pressure. A USB CDC "Write timeout" on the host
// says nothing on its own about whether the MCU restarted; the reset reason
// plus the boot counter do.
class BootDiagnostics {
public:
    // Reset-reason codes, mirrored from the ESP-IDF esp_reset_reason_t enum so
    // the classification logic is testable off-target. Values match the SDK.
    enum ResetReason : int {
        kUnknown = 0,
        kPowerOn = 1,
        kExternal = 2,
        kSoftware = 3,
        kPanic = 4,
        kInterruptWatchdog = 5,
        kTaskWatchdog = 6,
        kWatchdog = 7,
        kDeepSleep = 8,
        kBrownout = 9,
        kSdio = 10
    };

    // Prints the boot banner, reset reason, boot count and heap state.
    static void begin();

    // Prints current heap statistics.
    static void printHeap();

    // True for a watchdog or panic reset, i.e. a reset indicating the device
    // hung rather than being power-cycled.
    static bool looksLikeWatchdogReset(int reason);
};

inline bool BootDiagnostics::looksLikeWatchdogReset(int reason) {
    return reason == BootDiagnostics::kPanic ||
           reason == BootDiagnostics::kInterruptWatchdog ||
           reason == BootDiagnostics::kTaskWatchdog ||
           reason == BootDiagnostics::kWatchdog;
}
