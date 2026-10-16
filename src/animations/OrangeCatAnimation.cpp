#include "animations/OrangeCatAnimation.h"

#include "assets/PixelMask.h"
#include "core/Color.h"
#include "core/PixelMatrix.h"

#if defined(ARDUINO_ARCH_AVR)
#include <avr/pgmspace.h>
#endif

namespace {

#if defined(ARDUINO_ARCH_AVR)
#define CAT_MASK_STORAGE PROGMEM
#else
#define CAT_MASK_STORAGE
#endif

// Only the tiny lens reflections move; everything else is a still image.
constexpr uint16_t kFrameIntervalMs = 200u;

// Symmetric shimmer curve: up in small steps and back down the same way, so
// the last step lands exactly on the first. The dimmest step is still clearly
// lit, so a highlight can never strobe off.
constexpr uint8_t kGlintLevels[] = { 90u, 130u, 180u, 230u, 180u, 130u };
constexpr uint8_t kGlintPhaseCount = 6u;

// The right reflections trail the left by two steps: alive, never synchronised.
constexpr uint8_t kRightGlintOffset = 2u;

// Six complete shimmer cycles per play: 36 frames x 200 ms = 7200 ms, so the
// animation always ends on the exact frame the curve began on.
constexpr uint8_t kLoopFrameCount = 36u;

static_assert(
    kLoopFrameCount % kGlintPhaseCount == 0u,
    "Cat loop must contain complete glint cycles"
);
static_assert(
    (kGlintPhaseCount - kRightGlintOffset) % kGlintPhaseCount != 0u,
    "Glint sides must stay phase-offset"
);
static_assert(kGlintLevels[0] > 0u, "Glints must never be fully dark");

// Restrained colours: the diffuser blooms hard, so nothing here is full white.
constexpr Color kCatOrange{235u, 95u, 25u};   // fur
constexpr Color kCatInk{20u, 22u, 28u};       // glasses frame, nose and mouth
constexpr Color kLensBlue{20u, 85u, 210u};    // lens interiors
constexpr Color kGlintBase{220u, 235u, 255u}; // lens reflections (near-white)

// ---------------------------------------------------------------------------
// Geometry (logical x=0 -> bit 15, matching PixelMask.h)
//
//   body   : ears reach y=0, face reaches y=15, silhouette spans x=0..15
//   dark   : the sunglasses frame plus the nose/mouth structure
//   lens   : 4x3 blue interior per lens, fully enclosed by the dark frame
// ---------------------------------------------------------------------------

static const PixelMask16 kCatBodyMask CAT_MASK_STORAGE = {{
    0x381C, 0x7C3E, 0x7E7E, 0xFFFF,
    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    0x7FFE, 0x3FFC, 0x1FF8, 0x0FF0
}};

static const PixelMask16 kCatDarkMask CAT_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x7FFE, 0x43C2, 0x43C2,
    0x43C2, 0x7E7E, 0x0180, 0x0180,
    0x0240, 0x0180, 0x0000, 0x0000
}};

static const PixelMask16 kCatLensMask CAT_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x3C3C, 0x3C3C,
    0x3C3C, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
}};

#undef CAT_MASK_STORAGE

// Reflections. Every coordinate is inside its lens, never on the dark frame.
struct GlintPixel {
    uint8_t x;
    uint8_t y;
    uint8_t rightSide;
};

constexpr GlintPixel kGlints[] = {
    { 2u, 6u, 0u },
    { 3u, 6u, 0u },
    { 2u, 7u, 0u },
    { 4u, 7u, 0u },
    { 11u, 6u, 1u },
    { 12u, 6u, 1u },
    { 11u, 7u, 1u },
    { 10u, 8u, 1u }
};

constexpr uint8_t kGlintPixelCount =
    static_cast<uint8_t>(sizeof(kGlints) / sizeof(kGlints[0]));

uint8_t readGlintLevel(uint8_t index) {
#if defined(ARDUINO_ARCH_AVR)
    return pgm_read_byte(&kGlintLevels[index % kGlintPhaseCount]);
#else
    return kGlintLevels[index % kGlintPhaseCount];
#endif
}

}  // namespace

