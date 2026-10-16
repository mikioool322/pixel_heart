#include "animations/SlotMachineAnimation.h"

#include "core/Color.h"
#include "core/PixelMatrix.h"

namespace {

constexpr uint8_t kReelX[3] = {
    1u,
    6u,
    11u
};

constexpr uint8_t kSymbolWidth = 4u;
constexpr uint8_t kSymbolHeight = 5u;
constexpr uint8_t kSymbolPitch = 6u;

constexpr uint16_t kReelStepMs[3] = {
    70u,
    62u,
    76u
};

constexpr uint32_t kStopRequestMs[3] = {
    2200u,
    3000u,
    3800u
};

constexpr uint16_t kWinHoldMs = 700u;
constexpr uint32_t kJackpotDurationMs = 10000u;

constexpr Color kSlotSeven{
    255,
    205,
    35
};

constexpr Color kSlotHeart{
    255,
    45,
    105
};

constexpr Color kSlotCherry{
    240,
    25,
    35
};

constexpr Color kJackpotRed{
    255,
    35,
    45
};

constexpr Color kJackpotOrange{
    255,
    105,
    25
};

constexpr Color kJackpotGold{
    255,
    205,
    30
};

constexpr Color kJackpotGreen{
    45,
    230,
    85
};

constexpr Color kJackpotCyan{
    35,
    220,
    255
};

constexpr Color kJackpotBlue{
    55,
    100,
    255
};

constexpr Color kJackpotPurple{
    165,
    55,
    255
};

constexpr Color kJackpotPink{
    255,
    50,
    160
};

constexpr Color kJackpotBorderRed{
    255,
    0,
    25
};

}  // namespace

SlotMachineAnimation::SlotMachineAnimation()
    : startedAt_(0u),
      stageStartedAt_(0u),
      lastColorChangeAt_(0u),
      lastBorderToggleAt_(0u),
      reels_{},
      colorPhase_(0u),
      borderPhase_(0u),
      stage_(SlotStage::SPINNING),
      finished_(false) {
}

void SlotMachineAnimation::start(uint32_t now) {
    startedAt_ = now;
    stageStartedAt_ = now;

    colorPhase_ = 0u;
    borderPhase_ = 0u;

    lastColorChangeAt_ = now;
    lastBorderToggleAt_ = now;

    resetReels(now);

    stage_ = SlotStage::SPINNING;
    finished_ = false;
}

void SlotMachineAnimation::resetReels(uint32_t now) {
    reels_[0] = {
        0u,
        0u,
        false,
        false
    };

    reels_[1] = {
        2u,
        2u,
        false,
        false
    };

    reels_[2] = {
        1u,
        4u,
        false,
        false
    };

    reelLastStepAt_[0] = static_cast<uint32_t>(now - kReelStepMs[0]);
    reelLastStepAt_[1] = static_cast<uint32_t>(now - kReelStepMs[1]);
    reelLastStepAt_[2] = static_cast<uint32_t>(now - kReelStepMs[2]);
}

void SlotMachineAnimation::advanceReel(uint8_t reelIndex) {
    if (reelIndex >= 3u) {
        return;
    }

    ReelState& reel = reels_[reelIndex];

    if (reel.stopped) {
        return;
    }

    ++reel.pixelOffset;

    if (reel.pixelOffset >= kSymbolPitch) {
        reel.pixelOffset = 0u;

        reel.baseSymbol = static_cast<uint8_t>(
            (reel.baseSymbol + 2u) % 3u
        );

        if (
            reel.stopRequested &&
            reel.baseSymbol == 1u
        ) {
            reel.stopped = true;
        }
    }
}

