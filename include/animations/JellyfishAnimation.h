#pragma once

#include <stdint.h>

#include "core/Animation.h"
#include "core/PixelMatrix.h"

// A large glowing jellyfish behind a milky diffuser.
//
// The bell is the hero: it spans x=2..13 and the tentacles reach almost to the
// bottom edge. All motion is deliberately calm - the whole body rises and falls
// by one pixel, the bell breathes by changing only its own local brightness, and
// the tentacles ripple on a faster clock so the creature reads as alive.
//
// Everything is derived from elapsed time, so t=0 and t=kLoopDurationMs are the
// same logical frame by construction.
class JellyfishAnimation : public Animation {
public:
    JellyfishAnimation();

    void start(uint32_t now) override;
    void update(uint32_t now, PixelMatrix& matrix) override;
    bool isFinished() const override;
    void stop() override;

    // The render tick. One step is 200 ms; a whole loop is 24 steps = 4800 ms.
    static constexpr uint16_t kStepIntervalMs = 200u;
    static constexpr uint8_t kStepCount = 24u;
    static constexpr uint32_t kLoopDurationMs =
        static_cast<uint32_t>(kStepCount) * kStepIntervalMs;

    // Vertical bob: CENTER -> UP -> CENTER -> DOWN, one step every three ticks.
    static constexpr uint8_t kMotionIntervalSteps = 3u;
    static constexpr uint8_t kMotionCount = 4u;

    // Tentacles ripple every tick through four poses.
    static constexpr uint8_t kTentaclePoseCount = 4u;

    // Scene geometry, exposed so the tests can assert invariants directly.
    static bool bellContains(uint8_t x, uint8_t y);
    static uint8_t highlightCount();
    static uint8_t highlightX(uint8_t index);
    static uint8_t highlightY(uint8_t index);

    static int8_t verticalOffsetAt(uint8_t step);
    static uint8_t pulseLevelAt(uint8_t step);
    static uint8_t tentaclePoseAt(uint8_t step);

    static uint8_t tentacleCount();
    static uint8_t tentacleAnchorX(uint8_t index);
    static uint8_t tentacleLength(uint8_t index);
    static int8_t tentacleOffsetAt(uint8_t index, uint8_t rowIndex, uint8_t pose);

    static uint8_t particleCount();
    static uint8_t particleAtX(uint8_t index);
    static uint8_t particleAtBaseY(uint8_t index);
    static uint8_t particleAtY(uint8_t index, uint8_t step);
    static uint8_t particleAtLevel(uint8_t index, uint8_t step);

private:
    void renderFrame(PixelMatrix& matrix, uint8_t step) const;
    void drawParticles(PixelMatrix& matrix, uint8_t step) const;
    void drawBell(PixelMatrix& matrix, uint8_t step) const;
    void drawTentacles(PixelMatrix& matrix, uint8_t step) const;

    uint32_t startedAt_;
    // 0xFF is not a valid step, so the first update paints immediately.
    uint8_t lastStep_;
    bool finished_;
};
