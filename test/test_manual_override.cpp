#include <unity.h>

#include "scheduling/ManualOverride.h"
#include "scheduling/ManualTimeProvider.h"
#include "scheduling/Scheduler.h"

namespace {

uint8_t id(AnimationId value) {
    return static_cast<uint8_t>(value);
}

constexpr uint32_t kFiveMinutesMs = 5UL * 60UL * 1000UL;
constexpr uint32_t kFifteenMinutesMs = 15UL * 60UL * 1000UL;
constexpr uint32_t kMinuteMs = 60UL * 1000UL;

}  // namespace

void test_manual_override_inactive_by_default(void) {
    ManualOverride manual;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ManualOverrideMode::INACTIVE),
        static_cast<uint8_t>(manual.mode()));
    TEST_ASSERT_FALSE(manual.active(0u));
    TEST_ASSERT_FALSE(manual.active(1000000u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::NONE), id(manual.decision().animationId));
}

void test_manual_override_timed_expiration(void) {
    ManualOverride manual;
    manual.activateTimed(AnimationId::CAMPFIRE, PlayMode::LOOP, kFiveMinutesMs, 1000u);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ManualOverrideMode::TIMED),
        static_cast<uint8_t>(manual.mode()));
    TEST_ASSERT_TRUE(manual.active(1000u));
    TEST_ASSERT_TRUE(manual.active(1000u + kFiveMinutesMs - 1u));
    TEST_ASSERT_FALSE(manual.active(1000u + kFiveMinutesMs));
    TEST_ASSERT_FALSE(manual.active(1000u + kFiveMinutesMs + 60000u));

    const PlaybackDecision decision = manual.decision();
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::CAMPFIRE), id(decision.animationId));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(SchedulePriority::MANUAL),
        static_cast<uint8_t>(decision.priority));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(PlaybackReason::MANUAL_OVERRIDE),
        static_cast<uint8_t>(decision.reason));
    TEST_ASSERT_EQUAL_UINT32(kFiveMinutesMs, decision.holdDurationMs);
}

void test_manual_override_until_cancelled(void) {
    ManualOverride manual;
    manual.activateUntilCancelled(AnimationId::DANCING_DUCK, PlayMode::LOOP);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ManualOverrideMode::UNTIL_CANCELLED),
        static_cast<uint8_t>(manual.mode()));
    TEST_ASSERT_TRUE(manual.active(0u));
    TEST_ASSERT_TRUE(manual.active(0xFFFFFFFFu));

    manual.clear();
    TEST_ASSERT_FALSE(manual.active(0u));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ManualOverrideMode::INACTIVE),
        static_cast<uint8_t>(manual.mode()));
}

void test_manual_override_presets(void) {
    ManualOverride manual;

    manual.activateFor5Minutes(AnimationId::CAMPFIRE, PlayMode::LOOP, 0u);
    TEST_ASSERT_TRUE(manual.active(kFiveMinutesMs - 1u));
    TEST_ASSERT_FALSE(manual.active(kFiveMinutesMs));

    manual.activateFor15Minutes(AnimationId::CAMPFIRE, PlayMode::LOOP, 0u);
    TEST_ASSERT_TRUE(manual.active(kFifteenMinutesMs - 1u));
    TEST_ASSERT_FALSE(manual.active(kFifteenMinutesMs));

    manual.activateFor60Minutes(AnimationId::CAMPFIRE, PlayMode::LOOP, 0u);
    TEST_ASSERT_TRUE(manual.active(60UL * kMinuteMs - 1u));
    TEST_ASSERT_FALSE(manual.active(60UL * kMinuteMs));
}

void test_manual_override_beats_quiet_hours(void) {
    ManualTimeProvider clock;
    ManualOverride manual;
    Scheduler scheduler(clock, manual, SchedulerConfig());

    clock.set(makeClockSnapshot(2026u, 9u, 30u, 2u, 0u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::NONE), id(scheduler.evaluate(0u).animationId));

    manual.activateUntilCancelled(AnimationId::CAMPFIRE, PlayMode::LOOP);
    PlaybackDecision decision = scheduler.evaluate(0u);
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::CAMPFIRE), id(decision.animationId));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(SchedulePriority::MANUAL),
        static_cast<uint8_t>(decision.priority));

    manual.clear();
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::NONE), id(scheduler.evaluate(0u).animationId));
}

