#pragma once

#include <stdint.h>

#include "core/Animation.h"
#include "core/PixelMatrix.h"

// A bright daytime sky with three large clouds drifting slowly across it.
//
// Every cloud is derived from ONE global 32-step cycle, so the whole scene is
// periodic by construction: t=0 and t=kLoopDurationMs are the same frame, and no
// cloud can drift or teleport. Two clouds travel right and one travels left, so
// the sky never moves in lock step.
//
// Each cloud is drawn with a highlight row on top, a body and a shadow row
// underneath - that is what gives the soft shape depth behind the diffuser.
class BlueSkyCloudsAnimation : public Animation {
public:
    BlueSkyCloudsAnimation();

    void start(uint32_t now) override;
    void update(uint32_t now, PixelMatrix& matrix) override;
    bool isFinished() const override;
    void stop() override;

    // One step is 800 ms; a whole loop is 32 steps = 25600 ms.
    static constexpr uint16_t kStepMs = 800u;
    static constexpr uint8_t kPhaseCount = 32u;
    static constexpr uint32_t kLoopDurationMs =
        static_cast<uint32_t>(kPhaseCount) * kStepMs;

    static constexpr uint8_t kCloudCount = 3u;

    static uint8_t phaseAtElapsed(uint32_t elapsedMs);

    static uint8_t cloudBaseY(uint8_t index);
    static uint8_t cloudHeight(uint8_t index);
    static bool cloudMovesRight(uint8_t index);
    static uint8_t cloudPhaseOffset(uint8_t index);
    static int8_t cloudStartX(uint8_t index);
    static int8_t cloudEndX(uint8_t index);

    // Horizontal position of a cloud at a global phase, in logical coordinates.
    static int16_t cloudXAt(uint8_t index, uint8_t phase);

    // Membership of a cloud's own mask, in mask-local coordinates.
    static bool cloudContains(uint8_t index, uint8_t localX, uint8_t localY);

    static bool skyContains(uint8_t x, uint8_t y);

private:
    void renderFrame(PixelMatrix& matrix, uint8_t phase) const;
    void drawSky(PixelMatrix& matrix) const;
    void drawClouds(PixelMatrix& matrix, uint8_t phase) const;
    void drawCloud(PixelMatrix& matrix, uint8_t index, int16_t x) const;

    uint32_t startedAt_;
    // 0xFF is not a valid phase, so the first update paints immediately.
    uint8_t lastPhase_;
    bool finished_;
};
