#include "core/BootDiagnostics.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <Arduino.h>
#include <esp_system.h>

namespace {

// Boot-scoped counter, so a reboot is unmistakable in the serial log even
// when the USB CDC link merely glitches.
uint32_t gBootCount = 0u;

const char* resetReasonToText(int reason) {
    switch (reason) {
        case BootDiagnostics::kPowerOn:
            return "POWERON";
        case BootDiagnostics::kExternal:
            return "EXT";
        case BootDiagnostics::kSoftware:
            return "SW";
        case BootDiagnostics::kPanic:
            return "PANIC";
        case BootDiagnostics::kInterruptWatchdog:
            return "INT_WDT";
        case BootDiagnostics::kTaskWatchdog:
            return "TASK_WDT";
        case BootDiagnostics::kWatchdog:
            return "WDT";
        case BootDiagnostics::kDeepSleep:
            return "DEEPSLEEP";
        case BootDiagnostics::kBrownout:
            return "BROWNOUT";
        case BootDiagnostics::kSdio:
            return "SDIO";
        default:
            return "UNKNOWN";
    }
}

}  // namespace

void BootDiagnostics::begin() {
    ++gBootCount;

    // Read the reset reason ONCE, immediately, before anything else can
    // perturb it. This is what distinguishes a genuine device reset from a
    // host-side USB serial reconnect.
    const int reason = static_cast<int>(esp_reset_reason());

    Serial.println();
    Serial.println(F("[BOOT] PixelHeart boot"));
    Serial.print(F("[BOOT] resetReason="));
    Serial.println(resetReasonToText(reason));
    Serial.print(F("[BOOT] resetCode="));
    Serial.println(reason);
    Serial.print(F("[BOOT] bootCount="));
    Serial.println(gBootCount);
    printHeap();
}

void BootDiagnostics::printHeap() {
    Serial.print(F("[SYS] freeHeap="));
    Serial.println(static_cast<unsigned long>(ESP.getFreeHeap()));
    Serial.print(F("[SYS] minFreeHeap="));
    Serial.println(static_cast<unsigned long>(ESP.getMinFreeHeap()));
    Serial.print(F("[SYS] maxAllocHeap="));
    Serial.println(static_cast<unsigned long>(ESP.getMaxAllocHeap()));
}

#else  // !ARDUINO_ARCH_ESP32

// AVR has no reset-reason concept; the Uno path simply has nothing to report.
void BootDiagnostics::begin() {}
void BootDiagnostics::printHeap() {}

#endif  // ARDUINO_ARCH_ESP32
