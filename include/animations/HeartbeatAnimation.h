#pragma once

#include <stdint.h>

#include "core/Animation.h"
#include "core/PixelMatrix.h"

class HeartbeatAnimation : public Animation {
public:
    HeartbeatAnimation();

    void start(uint32_t now) override;
    void update(uint32_t now, PixelMatrix& matrix) override;
    bool isFinished() const override;
    void stop() override;

private:
    enum class HeartStage : uint8_t {
        OUTLINE_DRAW,
        FILL_EXPAND,
        FULL_HOLD,
        BEATING,
        FINISHED
    };

    void renderOutline(
        PixelMatrix& matrix,
        uint8_t maxRow,
        uint8_t scale = 255u
    ) const;

    void renderFilledHeart(
        PixelMatrix& matrix,
        uint8_t radius,
        uint8_t scale = 255u
    ) const;

    static uint8_t beatScale(uint8_t frame);
    static uint16_t beatDuration(uint8_t frame);

    uint32_t stageStartedAt_;

    uint8_t outlineRow_;
    uint8_t fillRadius_;

    uint8_t beatFrame_;
    uint8_t beatCycle_;

    HeartStage stage_;
    bool finished_;
};
