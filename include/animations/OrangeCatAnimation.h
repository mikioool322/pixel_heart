#pragma once

#include <stdint.h>

#include "core/Animation.h"
#include "core/Color.h"
#include "core/PixelMatrix.h"

// A large orange cat wearing sunglasses: the face fills essentially the whole
// 16x16 panel so it survives the diffuser's bloom.
//
// Everything is a still image except the small white reflections on the two
// lenses, which shimmer along a short symmetric, cyclic brightness curve.
class OrangeCatAnimation : public Animation {
public:
    OrangeCatAnimation();

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

    void renderCat(PixelMatrix& matrix) const;
    void renderGlints(
        PixelMatrix& matrix,
        uint8_t phase
    ) const;

    // Scene geometry. Single source of truth shared by rendering and tests so
    // the glint curve and the static sprite can never drift apart.
    static uint8_t glintLevelAt(uint8_t phase, bool rightSide);
    static uint8_t phaseCount();
    static uint8_t frameCount();

    static uint8_t glintPixelCount();
    static uint8_t glintPixelX(uint8_t index);
    static uint8_t glintPixelY(uint8_t index);
    static bool glintOnRight(uint8_t index);

    static bool bodyContains(uint8_t x, uint8_t y);
    static bool darkContains(uint8_t x, uint8_t y);
    static bool lensContains(uint8_t x, uint8_t y);
    static bool glintContains(uint8_t x, uint8_t y);

    uint32_t lastFrameAt_;

    uint8_t phase_;
    uint8_t renderedFrameCount_;

    bool finished_;
};
