#pragma once

#include <stdint.h>

#include "core/AnimationId.h"
#include "core/AnimationRegistry.h"

// Index <-> AnimationId helpers for the control panel.
//
// There is deliberately NO animation list here: AnimationId is the list. This
// header only provides the arithmetic and the step/wrap behaviour, so adding a
// new animation needs no change in this file (or anywhere else in the UI).
//
// Excluded from selection: NONE, which means "nothing is playing".
namespace AnimationCatalog {

constexpr uint8_t kSelectableCount =
    static_cast<uint8_t>(AnimationId::ANIMATION_ID_COUNT) - 1u;

constexpr uint8_t kNoIndex = 0xFFu;

// 0-based index of a selectable animation, or kNoIndex for NONE / out of range.
inline uint8_t indexOf(AnimationId id) {
    const uint8_t raw = static_cast<uint8_t>(id);
    if (raw == 0u || raw >= static_cast<uint8_t>(AnimationId::ANIMATION_ID_COUNT)) {
        return kNoIndex;
    }
    return static_cast<uint8_t>(raw - 1u);
}

inline AnimationId idAt(uint8_t index) {
    if (index >= kSelectableCount) {
        return AnimationId::NONE;
    }
    return static_cast<AnimationId>(static_cast<uint8_t>(index + 1u));
}

inline bool isSelectable(AnimationId id) {
    return indexOf(id) != kNoIndex;
}

// True for the animations the LAN panel may offer the user.
//
// Stricter than isSelectable(), and deliberately derived from the registry
// rather than from a second hand-written list, so the panel can never drift
// from what actually exists:
//
//   - NONE is not an animation, it means "nothing is playing"
//   - anything AnimationRegistry has no entry for would resolve to nullptr in
//     AnimationManager::play() and simply blank the panel, so it must not be
//     offered at all
//
// The second point matters today: BREATHING_HEART and LOVE_MODE are declared
// and scheduler-referenced but have no implementation. Deriving the list from
// the registry hides them automatically, and they will reappear on their own
// once they are implemented.
inline bool isUserSelectable(AnimationId id) {
    if (id == AnimationId::NONE) {
        return false;
    }
    if (!isSelectable(id)) {
        return false;
    }

    // Uses the registry's own answer, so the panel's notion of "renderable" can
    // never drift from the firmware's.
    return AnimationRegistry::isRenderable(id);
}

// Steps through the selectable animations, wrapping at both ends. `delta` is
// normally -1 or +1. If `current` is not selectable the first entry is used.
inline AnimationId step(AnimationId current, int8_t delta) {
    if (kSelectableCount == 0u) {
        return AnimationId::NONE;
    }

    const uint8_t current_index = indexOf(current);

    // Nothing selected yet: the first press SELECTS the first animation rather
    // than moving past it. Deterministic in both directions.
    if (current_index == kNoIndex) {
        return idAt(0u);
    }

    int16_t index = static_cast<int16_t>(current_index);

    const int16_t count = static_cast<int16_t>(kSelectableCount);
    int16_t next = static_cast<int16_t>(index + static_cast<int16_t>(delta));

    while (next < 0) {
        next = static_cast<int16_t>(next + count);
    }
    while (next >= count) {
        next = static_cast<int16_t>(next - count);
    }

    return idAt(static_cast<uint8_t>(next));
}

}  // namespace AnimationCatalog
