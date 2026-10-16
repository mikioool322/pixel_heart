#include "animations/MeadowFlowerAnimation.h"

#include "assets/PixelMask.h"
#include "core/Color.h"
#include "core/PixelMatrix.h"

#if defined(ARDUINO_ARCH_AVR)
#include <avr/pgmspace.h>
#endif

namespace {

#if defined(ARDUINO_ARCH_AVR)
#define MEADOW_MASK_STORAGE PROGMEM
#else
#define MEADOW_MASK_STORAGE
#endif

// ---------------------------------------------------------------------------
// Palette. The sky and the grass are the large, low-brightness areas; the flower
// is the only strongly saturated thing on the panel, which is what survives the
// diffuser. Nothing is pure white.
// ---------------------------------------------------------------------------
constexpr Color kSkyTop{15u, 85u, 145u};
constexpr Color kSkyLow{20u, 105u, 170u};    // lighter towards the meadow
constexpr Color kGrassDark{20u, 90u, 30u};
constexpr Color kGrassBright{55u, 165u, 45u};

constexpr Color kStem{45u, 155u, 40u};
constexpr Color kLeafDark{25u, 115u, 35u};
constexpr Color kLeafBright{75u, 185u, 55u};

constexpr Color kPetalRed{225u, 55u, 45u};
constexpr Color kPetalOrange{240u, 95u, 35u};
constexpr Color kCentreYellow{250u, 190u, 35u};
constexpr Color kCentreHighlight{255u, 220u, 80u};

constexpr Color kSmallFlower{190u, 200u, 190u};

// ---------------------------------------------------------------------------
// Scene bands. The background never animates.
// ---------------------------------------------------------------------------
constexpr uint8_t kSkyLowFirstRow = 6u;    // rows 6..12 lighter
constexpr uint8_t kMeadowFirstRow = 13u;   // rows 13..15

// ---------------------------------------------------------------------------
// Flower geometry (logical x=0 -> bit 15, matching PixelMask.h).
//
//   head       : rows 0..8,  x 2..11  (10 x 9, the hero)
//   upper stem : rows 9..11, x 7..8
//   lower stem : rows 12..15, x 7..8  (anchored, never moves)
//   leaves     : rows 9..11, flanking the stem
// ---------------------------------------------------------------------------
static const PixelMask16 kHeadMask MEADOW_MASK_STORAGE = {{
    0x0780, 0x0FC0, 0x1FE0, 0x3FF0,
    0x3FF0, 0x3FF0, 0x1FE0, 0x0FC0,
    0x0780, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
}};

static const PixelMask16 kHeadInnerMask MEADOW_MASK_STORAGE = {{
    0x0000, 0x0780, 0x0FC0, 0x1FE0,
    0x1FE0, 0x1FE0, 0x0FC0, 0x0780,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
}};

static const PixelMask16 kHeadCentreMask MEADOW_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x03C0,
    0x03C0, 0x03C0, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
}};

static const PixelMask16 kHeadHiliteMask MEADOW_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0180,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
}};

static const PixelMask16 kStemUpperMask MEADOW_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0180, 0x0180, 0x0180,
    0x0000, 0x0000, 0x0000, 0x0000
}};

static const PixelMask16 kStemLowerMask MEADOW_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0180, 0x0180, 0x0180, 0x0180
}};

// Leaves: bright upper rows, a darker lower edge for depth.
static const PixelMask16 kLeafMask MEADOW_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0660, 0x0E70, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
}};

static const PixelMask16 kLeafEdgeMask MEADOW_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x1C38,
    0x0000, 0x0000, 0x0000, 0x0000
}};

#undef MEADOW_MASK_STORAGE

// A few static distant flowers in the meadow.
constexpr uint8_t kSmallFlowerX[4] = { 2u, 13u, 4u, 11u };
constexpr uint8_t kSmallFlowerY[4] = { 14u, 14u, 15u, 15u };
constexpr uint8_t kSmallFlowerCount =
    static_cast<uint8_t>(sizeof(kSmallFlowerX) / sizeof(kSmallFlowerX[0]));

