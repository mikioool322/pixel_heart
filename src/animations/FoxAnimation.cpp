#include "animations/FoxAnimation.h"

#include "assets/PixelMask.h"
#include "core/Color.h"
#include "core/PixelMatrix.h"

#if defined(ARDUINO_ARCH_AVR)
#include <avr/pgmspace.h>
#endif

namespace {

#if defined(ARDUINO_ARCH_AVR)
#define FOX_MASK_STORAGE PROGMEM
#else
#define FOX_MASK_STORAGE
#endif

// A calm decorative fox: the body never moves, only the tail tip breathes.
constexpr uint16_t kFrameIntervalMs = 350u;

// LOW -> CENTER -> HIGH -> CENTER -> LOW. Returning through CENTER (never
// HIGH -> LOW directly) is what keeps the loop from visibly jumping.
constexpr uint8_t kTailSequence[] = { 0u, 1u, 2u, 1u };
constexpr uint8_t kSequenceLength = 4u;

// Two full tail cycles per loop: 8 frames x 350 ms = 2800 ms.
constexpr uint8_t kLoopFrameCount = 8u;

static_assert(
    kLoopFrameCount % kSequenceLength == 0u,
    "Fox loop must contain complete tail cycles"
);

// Tail poses: 0 = LOW, 1 = CENTER, 2 = HIGH.
constexpr uint8_t kTailLow = 0u;
constexpr uint8_t kTailCenter = 1u;
constexpr uint8_t kTailHigh = 2u;

constexpr Color kFoxInk{38u, 28u, 25u};
constexpr Color kFoxFur{235u, 105u, 35u};
constexpr Color kFoxCream{225u, 210u, 185u};
constexpr Color kFoxRust{125u, 65u, 38u};

// Static body. The distal tail pixels are deliberately absent here: the tail
// is drawn separately so it can move without touching the body.
static const PixelMask16 kFoxDark FOX_MASK_STORAGE = {{
    0x7070, 0x4890, 0x4710, 0x4010,
    0x8008, 0x9048, 0x9048, 0x8208,
    0x6030, 0x1040, 0x1040, 0x1040,
    0x2020, 0x2530, 0x2520, 0x1FF8
}};

static const PixelMask16 kFoxOrange FOX_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0880, 0x3FE0,
    0x7FF0, 0x6FB0, 0x6FB0, 0x78F0,
    0x1040, 0x0000, 0x0880, 0x0880,
    0x18C0, 0x18C0, 0x0010, 0x0000
}};

static const PixelMask16 kFoxWhite FOX_MASK_STORAGE = {{
    0x0000, 0x0000, 0x1040, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0500,
    0x0F80, 0x0F80, 0x0700, 0x0700,
    0x0700, 0x0200, 0x0200, 0x0000
}};

static const PixelMask16 kFoxBrown FOX_MASK_STORAGE = {{
    0x0000, 0x3060, 0x2020, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x18C0, 0x0000
}};

// Shared tail outline and fur (the CENTER reference geometry). The base stays
// anchored to the body in every pose; only the white tip below moves, so the
// tail can never detach. The topmost tip pixel is deliberately NOT part of the
// shared mask: it is the pixel that travels, and leaving it here would strand
// an isolated dot when the tip is low.
static const PixelMask16 kTailDark FOX_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x000A,
    0x000A, 0x0002, 0x0004, 0x0000
}};

static const PixelMask16 kTailOrange FOX_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x000C, 0x0008, 0x0000
}};

#undef FOX_MASK_STORAGE

}  // namespace

FoxAnimation::FoxAnimation()
    : lastFrameAt_(0u),
      phase_(0u),
      renderedFrameCount_(0u),
      finished_(false) {
}

void FoxAnimation::start(uint32_t now) {
    phase_ = 0u;
    renderedFrameCount_ = 0u;
    finished_ = false;
    lastFrameAt_ = static_cast<uint32_t>(now - kFrameIntervalMs);
}

void FoxAnimation::update(uint32_t now, PixelMatrix& matrix) {
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

bool FoxAnimation::isFinished() const {
    return finished_;
}

void FoxAnimation::stop() {
    lastFrameAt_ = 0u;
    phase_ = 0u;
    renderedFrameCount_ = 0u;
    finished_ = true;
}

void FoxAnimation::renderFrame(PixelMatrix& matrix, uint8_t frame) const {
    matrix.clear();
    renderBody(matrix);
    renderTail(matrix, tailPoseAt(frame));
}

void FoxAnimation::renderBody(PixelMatrix& matrix) const {
    drawMask(&kFoxDark, 0, 0, kFoxInk, matrix);
    drawMask(&kFoxOrange, 0, 0, kFoxFur, matrix);
    drawMask(&kFoxWhite, 0, 0, kFoxCream, matrix);
    drawMask(&kFoxBrown, 0, 0, kFoxRust, matrix);
}

void FoxAnimation::renderTail(PixelMatrix& matrix, uint8_t tailPose) const {
    drawMask(&kTailDark, 0, 0, kFoxInk, matrix);
    drawMask(&kTailOrange, 0, 0, kFoxFur, matrix);

    // The white tip rides one pixel up or down; the outline and fur it leaves
    // behind stay exactly where they were.
    matrix.setPixel(tailTipX(tailPose, 0u), tailTipY(tailPose, 0u), kFoxCream);
    matrix.setPixel(tailTipX(tailPose, 1u), tailTipY(tailPose, 1u), kFoxCream);
}

uint8_t FoxAnimation::tailPoseAt(uint8_t frame) {
    return kTailSequence[frame % kSequenceLength];
}

uint8_t FoxAnimation::frameCount() {
    return kLoopFrameCount;
}

uint8_t FoxAnimation::tailTipX(uint8_t tailPose, uint8_t tip) {
    (void)tailPose;
    (void)tip;
    return 13u;
}

uint8_t FoxAnimation::tailTipY(uint8_t tailPose, uint8_t tip) {
    // CENTER tip sits at y11..12; HIGH rides one row up, LOW one row down.
    switch (tailPose) {
        case kTailHigh:
            return static_cast<uint8_t>(10u + tip);
        case kTailLow:
            return static_cast<uint8_t>(12u + tip);
        case kTailCenter:
        default:
            return static_cast<uint8_t>(11u + tip);
    }
}

bool FoxAnimation::staticBodyContains(uint8_t x, uint8_t y) {
    return maskHasPixel(&kFoxDark, x, y) ||
           maskHasPixel(&kFoxOrange, x, y) ||
           maskHasPixel(&kFoxWhite, x, y) ||
           maskHasPixel(&kFoxBrown, x, y);
}

bool FoxAnimation::tailContains(uint8_t tailPose, uint8_t x, uint8_t y) {
    if (maskHasPixel(&kTailDark, x, y) || maskHasPixel(&kTailOrange, x, y)) {
        return true;
    }
    if (tailPose > kTailHigh) {
        return false;
    }
    return (x == tailTipX(tailPose, 0u) && y == tailTipY(tailPose, 0u)) ||
           (x == tailTipX(tailPose, 1u) && y == tailTipY(tailPose, 1u));
}
