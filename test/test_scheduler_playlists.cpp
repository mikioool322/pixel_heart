#include <unity.h>

#include "scheduling/ManualOverride.h"
#include "scheduling/ManualTimeProvider.h"
#include "scheduling/Playlists.h"
#include "scheduling/Scheduler.h"

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

// 17:00 -> 20:00 is the EVENING playlist row.
ClockSnapshot eveningAt(uint8_t hour, uint8_t minute, uint8_t second) {
    return makeClockSnapshot(2026u, 9u, 30u, hour, minute, second);
}

}  // namespace

void test_playlist_total_and_item_lookup(void) {
    const Playlist* evening = Playlists::find(PlaylistId::EVENING);
    TEST_ASSERT_NOT_NULL(evening);
    TEST_ASSERT_EQUAL_UINT32(780u, Playlists::totalSeconds(*evening));  // 300+180+300

    // First item for the whole first duration.
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::AURORA_SUNSET),
        id(Playlists::itemAt(*evening, 0u)->animation));
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::AURORA_SUNSET),
        id(Playlists::itemAt(*evening, 299u)->animation));

    // Second item starts exactly at 300s.
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::SUNSET_WATER),
        id(Playlists::itemAt(*evening, 300u)->animation));
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::SUNSET_WATER),
        id(Playlists::itemAt(*evening, 479u)->animation));

    // Third item at 480s.
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::CAMPFIRE),
        id(Playlists::itemAt(*evening, 480u)->animation));
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::CAMPFIRE),
        id(Playlists::itemAt(*evening, 779u)->animation));

    // Loops back to the first item at the end of the pass.
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::AURORA_SUNSET),
        id(Playlists::itemAt(*evening, 780u)->animation));
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::AURORA_SUNSET),
        id(Playlists::itemAt(*evening, 1560u)->animation));
}

void test_non_looping_playlist_finishes(void) {
    const PlaylistItem items[] = {
        { AnimationId::CAMPFIRE, 60u },
        { AnimationId::AURORA, 60u },
    };
    const Playlist playlist{items, 2u, false};

    TEST_ASSERT_NOT_NULL(Playlists::itemAt(playlist, 0u));
    TEST_ASSERT_NOT_NULL(Playlists::itemAt(playlist, 119u));
    TEST_ASSERT_NULL(Playlists::itemAt(playlist, 120u));
    TEST_ASSERT_NULL(Playlists::itemAt(playlist, 5000u));
}

void test_empty_playlist_is_harmless(void) {
    const Playlist empty{nullptr, 0u, true};
    TEST_ASSERT_NULL(Playlists::itemAt(empty, 0u));
    TEST_ASSERT_EQUAL_UINT32(0u, Playlists::totalSeconds(empty));

    const Playlist noItems{nullptr, 3u, true};
    TEST_ASSERT_NULL(Playlists::itemAt(noItems, 10u));
}

void test_playlist_is_timed_from_the_row_start(void) {
    Fixture f;

    // 17:00 is the very start of the EVENING row -> first item.
    f.clock.set(eveningAt(17u, 0u, 0u));
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::AURORA_SUNSET),
        id(f.scheduler.evaluate(0u).animationId));

    // 17:05 (300s in) -> second item.
    f.clock.set(eveningAt(17u, 5u, 0u));
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::SUNSET_WATER),
        id(f.scheduler.evaluate(0u).animationId));

    // 17:08 (480s in) -> third item.
    f.clock.set(eveningAt(17u, 8u, 0u));
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::CAMPFIRE),
        id(f.scheduler.evaluate(0u).animationId));

    // 17:13 (780s in) -> wrapped back to the first item.
    f.clock.set(eveningAt(17u, 13u, 0u));
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::AURORA_SUNSET),
        id(f.scheduler.evaluate(0u).animationId));
}

