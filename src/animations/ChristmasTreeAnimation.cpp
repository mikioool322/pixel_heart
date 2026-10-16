#include "animations/ChristmasTreeAnimation.h"

#include "core/Color.h"

namespace {

constexpr uint16_t kRenderIntervalMs = 90u;
constexpr uint16_t kSnowStepIntervalMs = 320u;
constexpr uint16_t kBlinkIntervalMs = 420u;

constexpr Color kStar{
    255, 210, 40
};

constexpr Color kStarGlow{
    255, 245, 140
};

constexpr Color kTreeDark{
    0, 120, 35
};

constexpr Color kTreeLight{
    20, 185, 60
};

constexpr Color kTrunk{
    120, 70, 25
};

constexpr Color kSnow{
    180, 220, 255
};

constexpr Color kSnowDim{
    100, 130, 160
};

constexpr Color kLightRed{
    255, 50, 50
};

constexpr Color kLightBlue{
    60, 140, 255
};

constexpr Color kLightYellow{
    255, 220, 60
};

constexpr Color kLightPink{
    255, 80, 180
};

}  // namespace

ChristmasTreeAnimation::ChristmasTreeAnimation()
    : startedAt_(0u),
      lastRenderAt_(0u),
      lastSnowStepAt_(0u),
      lastBlinkAt_(0u),
      snowStepCounter_(0u),
      blinkPhase_(0u),
      finished_(false),
      snow_{} {
}

void ChristmasTreeAnimation::start(uint32_t now) {
    startedAt_ = now;
    lastRenderAt_ = static_cast<uint32_t>(now - kRenderIntervalMs);
    lastSnowStepAt_ = now;
    lastBlinkAt_ = now;
    snowStepCounter_ = 0u;
    blinkPhase_ = 0u;
    finished_ = false;

    snow_[0] = Snowflake{1, 1, true};
    snow_[1] = Snowflake{4, 4, true};
    snow_[2] = Snowflake{7, 7, true};
    snow_[3] = Snowflake{10, 2, true};
    snow_[4] = Snowflake{13, 6, true};
    snow_[5] = Snowflake{15, 10, true};
    snow_[6] = Snowflake{5, 12, true};
    snow_[7] = Snowflake{11, 14, true};
}

void ChristmasTreeAnimation::update(uint32_t now, PixelMatrix& matrix) {
    if (finished_) {
        return;
    }

    bool frameChanged = false;

    const uint32_t snowElapsed =
        static_cast<uint32_t>(now - lastSnowStepAt_);

    if (snowElapsed >= kSnowStepIntervalMs) {
        advanceSnow();
        ++snowStepCounter_;
        lastSnowStepAt_ = now;
        frameChanged = true;
    }

    const uint32_t blinkElapsed =
        static_cast<uint32_t>(now - lastBlinkAt_);

    if (blinkElapsed >= kBlinkIntervalMs) {
        blinkPhase_ = static_cast<uint8_t>((blinkPhase_ + 1u) % 4u);
        lastBlinkAt_ = now;
        frameChanged = true;
    }

    const uint32_t renderElapsed =
        static_cast<uint32_t>(now - lastRenderAt_);

    if (frameChanged || renderElapsed >= kRenderIntervalMs) {
        renderFrame(matrix);
        lastRenderAt_ = now;
    }
}

bool ChristmasTreeAnimation::isFinished() const {
    return finished_;
}

void ChristmasTreeAnimation::stop() {
    finished_ = true;
}

void ChristmasTreeAnimation::renderFrame(PixelMatrix& matrix) const {
    matrix.clear();
    drawSnow(matrix);
    drawTree(matrix);
    drawLights(matrix);
}

void ChristmasTreeAnimation::setPixelSafe(
    PixelMatrix& matrix,
    int16_t x,
    int16_t y,
    Color color
) const {
    if (x < 0 || y < 0) {
        return;
    }

    if (x >= PixelMatrix::WIDTH || y >= PixelMatrix::HEIGHT) {
        return;
    }

    matrix.setPixel(
        static_cast<uint8_t>(x),
        static_cast<uint8_t>(y),
        color
    );
}

void ChristmasTreeAnimation::drawSnow(PixelMatrix& matrix) const {
    for (uint8_t i = 0u; i < 8u; ++i) {
        if (!snow_[i].active) {
            continue;
        }

        if (snow_[i].x < 0 || snow_[i].x >= PixelMatrix::WIDTH ||
            snow_[i].y < 0 || snow_[i].y >= PixelMatrix::HEIGHT) {
            continue;
        }

        const Color c =
            (static_cast<uint8_t>((snowStepCounter_ + i) % 3u) == 0u)
                ? kSnowDim
                : kSnow;

        matrix.setPixel(
            static_cast<uint8_t>(snow_[i].x),
            static_cast<uint8_t>(snow_[i].y),
            c
        );
    }
}

