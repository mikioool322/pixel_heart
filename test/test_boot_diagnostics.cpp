#include <unity.h>

#include "core/BootDiagnostics.h"

// The native environment has no ESP32 SDK, so these tests exercise the
// portable contract of the helper: it must classify reasons using the same
// numeric mapping the ESP32 enum uses, without pulling in esp_system.h.
namespace {
constexpr int kPanic = 4;
constexpr int kIntWdt = 5;
constexpr int kTaskWdt = 6;
constexpr int kWdt = 7;

constexpr int kUnknown = 0;
constexpr int kPoweron = 1;
constexpr int kExt = 2;
constexpr int kSw = 3;
constexpr int kDeepSleep = 8;
constexpr int kBrownout = 9;
constexpr int kSdio = 10;
}  // namespace

void test_watchdog_and_panic_are_flagged_as_hangs(void) {
    // A watchdog or panic reset means the device hung, as opposed to being
    // power-cycled. A non-hang must never be reported as one, or the
    // diagnostics would cry wolf and mask a real hang.
    TEST_ASSERT_TRUE(BootDiagnostics::looksLikeWatchdogReset(kPanic));
    TEST_ASSERT_TRUE(BootDiagnostics::looksLikeWatchdogReset(kIntWdt));
    TEST_ASSERT_TRUE(BootDiagnostics::looksLikeWatchdogReset(kTaskWdt));
    TEST_ASSERT_TRUE(BootDiagnostics::looksLikeWatchdogReset(kWdt));
}

void test_ordinary_reboots_are_not_reported_as_hangs(void) {
    TEST_ASSERT_FALSE(BootDiagnostics::looksLikeWatchdogReset(kPoweron));
    TEST_ASSERT_FALSE(BootDiagnostics::looksLikeWatchdogReset(kExt));
    TEST_ASSERT_FALSE(BootDiagnostics::looksLikeWatchdogReset(kSw));
    TEST_ASSERT_FALSE(BootDiagnostics::looksLikeWatchdogReset(kDeepSleep));
    TEST_ASSERT_FALSE(BootDiagnostics::looksLikeWatchdogReset(kBrownout));
    TEST_ASSERT_FALSE(BootDiagnostics::looksLikeWatchdogReset(kSdio));
    TEST_ASSERT_FALSE(BootDiagnostics::looksLikeWatchdogReset(kUnknown));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_watchdog_and_panic_are_flagged_as_hangs);
    RUN_TEST(test_ordinary_reboots_are_not_reported_as_hangs);
    return UNITY_END();
}
