#pragma once

#include <stdint.h>

#include "core/AnimationId.h"
#include "scheduling/ClockSnapshot.h"
#include "scheduling/Playlists.h"
#include "scheduling/SchedulerConfig.h"

// ===========================================================================
//                       SPECIAL DATES  (edit me)
// ===========================================================================
//
// One row per date rule. A matching row takes priority over the normal
// schedule.
//
//     month, day       the date the rule applies to
//     startMinute      inclusive, minute of day (0 = all day from midnight)
//     endMinute        exclusive; use kEndOfDay for "until midnight"
//     animation        any AnimationId; used when playlist is PlaylistId::NONE,
//                      and as the fallback if a playlist ever runs out
//     playlist         a PlaylistId, resolved exactly like a schedule row: when
//                      it is not NONE it takes precedence over `animation` and
//                      the sequence is timed from startMinute
//     overridesQuiet   true only if this rule may run during quiet hours
//
// Resolution rules, in order:
//   1. the FIRST matching row wins, so put more specific rows first
//   2. a row whose overridesQuiet is false does not apply during quiet hours
//   3. a manual selection always wins over every row
//
// Only New Year's midnight needs overridesQuiet; everything else behaves the
// same as the rest of the firmware and stays dark during quiet hours.
// ===========================================================================

struct SpecialDate {
    uint8_t month;
    uint8_t day;
    uint16_t startMinute;
    uint16_t endMinute;
    AnimationId animation;
    PlaylistId playlist;
    bool overridesQuiet;
};

constexpr SpecialDate kSpecialDates[] = {
    // New Year's midnight: the only rule allowed through quiet hours.
    { 1u, 1u, at(0u, 0u), at(0u, 10u), AnimationId::FIREWORKS, PlaylistId::NONE, true },

    // New Year's Eve countdown.
    { 12u, 31u, at(23u, 58u), kEndOfDay, AnimationId::FIREWORKS, PlaylistId::NONE, false },
    { 12u, 31u, at(20u, 0u), at(23u, 58u), AnimationId::AURORA_NEON, PlaylistId::NONE, false },

    // Christmas.
    { 12u, 24u, 0u, kEndOfDay, AnimationId::CHRISTMAS_TREE, PlaylistId::NONE, false },
    { 12u, 25u, 0u, kEndOfDay, AnimationId::CHRISTMAS_TREE, PlaylistId::NONE, false },
    { 12u, 26u, 0u, kEndOfDay, AnimationId::CHRISTMAS_TREE, PlaylistId::NONE, false },

    // Valentine's Day evening: an alternating HEARTBEAT / ROSE_BLOOM mix.
    //
    // It was a single LOVE_MODE id before. LOVE_MODE has no implementation in
    // this build, so playback fell back to AURORA rather than showing something
    // Valentine's. The mix uses two animations that already exist, so nothing
    // new had to be drawn and the enum is left untouched.
    { 2u, 14u, at(18u, 0u), kEndOfDay, AnimationId::NONE, PlaylistId::VALENTINES, false },

    // Woman's Day
    { 3u, 8u, at(0u, 0u), kEndOfDay, AnimationId::ROSE_BLOOM, PlaylistId::NONE, false },
    // Wiki Birthday 06.10.2000
    { 10u, 6u, at(0u, 0u), kEndOfDay, AnimationId::FIREWORKS, PlaylistId::NONE, true },
    // Halloween. Not allowed through quiet hours, like every other rule except
    // New Year's midnight.
    { 10u, 31u, at(0u, 0u), kEndOfDay, AnimationId::HALLOWEEN_GHOST, PlaylistId::NONE, false },
};

constexpr uint8_t kSpecialDateCount =
    static_cast<uint8_t>(sizeof(kSpecialDates) / sizeof(kSpecialDates[0]));

namespace SpecialDates {

inline bool matches(const SpecialDate& rule, const ClockSnapshot& snapshot, uint16_t minuteOfDay) {
    if (rule.month != snapshot.month || rule.day != snapshot.day) {
        return false;
    }

    return minuteOfDay >= rule.startMinute && minuteOfDay < rule.endMinute;
}

// Returns the first matching rule, or nullptr. `quiet` suppresses rules that
// are not allowed through quiet hours.
inline const SpecialDate* resolve(
    const ClockSnapshot& snapshot,
    uint16_t minuteOfDay,
    bool quiet) {
    for (uint8_t i = 0u; i < kSpecialDateCount; ++i) {
        if (!matches(kSpecialDates[i], snapshot, minuteOfDay)) {
            continue;
        }
        if (quiet && !kSpecialDates[i].overridesQuiet) {
            continue;
        }
        return &kSpecialDates[i];
    }

    return nullptr;
}

}  // namespace SpecialDates