// ---------------------------------------------------------------------------
// The breeze.
//
// Poses: 0 = CENTER, 1 = RIGHT_1, 2 = RIGHT_2, 3 = LEFT_1, 4 = LEFT_2.
//
// CENTER -> RIGHT_1 -> RIGHT_2 -> RIGHT_1 -> CENTER
//        -> LEFT_1  -> LEFT_2  -> LEFT_1  -> CENTER (loop)
//
// The sequence returns through CENTER instead of going from one extreme straight
// to the other, so no adjacent step ever moves a feature by more than one pixel
// - including the wrap, which is LEFT_1 -> CENTER.
// ---------------------------------------------------------------------------
constexpr uint8_t kPoseSequence[MeadowFlowerAnimation::kSequenceLength] = {
    0u, 1u, 2u, 1u, 0u, 3u, 4u, 3u
};

// The head leans twice as far as the stem, and the leaves follow the stem.
constexpr int8_t kHeadShift[MeadowFlowerAnimation::kPoseCount] = { 0, 1, 2, -1, -2 };
constexpr int8_t kStemShift[MeadowFlowerAnimation::kPoseCount] = { 0, 1, 1, -1, -1 };
constexpr int8_t kLeafShift[MeadowFlowerAnimation::kPoseCount] = { 0, 1, 1, -1, -1 };

static_assert(
    sizeof(kPoseSequence) / sizeof(kPoseSequence[0]) == MeadowFlowerAnimation::kSequenceLength,
    "Meadow flower sequence length must match the declared step count");

}  // namespace

MeadowFlowerAnimation::MeadowFlowerAnimation()
    : startedAt_(0u),
      lastPose_(0xFFu),
      finished_(false) {
}

void MeadowFlowerAnimation::start(uint32_t now) {
    startedAt_ = now;
    // 0xFF is not a valid pose, so the first update renders pose 0 immediately.
    lastPose_ = 0xFFu;
    finished_ = false;
}

void MeadowFlowerAnimation::update(uint32_t now, PixelMatrix& matrix) {
    if (finished_) {
        return;
    }

    // Derived from elapsed time rather than accumulated per frame, so the pose
    // never drifts and t=0 and t=loopDuration are the same frame by definition.
    const uint32_t elapsed = static_cast<uint32_t>(now - startedAt_);
    if (elapsed >= kLoopDurationMs) {
        finished_ = true;
        return;
    }

    const uint8_t step = static_cast<uint8_t>((elapsed / kFrameIntervalMs) % kSequenceLength);
    const uint8_t pose = poseAtStep(step);

    // Only repaint when the pose actually changes, so an idle tick does not
    // dirty the framebuffer.
    if (pose == lastPose_) {
        return;
    }

    lastPose_ = pose;
    renderFrame(matrix, pose);
}

bool MeadowFlowerAnimation::isFinished() const {
    return finished_;
}

void MeadowFlowerAnimation::stop() {
    startedAt_ = 0u;
    lastPose_ = 0xFFu;
    finished_ = true;
}

void MeadowFlowerAnimation::renderFrame(PixelMatrix& matrix, uint8_t pose) const {
    matrix.clear();
    drawBackground(matrix);
    drawFlower(matrix, pose);
}

void MeadowFlowerAnimation::drawBackground(PixelMatrix& matrix) const {
    for (uint8_t y = 0u; y < kMeadowFirstRow; ++y) {
        const Color sky = (y >= kSkyLowFirstRow) ? kSkyLow : kSkyTop;
        for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
            matrix.setPixel(x, y, sky);
        }
    }

    for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
        matrix.setPixel(x, kMeadowFirstRow, kGrassDark);
        matrix.setPixel(x, static_cast<uint8_t>(kMeadowFirstRow + 1u), kGrassBright);
        matrix.setPixel(x, static_cast<uint8_t>(kMeadowFirstRow + 2u), kGrassDark);
    }

    for (uint8_t i = 0u; i < kSmallFlowerCount; ++i) {
        matrix.setPixel(kSmallFlowerX[i], kSmallFlowerY[i], kSmallFlower);
    }
}