void ChristmasTreeAnimation::drawTree(PixelMatrix& matrix) const {
    // Star
    setPixelSafe(matrix, 7, 0, kStarGlow);
    setPixelSafe(matrix, 6, 1, kStar);
    setPixelSafe(matrix, 7, 1, kStarGlow);
    setPixelSafe(matrix, 8, 1, kStar);

    // Top tier
    for (int16_t x = 7; x <= 8; ++x) {
        setPixelSafe(matrix, x, 2, kTreeDark);
    }

    for (int16_t x = 6; x <= 9; ++x) {
        setPixelSafe(matrix, x, 3, (x == 6 || x == 9) ? kTreeDark : kTreeLight);
    }

    for (int16_t x = 5; x <= 10; ++x) {
        setPixelSafe(matrix, x, 4, (x == 5 || x == 10) ? kTreeDark : kTreeLight);
    }

    // Middle tier
    for (int16_t x = 6; x <= 9; ++x) {
        setPixelSafe(matrix, x, 5, (x == 6 || x == 9) ? kTreeDark : kTreeLight);
    }

    for (int16_t x = 5; x <= 10; ++x) {
        setPixelSafe(matrix, x, 6, (x == 5 || x == 10) ? kTreeDark : kTreeLight);
    }

    for (int16_t x = 4; x <= 11; ++x) {
        setPixelSafe(matrix, x, 7, (x == 4 || x == 11) ? kTreeDark : kTreeLight);
    }

    for (int16_t x = 3; x <= 12; ++x) {
        setPixelSafe(matrix, x, 8, (x == 3 || x == 12) ? kTreeDark : kTreeLight);
    }

    // Bottom tier
    for (int16_t x = 5; x <= 10; ++x) {
        setPixelSafe(matrix, x, 9, (x == 5 || x == 10) ? kTreeDark : kTreeLight);
    }

    for (int16_t x = 4; x <= 11; ++x) {
        setPixelSafe(matrix, x, 10, (x == 4 || x == 11) ? kTreeDark : kTreeLight);
    }

    for (int16_t x = 3; x <= 12; ++x) {
        setPixelSafe(matrix, x, 11, (x == 3 || x == 12) ? kTreeDark : kTreeLight);
    }

    for (int16_t x = 2; x <= 13; ++x) {
        setPixelSafe(matrix, x, 12, (x == 2 || x == 13) ? kTreeDark : kTreeLight);
    }

    // Trunk
    setPixelSafe(matrix, 7, 13, kTrunk);
    setPixelSafe(matrix, 8, 13, kTrunk);
    setPixelSafe(matrix, 7, 14, kTrunk);
    setPixelSafe(matrix, 8, 14, kTrunk);
}

void ChristmasTreeAnimation::drawLights(PixelMatrix& matrix) const {
    // Light positions on the tree
    const uint8_t phase = blinkPhase_;

    auto lightColorFor = [phase](uint8_t index) -> Color {
        switch (static_cast<uint8_t>((index + phase) % 4u)) {
            case 0u:
                return kLightRed;
            case 1u:
                return kLightBlue;
            case 2u:
                return kLightYellow;
            default:
                return kLightPink;
        }
    };

    auto lightVisible = [phase](uint8_t index) -> bool {
        switch (phase) {
            case 0u:
                return true;
            case 1u:
                return static_cast<uint8_t>(index % 3u) != 0u;
            case 2u:
                return static_cast<uint8_t>(index % 2u) == 0u;
            default:
                return static_cast<uint8_t>(index % 4u) != 1u;
        }
    };

    struct LightPoint {
        uint8_t x;
        uint8_t y;
    };

    const LightPoint lights[] = {
        {7u, 3u},
        {6u, 4u},
        {9u, 4u},
        {7u, 5u},
        {8u, 6u},
        {5u, 7u},
        {10u, 7u},
        {6u, 8u},
        {9u, 8u},
        {4u, 10u},
        {8u, 10u},
        {11u, 10u},
        {5u, 11u},
        {9u, 11u},
        {7u, 12u},
        {10u, 12u}
    };

    constexpr uint8_t kLightCount =
        static_cast<uint8_t>(sizeof(lights) / sizeof(lights[0]));

    for (uint8_t i = 0u; i < kLightCount; ++i) {
        if (!lightVisible(i)) {
            continue;
        }

        setPixelSafe(
            matrix,
            lights[i].x,
            lights[i].y,
            lightColorFor(i)
        );
    }
}

void ChristmasTreeAnimation::advanceSnow() {
    for (uint8_t i = 0u; i < 8u; ++i) {
        if (!snow_[i].active) {
            respawnSnowflake(i);
            continue;
        }

        ++snow_[i].y;

        if (static_cast<uint8_t>((snowStepCounter_ + i) % 2u) == 0u) {
            if (static_cast<uint8_t>(i % 2u) == 0u) {
                --snow_[i].x;
            } else {
                ++snow_[i].x;
            }
        }

        if (snow_[i].y > 15) {
            respawnSnowflake(i);
            continue;
        }

        if (snow_[i].x < 0) {
            snow_[i].x = 0;
        } else if (snow_[i].x > 15) {
            snow_[i].x = 15;
        }
    }
}

void ChristmasTreeAnimation::respawnSnowflake(uint8_t index) {
    const int8_t baseX[8] = {
        2, 5, 8, 11, 14, 1, 7, 13
    };

    const int8_t phaseOffset =
        static_cast<int8_t>((snowStepCounter_ + index * 3u) % 5u);

    snow_[index].x = static_cast<int8_t>((baseX[index] + phaseOffset) % 16);
    snow_[index].y = static_cast<int8_t>(
        -1 - (((snowStepCounter_ + index) % 3u))
    );
    snow_[index].active = true;
}
