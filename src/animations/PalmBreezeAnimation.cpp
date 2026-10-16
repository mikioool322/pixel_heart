#include "animations/PalmBreezeAnimation.h"

#include "assets/PixelMask.h"
#include "core/Color.h"
#include "core/PixelMatrix.h"

#if defined(ARDUINO_ARCH_AVR)
#include <avr/pgmspace.h>
#endif

namespace {

#if defined(ARDUINO_ARCH_AVR)
#define PALM_MASK_STORAGE PROGMEM
#else
#define PALM_MASK_STORAGE
#endif

// ---------------------------------------------------------------------------
// Palette
//
// Chosen for a milky diffuser, which blooms bright neighbours: the sky is the
// dimmest large area so the palm keeps the strongest contrast, the sun is small
// and no pure white appears anywhere.
// ---------------------------------------------------------------------------
constexpr Color kSkyHigh{10u, 85u, 145u};
constexpr Color kSky{8u, 65u, 115u};
constexpr Color kOcean{0u, 75u, 145u};
constexpr Color kOceanHigh{15u, 115u, 175u};
constexpr Color kSand{210u, 150u, 65u};
constexpr Color kSun{235u, 165u, 45u};

constexpr Color kTrunkDark{110u, 55u, 25u};
constexpr Color kTrunkLight{180u, 90u, 35u};
constexpr Color kLeafDark{20u, 100u, 35u};
constexpr Color kLeafBright{55u, 175u, 45u};

// ---------------------------------------------------------------------------
// Scene bands. The background never animates.
// ---------------------------------------------------------------------------
constexpr uint8_t kSkyHighLastRow = 5u;   // rows 0..5 slightly lifted
constexpr uint8_t kSkyLastRow = 11u;      // rows 6..11 base sky
constexpr uint8_t kOceanRow = 12u;
constexpr uint8_t kOceanHighRow = 13u;
constexpr uint8_t kSandFirstRow = 14u;    // rows 14..15

// The sun is deliberately only 2x2. On this diffuser a larger warm block blooms
// into a solid yellow square.
constexpr uint8_t kSunLeft = 2u;
constexpr uint8_t kSunTop = 1u;
constexpr uint8_t kSunSize = 2u;

// ---------------------------------------------------------------------------
// Geometry
//
// CENTER is the single source of truth. The trunk is split in two on purpose:
// the lower half is never offset, which is what anchors the tree.
// ---------------------------------------------------------------------------

// Fronds. Outer tips are the darker shade, the inner crown the brighter one.
static const PixelMask16 kLeavesBrightCenter PALM_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0180, 0x03C0, 0x0660, 0x0660, 0x03C0,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
}};

static const PixelMask16 kLeavesDarkCenter PALM_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0000, 0x0420, 0x1818, 0x300C, 0x0810,
    0x0420, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
}};

// Anchored half: rows 10..15. Drawn with a zero offset in every pose.
static const PixelMask16 kTrunkLowerDarkCenter PALM_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0200, 0x0200, 0x0200, 0x0100, 0x0100, 0x0100
}};

static const PixelMask16 kTrunkLowerLightCenter PALM_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0100, 0x0100, 0x0100, 0x0080, 0x0080, 0x0080
}};

// Wind-following half: rows 5..9. Offset by at most one pixel.
static const PixelMask16 kTrunkUpperDarkCenter PALM_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0100, 0x0100, 0x0100,
    0x0100, 0x0100, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
}};

static const PixelMask16 kTrunkUpperLightCenter PALM_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0080, 0x0080, 0x0080,
    0x0080, 0x0080, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
}};

#undef PALM_MASK_STORAGE

// ---------------------------------------------------------------------------
// The breeze
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
constexpr uint8_t kPoseSequence[8] = { 0u, 1u, 2u, 1u, 0u, 3u, 4u, 3u };

// Per pose horizontal offset. The trunk moves half as far as the leaves, which
// is what makes the motion read as wind rather than as a sliding tree.
constexpr int8_t kTrunkShift[5] = { 0, 1, 1, -1, -1 };
constexpr int8_t kLeafShift[5]  = { 0, 1, 2, -1, -2 };

// The loop completes after exactly one pass, so AnimationManager restarting it
// for PlayMode::LOOP lands back on the first frame and stays seamless.
constexpr uint8_t kLoopFrameCount = 8u;

static_assert(
    sizeof(kPoseSequence) / sizeof(kPoseSequence[0]) == PalmBreezeAnimation::kSequenceLength,
    "Palm breeze sequence length must match the declared step count");

static_assert(
    kLoopFrameCount % PalmBreezeAnimation::kSequenceLength == 0u,
    "Palm breeze loop must contain a whole number of sequences");

}  // namespace

PalmBreezeAnimation::PalmBreezeAnimation()
    : startedAt_(0u),
      sequenceIndex_(0u),
      renderedFrameCount_(0u),
      finished_(false) {
}

