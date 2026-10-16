#pragma once

#include <stdint.h>

#include "core/Animation.h"
#include "core/Color.h"
#include "core/PixelMatrix.h"

class FoxAnimation : public Animation {
public:
    FoxAnimation();

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
        uint8_t frame
    ) const;

    void renderBody(PixelMatrix& matrix) const;
    void renderTail(
        PixelMatrix& matrix,
        uint8_t tailPose
    ) const;

    // Scene geometry. Single source of truth shared by rendering and tests so
    // the tail poses and the static body can never drift apart.
    static uint8_t tailPoseAt(uint8_t frame);
    static uint8_t frameCount();

    // Tail tip (white) pixels for a pose. The outline and fur of the tail are
    // shared across poses; only these two pixels move.
    static uint8_t tailTipX(uint8_t tailPose, uint8_t tip);
    static uint8_t tailTipY(uint8_t tailPose, uint8_t tip);

    static bool staticBodyContains(uint8_t x, uint8_t y);
    static bool tailContains(uint8_t tailPose, uint8_t x, uint8_t y);

    uint32_t lastFrameAt_;

    uint8_t phase_;
    uint8_t renderedFrameCount_;

    bool finished_;
};
