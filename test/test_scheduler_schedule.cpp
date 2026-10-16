#include <unity.h>

#include "core/AnimationRegistry.h"
#include "scheduling/ManualOverride.h"
#include "scheduling/ManualTimeProvider.h"
#include "scheduling/ScheduleTable.h"
#include "scheduling/Scheduler.h"

namespace {

uint8_t id(AnimationId value) {
    return static_cast<uint8_t>(value);
}

ClockSnapshot atDate(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute) {
    return makeClockSnapshot(year, month, day, hour, minute, 0u);
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

constexpr uint16_t kQuietStart = SchedulerDefaults::kQuietStartMinute;  // 0
constexpr uint16_t kQuietEnd = SchedulerDefaults::kQuietEndMinute;      // 360

}  // namespace

// ---------------------------------------------------------------------------
// Schedule table resolution (pure)
// ---------------------------------------------------------------------------

void test_schedule_row_lookup_is_inclusive_start_exclusive_end(void) {
    // 09:00 -> 12:00 row: the start minute belongs to it, the end minute does not.
    const ScheduleEntry* found = ScheduleTable::resolveEntry(at(9u, 0u));
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::SUN_OVER_MOUNTAINS), id(found->animation));

    // One minute before the boundary still belongs to the previous row.
    found = ScheduleTable::resolveEntry(at(8u, 59u));
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::SUN_OVER_MOUNTAINS), id(found->animation));

    // Exactly at 12:00 the next row takes over: the daytime playlist row.
    found = ScheduleTable::resolveEntry(at(12u, 0u));
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(PlaylistId::DAY),
        static_cast<uint8_t>(found->playlist));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::NONE), id(found->animation));

    // 11:59 is still the morning row.
    found = ScheduleTable::resolveEntry(at(11u, 59u));
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::SUN_OVER_MOUNTAINS), id(found->animation));
}

void test_schedule_overnight_row_wraps_midnight(void) {
    // The 20:00 -> 06:00 row must match on both sides of midnight.
    const ScheduleEntry* lateEvening = ScheduleTable::resolveEntry(at(23u, 30u));
    const ScheduleEntry* afterMidnight = ScheduleTable::resolveEntry(at(2u, 0u));
    const ScheduleEntry* justBefore = ScheduleTable::resolveEntry(at(19u, 59u));

    // Both sides of midnight must select the same (overnight) row.
    TEST_ASSERT_NOT_NULL(lateEvening);
    TEST_ASSERT_NOT_NULL(afterMidnight);
    TEST_ASSERT_TRUE(lateEvening == afterMidnight);

    // 19:59 belongs to the previous (sunset) row, not the overnight one.
    TEST_ASSERT_NOT_NULL(justBefore);
    TEST_ASSERT_TRUE(justBefore != lateEvening);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(PlaylistId::EVENING),
        static_cast<uint8_t>(justBefore->playlist));

    // Exactly 06:00 leaves the overnight row.
    const ScheduleEntry* morning = ScheduleTable::resolveEntry(at(6u, 0u));
    TEST_ASSERT_NOT_NULL(morning);
    TEST_ASSERT_TRUE(morning != lateEvening);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(PlaylistId::MORNING),
        static_cast<uint8_t>(morning->playlist));
}

void test_schedule_covers_the_whole_active_day(void) {
    // Every non-quiet minute must resolve to a row, so the panel never falls
    // through to the fallback during the day.
    for (uint16_t minute = kQuietEnd; minute < kEndOfDay; ++minute) {
        TEST_ASSERT_NOT_NULL(ScheduleTable::resolveEntry(minute));
    }
}

// ---------------------------------------------------------------------------
// Scheduler evaluation
// ---------------------------------------------------------------------------

void test_auto_follows_the_schedule_through_the_day(void) {
    Fixture f;

    struct Case {
        uint8_t hour;
        uint8_t minute;
        AnimationId expected;
    };

    const Case cases[] = {
        {7u, 0u, AnimationId::AURORA},
        {9u, 0u, AnimationId::SUN_OVER_MOUNTAINS},
        {13u, 30u, AnimationId::PALM_BREEZE},
        {18u, 0u, AnimationId::CAMPFIRE},
        {21u, 0u, AnimationId::MOON_STARS},
    };

    for (uint8_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        f.clock.set(atDate(2026u, 9u, 30u, cases[i].hour, cases[i].minute));
        const PlaybackDecision decision = f.scheduler.evaluate(0u);
        TEST_ASSERT_EQUAL_UINT8(id(cases[i].expected), id(decision.animationId));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(SchedulePriority::TIME_OF_DAY),
            static_cast<uint8_t>(decision.priority));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(PlaybackReason::SCHEDULE),
            static_cast<uint8_t>(decision.reason));
    }
}

