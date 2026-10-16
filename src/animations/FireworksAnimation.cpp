#include "animations/FireworksAnimation.h"

#include "core/Color.h"
#include "core/PixelMatrix.h"

namespace {

constexpr uint8_t kFireworkCount = 4u; // Number of fireworks in the animation
constexpr uint8_t kRocketStartY = 15u; // Starting Y position of the rocket
constexpr uint8_t kRocketStepMs = 40u; // Duration of each rocket step in milliseconds
constexpr uint8_t kBurstStepMs = 90u; // Duration of each burst step in milliseconds
constexpr uint8_t kFadeStepMs = 170u; // Duration of each fade step in milliseconds
constexpr uint8_t kPauseMs = 180u; // Pause duration between fireworks in milliseconds
constexpr uint32_t kBirthdayDurationMs = 10000u;
constexpr uint16_t kBorderToggleMs = 500u;
constexpr uint16_t kLetterColorChangeMs = 1000u;

constexpr Color kBirthdayRed{255, 35, 55};
constexpr Color kBirthdayOrange{255, 105, 20};
constexpr Color kBirthdayGold{255, 205, 30};
constexpr Color kBirthdayGreen{45, 220, 90};
constexpr Color kBirthdayCyan{40, 210, 255};
constexpr Color kBirthdayBlue{60, 105, 255};
constexpr Color kBirthdayPurple{175, 60, 255};
constexpr Color kBirthdayPink{255, 55, 150};
constexpr Color kBirthdayMagenta{255, 40, 220};
constexpr Color kBirthdayLime{155, 255, 45};
constexpr Color kBirthdayBorderPink{255, 40, 145};

struct FireworkSpec {
    uint8_t launchX;
    uint8_t burstX;
    uint8_t burstY;
    Color primary;
    Color secondary;
    uint8_t maxRadius;
};

static const FireworkSpec kFireworks[kFireworkCount] = {
    {3u, 4u, 5u, Colors::FireworkRed, Colors::FireworkGold, 4u},
    {12u, 11u, 4u, Colors::FireworkBlue, Colors::FireworkWhite, 4u},
    {7u, 7u, 6u, Colors::FireworkPink, Colors::FireworkGold, 5u},
    {8u, 8u, 3u, Colors::FireworkGold, Colors::FireworkWhite, 6u}
};

bool inside(int16_t x, int16_t y) {
    return x >= 0 && y >= 0 && x < PixelMatrix::WIDTH && y < PixelMatrix::HEIGHT;
}

uint8_t glyphWidth(char glyph) {
    switch (glyph) {
        case 'S':
        case 'L':
            return 2u;
        case 'T':
        case 'O':
        case 'A':
            return 3u;
        default:
            return 0u;
    }
}

uint8_t glyphRow(char glyph, uint8_t row) {
    switch (glyph) {
        case 'S':
            switch (row) {
                case 0u: return 0b11u;
                case 1u: return 0b10u;
                case 2u: return 0b11u;
                case 3u: return 0b01u;
                case 4u: return 0b11u;
                default: return 0u;
            }
        case 'T':
            switch (row) {
                case 0u: return 0b111u;
                case 1u: return 0b010u;
                case 2u: return 0b010u;
                case 3u: return 0b010u;
                case 4u: return 0b010u;
                default: return 0u;
            }
        case 'O':
            switch (row) {
                case 0u: return 0b111u;
                case 1u: return 0b101u;
                case 2u: return 0b101u;
                case 3u: return 0b101u;
                case 4u: return 0b111u;
                default: return 0u;
            }
        case 'L':
            switch (row) {
                case 0u: return 0b10u;
                case 1u: return 0b10u;
                case 2u: return 0b10u;
                case 3u: return 0b10u;
                case 4u: return 0b11u;
                default: return 0u;
            }
        case 'A':
            switch (row) {
                case 0u: return 0b010u;
                case 1u: return 0b101u;
                case 2u: return 0b111u;
                case 3u: return 0b101u;
                case 4u: return 0b101u;
                default: return 0u;
            }
        default:
            return 0u;
    }
}

void drawSpark(PixelMatrix& matrix, int16_t x, int16_t y, Color color) {
    if (!inside(x, y)) {
        return;
    }

    matrix.setPixel(static_cast<uint8_t>(x), static_cast<uint8_t>(y), color);
}

void drawBurstSpark(
    PixelMatrix& matrix,
    int16_t cx,
    int16_t cy,
    int16_t dx,
    int16_t dy,
    uint8_t radius,
    Color primary,
    Color secondary,
    uint8_t scale
) {
    const int16_t px = cx + dx * static_cast<int16_t>(radius);
    const int16_t py = cy + dy * static_cast<int16_t>(radius);

    if (!inside(px, py)) {
        return;
    }

    const bool isCardinal = (dx == 0) || (dy == 0);
    const Color sparkColor = isCardinal ? scaleColor(primary, scale) : scaleColor(secondary, scale);
    drawSpark(matrix, px, py, sparkColor);
}

void drawFadeBurst(
    PixelMatrix& matrix,
    uint8_t burstX,
    uint8_t burstY,
    uint8_t radius,
    Color primary,
    Color secondary,
    uint8_t scale
) {
    const int16_t cx = static_cast<int16_t>(burstX);
    const int16_t cy = static_cast<int16_t>(burstY);

    const int16_t primaryDirections[][2] = {
        {-1, 0}, {1, 0}, {0, -1}, {0, 1},
        {-1, -1}, {1, -1}, {-1, 1}, {1, 1}
    };

    const int16_t extraDirections[][2] = {
        {-2, -1}, {2, -1}, {-1, 2}, {1, 2}
    };

    if (radius == 0u) {
        drawSpark(matrix, cx, cy, scaleColor(Colors::FireworkWhite, scale));
        return;
    }

    for (uint8_t i = 0u; i < 8u; ++i) {
        drawBurstSpark(
            matrix,
            cx,
            cy,
            primaryDirections[i][0],
            primaryDirections[i][1],
            radius,
            primary,
            secondary,
            scale
        );
    }

    for (uint8_t i = 0u; i < 4u; ++i) {
        const int16_t px = cx + extraDirections[i][0] * static_cast<int16_t>(radius);
        const int16_t py = cy + extraDirections[i][1] * static_cast<int16_t>(radius);
        if (inside(px, py)) {
            matrix.setPixel(static_cast<uint8_t>(px), static_cast<uint8_t>(py), scaleColor(secondary, scale));
        }
    }
}

}  // namespace

