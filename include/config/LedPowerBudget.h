#pragma once

#include <stdint.h>

// LED power / current safety budget.
//
// This is a firmware SAFETY CEILING, not a user preference. It is deliberately:
//   - not stored in NVS,
//   - not editable from the web panel,
//   - not influenceable by any animation, scheduler rule or playlist.
//
// It is also NOT a substitute for the physical fuse, correct wire gauge,
// correct polarity or solid joints. Firmware cannot protect against a wiring
// fault or a short.
namespace LedPowerBudget {

// Conservative software budget for the LED matrix ONLY. The controller and the
// rest of the electronics sit outside this budget and keep their own headroom
// on the supply.
constexpr uint16_t kLimitMilliAmp = 2000u;

// Planning model, taken from the WS2812B datasheet worst case:
//   20 mA per colour channel at full intensity
//   60 mA per pixel at full white (R + G + B)
//
// This is deliberately the WORST case, not a typical-content average. It is
// not "optimised" down to 20 mA/pixel, because that is a rule of thumb for
// typical content rather than a safety bound.
constexpr uint8_t kMilliAmpPerFullChannel = 20u;
constexpr uint16_t kPixelCount = 256u;
constexpr uint8_t kChannelsPerPixel = 3u;

// Unrestricted worst case for the whole matrix at full white: 15360 mA.
constexpr uint32_t kUnlimitedWhiteFrameMilliAmp =
    static_cast<uint32_t>(kPixelCount) * kChannelsPerPixel * kMilliAmpPerFullChannel;

// Adafruit_NeoPixel::setPixelColor() stores a channel scaled as
//     transmitted = (channel * (brightnessRaw + 1)) >> 8
// and show() transmits that buffer unchanged, so a configured brightness of
// `b` caps every transmitted channel at (255 * (b + 1)) >> 8.
//
// Largest channel whose worst-case frame still fits the budget:
//     256 * 3 * 20 * ch / 255 <= 2000   ->   ch <= 33.20   ->   ch = 33
constexpr uint8_t kMaxSafeChannel = static_cast<uint8_t>(
    (static_cast<uint32_t>(kLimitMilliAmp) * 255u) / kUnlimitedWhiteFrameMilliAmp);

// Largest brightness whose resulting channel cap is still <= kMaxSafeChannel.
//     (255*(b+1))>>8 <= ch   <=>   b+1 <= floor((256*(ch+1) - 1) / 255)
constexpr uint8_t kMaxSafeBrightnessRaw = static_cast<uint8_t>(
    (256u * (static_cast<uint16_t>(kMaxSafeChannel) + 1u) - 1u) / 255u - 1u);

// Compile-time proof that the ceiling is both SAFE and TIGHT. If a future edit
// changes the budget or the model, the build fails rather than silently
// shipping an over-current limit.
static_assert(
    ((255u * (static_cast<uint16_t>(kMaxSafeBrightnessRaw) + 1u)) >> 8) <= kMaxSafeChannel,
    "LED budget: brightness ceiling must keep every channel within the safe cap");
static_assert(
    ((255u * (static_cast<uint16_t>(kMaxSafeBrightnessRaw) + 2u)) >> 8) > kMaxSafeChannel,
    "LED budget: brightness ceiling must be the largest safe value");

// Estimated LED emission for a frame, in mA, from the sum of all channel
// values. Integer arithmetic only, and it never rounds up a frame's estimate.
constexpr uint32_t estimateMilliAmp(uint32_t channelSum) {
    return (channelSum * kMilliAmpPerFullChannel) / 255u;
}

// Worst case the limiter can actually produce: a fully white frame transmitted
// at the ceiling brightness.
constexpr uint32_t kWorstCaseMilliAmp = estimateMilliAmp(
    static_cast<uint32_t>(kPixelCount) * kChannelsPerPixel * kMaxSafeChannel);

static_assert(
    kWorstCaseMilliAmp <= kLimitMilliAmp,
    "LED budget: worst case frame must fit inside the current limit");

// Clamps a requested brightness to the safety ceiling.
//
// Uses the LESSER of the two, so a request below the ceiling is served exactly
// as before and only requests above it are reduced. That is what keeps the
// default brightness behaviour unchanged.
constexpr uint8_t clampBrightnessRaw(uint8_t requested) {
    return (requested > kMaxSafeBrightnessRaw) ? kMaxSafeBrightnessRaw : requested;
}

}  // namespace LedPowerBudget
