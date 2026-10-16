#pragma once

#include <stdint.h>

#include "core/Animation.h"
#include "core/Color.h"
#include "core/PixelMatrix.h"

enum class AuroraPaletteId : uint8_t {
    ARCTIC,
    SUNSET,
    NEON
};

class AuroraAnimation : public Animation {
public:
    explicit AuroraAnimation(AuroraPaletteId palette);

    void start(uint32_t now) override;
    void update(uint32_t now, PixelMatrix& matrix) override;
    bool isFinished() const override;
    void stop() override;

private:
    void renderFrame(
        PixelMatrix& matrix,
        uint16_t phase
    ) const;

    Color auroraColor(uint8_t value) const;
    static Color lerpColor(
        Color a,
        Color b,
        uint8_t amount
    );

    AuroraPaletteId palette_;

    uint32_t startedAt_;
    uint32_t lastFrameAt_;

    uint16_t phase_;
    uint16_t renderedFrameCount_;

    bool finished_;
};