FireworksAnimation::FireworksAnimation()
    : stageStartedAt_(0u),
      birthdayStartedAt_(0u),
      lastBorderToggleAt_(0u),
      lastColorChangeAt_(0u),
      fireworkIndex_(0u),
      rocketY_(kRocketStartY),
      burstRadius_(0u),
      fadeStep_(0u),
      borderPhase_(0u),
      colorPhase_(0u),
      current_({0u, 0u, 0u, Colors::Black, Colors::Black, 0u}),
      stage_(FireworkStage::ROCKET),
      finished_(false) {
}

void FireworksAnimation::start(uint32_t now) {
    fireworkIndex_ = 0u;
    rocketY_ = kRocketStartY;
    burstRadius_ = 0u;
    fadeStep_ = 0u;
    birthdayStartedAt_ = 0u;
    lastBorderToggleAt_ = 0u;
    lastColorChangeAt_ = 0u;
    borderPhase_ = 0u;
    colorPhase_ = 0u;
    stage_ = FireworkStage::ROCKET;
    finished_ = false;
    loadFirework(fireworkIndex_);
    stageStartedAt_ = now;
}

void FireworksAnimation::loadFirework(uint8_t index) {
    if (index >= kFireworkCount) {
        current_ = {0u, 0u, 0u, Colors::Black, Colors::Black, 0u};
        return;
    }

    const FireworkSpec& spec = kFireworks[index];
    current_.launchX = spec.launchX;
    current_.burstX = spec.burstX;
    current_.burstY = spec.burstY;
    current_.primary = spec.primary;
    current_.secondary = spec.secondary;
    current_.maxRadius = spec.maxRadius;
}

