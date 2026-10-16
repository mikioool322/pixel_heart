#include <unity.h>

#include "core/AnimationRegistry.h"
#include "scheduling/ManualOverride.h"
#include "scheduling/ManualTimeProvider.h"
#include "scheduling/Playlists.h"
#include "scheduling/ScheduleTable.h"
#include "scheduling/Scheduler.h"
#include "scheduling/SpecialDates.h"

// Valentine's Day content.
//
// The rule used to name AnimationId::LOVE_MODE, which has no implementation in
// this build, so playback fell back to AURORA instead of showing anything
// Valentine's. It now resolves to a playlist of two animations that already
// exist: HEARTBEAT alternating with ROSE_BLOOM.
//
// What is pinned down here:
//   - the DATE/TIME WINDOW is unchanged: 14 February, 18:00 -> end of day
//   - the mix is deterministic and repeats HEARTBEAT -> ROSE_BLOOM -> ...
//   - the slot length never cuts an animation off before it completes once
//   - LOVE_MODE is no longer reachable from the shipped tables
//   - manual still wins, quiet hours still win, neighbours' days are unaffected
//   - AnimationId ordinals are untouched

namespace {

uint8_t id(AnimationId value) {
    return static_cast<uint8_t>(value);
}

// The existing window, asserted rather than assumed.
constexpr uint16_t kValentinesStart = at(18u, 0u);
constexpr uint16_t kValentinesEnd = kEndOfDay;

struct Fixture {
    ManualTimeProvider clock;
    ManualOverride manual;
    Scheduler scheduler;

    Fixture()
        : clock(),
          manual(),
          scheduler(clock, manual, SchedulerConfig()) {}
};

Fixture onValentines(uint8_t hour, uint8_t minute, uint8_t second = 0u) {
    Fixture f;
    f.clock.set(makeClockSnapshot(2027u, 2u, 14u, hour, minute, second));
    return f;
}

// A scheduler evaluated at `offsetSeconds` into the Valentine's window, which
// starts at 18:00. The mix is timed from that start, so the offset fully
// determines the item.
Fixture atOffsetIntoWindow(uint32_t offsetSeconds) {
    const uint32_t minuteOfDay = at(18u, 0u) + (offsetSeconds / 60u);

    Fixture f;
    f.clock.set(makeClockSnapshot(
        2027u,
        2u,
        14u,
        static_cast<uint8_t>(minuteOfDay / 60u),
        static_cast<uint8_t>(minuteOfDay % 60u),
        static_cast<uint8_t>(offsetSeconds % 60u)));
    return f;
}

Fixture onDate(uint8_t month, uint8_t day, uint8_t hour, uint8_t minute) {
    Fixture f;
    f.clock.set(makeClockSnapshot(2027u, month, day, hour, minute, 0u));
    return f;
}

const SpecialDate* valentinesRule() {
    for (uint8_t i = 0u; i < kSpecialDateCount; ++i) {
        if (kSpecialDates[i].month == 2u && kSpecialDates[i].day == 14u) {
            return &kSpecialDates[i];
        }
    }
    return nullptr;
}

}  // namespace

// ===========================================================================
// The window itself: 14 February, 18:00 -> end of day. Unchanged.
// ===========================================================================

void test_valentines_window_is_unchanged(void) {
    const SpecialDate* rule = valentinesRule();
    TEST_ASSERT_NOT_NULL(rule);
    TEST_ASSERT_EQUAL_UINT8(2u, rule->month);
    TEST_ASSERT_EQUAL_UINT8(14u, rule->day);
    TEST_ASSERT_EQUAL_UINT16(kValentinesStart, rule->startMinute);
    TEST_ASSERT_EQUAL_UINT16(kValentinesEnd, rule->endMinute);

    // Deliberately NOT expanded to the whole day.
    TEST_ASSERT_EQUAL_UINT16(at(18u, 0u), rule->startMinute);
    TEST_ASSERT_NOT_EQUAL_UINT16(at(0u, 0u), rule->startMinute);

    // Quiet hours are not overridden, exactly as before.
    TEST_ASSERT_FALSE(rule->overridesQuiet);
}

// ===========================================================================
// 1/2/3/14: neighbouring days and times are untouched.
// ===========================================================================

