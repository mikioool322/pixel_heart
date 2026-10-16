#pragma once

#include <stdint.h>

#include "core/Animation.h"
#include "core/Color.h"
#include "core/PixelMatrix.h"

class FireworksAnimation : public Animation {
public:
    FireworksAnimation();

    void start(uint32_t now) override;
    void update(uint32_t now, PixelMatrix& matrix) override;
    bool isFinished() const override;
    void stop() override;

private:
    enum class FireworkStage : uint8_t {
        ROCKET,
        BURST,
        FADE,
        PAUSE,
        BIRTHDAY_MESSAGE,
        FINISHED
    };

    struct FireworkDefinition {
        uint8_t launchX;
        uint8_t burstX;
        uint8_t burstY;
        Color primary;
        Color secondary;
        uint8_t maxRadius;
    };

    void renderRocket(PixelMatrix& matrix) const;
    void renderBurst(PixelMatrix& matrix) const;
    void drawBirthdayGlyph(
        PixelMatrix& matrix,
        char glyph,
        uint8_t originX,
        uint8_t originY,
        Color color
    ) const;
    void renderBirthdayMessage(PixelMatrix& matrix) const;

    void loadFirework(uint8_t index);

    static Color fadeColor(Color color, uint8_t scale);
    static Color birthdayColor(uint8_t letterIndex, uint8_t colorPhase);

    uint32_t stageStartedAt_;
    uint32_t birthdayStartedAt_;
    uint32_t lastBorderToggleAt_;
    uint32_t lastColorChangeAt_;

    uint8_t fireworkIndex_;

    uint8_t rocketY_;
    uint8_t burstRadius_;
    uint8_t fadeStep_;
    uint8_t borderPhase_;
    uint8_t colorPhase_;

    FireworkDefinition current_;

    FireworkStage stage_;
    bool finished_;
};