void test_manual_override_beats_seasonal_and_special_events(void) {
    ManualTimeProvider clock;
    ManualOverride manual;
    Scheduler scheduler(clock, manual, SchedulerConfig());

    manual.activateUntilCancelled(AnimationId::SLOT_MACHINE, PlayMode::LOOP);

    // Seasonal (Dec 10, evening).
    clock.set(makeClockSnapshot(2026u, 12u, 10u, 20u, 0u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::SLOT_MACHINE), id(scheduler.evaluate(0u).animationId));

    // Special (Valentine's).
    clock.set(makeClockSnapshot(2027u, 2u, 14u, 19u, 0u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::SLOT_MACHINE), id(scheduler.evaluate(0u).animationId));

    // Special (New Year fireworks).
    clock.set(makeClockSnapshot(2026u, 12u, 31u, 23u, 58u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::SLOT_MACHINE), id(scheduler.evaluate(0u).animationId));
}

void test_manual_expiry_reevaluates_current_clock(void) {
    ManualTimeProvider clock;
    ManualOverride manual;
    Scheduler scheduler(clock, manual, SchedulerConfig());

    // 19:55 automatic selection.
    clock.set(makeClockSnapshot(2026u, 9u, 30u, 19u, 55u));
    const AnimationId automaticBefore = scheduler.evaluate(0u).animationId;

    // 19:58 manual DANCING_DUCK for 15 minutes.
    const uint32_t manualStart = 1000000u;
    manual.activateFor15Minutes(AnimationId::DANCING_DUCK, PlayMode::LOOP, manualStart);
    clock.set(makeClockSnapshot(2026u, 9u, 30u, 19u, 58u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::DANCING_DUCK), id(scheduler.evaluate(manualStart).animationId));

    // 20:13 manual expires: evaluate the CURRENT clock.
    clock.set(makeClockSnapshot(2026u, 9u, 30u, 20u, 13u));
    const PlaybackDecision decision = scheduler.evaluate(manualStart + kFifteenMinutesMs);
    TEST_ASSERT_NOT_EQUAL_UINT8(
        static_cast<uint8_t>(SchedulePriority::MANUAL),
        static_cast<uint8_t>(decision.priority));
    TEST_ASSERT_NOT_EQUAL_UINT8(id(AnimationId::DANCING_DUCK), id(decision.animationId));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(SchedulePriority::TIME_OF_DAY),
        static_cast<uint8_t>(decision.priority));

    // The stale pre-manual animation is not restored, but it is still part of
    // the evening pool, so only verify that the current slot is authoritative.
    const AnimationId automaticAfter = scheduler.evaluate(manualStart + kFifteenMinutesMs).animationId;
    TEST_ASSERT_NOT_EQUAL_UINT8(id(AnimationId::DANCING_DUCK), id(automaticAfter));
    (void)automaticBefore;
}

void test_manual_override_with_invalid_clock(void) {
    ManualTimeProvider clock;
    ManualOverride manual;
    Scheduler scheduler(clock, manual, SchedulerConfig());

    ClockSnapshot invalid;
    invalid.year = 0u;
    invalid.month = 1u;
    invalid.day = 1u;
    invalid.hour = 0u;
    invalid.minute = 0u;
    invalid.second = 0u;
    invalid.weekday = 4u;
    invalid.valid = false;
    clock.set(invalid);

    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::AURORA), id(scheduler.evaluate(0u).animationId));

    manual.activateUntilCancelled(AnimationId::CAMPFIRE, PlayMode::LOOP);
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::CAMPFIRE), id(scheduler.evaluate(0u).animationId));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_manual_override_inactive_by_default);
    RUN_TEST(test_manual_override_timed_expiration);
    RUN_TEST(test_manual_override_until_cancelled);
    RUN_TEST(test_manual_override_presets);
    RUN_TEST(test_manual_override_beats_quiet_hours);
    RUN_TEST(test_manual_override_beats_seasonal_and_special_events);
    RUN_TEST(test_manual_expiry_reevaluates_current_clock);
    RUN_TEST(test_manual_override_with_invalid_clock);
    return UNITY_END();
}