void test_feb_13_is_a_normal_day(void) {
    Fixture f = onDate(2u, 13u, 19u, 0u);

    TEST_ASSERT_NULL(SpecialDates::resolve(f.clock.now(), at(19u, 0u), false));

    // 19:00 on an ordinary day is the evening playlist, not a special date.
    const PlaybackDecision decision = f.scheduler.evaluate(0u);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(SchedulePriority::TIME_OF_DAY),
        static_cast<uint8_t>(decision.priority));
}

void test_feb_14_before_the_window_is_the_normal_schedule(void) {
    Fixture f = onValentines(17u, 59u);

    TEST_ASSERT_NULL(SpecialDates::resolve(f.clock.now(), at(17u, 59u), false));

    // 17:59 is still the evening block.
    const PlaybackDecision decision = f.scheduler.evaluate(0u);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(SchedulePriority::TIME_OF_DAY),
        static_cast<uint8_t>(decision.priority));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(PlaybackReason::SCHEDULE),
        static_cast<uint8_t>(decision.reason));

    // Exactly at 18:00 the rule begins.
    f.clock.set(makeClockSnapshot(2027u, 2u, 14u, 18u, 0u, 0u));
    TEST_ASSERT_NOT_NULL(SpecialDates::resolve(f.clock.now(), at(18u, 0u), false));
}

void test_feb_14_inside_the_window_is_a_special_date(void) {
    Fixture f = onValentines(19u, 0u);

    const PlaybackDecision decision = f.scheduler.evaluate(0u);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(SchedulePriority::SPECIAL_EVENT),
        static_cast<uint8_t>(decision.priority));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(PlaybackReason::SPECIAL_EVENT),
        static_cast<uint8_t>(decision.reason));
}

void test_feb_15_returns_to_the_normal_schedule(void) {
    Fixture f = onDate(2u, 15u, 20u, 0u);

    TEST_ASSERT_NULL(SpecialDates::resolve(f.clock.now(), at(20u, 0u), false));

    const PlaybackDecision decision = f.scheduler.evaluate(0u);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(SchedulePriority::TIME_OF_DAY),
        static_cast<uint8_t>(decision.priority));

    // The night block, as on any ordinary evening.
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::MOON_STARS), id(decision.animationId));
}

// ===========================================================================
// 4/5/6/7: the mix is HEARTBEAT -> ROSE_BLOOM -> HEARTBEAT -> ...
// ===========================================================================

void test_valentines_mix_alternates_and_repeats(void) {
    // Sampled exactly on slot boundaries: 15 s slots, 30 s per pass.
    //   HEARTBEAT -> ROSE_BLOOM -> HEARTBEAT -> ROSE_BLOOM -> ...
    const AnimationId expected[5] = {
        AnimationId::HEARTBEAT,
        AnimationId::ROSE_BLOOM,
        AnimationId::HEARTBEAT,
        AnimationId::ROSE_BLOOM,
        AnimationId::HEARTBEAT,
    };

    for (uint8_t i = 0u; i < 5u; ++i) {
        Fixture f = atOffsetIntoWindow(static_cast<uint32_t>(i) * 15u);
        TEST_ASSERT_EQUAL_UINT8(
            id(expected[i]),
            id(f.scheduler.evaluate(0u).animationId));
    }
}

// Every second of a full 30 s pass, so the mapping is pinned exactly rather
// than only at the boundaries.
void test_valentines_sequence_boundaries_are_exact(void) {
    for (uint32_t offset = 0u; offset < 30u; ++offset) {
        Fixture f = atOffsetIntoWindow(offset);

        const AnimationId expected =
            (offset < 15u) ? AnimationId::HEARTBEAT : AnimationId::ROSE_BLOOM;

        TEST_ASSERT_EQUAL_UINT8(id(expected), id(f.scheduler.evaluate(0u).animationId));
    }

    // The wrap back to the first item is at 30 s, exactly one pass later.
    Fixture wrap = atOffsetIntoWindow(30u);
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::HEARTBEAT), id(wrap.scheduler.evaluate(0u).animationId));

    // And the sequence keeps alternating well past the first pass.
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::ROSE_BLOOM), id(atOffsetIntoWindow(45u).scheduler.evaluate(0u).animationId));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::HEARTBEAT), id(atOffsetIntoWindow(60u).scheduler.evaluate(0u).animationId));

    // Late in the evening, still inside the window: 23:59 lands 21540 s in,
    // which is exactly 718 whole passes, so the rose slot is 21555 s.
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::HEARTBEAT), id(atOffsetIntoWindow(21540u).scheduler.evaluate(0u).animationId));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::ROSE_BLOOM), id(atOffsetIntoWindow(21555u).scheduler.evaluate(0u).animationId));
}