void test_quiet_hours_are_dark(void) {
    Fixture f;

    const uint8_t hours[] = {0u, 2u, 5u};
    for (uint8_t i = 0u; i < 3u; ++i) {
        f.clock.set(atDate(2026u, 9u, 30u, hours[i], 30u));
        const PlaybackDecision decision = f.scheduler.evaluate(0u);
        TEST_ASSERT_EQUAL_UINT8(id(AnimationId::NONE), id(decision.animationId));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(PlaybackReason::QUIET_HOURS),
            static_cast<uint8_t>(decision.reason));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(SchedulePriority::TIME_OF_DAY),
            static_cast<uint8_t>(decision.priority));
    }

    // 05:59 still quiet, 06:00 not.
    f.clock.set(atDate(2026u, 9u, 30u, 5u, 59u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::NONE), id(f.scheduler.evaluate(0u).animationId));
    f.clock.set(atDate(2026u, 9u, 30u, 6u, 0u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::AURORA), id(f.scheduler.evaluate(0u).animationId));
}

void test_midnight_and_rollover_are_consistent(void) {
    Fixture f;

    // 23:59 is 3h59m into the overnight NIGHT row (20:00 start) == 14340s.
    // NIGHT loops every 1200s, and 14340 % 1200 == 1140, which is the third
    // item. Deterministic, and derived from the clock rather than a counter.
    f.clock.set(atDate(2026u, 9u, 30u, 23u, 59u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::CAMPFIRE), id(f.scheduler.evaluate(0u).animationId));

    // Midnight itself is inside quiet hours, so the panel goes dark. This is
    // intentional: quiet hours span 00:00-06:00 and mask the overnight row.
    f.clock.set(atDate(2026u, 10u, 1u, 0u, 0u));
    const PlaybackDecision decision = f.scheduler.evaluate(0u);
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::NONE), id(decision.animationId));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(PlaybackReason::QUIET_HOURS),
        static_cast<uint8_t>(decision.reason));
}

void test_invalid_clock_uses_fallback(void) {
    Fixture f;

    ClockSnapshot invalid;
    invalid.year = 0u;
    invalid.month = 1u;
    invalid.day = 1u;
    invalid.hour = 5u;
    invalid.minute = 0u;
    invalid.second = 0u;
    invalid.weekday = 4u;
    invalid.valid = false;
    f.clock.set(invalid);

    const PlaybackDecision decision = f.scheduler.evaluate(0u);
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::AURORA), id(decision.animationId));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(SchedulePriority::FALLBACK),
        static_cast<uint8_t>(decision.priority));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(PlaybackReason::FALLBACK),
        static_cast<uint8_t>(decision.reason));
}

void test_evaluation_is_stable_within_a_minute(void) {
    // evaluate() must be a pure function of the clock: repeated calls cannot
    // advance anything or produce a different answer.
    Fixture f;
    f.clock.set(atDate(2026u, 9u, 30u, 13u, 37u));

    const PlaybackDecision first = f.scheduler.evaluate(0u);
    for (uint16_t i = 0u; i < 200u; ++i) {
        const PlaybackDecision again = f.scheduler.evaluate(0u);
        TEST_ASSERT_EQUAL_UINT8(id(first.animationId), id(again.animationId));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(first.reason),
            static_cast<uint8_t>(again.reason));
    }
}

void test_manual_mode_ignores_the_schedule(void) {
    Fixture f;

    // At a scheduled time, MANUAL still wins.
    f.clock.set(atDate(2026u, 9u, 30u, 13u, 0u));
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::MEADOW_FLOWER),
        id(f.scheduler.evaluate(0u).animationId));

    f.manual.activateUntilCancelled(AnimationId::DANCING_DUCK, PlayMode::LOOP);
    const PlaybackDecision manual = f.scheduler.evaluate(0u);
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::DANCING_DUCK), id(manual.animationId));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(SchedulePriority::MANUAL),
        static_cast<uint8_t>(manual.priority));

    // Even during quiet hours.
    f.clock.set(atDate(2026u, 9u, 30u, 2u, 0u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::DANCING_DUCK), id(f.scheduler.evaluate(0u).animationId));
}