Color FireworksAnimation::fadeColor(Color color, uint8_t scale) {
    return scaleColor(color, scale);
}

Color FireworksAnimation::birthdayColor(uint8_t letterIndex, uint8_t colorPhase) {
    const uint8_t index = static_cast<uint8_t>((letterIndex + colorPhase) % 10u);

    switch (index) {
        case 0u: return kBirthdayRed;
        case 1u: return kBirthdayOrange;
        case 2u: return kBirthdayGold;
        case 3u: return kBirthdayGreen;
        case 4u: return kBirthdayCyan;
        case 5u: return kBirthdayBlue;
        case 6u: return kBirthdayPurple;
        case 7u: return kBirthdayPink;
        case 8u: return kBirthdayMagenta;
        case 9u:
        default: return kBirthdayLime;
    }
}

void FireworksAnimation::drawBirthdayGlyph(
    PixelMatrix& matrix,
    char glyph,
    uint8_t originX,
    uint8_t originY,
    Color color
) const {
    const uint8_t width = glyphWidth(glyph);
    if (width == 0u) {
        return;
    }

    for (uint8_t row = 0u; row < 5u; ++row) {
        const uint8_t bits = glyphRow(glyph, row);
        for (uint8_t col = 0u; col < width; ++col) {
            const uint8_t bit = static_cast<uint8_t>(1u << (width - 1u - col));
            if ((bits & bit) == 0u) {
                continue;
            }

            const uint8_t mirroredCol = static_cast<uint8_t>(width - 1u - col);
            matrix.setPixel(
                static_cast<uint8_t>(originX + mirroredCol),
                static_cast<uint8_t>(originY + row),
                color
            );
        }
    }
}

void FireworksAnimation::renderBirthdayMessage(PixelMatrix& matrix) const {
    matrix.clear();

    // Logical order is intentionally reversed because physical matrix X is mirrored.
    drawBirthdayGlyph(matrix, 'O', 2u, 3u, birthdayColor(2u, colorPhase_));
    drawBirthdayGlyph(matrix, 'T', 7u, 3u, birthdayColor(1u, colorPhase_));
    drawBirthdayGlyph(matrix, 'S', 12u, 3u, birthdayColor(0u, colorPhase_));

    drawBirthdayGlyph(matrix, 'T', 2u, 9u, birthdayColor(5u, colorPhase_));
    drawBirthdayGlyph(matrix, 'A', 7u, 9u, birthdayColor(4u, colorPhase_));
    drawBirthdayGlyph(matrix, 'L', 12u, 9u, birthdayColor(3u, colorPhase_));

    for (uint8_t y = 0u; y < PixelMatrix::HEIGHT; ++y) {
        for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
            if ((x == 0u) || (x == PixelMatrix::WIDTH - 1u) || (y == 0u) || (y == PixelMatrix::HEIGHT - 1u)) {
                const bool shouldLight = (((x + y + borderPhase_) % 2u) == 0u);
                if (shouldLight) {
                    matrix.setPixel(x, y, kBirthdayBorderPink);
                }
            }
        }
    }
}

