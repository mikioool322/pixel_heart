#include "animations/RoseBloomAnimation.h"

#include "assets/PixelMask.h"
#include "core/Color.h"
#include "core/PixelMatrix.h"

namespace {

#if defined(ARDUINO_ARCH_AVR)
#define ROSE_MASK_STORAGE PROGMEM
#else
#define ROSE_MASK_STORAGE
#endif

// -----------------------------------------------------------------------------
// FINAL ABSOLUTE 16x16 ASSETS
//
// IMPORTANT:
// These masks are already positioned in final matrix coordinates.
// Always render them with xOffset=0, yOffset=0.
// -----------------------------------------------------------------------------

static const PixelMask16 kLeftLeafMask ROSE_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1c00, 0x0e00,
    0x0700, 0x0000, 0x0000, 0x0000
}};

static const PixelMask16 kRightLeafMask ROSE_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0038, 0x0070,
    0x0060, 0x0000, 0x0000, 0x0000
}};

static const PixelMask16 kSepalMask ROSE_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x03c0, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
}};

// Closed bud.
static const PixelMask16 kBudSmallMask ROSE_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0180, 0x03c0, 0x03c0,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
}};

// Intermediate opening silhouette.
static const PixelMask16 kBudOpeningMask ROSE_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0180,
    0x07e0, 0x0ff0, 0x07e0, 0x03c0,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
}};

static const PixelMask16 kBudInnerMask ROSE_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0180, 0x03c0, 0x0180, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
}};

// Full flower silhouette.
// First render this in RoseShadow.
static const PixelMask16 kFlowerBaseMask ROSE_MASK_STORAGE = {{
    0x0000,
    0x07c0,
    0x1fe0,
    0x3ff0,
    0x3ff8,
    0x3ff8,
    0x1ff0,
    0x0fe0,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};

// Main red petal regions.
static const PixelMask16 kMainPetalMask ROSE_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0780,
    0x18e0,
    0x1070,
    0x1830,
    0x0c60,
    0x07c0,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};

// Pink petal highlights.
// This deliberately forms irregular internal curves instead of a filled center.
static const PixelMask16 kHighlightPetalMask ROSE_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0000,
    0x0700,
    0x0880,
    0x0380,
    0x0280,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};

// Dark internal spiral/core.
static const PixelMask16 kCorePetalMask ROSE_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0500,
    0x0440,
    0x0100,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};

#undef ROSE_MASK_STORAGE

constexpr uint8_t kStemSteps = 8u; // Number of steps for the stem to fully grow
constexpr uint8_t kBreathFrames = 48u; // Total number of frames for the breathing animation
constexpr uint8_t kBreathCycleFrames = 24u; // Number of frames per breathing cycle

uint8_t breathScaleForStep(uint8_t step) {
    const uint8_t phase = step % kBreathCycleFrames;

    if (phase < 12u) {
        return static_cast<uint8_t>(195u + phase * 5u);
    }

    return static_cast<uint8_t>(255u - (phase - 12u) * 5u);
}

}  // namespace

RoseBloomAnimation::RoseBloomAnimation()
    : stageStartedAt_(0u),
      stemGrowthStep_(0u),
      breathStep_(0u),
      stage_(RoseStage::STEM_GROW),
      finished_(false) {
}

uint32_t RoseBloomAnimation::stageInterval(RoseStage stage) {
    switch (stage) {
        case RoseStage::STEM_GROW:
            return 110u;

        case RoseStage::LEAVES:
            return 350u;

        case RoseStage::BUD_SMALL:
            return 400u;

        case RoseStage::BUD_OPENING:
            return 450u;

        case RoseStage::BLOOM_OUTER:
            return 500u;

        case RoseStage::BLOOM_FULL:
            return 700u;

        case RoseStage::BREATHING:
            return 60u;

        case RoseStage::FINISHED:
        default:
            return 0u;
    }
}

void RoseBloomAnimation::advanceStage() {
    switch (stage_) {
        case RoseStage::STEM_GROW:
            if (stemGrowthStep_ < kStemSteps) {
                ++stemGrowthStep_;
            }

            if (stemGrowthStep_ >= kStemSteps) {
                stage_ = RoseStage::LEAVES;
            }
            break;

        case RoseStage::LEAVES:
            stage_ = RoseStage::BUD_SMALL;
            break;

        case RoseStage::BUD_SMALL:
            stage_ = RoseStage::BUD_OPENING;
            break;

        case RoseStage::BUD_OPENING:
            stage_ = RoseStage::BLOOM_OUTER;
            break;

        case RoseStage::BLOOM_OUTER:
            stage_ = RoseStage::BLOOM_FULL;
            break;

        case RoseStage::BLOOM_FULL:
            breathStep_ = 0u;
            stage_ = RoseStage::BREATHING;
            break;

        case RoseStage::BREATHING:
            break;

        case RoseStage::FINISHED:
        default:
            finished_ = true;
            break;
    }
}