void SlotMachineAnimation::renderReels(PixelMatrix& matrix) const {
    matrix.clear();

    for (uint8_t reel = 0u; reel < 3u; ++reel) {
        const ReelState& state = reels_[reel];

        for (uint8_t visible = 0u; visible < 4u; ++visible) {
            const SlotSymbol symbol = static_cast<SlotSymbol>(
                (state.baseSymbol + visible) % 3u
            );

            const int16_t y =
                static_cast<int16_t>(-7) +
                static_cast<int16_t>(state.pixelOffset) +
                static_cast<int16_t>(visible * 6u);

            drawSymbol(
                matrix,
                symbol,
                kReelX[reel],
                y,
                symbolColor(symbol)
            );
        }
    }
}

void SlotMachineAnimation::drawSymbol(
    PixelMatrix& matrix,
    SlotSymbol symbol,
    uint8_t originX,
    int16_t originY,
    Color color
) const {
    for (uint8_t row = 0u; row < 5u; ++row) {
        const uint8_t bits =
            symbolRow(symbol, row);

        for (uint8_t col = 0u; col < 4u; ++col) {
            const uint8_t bit =
                static_cast<uint8_t>(
                    1u << (3u - col)
                );

            if ((bits & bit) == 0u) {
                continue;
            }

            const int16_t px =
                static_cast<int16_t>(
                    originX + (3u - col)
                );
            const int16_t py =
                originY +
                static_cast<int16_t>(row);

            if (
                px < 0 ||
                py < 0 ||
                px >= PixelMatrix::WIDTH ||
                py >= PixelMatrix::HEIGHT
            ) {
                continue;
            }

            matrix.setPixel(
                static_cast<uint8_t>(px),
                static_cast<uint8_t>(py),
                color
            );
        }
    }
}

uint8_t SlotMachineAnimation::symbolRow(SlotSymbol symbol, uint8_t row) {
    if (row >= 5u) {
        return 0u;
    }

    switch (symbol) {
        case SlotSymbol::SEVEN: {
            static const uint8_t rows[5] = {
                0b1111,
                0b0001,
                0b0010,
                0b0100,
                0b0100
            };

            return rows[row];
        }

        case SlotSymbol::HEART: {
            static const uint8_t rows[5] = {
                0b1001,
                0b1111,
                0b1111,
                0b0110,
                0b0010
            };

            return rows[row];
        }

        case SlotSymbol::CHERRY: {
            static const uint8_t rows[5] = {
                0b0010,
                0b0110,
                0b1010,
                0b1101,
                0b1001
            };

            return rows[row];
        }

        default:
            return 0u;
    }
}

Color SlotMachineAnimation::symbolColor(SlotSymbol symbol) {
    switch (symbol) {
        case SlotSymbol::SEVEN:
            return kSlotSeven;
        case SlotSymbol::HEART:
            return kSlotHeart;
        case SlotSymbol::CHERRY:
            return kSlotCherry;
        default:
            return {0u, 0u, 0u};
    }
}

bool SlotMachineAnimation::allReelsStopped() const {
    return reels_[0].stopped && reels_[1].stopped && reels_[2].stopped;
}

void SlotMachineAnimation::renderWinning777(
    PixelMatrix& matrix,
    uint8_t colorPhase,
    uint8_t borderPhase
) const {
    matrix.clear();

    drawSymbol(
        matrix,
        SlotSymbol::SEVEN,
        1u,
        5,
        jackpotColor(0u, colorPhase)
    );

    drawSymbol(
        matrix,
        SlotSymbol::SEVEN,
        6u,
        5,
        jackpotColor(1u, colorPhase)
    );

    drawSymbol(
        matrix,
        SlotSymbol::SEVEN,
        11u,
        5,
        jackpotColor(2u, colorPhase)
    );

    drawJackpotBorder(matrix, borderPhase);
}

Color SlotMachineAnimation::jackpotColor(uint8_t reelIndex, uint8_t colorPhase) {
    static const Color palette[8] = {
        kJackpotRed,
        kJackpotOrange,
        kJackpotGold,
        kJackpotGreen,
        kJackpotCyan,
        kJackpotBlue,
        kJackpotPurple,
        kJackpotPink
    };

    const uint8_t index = (reelIndex + colorPhase) % 8u;
    return palette[index];
}

