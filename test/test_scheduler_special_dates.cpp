#include <unity.h>

#include "scheduling/ManualOverride.h"
#include "scheduling/ManualTimeProvider.h"
#include "scheduling/Scheduler.h"
#include "scheduling/SpecialDates.h"

namespace {

uint8_t id(AnimationId value) {
    return static_cast<uint8_t>(value);
}

struct Fixture {
    ManualTimeProvider clock;
    ManualOverride manual;
    Scheduler scheduler;

    Fixture()
        : clock(),
          manual(),
          scheduler(clock, manual, SchedulerConfig()) {}
};

const SpecialDate* findRule(uint8_t month, uint8_t day) {
    for (uint8_t i = 0u; i < kSpecialDateCount; ++i) {
        if (kSpecialDates[i].month == month && kSpecialDates[i].day == day) {
            return &kSpecialDates[i];
        }
    }
    return nullptr;
}

}  // namespace

void test_normal_date_follows_the_schedule(void) {
    Fixture f;

    // An ordinary afternoon: no rule matches, so the table decides. Since the
    // midday window became the DAY playlist, 13:00 is its first item.
    f.clock.set(makeClockSnapshot(2026u, 6u, 15u, 13u, 0u));
    const PlaybackDecision decision = f.scheduler.evaluate(0u);

    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::MEADOW_FLOWER), id(decision.animationId));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(SchedulePriority::TIME_OF_DAY),
        static_cast<uint8_t>(decision.priority));
    TEST_ASSERT_NULL(SpecialDates::resolve(f.clock.now(), at(13u, 0u), false));
}

void test_christmas_day_overrides_the_schedule(void) {
    Fixture f;

    // 25 December at 13:00 would normally be the DAY playlist's first item.
    f.clock.set(makeClockSnapshot(2026u, 12u, 25u, 13u, 0u));
    const PlaybackDecision decision = f.scheduler.evaluate(0u);

    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::CHRISTMAS_TREE), id(decision.animationId));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(SchedulePriority::SPECIAL_EVENT),
        static_cast<uint8_t>(decision.priority));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(PlaybackReason::SPECIAL_EVENT),
        static_cast<uint8_t>(decision.reason));
}

void test_all_configured_christmas_days(void) {
    Fixture f;

    const uint8_t days[] = {24u, 25u, 26u};
    for (uint8_t i = 0u; i < 3u; ++i) {
        f.clock.set(makeClockSnapshot(2026u, 12u, days[i], 13u, 0u));
        TEST_ASSERT_EQUAL_UINT8(
            id(AnimationId::CHRISTMAS_TREE),
            id(f.scheduler.evaluate(0u).animationId));
    }

    // 27 December is ordinary again.
    f.clock.set(makeClockSnapshot(2026u, 12u, 27u, 13u, 0u));
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::MEADOW_FLOWER),
        id(f.scheduler.evaluate(0u).animationId));
}

void test_valentines_window_starts_at_18_00(void) {
    Fixture f;

    // 17:59 is still the normal schedule.
    f.clock.set(makeClockSnapshot(2027u, 2u, 14u, 17u, 59u));
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::SUNSET_WATER),
        id(f.scheduler.evaluate(0u).animationId));

    // 18:00 the rule begins. It resolves to the Valentine's playlist, whose
    // first item is HEARTBEAT. (The rule used to name LOVE_MODE, which has no
    // implementation; the window itself is unchanged.)
    f.clock.set(makeClockSnapshot(2027u, 2u, 14u, 18u, 0u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::HEARTBEAT), id(f.scheduler.evaluate(0u).animationId));

    // Still a special event late in the evening, and still one of the two
    // Valentine's animations.
    f.clock.set(makeClockSnapshot(2027u, 2u, 14u, 23u, 30u));
    const PlaybackDecision late = f.scheduler.evaluate(0u);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(PlaybackReason::SPECIAL_EVENT),
        static_cast<uint8_t>(late.reason));
    const bool isLoveAnimation =
        late.animationId == AnimationId::HEARTBEAT ||
        late.animationId == AnimationId::ROSE_BLOOM;
    TEST_ASSERT_TRUE(isLoveAnimation);

    // The next day is ordinary.
    f.clock.set(makeClockSnapshot(2027u, 2u, 15u, 20u, 0u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::MOON_STARS), id(f.scheduler.evaluate(0u).animationId));
}

void test_new_year_countdown(void) {
    Fixture f;

    // 20:00 -> 23:58 is the countdown pool representative.
    f.clock.set(makeClockSnapshot(2026u, 12u, 31u, 21u, 0u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::AURORA_NEON), id(f.scheduler.evaluate(0u).animationId));

    // 23:58 -> fireworks.
    f.clock.set(makeClockSnapshot(2026u, 12u, 31u, 23u, 58u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::FIREWORKS), id(f.scheduler.evaluate(0u).animationId));

    f.clock.set(makeClockSnapshot(2026u, 12u, 31u, 23u, 59u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::FIREWORKS), id(f.scheduler.evaluate(0u).animationId));
}

