#pragma once

#include <stdint.h>

#include "core/Animation.h"
#include "core/PixelMatrix.h"

// A large disco ball hanging in a dark room.
//
// The ball NEVER changes shape - not one pixel - and it is never rotated
// geometrically. The whole illusion of rotation comes from three things that
// all derive from a single global phase:
//
//   * the tile colour classes shift as ((x + y + phase) % 4),
//   * a two-pixel highlight travels around the ball,
//   * the background beams and the floor reflections brighten in turn.
//
// One phase is 220 ms; eight phases make a 1760 ms loop that closes exactly.
class DiscoBallAnimation : public Animation {
public:
    DiscoBallAnimation();

    void start(uint32_t now) override;
    void update(uint32_t now, PixelMatrix& matrix) override;
    bool isFinished() const override;
    void stop() override;

    static constexpr uint16_t kStepIntervalMs = 220u;
    static constexpr uint8_t kPhaseCount = 8u;
    static constexpr uint32_t kLoopDurationMs =
        static_cast<uint32_t>(kPhaseCount) * kStepIntervalMs;

    // Tile classes, in palette order.
    static constexpr uint8_t kTileDark = 0u;
    static constexpr uint8_t kTileBlue = 1u;
    static constexpr uint8_t kTileViolet = 2u;
    static constexpr uint8_t kTileSilver = 3u;
    static constexpr uint8_t kTileClassCount = 4u;

    // Scene geometry, exposed so the tests can assert invariants directly.
    static uint8_t phaseAtStep(uint8_t step);
    static uint8_t phaseCount();

    static bool ballContains(uint8_t x, uint8_t y);
    static bool suspensionContains(uint8_t x, uint8_t y);

    static uint8_t tileClassAt(uint8_t x, uint8_t y, uint8_t phase);

    static uint8_t highlightAnchorX(uint8_t phase);
    static uint8_t highlightAnchorY(uint8_t phase);
    static uint8_t highlightPixelCount();

    static uint8_t cornerBeamCount();
    static uint8_t cornerBeamLength();
    static uint8_t cornerBeamX(uint8_t index, uint8_t step);
    static uint8_t cornerBeamY(uint8_t index, uint8_t step);
    static uint8_t cornerBeamLevel(uint8_t index, uint8_t phase);

    static uint8_t floorCount();
    static uint8_t floorX(uint8_t index);
    static uint8_t floorY(uint8_t index);
    static uint8_t floorLevel(uint8_t index, uint8_t phase);

private:
    void renderFrame(PixelMatrix& matrix, uint8_t phase) const;
    void drawBackground(PixelMatrix& matrix) const;
    void drawCornerBeams(PixelMatrix& matrix, uint8_t phase) const;
    void drawBall(PixelMatrix& matrix, uint8_t phase) const;
    void drawHighlight(PixelMatrix& matrix, uint8_t phase) const;
    void drawFloor(PixelMatrix& matrix, uint8_t phase) const;

    uint32_t startedAt_;
    // 0xFF is not a valid phase, so the first update paints immediately.
    uint8_t lastPhase_;
    bool finished_;
};
