#include "animations/HeartbeatAnimation.h"

#include "assets/HeartAssets.h"
#include "core/Color.h"
#include "core/PixelMatrix.h"

namespace {

constexpr uint8_t kLastMatrixRow = 15u; // Index of the last row in the 16x16 matrix

constexpr uint8_t kMaxFillRadius = 13u; // Maximum radius for the heart fill expansion

constexpr uint8_t kBeatFrameCount = 10u; // Number of frames per beat cycle
constexpr uint8_t kBeatCycleCount = 6u; // Number of beat cycles per animation loop

constexpr uint16_t kOutlineStepMs = 90u; // Duration of each outline drawing step in milliseconds
constexpr uint16_t kFillStepMs = 85u; // Duration of each fill step in milliseconds
constexpr uint16_t kFullHoldMs = 400u; // Duration to hold the full heart before the next beat


bool heartContains(int16_t x, int16_t y) {
    return HeartAssets::contains(x, y);
}


bool isHeartOutlinePixel(uint8_t x, uint8_t y) {
    return HeartAssets::isOutlinePixel(x, y);
}


uint8_t absDiff(uint8_t a, uint8_t b) {
    return (a >= b)
        ? static_cast<uint8_t>(a - b)
        : static_cast<uint8_t>(b - a);
}


uint8_t fillDistance(uint8_t x, uint8_t y) {
    // Use the two central pixels of an even-width 16x16 matrix
    // as the fill origin.
    //
    // This produces a diamond-like radial expansion clipped by
    // the heart silhouette.
    //
    // Fill grows naturally from the visual center toward the contour.

    const uint8_t distanceFromLeftCenter =
        static_cast<uint8_t>(
            absDiff(x, 7u) +
            absDiff(y, 7u)
        );

    const uint8_t distanceFromRightCenter =
        static_cast<uint8_t>(
            absDiff(x, 8u) +
            absDiff(y, 7u)
        );

    return
        (distanceFromLeftCenter < distanceFromRightCenter)
        ? distanceFromLeftCenter
        : distanceFromRightCenter;
}

}  // namespace


HeartbeatAnimation::HeartbeatAnimation()
    : stageStartedAt_(0u),
      outlineRow_(0u),
      fillRadius_(0u),
      beatFrame_(0u),
      beatCycle_(0u),
      stage_(HeartStage::OUTLINE_DRAW),
      finished_(false) {
}


void HeartbeatAnimation::start(uint32_t now) {
    outlineRow_ = 0u;
    fillRadius_ = 0u;

    beatFrame_ = 0u;
    beatCycle_ = 0u;

    stage_ = HeartStage::OUTLINE_DRAW;
    finished_ = false;

    // Force first update() to draw row 0 immediately.
    //
    // This is important for LOOP:
    // the previous full-heart frame must disappear immediately when
    // the animation restarts.
    stageStartedAt_ =
        static_cast<uint32_t>(now - kOutlineStepMs);
}


void HeartbeatAnimation::renderOutline(
    PixelMatrix& matrix,
    uint8_t maxRow,
    uint8_t scale
) const {
    matrix.clear();

    if (maxRow > kLastMatrixRow) {
        maxRow = kLastMatrixRow;
    }

    const Color outline =
        scaleColor(Colors::HeartOutline, scale);

    for (uint8_t y = 0u; y <= maxRow; ++y) {
        for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
            if (!isHeartOutlinePixel(x, y)) {
                continue;
            }

            matrix.setPixel(
                x,
                y,
                outline
            );
        }
    }
}


void HeartbeatAnimation::renderFilledHeart(
    PixelMatrix& matrix,
    uint8_t radius,
    uint8_t scale
) const {
    matrix.clear();

    if (radius > kMaxFillRadius) {
        radius = kMaxFillRadius;
    }

    const Color fill =
        scaleColor(Colors::HeartFill, scale);

    const Color core =
        scaleColor(Colors::HeartCore, scale);

    const Color outline =
        scaleColor(Colors::HeartOutline, scale);


    // -----------------------------------------------------------------
    // 1. Fill heart from center outward
    // -----------------------------------------------------------------

    for (uint8_t y = 0u; y < PixelMatrix::HEIGHT; ++y) {
        for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
            if (!heartContains(x, y)) {
                continue;
            }

            const uint8_t distance =
                fillDistance(x, y);

            if (distance > radius) {
                continue;
            }

            // Keep the very center slightly brighter.
            // It helps visually communicate that the fill originates
            // from inside the heart.

            const Color pixelColor =
                (distance <= 2u)
                ? core
                : fill;

            matrix.setPixel(
                x,
                y,
                pixelColor
            );
        }
    }


    // -----------------------------------------------------------------
    // 2. Draw the complete contour over the fill
    // -----------------------------------------------------------------

    for (uint8_t y = 0u; y < PixelMatrix::HEIGHT; ++y) {
        for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
            if (!isHeartOutlinePixel(x, y)) {
                continue;
            }

            matrix.setPixel(
                x,
                y,
                outline
            );
        }
    }
}