void test_playlist_advances_within_a_minute(void) {
    // The seconds field must be honoured, not just whole minutes, otherwise
    // short items would never appear.
    Fixture f;

    f.clock.set(eveningAt(17u, 4u, 59u));   // 299s in
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::AURORA_SUNSET),
        id(f.scheduler.evaluate(0u).animationId));

    f.clock.set(eveningAt(17u, 5u, 0u));    // 300s in
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::SUNSET_WATER),
        id(f.scheduler.evaluate(0u).animationId));
}

void test_playlist_evaluation_is_stable_and_pure(void) {
    // Re-evaluating within the same second must not advance anything, and the
    // item must not depend on how long the device has been running.
    Fixture f;
    f.clock.set(eveningAt(17u, 2u, 30u));

    const PlaybackDecision first = f.scheduler.evaluate(0u);
    for (uint16_t i = 0u; i < 200u; ++i) {
        TEST_ASSERT_EQUAL_UINT8(id(first.animationId), id(f.scheduler.evaluate(0u).animationId));
    }

    // "Reboot": a brand new scheduler over the same clock must agree.
    ManualOverride freshManual;
    Scheduler fresh(f.clock, freshManual, SchedulerConfig());
    TEST_ASSERT_EQUAL_UINT8(id(first.animationId), id(fresh.evaluate(0u).animationId));
}

void test_overnight_playlist_within_the_evening(void) {
    // The NIGHT row starts at 20:00 and loops every 1200s.
    Fixture f;

    // 21:00 is exactly 3600s in; 3600 % 1200 == 0 -> first item.
    f.clock.set(makeClockSnapshot(2026u, 9u, 30u, 21u, 0u, 0u));
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::MOON_STARS),
        id(f.scheduler.evaluate(0u).animationId));

    // 21:10 is 4200s in; 4200 % 1200 == 600 -> second item.
    f.clock.set(makeClockSnapshot(2026u, 9u, 30u, 21u, 10u, 0u));
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::FALLING_STARS),
        id(f.scheduler.evaluate(0u).animationId));
}

void test_overnight_row_is_continuous_across_midnight(void) {
    // The row is timed from 20:00, not from midnight, so the sequence does not
    // restart when the date changes. Quiet hours normally mask 00:00-06:00, so
    // this property is checked with quiet hours disabled.
    SchedulerConfig noQuiet;
    noQuiet.quietStartMinute = 0u;
    noQuiet.quietEndMinute = 0u;

    ManualTimeProvider clock;
    ManualOverride manual;
    Scheduler scheduler(clock, manual, noQuiet);

    // 23:10 -> 3h10m into the row == 11400s; 11400 % 1200 == 600 -> item 2.
    clock.set(makeClockSnapshot(2026u, 9u, 30u, 23u, 10u, 0u));
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::FALLING_STARS),
        id(scheduler.evaluate(0u).animationId));

    // 00:10 the next day -> 4h10m == 15000s; 15000 % 1200 == 600 -> item 2.
    // Same item despite the date change: the sequence is continuous.
    clock.set(makeClockSnapshot(2026u, 10u, 1u, 0u, 10u, 0u));
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::FALLING_STARS),
        id(scheduler.evaluate(0u).animationId));

    // 00:20 -> 15600s; 15600 % 1200 == 0 -> back to item 1, exactly on
    // schedule rather than restarted by the rollover.
    clock.set(makeClockSnapshot(2026u, 10u, 1u, 0u, 20u, 0u));
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::MOON_STARS),
        id(scheduler.evaluate(0u).animationId));
}

