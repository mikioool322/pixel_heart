#pragma once

#include <stdint.h>

#include "core/Animation.h"
#include "core/PixelMatrix.h"

class SunOverMountainsAnimation : public Animation {
public:
    static constexpr uint16_t kFrameIntervalMs = 120u;
    static constexpr uint8_t kLoopFrameCount = 64u;
    static constexpr uint8_t kPhaseCount = 32u;

    SunOverMountainsAnimation();

    void start(uint32_t now) override;
    void update(uint32_t now, PixelMatrix& matrix) override;
    bool isFinished() const override;
    void stop() override;

private:
    void renderFrame(PixelMatrix& matrix, uint8_t phase) const;
    void renderSky(PixelMatrix& matrix, uint8_t phase) const;
    void renderSun(PixelMatrix& matrix, uint8_t phase) const;
    void renderMountains(PixelMatrix& matrix) const;

    static uint8_t shimmerValue(uint8_t phase);

    uint32_t lastFrameAt_;
    uint8_t phase_;
    uint8_t renderedFrameCount_;
    bool finished_;
};