// The sequence is a pure function of the clock: re-evaluating, rebooting and
// evaluating from a fresh Scheduler all give the same answer.
void test_valentines_sequence_is_pure_and_deterministic(void) {
    Fixture f = onValentines(18u, 37u);

    const PlaybackDecision first = f.scheduler.evaluate(0u);
    for (uint8_t i = 0u; i < 50u; ++i) {
        const PlaybackDecision again = f.scheduler.evaluate(0u);
        TEST_ASSERT_EQUAL_UINT8(id(first.animationId), id(again.animationId));
    }

    ManualOverride freshManual;
    Scheduler fresh(f.clock, freshManual, SchedulerConfig());
    TEST_ASSERT_EQUAL_UINT8(id(first.animationId), id(fresh.evaluate(0u).animationId));

    // Crossing the midnight boundary the evening of the 14th continues into
    // the 15th, but quiet hours take over, so the mix simply stops.
    f.clock.set(makeClockSnapshot(2027u, 2u, 14u, 23u, 59u, 30u));
    const PlaybackDecision late = f.scheduler.evaluate(0u);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(PlaybackReason::SPECIAL_EVENT),
        static_cast<uint8_t>(late.reason));
    TEST_ASSERT_NOT_EQUAL_UINT8(id(AnimationId::NONE), id(late.animationId));

    f.clock.set(makeClockSnapshot(2027u, 2u, 15u, 0u, 0u, 0u));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::NONE), id(f.scheduler.evaluate(0u).animationId));
}

// ===========================================================================
// 8: neither animation is cut off before it completes once.
// ===========================================================================

void test_slots_are_longer_than_one_natural_performance(void) {
    const Playlist* playlist = Playlists::find(PlaylistId::VALENTINES);
    TEST_ASSERT_NOT_NULL(playlist);
    TEST_ASSERT_EQUAL_UINT8(2u, playlist->count);

    // Measured from the animations' own stage tables, not guessed:
    //
    //   HEARTBEAT   16*90 outline + 14*85 fill + 400 hold
    //               + 6 * (170+45+45+70+85+110+45+60+100+340) beat  ~= 9450 ms
    //   ROSE_BLOOM  8*110 stem + 350 + 400 + 450 + 500 + 700 + 48*60
    //                                                        breathing ~= 6160 ms
    const uint32_t heartbeatPerformanceMs = 9450u;
    const uint32_t roseBloomPerformanceMs = 6160u;

    for (uint8_t i = 0u; i < playlist->count; ++i) {
        const uint32_t slotMs =
            static_cast<uint32_t>(playlist->items[i].durationSeconds) * 1000u;

        uint32_t performanceMs = 0u;
        if (playlist->items[i].animation == AnimationId::HEARTBEAT) {
            performanceMs = heartbeatPerformanceMs;
        } else if (playlist->items[i].animation == AnimationId::ROSE_BLOOM) {
            performanceMs = roseBloomPerformanceMs;
        } else {
            TEST_FAIL_MESSAGE("Valentines playlist must contain only the two love animations");
            return;
        }

        // Comfortably longer than one complete performance, so the switch
        // never lands before the animation has been seen start to finish.
        TEST_ASSERT_TRUE(slotMs >= performanceMs + 2000u);
    }
}

// ===========================================================================
// 9/10/15: LOVE_MODE unreachable, the two replacements renderable.
// ===========================================================================