void test_manual_interrupts_and_auto_resumes_the_sequence(void) {
    Fixture f;

    // MANUAL overrides the playlist entirely.
    f.clock.set(eveningAt(17u, 5u, 0u));
    f.manual.activateUntilCancelled(AnimationId::DANCING_DUCK, PlayMode::LOOP);
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::DANCING_DUCK), id(f.scheduler.evaluate(0u).animationId));

    // Back to AUTO: the playlist resumes at the item the CLOCK says, not at
    // some remembered position, so no state was lost or duplicated.
    f.manual.clear();
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::SUNSET_WATER), id(f.scheduler.evaluate(0u).animationId));

    // Still the same item a few seconds later, within the same duration.
    f.clock.set(eveningAt(17u, 6u, 0u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::SUNSET_WATER), id(f.scheduler.evaluate(0u).animationId));
}

void test_schedule_boundary_switches_block(void) {
    Fixture f;

    // 19:59 is still the evening playlist (third item, 17:00 + 780s loops).
    f.clock.set(eveningAt(19u, 59u, 0u));
    const PlaybackDecision before = f.scheduler.evaluate(0u);
    TEST_ASSERT_NOT_EQUAL_UINT8(id(AnimationId::NONE), id(before.animationId));

    // 20:00 begins the night block: first item of the NIGHT playlist.
    f.clock.set(eveningAt(20u, 0u, 0u));
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::MOON_STARS),
        id(f.scheduler.evaluate(0u).animationId));
}

void test_special_date_overrides_a_playlist_row(void) {
    Fixture f;

    // Christmas Day 18:00 falls inside the evening playlist row, but the
    // special date has higher priority.
    f.clock.set(makeClockSnapshot(2026u, 12u, 25u, 18u, 0u, 0u));
    const PlaybackDecision decision = f.scheduler.evaluate(0u);
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::CHRISTMAS_TREE), id(decision.animationId));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(SchedulePriority::SPECIAL_EVENT),
        static_cast<uint8_t>(decision.priority));
}

void test_morning_playlist_row(void) {
    Fixture f;

    // The morning row is a playlist too: AURORA for 5 min then SUN_OVER_MOUNTAINS.
    f.clock.set(makeClockSnapshot(2026u, 9u, 30u, 6u, 0u, 0u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::AURORA), id(f.scheduler.evaluate(0u).animationId));

    f.clock.set(makeClockSnapshot(2026u, 9u, 30u, 6u, 5u, 0u));
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::SUN_OVER_MOUNTAINS),
        id(f.scheduler.evaluate(0u).animationId));

    // Loops back at 10 minutes.
    f.clock.set(makeClockSnapshot(2026u, 9u, 30u, 6u, 10u, 0u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::AURORA), id(f.scheduler.evaluate(0u).animationId));
}

void test_every_playlist_id_resolves(void) {
    for (uint8_t raw = 1u; raw < static_cast<uint8_t>(PlaylistId::PLAYLIST_ID_COUNT); ++raw) {
        const Playlist* playlist = Playlists::find(static_cast<PlaylistId>(raw));
        TEST_ASSERT_NOT_NULL(playlist);
        TEST_ASSERT_TRUE(playlist->count > 0u);
        TEST_ASSERT_NOT_NULL(playlist->items);
        TEST_ASSERT_TRUE(Playlists::totalSeconds(*playlist) > 0u);
    }
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_playlist_total_and_item_lookup);
    RUN_TEST(test_non_looping_playlist_finishes);
    RUN_TEST(test_empty_playlist_is_harmless);
    RUN_TEST(test_playlist_is_timed_from_the_row_start);
    RUN_TEST(test_playlist_advances_within_a_minute);
    RUN_TEST(test_playlist_evaluation_is_stable_and_pure);
    RUN_TEST(test_overnight_playlist_within_the_evening);
    RUN_TEST(test_overnight_row_is_continuous_across_midnight);
    RUN_TEST(test_manual_interrupts_and_auto_resumes_the_sequence);
    RUN_TEST(test_schedule_boundary_switches_block);
    RUN_TEST(test_special_date_overrides_a_playlist_row);
    RUN_TEST(test_morning_playlist_row);
    RUN_TEST(test_every_playlist_id_resolves);
    return UNITY_END();
}
