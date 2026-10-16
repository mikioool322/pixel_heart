#pragma once

#include <stdint.h>

#include "core/Animation.h"
#include "core/PixelMatrix.h"

// A large flower standing in a meadow, swaying gently in a daytime breeze.
//
// The scene is static - blue sky, three rows of meadow - and only the flower
// moves. The pose offsets are deliberately graduated: the lower stem NEVER
// moves, the middle stem and the leaves follow the wind by at most one pixel,
// and the head leans up to two. That is what makes it read as bending rather
// than as sliding across the panel.
//
// One base geometry plus five per-pose offsets is all the animation stores -
// no pre-rendered frames and no framebuffer.
class MeadowFlowerAnimation : public Animation {
public:
    MeadowFlowerAnimation();

    void start(uint32_t now) override;
    void update(uint32_t now, PixelMatrix& matrix) override;
    bool isFinished() const override;
    void stop() override;

    // One pose holds for 500 ms; a whole loop is eight steps = 4000 ms.
    static constexpr uint16_t kFrameIntervalMs = 500u;

    // Five distinct poses, played as an eight-step sequence.
    static constexpr uint8_t kPoseCount = 5u;
    static constexpr uint8_t kSequenceLength = 8u;
    static constexpr uint32_t kLoopDurationMs =
        static_cast<uint32_t>(kSequenceLength) * kFrameIntervalMs;

    // Poses: 0 = CENTER, 1 = RIGHT_1, 2 = RIGHT_2, 3 = LEFT_1, 4 = LEFT_2.
    static uint8_t poseAtStep(uint8_t step);
    static int8_t headShiftAt(uint8_t pose);
    static int8_t stemShiftAt(uint8_t pose);
    static int8_t leafShiftAt(uint8_t pose);

    // Scene geometry, exposed so the tests can assert the sway invariants
    // without scraping rendered pixels.
    static bool headContains(uint8_t x, uint8_t y);
    static bool stemLowerContains(uint8_t x, uint8_t y);
    static bool stemUpperContains(uint8_t x, uint8_t y);
    static bool leafContains(uint8_t x, uint8_t y);
    static bool meadowContains(uint8_t x, uint8_t y);
    static bool skyContains(uint8_t x, uint8_t y);
    static uint8_t smallFlowerCount();
    static uint8_t smallFlowerX(uint8_t index);
    static uint8_t smallFlowerY(uint8_t index);

    static uint8_t sequenceLength();
    static uint32_t loopDurationMs();

private:
    void renderFrame(PixelMatrix& matrix, uint8_t pose) const;
    void drawBackground(PixelMatrix& matrix) const;
    void drawFlower(PixelMatrix& matrix, uint8_t pose) const;

    uint32_t startedAt_;
    // 0xFF is not a valid pose, so the first update paints immediately.
    uint8_t lastPose_;
    bool finished_;
};
