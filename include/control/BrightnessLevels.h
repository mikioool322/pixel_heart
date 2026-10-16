#pragma once

#include <stdint.h>

#include "config/LedPowerBudget.h"
#include "control/UserSettings.h"

// Five user-facing brightness levels.
//
// Why levels instead of a percentage: this device's safe brightness range is
// only about 13% of full scale, so a percentage would be actively misleading -
// the panel could show "100%" while the strip is capped far below that. Levels
// are a pure PRESENTATION concept.
//
// The persisted value is still a percentage, deliberately: storing a level in
// the same key would collide with legacy values (a device saved at 3% would be
// misread as level 3, i.e. brighter), which would force a settings migration.
// Keeping percent means no format change, no version bump and no migration.
// The stored percentage is only a CARRIER for the level: the output path
// resolves it to a level and then to the exact raw value below.
//
// The level is NOT a safety mechanism. PixelMatrix independently clamps every
// request through LedPowerBudget, so a bug here - or a hand-crafted POST -
// cannot overdrive the LEDs.
namespace BrightnessLevels {

constexpr uint8_t kCount = 5u;

// THE canonical mapping, and the single source of truth for what each level
// actually outputs. Index 0 is unused: levels are 1-based so the numbers match
// the buttons.
//
// The steps are deliberately NOT evenly spaced. A night-time lamp needs
// resolution at the bottom of the range, not the top: the first three steps are
// 1 / 3 / 7, which is a large RELATIVE change for a tiny absolute one, so the
// dimmest level is genuinely usable as a bedside lamp instead of lighting up
// the whole room.
//
// Level 5 is exactly LedPowerBudget::kMaxSafeBrightnessRaw. The static_asserts
// below fail the build if that ever stops being true.
constexpr uint8_t kRawByLevel[] = {
    0u,   // unused
    1u,   // 1  Nocna        - barely lit, for a dark bedroom
    3u,   // 2  Bardzo niska
    7u,   // 3  Niska
    16u,  // 4  Średnia
    33u,  // 5  Maksymalna   - the hard ceiling, unchanged
};

// Canonical percentage stored for each level. These are chosen so that
// levelForPercent(percentForLevel(level)) == level with the raw table above;
// the percent scale is far too coarse to express 1 / 3 / 16 directly.
constexpr uint8_t kPercent[kCount] = {1u, 2u, 3u, 7u, 13u};

constexpr const char* kLabels[kCount] = {
    "Nocna",
    "Bardzo niska",
    "Niska",
    "Średnia",
    "Maksymalna",
};

// Clamps a 1-based level into the table's range.
constexpr uint8_t clampLevel(uint8_t level) {
    return (level < 1u) ? 1u : ((level > kCount) ? kCount : level);
}

// Levels are 1-based so the numbers match the buttons.
inline bool isValidLevel(uint8_t level) {
    return level >= 1u && level <= kCount;
}

constexpr uint8_t rawForLevel(uint8_t level) {
    return kRawByLevel[clampLevel(level)];
}

constexpr uint8_t percentForLevel(uint8_t level) {
    return kPercent[clampLevel(level) - 1u];
}

// Recursive helper. The AVR toolchain compiles at C++11, where a constexpr
// function body may only be a single return statement, so the obvious loop has
// to be written as recursion. Depth is at most kCount, so it is trivial.
constexpr uint8_t levelForPercentFrom(uint8_t percent, uint8_t level, uint8_t chosen) {
    return (level > kCount)
               ? chosen
               : levelForPercentFrom(
                     percent,
                     static_cast<uint8_t>(level + 1u),
                     (rawForLevel(level) <= brightnessPercentToRaw(percent)) ? level : chosen);
}

// Maps any stored or requested percentage onto a brightness level.
//
// Chooses the HIGHEST level whose output does not exceed the stored output,
// rather than a plain nearest-rounding. That matters: nearest-rounding would
// map a legacy raw value of 12 up to a level that outputs more, i.e. the panel
// would claim the device is brighter than it actually is. Rounding down can
// only ever understate, which is the safe direction here.
//
// The one unavoidable exception is a stored value below the dimmest level
// (raw below 1, i.e. fully off): there is no lower level to fall back to, so
// level 1 is shown. Nothing is rewritten, so in every case the ACTUAL output
// only ever moves DOWN when this function is used - never up.
constexpr uint8_t levelForPercent(uint8_t percent) {
    return levelForPercentFrom(percent, 1u, 1u);
}

// THE conversion the output path uses.
//
// A stored percentage is resolved to its level and then to that level's exact
// raw value, rather than converted straight to raw. Going through the level is
// what lets the levels hit 1 / 3 / 7 / 16 / 33 exactly, which the coarse
// percent scale cannot express on its own, while keeping the persisted format
// (a single percentage byte) completely unchanged.
constexpr uint8_t rawForPercent(uint8_t percent) {
    return rawForLevel(levelForPercent(percent));
}

inline const char* labelForLevel(uint8_t level) {
    // Out-of-range callers get a real label rather than a null deref.
    return kLabels[clampLevel(level) - 1u];
}

// Compile-time coupling to the hard safety ceiling. If the power budget is ever
// tightened, this fails the BUILD rather than leaving the UI able to ask for
// more than the device is allowed to output.
static_assert(
    rawForLevel(kCount) <= LedPowerBudget::kMaxSafeBrightnessRaw,
    "brightness level 5 must not exceed the LED safety ceiling");

// And the levels must actually be usable: strictly increasing.
static_assert(
    rawForLevel(1u) < rawForLevel(2u) &&
    rawForLevel(2u) < rawForLevel(3u) &&
    rawForLevel(3u) < rawForLevel(4u) &&
    rawForLevel(4u) < rawForLevel(5u),
    "brightness levels must be strictly increasing");

// The top level should reach the ceiling exactly, so "Maksymalna" means the
// maximum this device is allowed to produce - not an arbitrary lower value.
static_assert(
    rawForLevel(kCount) == LedPowerBudget::kMaxSafeBrightnessRaw,
    "brightness level 5 must equal the maximum safe brightness");

// The dimmest level must still light the panel: level 1 is "night", not "off".
static_assert(rawForLevel(1u) >= 1u, "the dimmest level must not be fully off");

// A stored percentage must always resolve back to the level it was saved for,
// otherwise re-selecting a level would silently change the output. Same
// single-return recursion constraint as above.
constexpr bool levelsRoundTripFrom(uint8_t level) {
    return (level > kCount)
               ? true
               : ((levelForPercent(percentForLevel(level)) == level &&
                   rawForPercent(percentForLevel(level)) == rawForLevel(level))
                      ? levelsRoundTripFrom(static_cast<uint8_t>(level + 1u))
                      : false);
}

constexpr bool levelsRoundTrip() {
    return levelsRoundTripFrom(1u);
}

static_assert(levelsRoundTrip(), "stored percents and levels must round-trip");

}  // namespace BrightnessLevels