void FireworksAnimation::renderRocket(PixelMatrix& matrix) const {
    matrix.clear();

    const int16_t burstDeltaX = static_cast<int16_t>(current_.burstX) - static_cast<int16_t>(current_.launchX);
    const int16_t progressNumerator = static_cast<int16_t>(kRocketStartY) - static_cast<int16_t>(rocketY_);
    const int16_t denominator = static_cast<int16_t>(kRocketStartY) - static_cast<int16_t>(current_.burstY);

    int16_t rocketX = static_cast<int16_t>(current_.launchX);
    if (denominator > 0) {
        rocketX += (burstDeltaX * progressNumerator) / denominator;
    }

    const int16_t rocketY = static_cast<int16_t>(rocketY_);
    const int16_t trailY1 = rocketY + 1;
    const int16_t trailY2 = rocketY + 2;

    drawSpark(matrix, rocketX, rocketY, current_.primary);

    if (inside(rocketX, trailY1)) {
        matrix.setPixel(static_cast<uint8_t>(rocketX), static_cast<uint8_t>(trailY1), scaleColor(Colors::FireworkTrail, 150u));
    }

    if (inside(rocketX, trailY2)) {
        matrix.setPixel(static_cast<uint8_t>(rocketX), static_cast<uint8_t>(trailY2), scaleColor(Colors::FireworkTrail, 80u));
    }
}

void FireworksAnimation::renderBurst(PixelMatrix& matrix) const {
    matrix.clear();

    const int16_t cx = static_cast<int16_t>(current_.burstX);
    const int16_t cy = static_cast<int16_t>(current_.burstY);

    if (burstRadius_ == 0u) {
        drawSpark(matrix, cx, cy, Colors::FireworkWhite);
        return;
    }

    const int16_t primaryDirections[][2] = {
        {-1, 0}, {1, 0}, {0, -1}, {0, 1},
        {-1, -1}, {1, -1}, {-1, 1}, {1, 1}
    };

    const int16_t extraDirections[][2] = {
        {-2, -1}, {2, -1}, {-1, 2}, {1, 2}
    };

    const int16_t prevRadius = static_cast<int16_t>(burstRadius_) - 1;

    if (prevRadius >= 0) {
        for (uint8_t i = 0u; i < 8u; ++i) {
            drawBurstSpark(matrix, cx, cy, primaryDirections[i][0], primaryDirections[i][1], static_cast<uint8_t>(prevRadius), current_.primary, current_.secondary, 110u);
        }

        for (uint8_t i = 0u; i < 4u; ++i) {
            const int16_t px = cx + extraDirections[i][0] * prevRadius;
            const int16_t py = cy + extraDirections[i][1] * prevRadius;
            if (inside(px, py)) {
                matrix.setPixel(static_cast<uint8_t>(px), static_cast<uint8_t>(py), scaleColor(current_.secondary, 110u));
            }
        }
    }

    for (uint8_t i = 0u; i < 8u; ++i) {
        drawBurstSpark(matrix, cx, cy, primaryDirections[i][0], primaryDirections[i][1], burstRadius_, current_.primary, current_.secondary, 255u);
    }

    for (uint8_t i = 0u; i < 4u; ++i) {
        const int16_t px = cx + extraDirections[i][0] * static_cast<int16_t>(burstRadius_);
        const int16_t py = cy + extraDirections[i][1] * static_cast<int16_t>(burstRadius_);
        if (inside(px, py)) {
            matrix.setPixel(static_cast<uint8_t>(px), static_cast<uint8_t>(py), scaleColor(current_.secondary, 180u));
        }
    }

    drawSpark(matrix, cx, cy, Colors::FireworkWhite);
}

