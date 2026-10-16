#pragma once

#include <stdint.h>

#include "core/Animation.h"
#include "core/Color.h"
#include "core/PixelMatrix.h"

class SlotMachineAnimation : public Animation {
public:
    SlotMachineAnimation();

    void start(uint32_t now) override;
    void update(uint32_t now, PixelMatrix& matrix) override;
    bool isFinished() const override;
    void stop() override;

private:
    enum class SlotStage : uint8_t {
        SPINNING,
        WIN_HOLD,
        JACKPOT,
        FINISHED
    };

    enum class SlotSymbol : uint8_t {
        SEVEN = 0,
        HEART = 1,
        CHERRY = 2
    };

    struct ReelState {
        uint8_t baseSymbol;
        uint8_t pixelOffset;
        bool stopRequested;
        bool stopped;
    };

    void resetReels(uint32_t now);

    void advanceReel(uint8_t reelIndex);

    void renderReels(
        PixelMatrix& matrix
    ) const;

    void drawSymbol(
        PixelMatrix& matrix,
        SlotSymbol symbol,
        uint8_t originX,
        int16_t originY,
        Color color
    ) const;

    static uint8_t symbolRow(
        SlotSymbol symbol,
        uint8_t row
    );

    static Color symbolColor(
        SlotSymbol symbol
    );

    bool allReelsStopped() const;

    void renderWinning777(
        PixelMatrix& matrix,
        uint8_t colorPhase,
        uint8_t borderPhase
    ) const;

    static Color jackpotColor(
        uint8_t reelIndex,
        uint8_t colorPhase
    );

    void drawJackpotBorder(
        PixelMatrix& matrix,
        uint8_t borderPhase
    ) const;

    uint32_t startedAt_;
    uint32_t stageStartedAt_;

    uint32_t reelLastStepAt_[3];

    uint32_t lastColorChangeAt_;
    uint32_t lastBorderToggleAt_;

    ReelState reels_[3];

    uint8_t colorPhase_;
    uint8_t borderPhase_;

    SlotStage stage_;
    bool finished_;
};