void test_new_year_midnight_is_the_only_quiet_hours_override(void) {
    Fixture f;

    // 00:05 on 1 January is inside quiet hours, but the rule is marked
    // overridesQuiet so the fireworks still play.
    f.clock.set(makeClockSnapshot(2027u, 1u, 1u, 0u, 5u));
    const PlaybackDecision decision = f.scheduler.evaluate(0u);
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::FIREWORKS), id(decision.animationId));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(SchedulePriority::SPECIAL_EVENT),
        static_cast<uint8_t>(decision.priority));

    // 00:10 the rule ends and quiet hours take over again.
    f.clock.set(makeClockSnapshot(2027u, 1u, 1u, 0u, 10u));
    const PlaybackDecision after = f.scheduler.evaluate(0u);
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::NONE), id(after.animationId));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(PlaybackReason::QUIET_HOURS),
        static_cast<uint8_t>(after.reason));
}

void test_special_dates_do_not_override_quiet_hours_by_default(void) {
    Fixture f;

    // Valentine's and Christmas rules are not marked overridesQuiet, so the
    // small hours stay dark.
    f.clock.set(makeClockSnapshot(2027u, 2u, 14u, 2u, 0u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::NONE), id(f.scheduler.evaluate(0u).animationId));

    f.clock.set(makeClockSnapshot(2026u, 12u, 25u, 3u, 0u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::NONE), id(f.scheduler.evaluate(0u).animationId));

    // The rules exist but are suppressed while quiet.
    const SpecialDate* valentine = findRule(2u, 14u);
    TEST_ASSERT_NOT_NULL(valentine);
    TEST_ASSERT_FALSE(valentine->overridesQuiet);
    TEST_ASSERT_NOT_NULL(
        SpecialDates::resolve(makeClockSnapshot(2027u, 2u, 14u, 19u, 0u), at(19u, 0u), false));
    TEST_ASSERT_NULL(
        SpecialDates::resolve(makeClockSnapshot(2027u, 2u, 14u, 2u, 0u), at(2u, 0u), true));
}

void test_manual_beats_special_dates(void) {
    Fixture f;

    f.manual.activateUntilCancelled(AnimationId::SLOT_MACHINE, PlayMode::LOOP);

    f.clock.set(makeClockSnapshot(2026u, 12u, 25u, 13u, 0u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::SLOT_MACHINE), id(f.scheduler.evaluate(0u).animationId));

    f.clock.set(makeClockSnapshot(2027u, 2u, 14u, 19u, 0u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::SLOT_MACHINE), id(f.scheduler.evaluate(0u).animationId));

    f.clock.set(makeClockSnapshot(2027u, 1u, 1u, 0u, 5u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::SLOT_MACHINE), id(f.scheduler.evaluate(0u).animationId));
}

void test_special_date_resolution_is_stable(void) {
    // A special day must not "restart" anything when re-evaluated: the
    // decision is identical every time within the same minute.
    Fixture f;
    f.clock.set(makeClockSnapshot(2026u, 12u, 25u, 13u, 0u));

    const PlaybackDecision first = f.scheduler.evaluate(0u);
    for (uint16_t i = 0u; i < 100u; ++i) {
        const PlaybackDecision again = f.scheduler.evaluate(0u);
        TEST_ASSERT_EQUAL_UINT8(id(first.animationId), id(again.animationId));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(first.priority),
            static_cast<uint8_t>(again.priority));
    }
}

void test_year_rollover_is_handled(void) {
    Fixture f;

    // 31 Dec 23:59 -> 1 Jan 00:05 crosses both the year and the rule.
    f.clock.set(makeClockSnapshot(2026u, 12u, 31u, 23u, 59u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::FIREWORKS), id(f.scheduler.evaluate(0u).animationId));

    f.clock.set(makeClockSnapshot(2027u, 1u, 1u, 0u, 5u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::FIREWORKS), id(f.scheduler.evaluate(0u).animationId));
}

void test_halloween_is_october_31_only(void) {
    Fixture f;

    // 30 October is ordinary.
    f.clock.set(makeClockSnapshot(2026u, 10u, 30u, 20u, 0u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::MOON_STARS), id(f.scheduler.evaluate(0u).animationId));

    // 31 October: the ghost.
    f.clock.set(makeClockSnapshot(2026u, 10u, 31u, 20u, 0u));
    const PlaybackDecision halloween = f.scheduler.evaluate(0u);
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::HALLOWEEN_GHOST), id(halloween.animationId));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(SchedulePriority::SPECIAL_EVENT),
        static_cast<uint8_t>(halloween.priority));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(PlaybackReason::SPECIAL_EVENT),
        static_cast<uint8_t>(halloween.reason));

    // 1 November is ordinary again.
    f.clock.set(makeClockSnapshot(2026u, 11u, 1u, 20u, 0u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::MOON_STARS), id(f.scheduler.evaluate(0u).animationId));
}