void FireworksAnimation::update(uint32_t now, PixelMatrix& matrix) {
    if (finished_) {
        return;
    }

    if (stage_ == FireworkStage::ROCKET) {
        const uint32_t elapsed = static_cast<uint32_t>(now - stageStartedAt_);

        if (elapsed < kRocketStepMs) {
            renderRocket(matrix);
            return;
        }

        if (rocketY_ == current_.burstY) {
            stage_ = FireworkStage::BURST;
            burstRadius_ = 0u;
            stageStartedAt_ = now;
            renderBurst(matrix);
            return;
        }

        if (rocketY_ > current_.burstY) {
            --rocketY_;
        }

        stageStartedAt_ = now;
        renderRocket(matrix);
        return;
    }

    if (stage_ == FireworkStage::BURST) {
        const uint32_t elapsed = static_cast<uint32_t>(now - stageStartedAt_);

        if (elapsed < kBurstStepMs) {
            renderBurst(matrix);
            return;
        }

        if (burstRadius_ >= current_.maxRadius) {
            stage_ = FireworkStage::FADE;
            fadeStep_ = 0u;
            stageStartedAt_ = now;
            matrix.clear();
            return;
        }

        ++burstRadius_;
        stageStartedAt_ = now;
        renderBurst(matrix);
        return;
    }

    if (stage_ == FireworkStage::FADE) {
        const uint32_t elapsed = static_cast<uint32_t>(now - stageStartedAt_);
        if (elapsed < kFadeStepMs) {
            return;
        }

        const uint8_t fadeScales[5] = {180u, 130u, 90u, 50u, 20u};
        if (fadeStep_ < 5u) {
            matrix.clear();
            drawFadeBurst(matrix, current_.burstX, current_.burstY, current_.maxRadius, current_.primary, current_.secondary, fadeScales[fadeStep_]);
            stageStartedAt_ = now;
            ++fadeStep_;

            if (fadeStep_ >= 5u) {
                matrix.clear();
                stage_ = FireworkStage::PAUSE;
            }
            return;
        }

        matrix.clear();
        stage_ = FireworkStage::PAUSE;
        stageStartedAt_ = now;
        return;
    }

    if (stage_ == FireworkStage::PAUSE) {
        const uint32_t elapsed = static_cast<uint32_t>(now - stageStartedAt_);
        if (elapsed < kPauseMs) {
            matrix.clear();
            return;
        }

        if (fireworkIndex_ + 1u < kFireworkCount) {
            ++fireworkIndex_;
            loadFirework(fireworkIndex_);
            rocketY_ = kRocketStartY;
            stage_ = FireworkStage::ROCKET;
            stageStartedAt_ = now;
            matrix.clear();
            return;
        }

        stage_ = FireworkStage::BIRTHDAY_MESSAGE;
        birthdayStartedAt_ = now;
        lastBorderToggleAt_ = now;
        lastColorChangeAt_ = now;
        borderPhase_ = 0u;
        colorPhase_ = 0u;
        renderBirthdayMessage(matrix);
        return;
    }

    if (stage_ == FireworkStage::BIRTHDAY_MESSAGE) {
        const uint32_t birthdayElapsed = static_cast<uint32_t>(now - birthdayStartedAt_);
        if (birthdayElapsed >= kBirthdayDurationMs) {
            renderBirthdayMessage(matrix);
            stage_ = FireworkStage::FINISHED;
            finished_ = true;
            return;
        }

        bool birthdayFrameChanged = false;

        const uint32_t borderElapsed = static_cast<uint32_t>(now - lastBorderToggleAt_);
        if (borderElapsed >= kBorderToggleMs) {
            borderPhase_ ^= 1u;
            lastBorderToggleAt_ = now;
            birthdayFrameChanged = true;
        }

        const uint32_t colorElapsed = static_cast<uint32_t>(now - lastColorChangeAt_);
        if (colorElapsed >= kLetterColorChangeMs) {
            colorPhase_ = static_cast<uint8_t>((colorPhase_ + 1u) % 10u);
            lastColorChangeAt_ = now;
            birthdayFrameChanged = true;
        }

        if (birthdayFrameChanged) {
            renderBirthdayMessage(matrix);
        }

        return;
    }
}

bool FireworksAnimation::isFinished() const {
    return finished_;
}

void FireworksAnimation::stop() {
    stageStartedAt_ = 0u;
    birthdayStartedAt_ = 0u;
    lastBorderToggleAt_ = 0u;
    lastColorChangeAt_ = 0u;
    fireworkIndex_ = 0u;
    rocketY_ = kRocketStartY;
    burstRadius_ = 0u;
    fadeStep_ = 0u;
    borderPhase_ = 0u;
    colorPhase_ = 0u;
    stage_ = FireworkStage::FINISHED;
    finished_ = true;
}