uint8_t HeartbeatAnimation::beatScale(uint8_t frame) {
    // One complete heartbeat cycle:
    //
    // baseline
    //   -> strong BUM
    //   -> fall
    //   -> smaller bum
    //   -> long pause
    //
    // Do not replace with sine/floating point logic.

    switch (frame) {
        case 0u:
            return 130u;  // resting

        case 1u:
            return 185u;

        case 2u:
            return 235u;

        case 3u:
            return 255u;  // first strong beat

        case 4u:
            return 185u;

        case 5u:
            return 130u;  // short valley

        case 6u:
            return 175u;

        case 7u:
            return 220u;  // second smaller beat

        case 8u:
            return 165u;

        case 9u:
        default:
            return 130u;  // resting pause
    }
}


uint16_t HeartbeatAnimation::beatDuration(uint8_t frame) {
    // Duration belonging to each scale above.

    switch (frame) {
        case 0u:
            return 170u;

        case 1u:
            return 45u;

        case 2u:
            return 45u;

        case 3u:
            return 70u;

        case 4u:
            return 85u;

        case 5u:
            return 110u;

        case 6u:
            return 45u;

        case 7u:
            return 60u;

        case 8u:
            return 100u;

        case 9u:
        default:
            return 340u;
    }
}


void HeartbeatAnimation::update(
    uint32_t now,
    PixelMatrix& matrix
) {
    if (finished_) {
        return;
    }

    const uint32_t elapsed =
        static_cast<uint32_t>(
            now - stageStartedAt_
        );


    // ================================================================
    // OUTLINE
    // ================================================================

    if (stage_ == HeartStage::OUTLINE_DRAW) {
        if (elapsed < kOutlineStepMs) {
            return;
        }

        renderOutline(
            matrix,
            outlineRow_,
            255u
        );

        stageStartedAt_ = now;

        if (outlineRow_ >= kLastMatrixRow) {
            stage_ = HeartStage::FILL_EXPAND;
            fillRadius_ = 0u;
        } else {
            ++outlineRow_;
        }

        return;
    }


    // ================================================================
    // CENTER -> OUTSIDE FILL
    // ================================================================

    if (stage_ == HeartStage::FILL_EXPAND) {
        if (elapsed < kFillStepMs) {
            return;
        }

        renderFilledHeart(
            matrix,
            fillRadius_,
            255u
        );

        stageStartedAt_ = now;

        if (fillRadius_ >= kMaxFillRadius) {
            stage_ = HeartStage::FULL_HOLD;
        } else {
            ++fillRadius_;
        }

        return;
    }


    // ================================================================
    // FULL HEART HOLD
    // ================================================================

    if (stage_ == HeartStage::FULL_HOLD) {
        if (elapsed < kFullHoldMs) {
            return;
        }

        stage_ = HeartStage::BEATING;

        beatFrame_ = 0u;
        beatCycle_ = 0u;

        stageStartedAt_ = now;

        renderFilledHeart(
            matrix,
            kMaxFillRadius,
            beatScale(beatFrame_)
        );

        return;
    }


    // ================================================================
    // DOUBLE HEARTBEAT
    // ================================================================

    if (stage_ == HeartStage::BEATING) {
        if (
            elapsed <
            beatDuration(beatFrame_)
        ) {
            return;
        }

        ++beatFrame_;

        if (beatFrame_ >= kBeatFrameCount) {
            beatFrame_ = 0u;
            ++beatCycle_;

            if (beatCycle_ >= kBeatCycleCount) {
                stage_ = HeartStage::FINISHED;
                finished_ = true;

                // Always finish with a full-bright, complete heart.
                renderFilledHeart(
                    matrix,
                    kMaxFillRadius,
                    255u
                );

                return;
            }
        }

        stageStartedAt_ = now;

        renderFilledHeart(
            matrix,
            kMaxFillRadius,
            beatScale(beatFrame_)
        );

        return;
    }


    // ================================================================
    // FINISHED
    // ================================================================

    if (stage_ == HeartStage::FINISHED) {
        finished_ = true;
    }
}


bool HeartbeatAnimation::isFinished() const {
    return finished_;
}


void HeartbeatAnimation::stop() {
    stageStartedAt_ = 0u;

    outlineRow_ = 0u;
    fillRadius_ = 0u;

    beatFrame_ = 0u;
    beatCycle_ = 0u;

    stage_ = HeartStage::FINISHED;
    finished_ = true;
}
