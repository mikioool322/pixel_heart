#pragma once

#include <stdint.h>

#include "core/Animation.h"
#include "core/Color.h"
#include "core/PixelMatrix.h"

// Immutable description of the aquarium scene.
//
// These are facts about the animation, not state: they live outside the class
// so the implementation's data tables and the focused tests can both use them
// without duplicating a single number.
namespace SwimmingFish {

struct Palette {
    Color body;
    Color highlight;
    Color tail;
};

// -------------------------------------------------------------------------
// Timeline
// -------------------------------------------------------------------------
// One render tick. 125 ms keeps the tail and the bubbles lively while the
// horizontal step (every other tick) stays a calm 250 ms per pixel.
constexpr uint16_t kStepIntervalMs = 125u;
constexpr uint16_t kMoveIntervalMs = 250u;
constexpr uint8_t kStepsPerMove =
    static_cast<uint8_t>(kMoveIntervalMs / kStepIntervalMs);

constexpr uint16_t kStepCount = 108u;
constexpr uint32_t kLoopDurationMs =
    static_cast<uint32_t>(kStepCount) * kStepIntervalMs;   // 13500 ms

// The play duration is exactly one visual loop, so playback never stops
// halfway through the choreography and the restart lands on the same frame.
constexpr uint32_t kAnimationDurationMs = kLoopDurationMs;

// -------------------------------------------------------------------------
// Fish
// -------------------------------------------------------------------------
constexpr uint8_t kFishCount = 3u;
constexpr uint8_t kFishWidth = 6u;
constexpr uint8_t kFishHeight = 5u;

// Fully outside one edge to fully outside the other: -6..16 is 23 positions,
// i.e. 5.75 s per crossing at 250 ms per pixel.
constexpr int8_t kEntryLeftX = -6;
constexpr int8_t kEntryRightX = 16;
constexpr uint16_t kCrossingMoveSteps =
    static_cast<uint16_t>(kEntryRightX - kEntryLeftX + 1);

constexpr uint8_t kLaneCount = 2u;
constexpr uint8_t kBobTableSize = 4u;

// -------------------------------------------------------------------------
// Bubbles
// -------------------------------------------------------------------------
constexpr uint8_t kBubbleCount = 3u;
constexpr uint8_t kBubbleRiseStepsPerPixel = 3u;
constexpr uint8_t kBubbleDriftStepsPerPixel = 10u;

}  // namespace SwimmingFish

// THREE-FISH AQUARIUM - one deterministic, mathematically closed loop.
//
// Nothing is simulated frame to frame. Every position is derived from
//
//     step = ((now - startedAt_) / kStepIntervalMs) % kStepCount
//
// so the complete logical frame at t = 0 and at t = kLoopDurationMs is
// identical by construction, and the sequence can never drift, teleport or
// depend on how often update() happens to be called.
//
// The three fish share ONE geometry routine and differ only by palette, lane
// and schedule. They enter fully off one edge and exit fully off the other;
// nothing reverses on screen and no two fish ever share a row.
class SwimmingFishAnimation : public Animation {
public:
    SwimmingFishAnimation();

    void start(uint32_t now) override;
    void update(uint32_t now, PixelMatrix& matrix) override;
    bool isFinished() const override;
    void stop() override;

private:
    // ---- derived scene queries (single source of truth for tests) -------

    static const SwimmingFish::Palette& paletteAt(uint8_t index);
    static uint8_t paletteCount();

    static uint8_t fishPaletteIndexAt(uint8_t index);
    static uint8_t fishLaneAt(uint8_t index);
    static bool fishFacesRightAt(uint8_t index);
    static uint16_t fishStartMoveStepAt(uint8_t index);
    static uint16_t fishEndMoveStepAt(uint8_t index);
    static uint8_t fishBobPhaseAt(uint8_t index);
    static uint8_t fishTailPhaseAt(uint8_t index);

    static int8_t laneBaseY(uint8_t lane);
    static int8_t bobOffsetAt(uint8_t index, uint16_t moveStep);

    static bool fishActiveAt(uint8_t index, uint16_t moveStep);
    static int8_t fishXAt(uint8_t index, uint16_t moveStep);
    static int8_t fishYAt(uint8_t index, uint16_t moveStep);
    static uint8_t tailFrameAt(uint8_t index, uint16_t step);

    static uint8_t bubbleSourceFish(uint8_t slot);
    static uint16_t bubbleStartStep(uint8_t slot);
    static uint16_t bubbleLifeSteps(uint8_t slot);
    static int8_t bubbleDrift(uint8_t slot);

    static bool bubbleActiveAt(uint8_t slot, uint16_t step);
    static int8_t bubbleXAt(uint8_t slot, uint16_t step);
    static int8_t bubbleYAt(uint8_t slot, uint16_t step);

    static uint16_t stepAtElapsed(uint32_t elapsedMs);

    // ---- rendering ------------------------------------------------------

    void renderFrame(PixelMatrix& matrix, uint16_t step) const;

    void renderBubbles(PixelMatrix& matrix, uint16_t step) const;

    void drawFish(
        PixelMatrix& matrix,
        int16_t originX,
        int16_t originY,
        bool facingRight,
        uint8_t tailFrame,
        const SwimmingFish::Palette& palette
    ) const;

    // ---- state ----------------------------------------------------------

    // Sentinel: nothing rendered yet for the current play.
    static constexpr uint16_t kNoStep = 0xFFFFu;

    uint32_t startedAt_;
    uint16_t lastRenderedStep_;

    bool finished_;
};
