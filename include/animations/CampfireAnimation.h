#pragma once

#include <stdint.h>

#include "core/Animation.h"
#include "core/PixelMatrix.h"

class CampfireAnimation : public Animation {
public:
    CampfireAnimation();

    void start(uint32_t now) override;

    void update(
        uint32_t now,
        PixelMatrix& matrix
    ) override;

    bool isFinished() const override;

    void stop() override;

private:
    static constexpr uint8_t kFrameCount = 4u;
    static constexpr uint16_t kFrameDurationMs = 140u;
    static constexpr uint32_t kAnimationDurationMs = 12000u;

    void renderFrame(
        PixelMatrix& matrix,
        uint8_t frameIndex
    ) const;

    void renderLogs(
        PixelMatrix& matrix
    ) const;

    void renderEmbers(
        PixelMatrix& matrix,
        uint8_t frameIndex
    ) const;

    void renderFlameFrame(
        PixelMatrix& matrix,
        uint8_t frameIndex
    ) const;

    uint32_t startedAt_;
    uint32_t lastFrameAt_;

    uint8_t frameIndex_;

    bool finished_;
};