void test_love_mode_is_no_longer_reachable_from_any_shipped_table(void) {
    // Schedule rows.
    for (uint8_t i = 0u; i < kScheduleCount; ++i) {
        TEST_ASSERT_NOT_EQUAL_UINT8(
            id(AnimationId::LOVE_MODE),
            id(kSchedule[i].animation));
    }

    // Playlists, including the new one.
    for (uint8_t p = 1u; p < static_cast<uint8_t>(PlaylistId::PLAYLIST_ID_COUNT); ++p) {
        const Playlist* playlist = Playlists::find(static_cast<PlaylistId>(p));
        TEST_ASSERT_NOT_NULL(playlist);
        for (uint8_t i = 0u; i < playlist->count; ++i) {
            TEST_ASSERT_NOT_EQUAL_UINT8(
                id(AnimationId::LOVE_MODE),
                id(playlist->items[i].animation));
        }
    }

    // Special dates.
    for (uint8_t i = 0u; i < kSpecialDateCount; ++i) {
        TEST_ASSERT_NOT_EQUAL_UINT8(
            id(AnimationId::LOVE_MODE),
            id(kSpecialDates[i].animation));

        const Playlist* playlist = Playlists::find(kSpecialDates[i].playlist);
        if (playlist != nullptr) {
            for (uint8_t p = 0u; p < playlist->count; ++p) {
                TEST_ASSERT_NOT_EQUAL_UINT8(
                    id(AnimationId::LOVE_MODE),
                    id(playlist->items[p].animation));
            }
        }
    }

    // And it is genuinely unreachable by evaluating a whole year of dates.
    for (uint16_t month = 1u; month <= 12u; ++month) {
        for (uint8_t day = 1u; day <= 28u; ++day) {
            ManualTimeProvider clock;
            ManualOverride manual;
            Scheduler scheduler(clock, manual, SchedulerConfig());
            clock.set(makeClockSnapshot(2027u, static_cast<uint8_t>(month), day, 19u, 0u, 0u));
            TEST_ASSERT_NOT_EQUAL_UINT8(
                id(AnimationId::LOVE_MODE),
                id(scheduler.evaluate(0u).animationId));
        }
    }
}

void test_love_mode_enum_value_is_preserved(void) {
    // Kept for stored-ordinal compatibility even though nothing uses it now.
    TEST_ASSERT_EQUAL_UINT8(5u, id(AnimationId::LOVE_MODE));
}

void test_both_valentines_animations_are_renderable(void) {
    TEST_ASSERT_TRUE(AnimationRegistry::isRenderable(AnimationId::HEARTBEAT));
    TEST_ASSERT_TRUE(AnimationRegistry::isRenderable(AnimationId::ROSE_BLOOM));

    TEST_ASSERT_NOT_NULL(AnimationRegistry::getAnimation(AnimationId::HEARTBEAT));
    TEST_ASSERT_NOT_NULL(AnimationRegistry::getAnimation(AnimationId::ROSE_BLOOM));
}

// ===========================================================================
// 11: playback safety must NOT be involved during the normal mix.
// ===========================================================================

void test_no_item_in_the_valentines_mix_needs_the_fallback(void) {
    const Playlist* playlist = Playlists::find(PlaylistId::VALENTINES);
    TEST_ASSERT_NOT_NULL(playlist);

    for (uint8_t i = 0u; i < playlist->count; ++i) {
        // Every item is renderable, so PlaybackPolicy never substitutes the
        // canonical fallback and never enters the OFF state.
        TEST_ASSERT_TRUE(AnimationRegistry::isRenderable(playlist->items[i].animation));
        TEST_ASSERT_NOT_EQUAL_UINT8(id(AnimationId::NONE), id(playlist->items[i].animation));
    }

    // The whole pass resolves to renderable content.
    const uint32_t total = Playlists::totalSeconds(*playlist);
    TEST_ASSERT_EQUAL_UINT32(30u, total);

    for (uint32_t elapsed = 0u; elapsed < total; ++elapsed) {
        const PlaylistItem* item = Playlists::itemAt(*playlist, elapsed);
        TEST_ASSERT_NOT_NULL(item);
        TEST_ASSERT_TRUE(AnimationRegistry::isRenderable(item->animation));
    }
}

// ===========================================================================
// 12: manual precedence unchanged.
// ===========================================================================