void PalmBreezeAnimation::start(uint32_t now) {
    startedAt_ = now;
    // 0xFF is not a valid step index, so the first update renders pose 0 on the
    // very next tick instead of waiting out a whole interval.
    sequenceIndex_ = 0xFFu;
    renderedFrameCount_ = 0u;
    finished_ = false;
}

void PalmBreezeAnimation::update(uint32_t now, PixelMatrix& matrix) {
    if (finished_) {
        return;
    }

    // Derived from elapsed time rather than accumulated per-frame, so the pose
    // never drifts and t=0 and t=loopDuration are the same frame by definition.
    const uint32_t elapsed = static_cast<uint32_t>(now - startedAt_);
    const uint8_t step = static_cast<uint8_t>((elapsed / kFrameIntervalMs) % kSequenceLength);

    // Only repaint when the pose actually changes, so an idle App tick does not
    // dirty the framebuffer.
    if (step == sequenceIndex_) {
        return;
    }

    sequenceIndex_ = step;
    renderFrame(matrix);

    ++renderedFrameCount_;
    if (renderedFrameCount_ >= kLoopFrameCount) {
        finished_ = true;
    }
}

bool PalmBreezeAnimation::isFinished() const {
    return finished_;
}

void PalmBreezeAnimation::stop() {
    sequenceIndex_ = 0u;
    renderedFrameCount_ = 0u;
    finished_ = true;
}

void PalmBreezeAnimation::renderFrame(PixelMatrix& matrix) const {
    matrix.clear();
    drawBackground(matrix);
    drawPalm(matrix, poseAtStep(sequenceIndex_));
}

void PalmBreezeAnimation::drawBackground(PixelMatrix& matrix) const {
    for (uint8_t y = 0u; y <= kSkyLastRow; ++y) {
        const Color shade = (y <= kSkyHighLastRow) ? kSkyHigh : kSky;
        for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
            matrix.setPixel(x, y, shade);
        }
    }

    // A two-row horizon: darker water, then a lighter line at the shore.
    for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
        matrix.setPixel(x, kOceanRow, kOcean);
        matrix.setPixel(x, kOceanHighRow, kOceanHigh);
    }

    for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
        for (uint8_t y = kSandFirstRow; y < PixelMatrix::HEIGHT; ++y) {
            matrix.setPixel(x, y, kSand);
        }
    }

    for (uint8_t dy = 0u; dy < kSunSize; ++dy) {
        for (uint8_t dx = 0u; dx < kSunSize; ++dx) {
            matrix.setPixel(kSunLeft + dx, kSunTop + dy, kSun);
        }
    }
}

void PalmBreezeAnimation::drawPalm(PixelMatrix& matrix, uint8_t pose) const {
    const int8_t leafShift = leafShiftAt(pose);
    const int8_t trunkShift = trunkShiftAt(pose);

    // Fronds first, so the trunk stays visible where it rises through the crown.
    drawMask(&kLeavesDarkCenter, leafShift, 0, kLeafDark, matrix);
    drawMask(&kLeavesBrightCenter, leafShift, 0, kLeafBright, matrix);

    // Anchored base: always at offset zero.
    drawMask(&kTrunkLowerDarkCenter, 0, 0, kTrunkDark, matrix);
    drawMask(&kTrunkLowerLightCenter, 0, 0, kTrunkLight, matrix);

    // Wind-following crown half.
    drawMask(&kTrunkUpperDarkCenter, trunkShift, 0, kTrunkDark, matrix);
    drawMask(&kTrunkUpperLightCenter, trunkShift, 0, kTrunkLight, matrix);
}

uint8_t PalmBreezeAnimation::poseAtStep(uint8_t step) {
    return kPoseSequence[step % kSequenceLength];
}

int8_t PalmBreezeAnimation::trunkShiftAt(uint8_t pose) {
    return kTrunkShift[pose % kPoseCount];
}

int8_t PalmBreezeAnimation::leafShiftAt(uint8_t pose) {
    return kLeafShift[pose % kPoseCount];
}

bool PalmBreezeAnimation::trunkLowerContains(uint8_t x, uint8_t y) {
    return maskHasPixel(&kTrunkLowerDarkCenter, x, y) ||
           maskHasPixel(&kTrunkLowerLightCenter, x, y);
}

bool PalmBreezeAnimation::trunkUpperContains(uint8_t x, uint8_t y) {
    return maskHasPixel(&kTrunkUpperDarkCenter, x, y) ||
           maskHasPixel(&kTrunkUpperLightCenter, x, y);
}

bool PalmBreezeAnimation::leavesDarkContains(uint8_t x, uint8_t y) {
    return maskHasPixel(&kLeavesDarkCenter, x, y);
}

bool PalmBreezeAnimation::leavesBrightContains(uint8_t x, uint8_t y) {
    return maskHasPixel(&kLeavesBrightCenter, x, y);
}

uint8_t PalmBreezeAnimation::sequenceLength() {
    return kSequenceLength;
}

uint32_t PalmBreezeAnimation::loopDurationMs() {
    return static_cast<uint32_t>(kSequenceLength) * kFrameIntervalMs;
}
