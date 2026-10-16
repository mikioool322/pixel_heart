#pragma once

#include <stdint.h>

#include "core/Animation.h"
#include "core/PixelMatrix.h"

// A calm tropical palm on a 16x16 matrix.
//
// The scene is deliberately static - dim blue sky, a small warm sun, two rows of
// ocean and two rows of sand - and only the palm moves. The palm sways in a
// slow, seamless breeze: the crown and the upper trunk follow the wind by at
// most one pixel, the leaves by at most two, and the lower trunk NEVER moves, so
// the tree stays anchored in the sand instead of sliding across the panel.
//
// Designed for a milky diffuser, which blooms bright adjacent pixels: the sky is
// dim, the sun is small and no pure white is used anywhere, so the palm keeps the
// strongest contrast in the frame.
//
// One base geometry plus per-pose offsets is all the animation stores - no
// pre-rendered frames and no framebuffer.
class PalmBreezeAnimation : public Animation {
public:
    PalmBreezeAnimation();

    void start(uint32_t now) override;
    void update(uint32_t now, PixelMatrix& matrix) override;
    bool isFinished() const override;
    void stop() override;

    // One step of the breeze, in milliseconds. A full loop is eight steps.
    static constexpr uint16_t kFrameIntervalMs = 500u;

    // Scene geometry, exposed so the tests can assert the sway invariants
    // without scraping rendered pixels.
    static constexpr uint8_t kPoseCount = 5u;
    static constexpr uint8_t kSequenceLength = 8u;

    // Poses: 0 = CENTER, 1 = RIGHT_1, 2 = RIGHT_2, 3 = LEFT_1, 4 = LEFT_2.
    static uint8_t poseAtStep(uint8_t step);
    static int8_t trunkShiftAt(uint8_t pose);
    static int8_t leafShiftAt(uint8_t pose);

    // The half of the trunk that is anchored in the sand, in CENTER geometry.
    static bool trunkLowerContains(uint8_t x, uint8_t y);
    // The half of the trunk that follows the wind.
    static bool trunkUpperContains(uint8_t x, uint8_t y);
    static bool leavesDarkContains(uint8_t x, uint8_t y);
    static bool leavesBrightContains(uint8_t x, uint8_t y);

    static uint8_t sequenceLength();
    static uint32_t loopDurationMs();

private:
    void renderFrame(PixelMatrix& matrix) const;
    void drawBackground(PixelMatrix& matrix) const;
    void drawPalm(PixelMatrix& matrix, uint8_t pose) const;

    uint32_t startedAt_;
    uint8_t sequenceIndex_;
    uint8_t renderedFrameCount_;
    bool finished_;
};
