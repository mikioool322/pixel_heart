#pragma once

#include <stdint.h>

#include "core/AnimationId.h"
#include "scheduling/Playlists.h"
#include "scheduling/SchedulerConfig.h"

// ===========================================================================
//                         THE SCHEDULE  (edit me)
// ===========================================================================
//
// This table is the single source of truth for what plays when in AUTO mode.
// Each row is one time range:
//
//     startMinute  inclusive, minute of day 0..1440
//     endMinute    exclusive, minute of day 0..1440
//     animation    any AnimationId (used when playlist is PlaylistId::NONE)
//     playlist     a PlaylistId; when not NONE it takes precedence over
//                  `animation`, and the sequence is timed from startMinute
//
// Use at(hour, minute) for readability. A range whose end is less than or
// equal to its start wraps past midnight (an overnight rule).
//
// If two ranges overlap, the LAST matching row wins, so put more specific
// ranges further down.
//
// Quiet hours are configured separately in SchedulerConfig.h and take
// precedence over this table. A range that falls entirely inside quiet hours
// therefore has no visible effect; that is intentional.
// ===========================================================================

struct ScheduleEntry {
    uint16_t startMinute;
    uint16_t endMinute;
    AnimationId animation;
    PlaylistId playlist;
};

constexpr ScheduleEntry kSchedule[] = {
    { at(6u, 0u),   at(8u, 0u),  AnimationId::NONE,               PlaylistId::MORNING },
    { at(8u, 0u),   at(12u, 0u), AnimationId::SUN_OVER_MOUNTAINS, PlaylistId::NONE },
    { at(12u, 0u),  at(17u, 0u), AnimationId::NONE,               PlaylistId::DAY },
    { at(17u, 0u),  at(20u, 0u), AnimationId::NONE,               PlaylistId::EVENING },
    // Overnight range: 20:00 right through to 06:00 the next morning, so
    // there is no gap between the last daylight row and quiet hours.
    { at(20u, 0u),  at(6u, 0u),  AnimationId::NONE,               PlaylistId::NIGHT },
};

constexpr uint8_t kScheduleCount =
    static_cast<uint8_t>(sizeof(kSchedule) / sizeof(kSchedule[0]));

namespace ScheduleTable {

// True when `minuteOfDay` falls inside the entry's range, handling an
// overnight range (end <= start) such as 23:00 -> 06:00.
inline bool contains(const ScheduleEntry& entry, uint16_t minuteOfDay) {
    if (entry.endMinute <= entry.startMinute) {
        // Overnight: the range wraps through midnight.
        return minuteOfDay >= entry.startMinute || minuteOfDay < entry.endMinute;
    }

    return minuteOfDay >= entry.startMinute && minuteOfDay < entry.endMinute;
}

// The last matching row for `minuteOfDay`, or nullptr when none matches.
// Last-match-wins is implemented once, here, so the Scheduler and the tests
// cannot disagree about overlap behaviour.
inline const ScheduleEntry* resolveEntry(uint16_t minuteOfDay) {
    const ScheduleEntry* found = nullptr;

    for (uint8_t i = 0u; i < kScheduleCount; ++i) {
        if (contains(kSchedule[i], minuteOfDay)) {
            found = &kSchedule[i];
        }
    }

    return found;
}

}  // namespace ScheduleTable