OrangeCatAnimation::OrangeCatAnimation()
    : lastFrameAt_(0u),
      phase_(0u),
      renderedFrameCount_(0u),
      finished_(false) {
}

void OrangeCatAnimation::start(uint32_t now) {
    phase_ = 0u;
    renderedFrameCount_ = 0u;
    finished_ = false;
    lastFrameAt_ = static_cast<uint32_t>(now - kFrameIntervalMs);
}

void OrangeCatAnimation::update(uint32_t now, PixelMatrix& matrix) {
    if (finished_) {
        return;
    }

    const uint32_t elapsed = static_cast<uint32_t>(now - lastFrameAt_);
    if (elapsed < kFrameIntervalMs) {
        return;
    }

    renderFrame(matrix, phase_);

    phase_ = static_cast<uint8_t>((phase_ + 1u) % kLoopFrameCount);
    ++renderedFrameCount_;
    lastFrameAt_ = now;

    if (renderedFrameCount_ >= kLoopFrameCount) {
        finished_ = true;
    }
}

bool OrangeCatAnimation::isFinished() const {
    return finished_;
}

void OrangeCatAnimation::stop() {
    lastFrameAt_ = 0u;
    phase_ = 0u;
    renderedFrameCount_ = 0u;
    finished_ = true;
}

void OrangeCatAnimation::renderFrame(PixelMatrix& matrix, uint8_t phase) const {
    matrix.clear();

    renderCat(matrix);
    renderGlints(matrix, phase);
}

void OrangeCatAnimation::renderCat(PixelMatrix& matrix) const {
    // Order matters: fur, then the dark frame and nose/mouth, then the lens
    // interiors, which sit inside the frame openings.
    drawMask(&kCatBodyMask, 0, 0, kCatOrange, matrix);
    drawMask(&kCatDarkMask, 0, 0, kCatInk, matrix);
    drawMask(&kCatLensMask, 0, 0, kLensBlue, matrix);
}

void OrangeCatAnimation::renderGlints(PixelMatrix& matrix, uint8_t phase) const {
    const Color left = scaleColor(kGlintBase, glintLevelAt(phase, false));
    const Color right = scaleColor(kGlintBase, glintLevelAt(phase, true));

    for (uint8_t i = 0u; i < kGlintPixelCount; ++i) {
        matrix.setPixel(
            kGlints[i].x,
            kGlints[i].y,
            (kGlints[i].rightSide != 0u) ? right : left);
    }
}

uint8_t OrangeCatAnimation::glintLevelAt(uint8_t phase, bool rightSide) {
    uint8_t index = static_cast<uint8_t>(phase % kGlintPhaseCount);
    if (rightSide) {
        index = static_cast<uint8_t>((index + kRightGlintOffset) % kGlintPhaseCount);
    }
    return readGlintLevel(index);
}

uint8_t OrangeCatAnimation::phaseCount() {
    return kGlintPhaseCount;
}

uint8_t OrangeCatAnimation::frameCount() {
    return kLoopFrameCount;
}

uint8_t OrangeCatAnimation::glintPixelCount() {
    return kGlintPixelCount;
}

uint8_t OrangeCatAnimation::glintPixelX(uint8_t index) {
    return kGlints[index % kGlintPixelCount].x;
}

uint8_t OrangeCatAnimation::glintPixelY(uint8_t index) {
    return kGlints[index % kGlintPixelCount].y;
}

bool OrangeCatAnimation::glintOnRight(uint8_t index) {
    return kGlints[index % kGlintPixelCount].rightSide != 0u;
}

bool OrangeCatAnimation::bodyContains(uint8_t x, uint8_t y) {
    return maskHasPixel(&kCatBodyMask, x, y);
}

bool OrangeCatAnimation::darkContains(uint8_t x, uint8_t y) {
    return maskHasPixel(&kCatDarkMask, x, y);
}

bool OrangeCatAnimation::lensContains(uint8_t x, uint8_t y) {
    return maskHasPixel(&kCatLensMask, x, y);
}

bool OrangeCatAnimation::glintContains(uint8_t x, uint8_t y) {
    for (uint8_t i = 0u; i < kGlintPixelCount; ++i) {
        if (kGlints[i].x == x && kGlints[i].y == y) {
            return true;
        }
    }
    return false;
}
