#pragma once

#include <stdint.h>

#include "core/Animation.h"
#include "core/Color.h"
#include "core/PixelMatrix.h"

class RoseBloomAnimation : public Animation {
public:
    RoseBloomAnimation();

    void start(uint32_t now) override;
    void update(uint32_t now, PixelMatrix& matrix) override;
    bool isFinished() const override;
    void stop() override;

private:
    enum class RoseStage : uint8_t {
        STEM_GROW,
        LEAVES,
        BUD_SMALL,
        BUD_OPENING,
        BLOOM_OUTER,
        BLOOM_FULL,
        BREATHING,
        FINISHED
    };

    static uint32_t stageInterval(RoseStage stage);
    void advanceStage();
    void renderStage(PixelMatrix& matrix);
    void renderStem(PixelMatrix& matrix, uint8_t stemHeight, Color baseColor) const;

    uint32_t stageStartedAt_;
    uint8_t stemGrowthStep_;
    uint8_t breathStep_;
    RoseStage stage_;
    bool finished_;
};
