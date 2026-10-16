#pragma once

#include <stdint.h>

#include "core/Animation.h"
#include "core/Color.h"
#include "core/PixelMatrix.h"

class SunsetWaterAnimation : public Animation {
public:
    SunsetWaterAnimation();

    void start(uint32_t now) override;

    void update(
        uint32_t now,
        PixelMatrix& matrix
    ) override;

    bool isFinished() const override;

    void stop() override;

private:
    void renderFrame(
        PixelMatrix& matrix,
        uint8_t phase
    ) const;

    void renderSky(
        PixelMatrix& matrix
    ) const;

    void renderSun(
        PixelMatrix& matrix
    ) const;

    void renderWater(
        PixelMatrix& matrix,
        uint8_t phase
    ) const;

    static uint8_t shimmerValue(
        uint8_t phase
    );

    static Color blend(
        Color a,
        Color b,
        uint8_t amount
    );

    uint32_t lastFrameAt_;

    uint8_t phase_;
    uint8_t renderedFrameCount_;

    bool finished_;
};