void test_manual_to_auto_reconciles_immediately(void) {
    Fixture f;
    f.clock.set(atDate(2026u, 9u, 30u, 21u, 0u));

    f.manual.activateUntilCancelled(AnimationId::CAMPFIRE, PlayMode::LOOP);
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::CAMPFIRE), id(f.scheduler.evaluate(0u).animationId));

    // Switching back to AUTO must reflect the CURRENT time at once, not at the
    // next schedule boundary.
    f.manual.clear();
    const PlaybackDecision autoDecision = f.scheduler.evaluate(0u);
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::MOON_STARS), id(autoDecision.animationId));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(SchedulePriority::TIME_OF_DAY),
        static_cast<uint8_t>(autoDecision.priority));
}

void test_ntp_correction_does_not_storm_the_animation(void) {
    // A large jump in civil time (what an NTP correction looks like) must
    // produce one new decision, not a burst. Because evaluate() is pure, the
    // decision depends only on where the clock lands.
    Fixture f;

    f.clock.set(atDate(2026u, 9u, 30u, 3u, 0u));   // wrong: quiet hours
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::NONE), id(f.scheduler.evaluate(0u).animationId));

    f.clock.set(atDate(2026u, 9u, 30u, 14u, 0u));  // corrected
    const PlaybackDecision corrected = f.scheduler.evaluate(0u);
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::MEADOW_FLOWER), id(corrected.animationId));

    // Re-evaluating at the corrected time yields the same decision, so
    // PlaybackPolicy sees no change and does not restart anything.
    for (uint8_t i = 0u; i < 50u; ++i) {
        TEST_ASSERT_EQUAL_UINT8(
            id(corrected.animationId),
            id(f.scheduler.evaluate(0u).animationId));
    }
}

void test_configurable_quiet_hours(void) {
    // Quiet hours are the one part of the schedule that lives in the config
    // object rather than the table, so verify the boundary maths directly.
    SchedulerConfig config;
    config.quietStartMinute = at(23u, 0u);
    config.quietEndMinute = at(7u, 0u);

    ManualTimeProvider clock;
    ManualOverride manual;
    Scheduler scheduler(clock, manual, config);

    TEST_ASSERT_TRUE(scheduler.isQuietMinute(at(23u, 30u)));
    TEST_ASSERT_TRUE(scheduler.isQuietMinute(at(2u, 0u)));
    TEST_ASSERT_TRUE(scheduler.isQuietMinute(at(6u, 59u)));
    TEST_ASSERT_FALSE(scheduler.isQuietMinute(at(7u, 0u)));
    TEST_ASSERT_FALSE(scheduler.isQuietMinute(at(12u, 0u)));
}

// ---------------------------------------------------------------------------
// DAY playlist: 12:00 -> 17:00
//
// Midday and early afternoon used to show SUNSET_WATER, which is semantically
// wrong for the brightest part of the day. The window is now a canonical
// playlist row, exactly like MORNING / EVENING / NIGHT.
// ---------------------------------------------------------------------------

namespace {
constexpr uint16_t kDayStart = at(12u, 0u);
constexpr uint16_t kDayEnd = at(17u, 0u);
}  // namespace

void test_day_row_is_a_playlist_row_not_a_single_animation(void) {
    const ScheduleEntry* row = ScheduleTable::resolveEntry(kDayStart);
    TEST_ASSERT_NOT_NULL(row);
    TEST_ASSERT_EQUAL_UINT16(kDayStart, row->startMinute);
    TEST_ASSERT_EQUAL_UINT16(kDayEnd, row->endMinute);

    // The row carries the DAY playlist, and no direct animation: exactly the
    // shape MORNING / EVENING / NIGHT already use.
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(PlaylistId::DAY),
        static_cast<uint8_t>(row->playlist));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::NONE), id(row->animation));
}