void SlotMachineAnimation::drawJackpotBorder(
    PixelMatrix& matrix,
    uint8_t borderPhase
) const {
    for (uint8_t y = 0u; y < PixelMatrix::HEIGHT; ++y) {
        for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
            const bool border =
                x == 0u ||
                x == 15u ||
                y == 0u ||
                y == 15u;

            if (!border) {
                continue;
            }

            const bool lit = ((x + y + borderPhase) % 2u) == 0u;

            if (!lit) {
                continue;
            }

            matrix.setPixel(x, y, kJackpotBorderRed);
        }
    }
}

void SlotMachineAnimation::update(uint32_t now, PixelMatrix& matrix) {
    if (finished_) {
        return;
    }

    switch (stage_) {
        case SlotStage::SPINNING: {
            uint32_t totalElapsed = static_cast<uint32_t>(now - startedAt_);

            for (uint8_t reel = 0u; reel < 3u; ++reel) {
                if (totalElapsed >= kStopRequestMs[reel]) {
                    reels_[reel].stopRequested = true;
                }
            }

            bool frameChanged = false;

            for (uint8_t reel = 0u; reel < 3u; ++reel) {
                if (reels_[reel].stopped) {
                    continue;
                }

                const uint32_t elapsed = static_cast<uint32_t>(
                    now - reelLastStepAt_[reel]
                );

                if (elapsed >= kReelStepMs[reel]) {
                    advanceReel(reel);
                    reelLastStepAt_[reel] = now;
                    frameChanged = true;
                }
            }

            if (frameChanged) {
                renderReels(matrix);
            }

            if (allReelsStopped()) {
                stage_ = SlotStage::WIN_HOLD;
                stageStartedAt_ = now;
                renderReels(matrix);
            }

            break;
        }

        case SlotStage::WIN_HOLD: {
            if (static_cast<uint32_t>(now - stageStartedAt_) >= kWinHoldMs) {
                stage_ = SlotStage::JACKPOT;
                colorPhase_ = 0u;
                borderPhase_ = 0u;
                stageStartedAt_ = now;
                lastColorChangeAt_ = now;
                lastBorderToggleAt_ = now;
                renderWinning777(matrix, colorPhase_, borderPhase_);
            }
            break;
        }

        case SlotStage::JACKPOT: {
            bool frameChanged = false;

            if (static_cast<uint32_t>(now - lastColorChangeAt_) >= 1000u) {
                colorPhase_ = static_cast<uint8_t>((colorPhase_ + 1u) % 8u);
                lastColorChangeAt_ = now;
                frameChanged = true;
            }

            if (static_cast<uint32_t>(now - lastBorderToggleAt_) >= 500u) {
                borderPhase_ ^= 1u;
                lastBorderToggleAt_ = now;
                frameChanged = true;
            }

            if (frameChanged) {
                renderWinning777(matrix, colorPhase_, borderPhase_);
            }

            if (static_cast<uint32_t>(now - stageStartedAt_) >= kJackpotDurationMs) {
                renderWinning777(matrix, colorPhase_, borderPhase_);
                stage_ = SlotStage::FINISHED;
                finished_ = true;
                return;
            }
            break;
        }

        case SlotStage::FINISHED:
            finished_ = true;
            return;
    }
}

bool SlotMachineAnimation::isFinished() const {
    return finished_;
}

void SlotMachineAnimation::stop() {
    startedAt_ = 0u;
    stageStartedAt_ = 0u;

    lastColorChangeAt_ = 0u;
    lastBorderToggleAt_ = 0u;

    for (uint8_t i = 0u; i < 3u; ++i) {
        reelLastStepAt_[i] = 0u;

        reels_[i].baseSymbol = 0u;
        reels_[i].pixelOffset = 0u;
        reels_[i].stopRequested = false;
        reels_[i].stopped = false;
    }

    colorPhase_ = 0u;
    borderPhase_ = 0u;

    stage_ = SlotStage::FINISHED;
    finished_ = true;
}