void test_manual_still_wins_over_the_valentines_mix(void) {
    const AnimationId choices[] = {
        AnimationId::FOX,
        AnimationId::HALLOWEEN_GHOST,
        AnimationId::ORANGE_CAT,
        AnimationId::MOON_STARS,
    };

    for (uint8_t i = 0u; i < sizeof(choices) / sizeof(choices[0]); ++i) {
        Fixture f = onValentines(19u, 0u);

        // Both Valentine's items are live at this instant.
        TEST_ASSERT_TRUE(AnimationRegistry::isRenderable(f.scheduler.evaluate(0u).animationId));

        f.manual.activateUntilCancelled(choices[i], PlayMode::LOOP);
        const PlaybackDecision decision = f.scheduler.evaluate(0u);

        TEST_ASSERT_EQUAL_UINT8(id(choices[i]), id(decision.animationId));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(SchedulePriority::MANUAL),
            static_cast<uint8_t>(decision.priority));
    }
}

// ===========================================================================
// 13: quiet hours unchanged. Valentine's does not light the matrix at 03:00.
// ===========================================================================

void test_valentines_does_not_override_quiet_hours(void) {
    // 14 February, 03:00: inside the date, but deep inside quiet hours.
    Fixture f;
    f.clock.set(makeClockSnapshot(2027u, 2u, 14u, 3u, 0u, 0u));

    TEST_ASSERT_TRUE(f.scheduler.isQuietMinute(at(3u, 0u)));
    TEST_ASSERT_NULL(SpecialDates::resolve(f.clock.now(), at(3u, 0u), true));

    const PlaybackDecision decision = f.scheduler.evaluate(0u);
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::NONE), id(decision.animationId));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(PlaybackReason::QUIET_HOURS),
        static_cast<uint8_t>(decision.reason));

    // The rule itself never claims quiet-hours permission.
    TEST_ASSERT_FALSE(valentinesRule()->overridesQuiet);
}

// ===========================================================================
// The playlist table stays consistent with its enum.
// ===========================================================================

void test_valentines_playlist_is_well_formed(void) {
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(PlaylistId::VALENTINES),
        4u);

    const Playlist* playlist = Playlists::find(PlaylistId::VALENTINES);
    TEST_ASSERT_NOT_NULL(playlist);
    TEST_ASSERT_TRUE(playlist->loop);
    TEST_ASSERT_EQUAL_UINT32(30u, Playlists::totalSeconds(*playlist));

    // First item and second item, and the loop-back point.
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::HEARTBEAT),
        id(Playlists::itemAt(*playlist, 0u)->animation));
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::HEARTBEAT),
        id(Playlists::itemAt(*playlist, 14u)->animation));
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::ROSE_BLOOM),
        id(Playlists::itemAt(*playlist, 15u)->animation));
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::ROSE_BLOOM),
        id(Playlists::itemAt(*playlist, 29u)->animation));
    TEST_ASSERT_EQUAL_UINT8(
        id(AnimationId::HEARTBEAT),
        id(Playlists::itemAt(*playlist, 30u)->animation));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_valentines_window_is_unchanged);
    RUN_TEST(test_feb_13_is_a_normal_day);
    RUN_TEST(test_feb_14_before_the_window_is_the_normal_schedule);
    RUN_TEST(test_feb_14_inside_the_window_is_a_special_date);
    RUN_TEST(test_feb_15_returns_to_the_normal_schedule);
    RUN_TEST(test_valentines_mix_alternates_and_repeats);
    RUN_TEST(test_valentines_sequence_boundaries_are_exact);
    RUN_TEST(test_valentines_sequence_is_pure_and_deterministic);
    RUN_TEST(test_slots_are_longer_than_one_natural_performance);
    RUN_TEST(test_love_mode_is_no_longer_reachable_from_any_shipped_table);
    RUN_TEST(test_love_mode_enum_value_is_preserved);
    RUN_TEST(test_both_valentines_animations_are_renderable);
    RUN_TEST(test_no_item_in_the_valentines_mix_needs_the_fallback);
    RUN_TEST(test_manual_still_wins_over_the_valentines_mix);
    RUN_TEST(test_valentines_does_not_override_quiet_hours);
    RUN_TEST(test_valentines_playlist_is_well_formed);
    return UNITY_END();
}