void test_day_playlist_content_and_length(void) {
    const Playlist* day = Playlists::find(PlaylistId::DAY);
    TEST_ASSERT_NOT_NULL(day);
    TEST_ASSERT_TRUE(day->loop);
    TEST_ASSERT_EQUAL_UINT8(4u, day->count);

    const AnimationId expected[4] = {
        AnimationId::MEADOW_FLOWER,
        AnimationId::BLUE_SKY_CLOUDS,
        AnimationId::PALM_BREEZE,
        AnimationId::JELLYFISH,
    };

    for (uint8_t i = 0u; i < 4u; ++i) {
        TEST_ASSERT_EQUAL_UINT8(id(expected[i]), id(day->items[i].animation));
        TEST_ASSERT_EQUAL_UINT16(300u, day->items[i].durationSeconds);   // 5 min
    }

    // One full pass is 20 minutes.
    TEST_ASSERT_EQUAL_UINT32(1200u, Playlists::totalSeconds(*day));
}

void test_day_playlist_items_are_all_renderable(void) {
    const Playlist* day = Playlists::find(PlaylistId::DAY);
    TEST_ASSERT_NOT_NULL(day);

    for (uint8_t i = 0u; i < day->count; ++i) {
        TEST_ASSERT_TRUE(AnimationRegistry::isRenderable(day->items[i].animation));
    }
}

void test_day_window_resolves_through_the_playlist(void) {
    Fixture f;

    struct Case {
        uint8_t hour;
        uint8_t minute;
        AnimationId expected;
    };

    // The item is derived purely from how far into the 12:00 window we are.
    const Case cases[] = {
        {12u, 0u,  AnimationId::MEADOW_FLOWER},
        {12u, 4u,  AnimationId::MEADOW_FLOWER},
        {12u, 5u,  AnimationId::BLUE_SKY_CLOUDS},
        {12u, 9u,  AnimationId::BLUE_SKY_CLOUDS},
        {12u, 10u, AnimationId::PALM_BREEZE},
        {12u, 14u, AnimationId::PALM_BREEZE},
        {12u, 15u, AnimationId::JELLYFISH},
        {12u, 19u, AnimationId::JELLYFISH},
        {12u, 20u, AnimationId::MEADOW_FLOWER},   // second cycle begins
        {13u, 0u,  AnimationId::MEADOW_FLOWER},   // 60 min in, 60 % 20 == 0
        {13u, 30u, AnimationId::PALM_BREEZE},
        {14u, 37u, AnimationId::JELLYFISH},
        {16u, 0u,  AnimationId::MEADOW_FLOWER},
        {16u, 59u, AnimationId::JELLYFISH},
    };

    for (uint8_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        f.clock.set(atDate(2026u, 6u, 15u, cases[i].hour, cases[i].minute));
        const PlaybackDecision decision = f.scheduler.evaluate(0u);

        TEST_ASSERT_EQUAL_UINT8(id(cases[i].expected), id(decision.animationId));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(SchedulePriority::TIME_OF_DAY),
            static_cast<uint8_t>(decision.priority));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(PlaybackReason::SCHEDULE),
            static_cast<uint8_t>(decision.reason));
    }
}

void test_day_window_boundaries_are_inclusive_start_exclusive_end(void) {
    Fixture f;

    // 11:59:59 is still the morning row.
    f.clock.set(atDate(2026u, 6u, 15u, 11u, 59u));
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::SUN_OVER_MOUNTAINS),
        id(f.scheduler.evaluate(0u).animationId));

    // 12:00:00 the DAY playlist takes over, at its first item.
    f.clock.set(atDate(2026u, 6u, 15u, 12u, 0u));
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::MEADOW_FLOWER),
        id(f.scheduler.evaluate(0u).animationId));

    // 16:59:59 the DAY playlist is still active.
    f.clock.set(atDate(2026u, 6u, 15u, 16u, 59u));
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::JELLYFISH),
        id(f.scheduler.evaluate(0u).animationId));

    // 17:00:00 the EVENING playlist begins, untouched by this change.
    f.clock.set(atDate(2026u, 6u, 15u, 17u, 0u));
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::AURORA_SUNSET),
        id(f.scheduler.evaluate(0u).animationId));
}

void test_day_window_holds_exactly_fifteen_cycles(void) {
    const Playlist* day = Playlists::find(PlaylistId::DAY);
    TEST_ASSERT_NOT_NULL(day);

    const uint32_t windowMinutes = static_cast<uint32_t>(kDayEnd - kDayStart);
    TEST_ASSERT_EQUAL_UINT32(300u, windowMinutes);   // 5 hours

    const uint32_t cycleSeconds = Playlists::totalSeconds(*day);
    TEST_ASSERT_EQUAL_UINT32(1200u, cycleSeconds);   // 20 minutes

    // A whole number of cycles, with no partial cycle left at 17:00.
    const uint32_t windowSeconds = windowMinutes * 60u;
    TEST_ASSERT_EQUAL_UINT32(15u, windowSeconds / cycleSeconds);
    TEST_ASSERT_EQUAL_UINT32(0u, windowSeconds % cycleSeconds);
}