void MeadowFlowerAnimation::drawFlower(PixelMatrix& matrix, uint8_t pose) const {
    const int8_t headShift = headShiftAt(pose);
    const int8_t stemShift = stemShiftAt(pose);
    const int8_t leafShift = leafShiftAt(pose);

    // Anchored base first: always at offset zero, so the flower cannot slide.
    drawMask(&kStemLowerMask, 0, 0, kStem, matrix);

    // Leaves and the wind-following half of the stem share one offset, which
    // keeps them visually attached to each other.
    drawMask(&kLeafEdgeMask, leafShift, 0, kLeafDark, matrix);
    drawMask(&kLeafMask, leafShift, 0, kLeafBright, matrix);
    drawMask(&kStemUpperMask, stemShift, 0, kStem, matrix);

    // The head leans furthest, and is drawn last so it stays on top.
    drawMask(&kHeadMask, headShift, 0, kPetalRed, matrix);
    drawMask(&kHeadInnerMask, headShift, 0, kPetalOrange, matrix);
    drawMask(&kHeadCentreMask, headShift, 0, kCentreYellow, matrix);
    drawMask(&kHeadHiliteMask, headShift, 0, kCentreHighlight, matrix);
}

// ---------------------------------------------------------------------------
// Derived scene queries
// ---------------------------------------------------------------------------

uint8_t MeadowFlowerAnimation::poseAtStep(uint8_t step) {
    return kPoseSequence[step % kSequenceLength];
}

int8_t MeadowFlowerAnimation::headShiftAt(uint8_t pose) {
    return kHeadShift[pose % kPoseCount];
}

int8_t MeadowFlowerAnimation::stemShiftAt(uint8_t pose) {
    return kStemShift[pose % kPoseCount];
}

int8_t MeadowFlowerAnimation::leafShiftAt(uint8_t pose) {
    return kLeafShift[pose % kPoseCount];
}

bool MeadowFlowerAnimation::headContains(uint8_t x, uint8_t y) {
    return maskHasPixel(&kHeadMask, x, y);
}

bool MeadowFlowerAnimation::stemLowerContains(uint8_t x, uint8_t y) {
    return maskHasPixel(&kStemLowerMask, x, y);
}

bool MeadowFlowerAnimation::stemUpperContains(uint8_t x, uint8_t y) {
    return maskHasPixel(&kStemUpperMask, x, y);
}

bool MeadowFlowerAnimation::leafContains(uint8_t x, uint8_t y) {
    return maskHasPixel(&kLeafMask, x, y) || maskHasPixel(&kLeafEdgeMask, x, y);
}

bool MeadowFlowerAnimation::meadowContains(uint8_t x, uint8_t y) {
    (void)x;
    return y >= kMeadowFirstRow;
}

bool MeadowFlowerAnimation::skyContains(uint8_t x, uint8_t y) {
    (void)x;
    return y < kMeadowFirstRow;
}

uint8_t MeadowFlowerAnimation::smallFlowerCount() {
    return kSmallFlowerCount;
}

uint8_t MeadowFlowerAnimation::smallFlowerX(uint8_t index) {
    return kSmallFlowerX[index % kSmallFlowerCount];
}

uint8_t MeadowFlowerAnimation::smallFlowerY(uint8_t index) {
    return kSmallFlowerY[index % kSmallFlowerCount];
}

uint8_t MeadowFlowerAnimation::sequenceLength() {
    return kSequenceLength;
}

uint32_t MeadowFlowerAnimation::loopDurationMs() {
    return kLoopDurationMs;
}
