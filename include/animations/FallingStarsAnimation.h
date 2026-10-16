#pragma once

#include <stdint.h>

#include "core/Animation.h"
#include "core/PixelMatrix.h"

class FallingStarsAnimation : public Animation {
public:
    static constexpr uint16_t kFrameIntervalMs = 90u;
    static constexpr uint8_t kLoopFrameCount = 64u;

    FallingStarsAnimation();

    void start(uint32_t now) override;
    void update(uint32_t now, PixelMatrix& matrix) override;
    bool isFinished() const override;
    void stop() override;

private:
    void renderFrame(PixelMatrix& matrix, uint8_t phase) const;
    void renderBackground(PixelMatrix& matrix, uint8_t phase) const;
    void renderMeteor(PixelMatrix& matrix, uint8_t meteorIndex, uint8_t phase) const;

    static uint8_t tailBrightness(uint8_t distance);

    uint32_t lastFrameAt_;
    uint8_t phase_;
    uint8_t renderedFrameCount_;
    bool finished_;
};
