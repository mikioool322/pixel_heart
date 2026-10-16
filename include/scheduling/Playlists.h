#pragma once

#include <stdint.h>

#include "core/AnimationId.h"

// ===========================================================================
//                        PLAYLISTS  (edit me)
// ===========================================================================
//
// A playlist is a fixed sequence of animations, each shown for a number of
// seconds. A schedule row can point at a playlist instead of a single
// animation, which is how an evening or night block rotates without any
// per-animation rotation machinery.
//
//     { animation, seconds }
//
// `loop` decides what happens once the sequence reaches the end:
//     true  -> start again from the first item (ambient blocks)
//     false -> stop; the scheduler then falls back to the row's own
//              `animation` field, or to the global fallback if that is NONE
//
// There is NO timer, no state and no millis() involved. The active item is
// derived from the clock: "how many seconds into the schedule row are we".
// That makes playlists immune to millis() rollover, reboots, NTP corrections
// and to being evaluated any number of times per loop.
// ===========================================================================

struct PlaylistItem {
    AnimationId animation;
    uint16_t durationSeconds;
};

struct Playlist {
    const PlaylistItem* items;
    uint8_t count;
    bool loop;
};

enum class PlaylistId : uint8_t {
    NONE = 0,
    MORNING,
    EVENING,
    NIGHT,
    VALENTINES,
    DAY,
    PLAYLIST_ID_COUNT
};

constexpr PlaylistItem kEveningItems[] = {
    { AnimationId::AURORA_SUNSET, 300u },   // 5 min
    { AnimationId::SUNSET_WATER, 180u },    // 3 min
    { AnimationId::CAMPFIRE, 300u },        // 5 min
};

constexpr PlaylistItem kNightItems[] = {
    { AnimationId::MOON_STARS, 600u },      // 10 min
    { AnimationId::FALLING_STARS, 300u },   // 5 min
    { AnimationId::CAMPFIRE, 300u },        // 5 min
};

constexpr PlaylistItem kMorningItems[] = {
    { AnimationId::AURORA, 300u },
    { AnimationId::SUN_OVER_MOUNTAINS, 300u },
};

// Midday / afternoon block: the bright daytime scenes, five minutes each.
//
// One pass is 20 minutes, and the 12:00 -> 17:00 window is exactly 300 minutes,
// so the block always ends on a cycle boundary and never cuts an item short.
constexpr PlaylistItem kDayItems[] = {
    { AnimationId::MEADOW_FLOWER, 300u },    // 5 min
    { AnimationId::BLUE_SKY_CLOUDS, 300u },  // 5 min
    { AnimationId::PALM_BREEZE, 300u },      // 5 min
    { AnimationId::JELLYFISH, 300u },        // 5 min
};

// Valentine's Day mix. Alternates the two existing love animations and repeats.
//
// Slot lengths are deliberately longer than one full natural performance of the
// animation they hold, so neither is ever cut off before it finishes once:
//
//   HEARTBEAT   one full performance is about 9450 ms  -> 15 s slot
//   ROSE_BLOOM  one full performance is about 6160 ms  -> 15 s slot
//
// The scheduler drives every decision with PlayMode::LOOP, so each animation
// replays on its own within its slot; the slot boundary is the only switch, and
// it never lands before a complete performance has been seen.
constexpr PlaylistItem kValentinesItems[] = {
    { AnimationId::HEARTBEAT, 15u },
    { AnimationId::ROSE_BLOOM, 15u },
};

// Indexed by PlaylistId. Keep in the same order as the enum.
constexpr Playlist kPlaylists[] = {
    { nullptr, 0u, false },  // NONE
    { kMorningItems, static_cast<uint8_t>(sizeof(kMorningItems) / sizeof(kMorningItems[0])), true },
    { kEveningItems, static_cast<uint8_t>(sizeof(kEveningItems) / sizeof(kEveningItems[0])), true },
    { kNightItems, static_cast<uint8_t>(sizeof(kNightItems) / sizeof(kNightItems[0])), true },
    { kValentinesItems, static_cast<uint8_t>(sizeof(kValentinesItems) / sizeof(kValentinesItems[0])), true },
    { kDayItems, static_cast<uint8_t>(sizeof(kDayItems) / sizeof(kDayItems[0])), true },
};

constexpr uint8_t kPlaylistCount = static_cast<uint8_t>(sizeof(kPlaylists) / sizeof(kPlaylists[0]));

namespace Playlists {

inline const Playlist* find(PlaylistId id) {
    const uint8_t index = static_cast<uint8_t>(id);
    if (index == 0u || index >= kPlaylistCount) {
        return nullptr;
    }
    return &kPlaylists[index];
}

// Total length of one full pass, in seconds. 0 for an empty playlist.
inline uint32_t totalSeconds(const Playlist& playlist) {
    uint32_t total = 0u;
    for (uint8_t i = 0u; i < playlist.count; ++i) {
        total += playlist.items[i].durationSeconds;
    }
    return total;
}

// The item active at `elapsedSeconds` into the playlist.
//
// Returns nullptr only when the playlist is empty, or when it does not loop
// and the elapsed time is past the end.
inline const PlaylistItem* itemAt(const Playlist& playlist, uint32_t elapsedSeconds) {
    if (playlist.items == nullptr || playlist.count == 0u) {
        return nullptr;
    }

    uint32_t offset = elapsedSeconds;

    if (playlist.loop) {
        const uint32_t total = totalSeconds(playlist);
        if (total == 0u) {
            return &playlist.items[0];
        }
        offset %= total;
    }

    uint32_t remaining = offset;
    for (uint8_t i = 0u; i < playlist.count; ++i) {
        const uint16_t duration = playlist.items[i].durationSeconds;
        if (remaining < duration) {
            return &playlist.items[i];
        }
        remaining -= duration;
    }

    // Past the end: only reachable for a non-looping playlist.
    return nullptr;
}

}  // namespace Playlists