void test_day_resolution_is_time_derived_and_stateless(void) {
    // A reboot at 14:37 must land on the correct item directly: evaluate() is a
    // pure function of the civil clock, with no stored playlist position.
    Fixture first;
    Fixture second;

    first.clock.set(atDate(2026u, 6u, 15u, 14u, 37u));
    second.clock.set(atDate(2026u, 6u, 15u, 14u, 37u));

    const PlaybackDecision a = first.scheduler.evaluate(0u);
    const PlaybackDecision b = second.scheduler.evaluate(0u);

    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::JELLYFISH), id(a.animationId));
    TEST_ASSERT_EQUAL_UINT8(id(a.animationId), id(b.animationId));
}

void test_manual_still_overrides_the_day_playlist(void) {
    Fixture f;
    f.clock.set(atDate(2026u, 6u, 15u, 14u, 37u));

    // AUTO picks the DAY item...
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::JELLYFISH),
        id(f.scheduler.evaluate(0u).animationId));

    // ...and MANUAL still wins over it.
    f.manual.activateUntilCancelled(AnimationId::DANCING_DUCK, PlayMode::LOOP);
    const PlaybackDecision manual = f.scheduler.evaluate(0u);
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::DANCING_DUCK), id(manual.animationId));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(SchedulePriority::MANUAL),
        static_cast<uint8_t>(manual.priority));
}

void test_sunset_water_left_the_midday_row_but_survived_in_evening(void) {
    // The midday row no longer names SUNSET_WATER directly...
    for (uint16_t minute = kDayStart; minute < kDayEnd; ++minute) {
        const ScheduleEntry* row = ScheduleTable::resolveEntry(minute);
        TEST_ASSERT_NOT_NULL(row);
        TEST_ASSERT_EQUAL_UINT8(id(AnimationId::NONE), id(row->animation));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(PlaylistId::DAY),
            static_cast<uint8_t>(row->playlist));
    }

    // ...but it is still the second item of the untouched EVENING playlist.
    const Playlist* evening = Playlists::find(PlaylistId::EVENING);
    TEST_ASSERT_NOT_NULL(evening);

    bool found = false;
    for (uint8_t i = 0u; i < evening->count; ++i) {
        if (evening->items[i].animation == AnimationId::SUNSET_WATER) {
            found = true;
        }
    }
    TEST_ASSERT_TRUE(found);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_schedule_row_lookup_is_inclusive_start_exclusive_end);
    RUN_TEST(test_schedule_overnight_row_wraps_midnight);
    RUN_TEST(test_schedule_covers_the_whole_active_day);
    RUN_TEST(test_auto_follows_the_schedule_through_the_day);
    RUN_TEST(test_quiet_hours_are_dark);
    RUN_TEST(test_midnight_and_rollover_are_consistent);
    RUN_TEST(test_invalid_clock_uses_fallback);
    RUN_TEST(test_evaluation_is_stable_within_a_minute);
    RUN_TEST(test_manual_mode_ignores_the_schedule);
    RUN_TEST(test_manual_to_auto_reconciles_immediately);
    RUN_TEST(test_ntp_correction_does_not_storm_the_animation);
    RUN_TEST(test_configurable_quiet_hours);
    RUN_TEST(test_day_row_is_a_playlist_row_not_a_single_animation);
    RUN_TEST(test_day_playlist_content_and_length);
    RUN_TEST(test_day_playlist_items_are_all_renderable);
    RUN_TEST(test_day_window_resolves_through_the_playlist);
    RUN_TEST(test_day_window_boundaries_are_inclusive_start_exclusive_end);
    RUN_TEST(test_day_window_holds_exactly_fifteen_cycles);
    RUN_TEST(test_day_resolution_is_time_derived_and_stateless);
    RUN_TEST(test_manual_still_overrides_the_day_playlist);
    RUN_TEST(test_sunset_water_left_the_midday_row_but_survived_in_evening);
    return UNITY_END();
}
