#pragma once

#include <stdint.h>

#include "core/Animation.h"
#include "core/Color.h"
#include "core/PixelMatrix.h"

class ChristmasTreeAnimation : public Animation {
public:
    ChristmasTreeAnimation();

    void start(uint32_t now) override;
    void update(uint32_t now, PixelMatrix& matrix) override;
    bool isFinished() const override;
    void stop() override;

private:
    struct Snowflake {
        int8_t x;
        int8_t y;
        bool active;
    };

    void renderFrame(PixelMatrix& matrix) const;
    void drawSnow(PixelMatrix& matrix) const;
    void drawTree(PixelMatrix& matrix) const;
    void drawLights(PixelMatrix& matrix) const;
    void setPixelSafe(PixelMatrix& matrix, int16_t x, int16_t y, Color color) const;

    void advanceSnow();
    void respawnSnowflake(uint8_t index);

    uint32_t startedAt_;
    uint32_t lastRenderAt_;
    uint32_t lastSnowStepAt_;
    uint32_t lastBlinkAt_;

    uint16_t snowStepCounter_;
    uint8_t blinkPhase_;
    // Persistent ambient animation: never auto-finish during normal operation.
    bool finished_;

    Snowflake snow_[8];
};
