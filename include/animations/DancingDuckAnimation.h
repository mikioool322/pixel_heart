#pragma once

#include <stdint.h>

#include "core/Animation.h"
#include "core/PixelMatrix.h"

class DancingDuckAnimation : public Animation {
public:
    DancingDuckAnimation();

    void start(uint32_t now) override;

    void update(
        uint32_t now,
        PixelMatrix& matrix
    ) override;

    bool isFinished() const override;

    void stop() override;

private:
    static constexpr uint8_t kFrameCount = 4u;

    // Matches the supplied reference GIF.
    static constexpr uint16_t kFrameDurationMs = 320u; // Duration of each frame in milliseconds

    static constexpr uint32_t kAnimationDurationMs = 12000u; // Total duration of the animation in milliseconds

    void renderFrame(
        PixelMatrix& matrix,
        uint8_t frameIndex
    ) const;

    uint32_t startedAt_;
    uint32_t lastFrameAt_;

    uint8_t frameIndex_;

    bool finished_;
};
