#pragma once

#include <stdint.h>

#include "core/Animation.h"
#include "core/Color.h"
#include "core/PixelMatrix.h"

class MoonStarsAnimation : public Animation {
public:
    MoonStarsAnimation();

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

    void renderMoon(
        PixelMatrix& matrix
    ) const;

    void renderStars(
        PixelMatrix& matrix,
        uint8_t phase
    ) const;

    static uint8_t twinkleScale(
        uint8_t phase
    );

    static Color starBaseColor(
        uint8_t type
    );

    // Scene geometry. These are the single source of truth for the moon shape
    // and the star layout: rendering uses them, and the focused tests assert
    // the composition invariants (size, crescent orientation, star spacing and
    // the moon exclusion halo) against them so the two can never drift.
    static bool moonContains(
        uint8_t x,
        uint8_t y
    );

    static bool moonHaloContains(
        uint8_t x,
        uint8_t y
    );

    static Color moonShadeAt(
        uint8_t x,
        uint8_t y
    );

    static uint8_t starCount();

    static uint8_t starAnchorX(
        uint8_t index
    );

    static uint8_t starAnchorY(
        uint8_t index
    );

    static uint8_t starFootprintRadius(
        uint8_t index
    );

    static bool starContains(
        uint8_t index,
        uint8_t x,
        uint8_t y
    );

    uint32_t lastFrameAt_;

    uint8_t phase_;
    uint8_t renderedFrameCount_;

    bool finished_;
};