void RoseBloomAnimation::start(uint32_t now) {
    stageStartedAt_ = now;
    stemGrowthStep_ = 0u;
    breathStep_ = 0u;
    stage_ = RoseStage::STEM_GROW;
    finished_ = false;
}

void RoseBloomAnimation::renderStem(
    PixelMatrix& matrix,
    uint8_t stemHeight,
    Color baseColor
) const {
    if (stemHeight == 0u) {
        return;
    }

    if (stemHeight > kStemSteps) {
        stemHeight = kStemSteps;
    }

    for (uint8_t step = 0u; step < stemHeight; ++step) {
        const uint8_t y = static_cast<uint8_t>(15u - step);

        uint8_t x = 7u;

        if (y == 11u || y == 12u) {
            x = 8u;
        }

        matrix.setPixel(x, y, baseColor);
    }
}

void RoseBloomAnimation::renderStage(PixelMatrix& matrix) {
    matrix.clear();

    switch (stage_) {
        case RoseStage::STEM_GROW:
            renderStem(
                matrix,
                stemGrowthStep_,
                Colors::StemGreen
            );
            break;

        case RoseStage::LEAVES:
            renderStem(
                matrix,
                kStemSteps,
                Colors::StemGreen
            );

            drawMask(
                &kLeftLeafMask,
                0,
                0,
                Colors::LeafGreen,
                matrix
            );

            drawMask(
                &kRightLeafMask,
                0,
                0,
                Colors::LeafGreen,
                matrix
            );
            break;

        case RoseStage::BUD_SMALL:
            renderStem(
                matrix,
                kStemSteps,
                Colors::StemGreen
            );

            drawMask(
                &kLeftLeafMask,
                0,
                0,
                Colors::LeafGreen,
                matrix
            );

            drawMask(
                &kRightLeafMask,
                0,
                0,
                Colors::LeafGreen,
                matrix
            );

            drawMask(
                &kSepalMask,
                0,
                0,
                Colors::StemGreen,
                matrix
            );

            drawMask(
                &kBudSmallMask,
                0,
                0,
                Colors::RoseShadow,
                matrix
            );

            matrix.setPixel(8u, 5u, Colors::RosePink);
            matrix.setPixel(7u, 6u, Colors::RoseRed);
            break;

        case RoseStage::BUD_OPENING:
            renderStem(
                matrix,
                kStemSteps,
                Colors::StemGreen
            );

            drawMask(
                &kLeftLeafMask,
                0,
                0,
                Colors::LeafGreen,
                matrix
            );

            drawMask(
                &kRightLeafMask,
                0,
                0,
                Colors::LeafGreen,
                matrix
            );

            drawMask(
                &kSepalMask,
                0,
                0,
                Colors::StemGreen,
                matrix
            );

            drawMask(
                &kBudOpeningMask,
                0,
                0,
                Colors::RoseShadow,
                matrix
            );

            drawMask(
                &kBudInnerMask,
                0,
                0,
                Colors::RoseRed,
                matrix
            );

            matrix.setPixel(7u, 4u, Colors::RosePink);
            matrix.setPixel(8u, 5u, Colors::RoseBlush);
            break;

        case RoseStage::BLOOM_OUTER:
            renderStem(
                matrix,
                kStemSteps,
                Colors::StemGreen
            );

            drawMask(
                &kLeftLeafMask,
                0,
                0,
                Colors::LeafGreen,
                matrix
            );

            drawMask(
                &kRightLeafMask,
                0,
                0,
                Colors::LeafGreen,
                matrix
            );

            drawMask(
                &kSepalMask,
                0,
                0,
                Colors::StemGreen,
                matrix
            );

            drawMask(
                &kFlowerBaseMask,
                0,
                0,
                Colors::RoseShadow,
                matrix
            );

            drawMask(
                &kMainPetalMask,
                0,
                0,
                Colors::RoseRed,
                matrix
            );
            break;

        case RoseStage::BLOOM_FULL:
            renderStem(
                matrix,
                kStemSteps,
                Colors::StemGreen
            );

            drawMask(
                &kLeftLeafMask,
                0,
                0,
                Colors::LeafGreen,
                matrix
            );

            drawMask(
                &kRightLeafMask,
                0,
                0,
                Colors::LeafGreen,
                matrix
            );

            drawMask(
                &kSepalMask,
                0,
                0,
                Colors::StemGreen,
                matrix
            );

            drawMask(
                &kFlowerBaseMask,
                0,
                0,
                Colors::RoseShadow,
                matrix
            );

            drawMask(
                &kMainPetalMask,
                0,
                0,
                Colors::RoseRed,
                matrix
            );

            drawMask(
                &kHighlightPetalMask,
                0,
                0,
                Colors::RosePink,
                matrix
            );

            drawMask(
                &kCorePetalMask,
                0,
                0,
                Colors::RoseCore,
                matrix
            );

            matrix.setPixel(6u, 3u, Colors::RoseBlush);
            matrix.setPixel(8u, 5u, Colors::RoseBlush);
            break;

        case RoseStage::BREATHING: {
            renderStem(
                matrix,
                kStemSteps,
                Colors::StemGreen
            );

            drawMask(
                &kLeftLeafMask,
                0,
                0,
                Colors::LeafGreen,
                matrix
            );

            drawMask(
                &kRightLeafMask,
                0,
                0,
                Colors::LeafGreen,
                matrix
            );

            drawMask(
                &kSepalMask,
                0,
                0,
                Colors::StemGreen,
                matrix
            );

            const uint8_t pulse =
                breathScaleForStep(breathStep_);

            const Color shadow =
                scaleColor(Colors::RoseShadow, pulse);

            const Color red =
                scaleColor(Colors::RoseRed, pulse);

            const Color pink =
                scaleColor(Colors::RosePink, pulse);

            const Color core =
                scaleColor(Colors::RoseCore, pulse);

            const Color blush =
                scaleColor(Colors::RoseBlush, pulse);

            drawMask(
                &kFlowerBaseMask,
                0,
                0,
                shadow,
                matrix
            );

            drawMask(
                &kMainPetalMask,
                0,
                0,
                red,
                matrix
            );

            drawMask(
                &kHighlightPetalMask,
                0,
                0,
                pink,
                matrix
            );

            drawMask(
                &kCorePetalMask,
                0,
                0,
                core,
                matrix
            );

            matrix.setPixel(6u, 3u, blush);
            matrix.setPixel(8u, 5u, blush);
            break;
        }

        case RoseStage::FINISHED:
            renderStem(
                matrix,
                kStemSteps,
                Colors::StemGreen
            );

            drawMask(
                &kLeftLeafMask,
                0,
                0,
                Colors::LeafGreen,
                matrix
            );

            drawMask(
                &kRightLeafMask,
                0,
                0,
                Colors::LeafGreen,
                matrix
            );

            drawMask(
                &kSepalMask,
                0,
                0,
                Colors::StemGreen,
                matrix
            );

            drawMask(
                &kFlowerBaseMask,
                0,
                0,
                Colors::RoseShadow,
                matrix
            );

            drawMask(
                &kMainPetalMask,
                0,
                0,
                Colors::RoseRed,
                matrix
            );

            drawMask(
                &kHighlightPetalMask,
                0,
                0,
                Colors::RosePink,
                matrix
            );

            drawMask(
                &kCorePetalMask,
                0,
                0,
                Colors::RoseCore,
                matrix
            );

            matrix.setPixel(6u, 3u, Colors::RoseBlush);
            matrix.setPixel(8u, 5u, Colors::RoseBlush);
            break;

        default:
            break;
    }
}

void RoseBloomAnimation::update(
    uint32_t now,
    PixelMatrix& matrix
) {
    if (finished_) {
        return;
    }

    const uint32_t elapsed =
        static_cast<uint32_t>(now - stageStartedAt_);

    const uint32_t interval =
        stageInterval(stage_);

    if (elapsed < interval) {
        return;
    }

    if (stage_ == RoseStage::BREATHING) {
        ++breathStep_;
        stageStartedAt_ = now;

        if (breathStep_ >= kBreathFrames) {
            finished_ = true;
            stage_ = RoseStage::FINISHED;

            renderStage(matrix);
            return;
        }

        renderStage(matrix);
        return;
    }

    advanceStage();
    stageStartedAt_ = now;

    renderStage(matrix);
}

bool RoseBloomAnimation::isFinished() const {
    return finished_;
}

void RoseBloomAnimation::stop() {
    stageStartedAt_ = 0u;
    stemGrowthStep_ = 0u;
    breathStep_ = 0u;

    stage_ = RoseStage::FINISHED;
    finished_ = true;
}