void test_halloween_covers_the_whole_local_day(void) {
    Fixture f;

    // The rule runs from local midnight to local midnight, so it is the civil
    // date that decides, never UTC. Inside quiet hours it is suppressed like
    // every rule except New Year's midnight, so the window starts at 06:00.
    f.clock.set(makeClockSnapshot(2026u, 10u, 31u, 6u, 0u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::HALLOWEEN_GHOST), id(f.scheduler.evaluate(0u).animationId));

    f.clock.set(makeClockSnapshot(2026u, 10u, 31u, 13u, 0u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::HALLOWEEN_GHOST), id(f.scheduler.evaluate(0u).animationId));

    f.clock.set(makeClockSnapshot(2026u, 10u, 31u, 23u, 59u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::HALLOWEEN_GHOST), id(f.scheduler.evaluate(0u).animationId));

    // 05:59 is still quiet, so the rule exists but stays suppressed.
    f.clock.set(makeClockSnapshot(2026u, 10u, 31u, 5u, 59u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::NONE), id(f.scheduler.evaluate(0u).animationId));
}

void test_halloween_does_not_override_quiet_hours(void) {
    Fixture f;

    // 03:00 on 31 October is quiet, and the rule is not marked overridesQuiet,
    // exactly like every rule except New Year's midnight.
    f.clock.set(makeClockSnapshot(2026u, 10u, 31u, 3u, 0u));
    const PlaybackDecision decision = f.scheduler.evaluate(0u);
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::NONE), id(decision.animationId));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(PlaybackReason::QUIET_HOURS),
        static_cast<uint8_t>(decision.reason));
}

void test_halloween_rule_is_present_and_not_a_quiet_override(void) {
    const SpecialDate* rule = findRule(10u, 31u);
    TEST_ASSERT_NOT_NULL(rule);
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::HALLOWEEN_GHOST), id(rule->animation));
    TEST_ASSERT_EQUAL_UINT16(0u, rule->startMinute);
    TEST_ASSERT_EQUAL_UINT16(kEndOfDay, rule->endMinute);
    TEST_ASSERT_FALSE(rule->overridesQuiet);
}

void test_manual_beats_halloween(void) {
    Fixture f;
    f.manual.activateUntilCancelled(AnimationId::CAMPFIRE, PlayMode::LOOP);

    f.clock.set(makeClockSnapshot(2026u, 10u, 31u, 20u, 0u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::CAMPFIRE), id(f.scheduler.evaluate(0u).animationId));
}

void test_halloween_is_never_guessed_without_a_valid_clock(void) {
    Fixture f;

    // Pre-NTP the clock is not trustworthy, so the scheduler must fall back
    // rather than invent a date.
    ClockSnapshot invalid;
    invalid.year = 0u;
    invalid.month = 1u;
    invalid.day = 1u;
    invalid.hour = 0u;
    invalid.minute = 0u;
    invalid.second = 0u;
    invalid.weekday = 0u;
    invalid.valid = false;
    f.clock.set(invalid);

    const PlaybackDecision decision = f.scheduler.evaluate(0u);
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::AURORA), id(decision.animationId));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(PlaybackReason::FALLBACK),
        static_cast<uint8_t>(decision.reason));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_normal_date_follows_the_schedule);
    RUN_TEST(test_christmas_day_overrides_the_schedule);
    RUN_TEST(test_all_configured_christmas_days);
    RUN_TEST(test_valentines_window_starts_at_18_00);
    RUN_TEST(test_new_year_countdown);
    RUN_TEST(test_new_year_midnight_is_the_only_quiet_hours_override);
    RUN_TEST(test_special_dates_do_not_override_quiet_hours_by_default);
    RUN_TEST(test_manual_beats_special_dates);
    RUN_TEST(test_special_date_resolution_is_stable);
    RUN_TEST(test_year_rollover_is_handled);
    RUN_TEST(test_halloween_is_october_31_only);
    RUN_TEST(test_halloween_covers_the_whole_local_day);
    RUN_TEST(test_halloween_does_not_override_quiet_hours);
    RUN_TEST(test_halloween_rule_is_present_and_not_a_quiet_override);
    RUN_TEST(test_manual_beats_halloween);
    RUN_TEST(test_halloween_is_never_guessed_without_a_valid_clock);
    return UNITY_END();
}
