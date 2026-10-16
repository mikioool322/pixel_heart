#include "animations/DancingDuckAnimation.h"

#include "assets/PixelMask.h"
#include "core/Color.h"
#include "core/PixelMatrix.h"

namespace {

#if defined(ARDUINO_ARCH_AVR)
#define DUCK_MASK_STORAGE PROGMEM
#else
#define DUCK_MASK_STORAGE
#endif


// ============================================================================
// PALETTE
// ============================================================================

constexpr Color kDuckDark{
    115,
    30,
    0
};

constexpr Color kDuckOrange{
    255,
    110,
    0
};

constexpr Color kDuckGold{
    255,
    185,
    20
};

constexpr Color kDuckRed{
    255,
    40,
    5
};


// ============================================================================
// FRAME 0
// LEFT / NORMAL
// ============================================================================

static const PixelMask16 kDuck0Dark DUCK_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x03C0,
    0x0420,
    0x0428,
    0x3C38,
    0x4024,
    0x3804,
    0x1004,
    0x0804,
    0x0808,
    0x0408,
    0x03F0,
    0x0240,
    0x0000,
    0x0000
}};


static const PixelMask16 kDuck0Orange DUCK_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0000,
    0x03C0,
    0x02C0,
    0x03C0,
    0x07D8,
    0x07F8,
    0x0E78,
    0x0438,
    0x0670,
    0x03F0,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};


static const PixelMask16 kDuck0Gold DUCK_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0180,
    0x03C0,
    0x0180,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};


static const PixelMask16 kDuck0Red DUCK_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x3800,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0420,
    0x0660,
    0x0000
}};


// ============================================================================
// FRAME 1
// LEFT / HOP UP
// ============================================================================

static const PixelMask16 kDuck1Dark DUCK_MASK_STORAGE = {{
    0x0000,
    0x03C0,
    0x0420,
    0x0428,
    0x3C38,
    0x4024,
    0x3804,
    0x1004,
    0x0804,
    0x0808,
    0x0408,
    0x03F0,
    0x0240,
    0x0000,
    0x0000,
    0x0000
}};


static const PixelMask16 kDuck1Orange DUCK_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x03C0,
    0x02C0,
    0x03C0,
    0x07D8,
    0x07F8,
    0x0E78,
    0x0438,
    0x0670,
    0x03F0,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};


static const PixelMask16 kDuck1Gold DUCK_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0180,
    0x03C0,
    0x0180,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};


static const PixelMask16 kDuck1Red DUCK_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x3800,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0420,
    0x0660,
    0x0000,
    0x0000
}};


// ============================================================================
// FRAME 2
// RIGHT / NORMAL
// ============================================================================

static const PixelMask16 kDuck2Dark DUCK_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x03C0,
    0x0420,
    0x1420,
    0x1C3C,
    0x2402,
    0x201C,
    0x2008,
    0x2010,
    0x1010,
    0x1020,
    0x0FC0,
    0x0240,
    0x0000,
    0x0000
}};


static const PixelMask16 kDuck2Orange DUCK_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0000,
    0x03C0,
    0x0340,
    0x03C0,
    0x1BE0,
    0x1FE0,
    0x1E70,
    0x1C20,
    0x0E60,
    0x0FC0,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};


static const PixelMask16 kDuck2Gold DUCK_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0180,
    0x03C0,
    0x0180,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};


static const PixelMask16 kDuck2Red DUCK_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x001C,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0420,
    0x0660,
    0x0000
}};


// ============================================================================
// FRAME 3
// RIGHT / HOP UP
// ============================================================================

static const PixelMask16 kDuck3Dark DUCK_MASK_STORAGE = {{
    0x0000,
    0x03C0,
    0x0420,
    0x1420,
    0x1C3C,
    0x2402,
    0x201C,
    0x2008,
    0x2010,
    0x1010,
    0x1020,
    0x0FC0,
    0x0240,
    0x0000,
    0x0000,
    0x0000
}};


static const PixelMask16 kDuck3Orange DUCK_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x03C0,
    0x0340,
    0x03C0,
    0x1BE0,
    0x1FE0,
    0x1E70,
    0x1C20,
    0x0E60,
    0x0FC0,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};


static const PixelMask16 kDuck3Gold DUCK_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0180,
    0x03C0,
    0x0180,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};


static const PixelMask16 kDuck3Red DUCK_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x001C,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0420,
    0x0660,
    0x0000,
    0x0000
}};


#undef DUCK_MASK_STORAGE


// ============================================================================
// SMALL RENDER HELPER
// ============================================================================

void renderDuckLayers(
    PixelMatrix& matrix,
    const PixelMask16* dark,
    const PixelMask16* orange,
    const PixelMask16* gold,
    const PixelMask16* red
) {
    // Dark illuminated silhouette / outline.
    drawMask(
        dark,
        0,
        0,
        kDuckDark,
        matrix
    );

    // Main orange body.
    drawMask(
        orange,
        0,
        0,
        kDuckOrange,
        matrix
    );

    // Wing/highlight.
    drawMask(
        gold,
        0,
        0,
        kDuckGold,
        matrix
    );

    // Beak and feet.
    drawMask(
        red,
        0,
        0,
        kDuckRed,
        matrix
    );
}


}  // namespace


// ============================================================================
// CONSTRUCTOR
// ============================================================================

DancingDuckAnimation::DancingDuckAnimation()
    : startedAt_(0u),
      lastFrameAt_(0u),
      frameIndex_(0u),
      finished_(false) {
}


// ============================================================================
// START
// ============================================================================

void DancingDuckAnimation::start(
    uint32_t now
) {
    startedAt_ = now;

    // Force frame 0 to render immediately on the next update.
    lastFrameAt_ =
        static_cast<uint32_t>(
            now - kFrameDurationMs
        );

    frameIndex_ = 0u;

    finished_ = false;
}


// ============================================================================
// RENDER
// ============================================================================

void DancingDuckAnimation::renderFrame(
    PixelMatrix& matrix,
    uint8_t frameIndex
) const {
    matrix.clear();

    switch (
        frameIndex % kFrameCount
    ) {
        case 0u:
            renderDuckLayers(
                matrix,
                &kDuck0Dark,
                &kDuck0Orange,
                &kDuck0Gold,
                &kDuck0Red
            );
            break;


        case 1u:
            renderDuckLayers(
                matrix,
                &kDuck1Dark,
                &kDuck1Orange,
                &kDuck1Gold,
                &kDuck1Red
            );
            break;


        case 2u:
            renderDuckLayers(
                matrix,
                &kDuck2Dark,
                &kDuck2Orange,
                &kDuck2Gold,
                &kDuck2Red
            );
            break;


        case 3u:
        default:
            renderDuckLayers(
                matrix,
                &kDuck3Dark,
                &kDuck3Orange,
                &kDuck3Gold,
                &kDuck3Red
            );
            break;
    }
}


// ============================================================================
// UPDATE
// ============================================================================

void DancingDuckAnimation::update(
    uint32_t now,
    PixelMatrix& matrix
) {
    if (finished_) {
        return;
    }


    const uint32_t frameElapsed =
        static_cast<uint32_t>(
            now - lastFrameAt_
        );


    if (
        frameElapsed <
        kFrameDurationMs
    ) {
        return;
    }


    // IMPORTANT:
    // Render at most ONE visible frame per update.
    renderFrame(
        matrix,
        frameIndex_
    );


    frameIndex_ =
        static_cast<uint8_t>(
            (
                frameIndex_ +
                1u
            ) %
            kFrameCount
        );


    lastFrameAt_ = now;


    const uint32_t totalElapsed =
        static_cast<uint32_t>(
            now - startedAt_
        );


    if (
        totalElapsed >=
        kAnimationDurationMs
    ) {
        // Do NOT clear the display.
        //
        // HOLD should retain the final duck frame.
        finished_ = true;
    }
}


// ============================================================================
// FINISHED
// ============================================================================

bool DancingDuckAnimation::isFinished() const {
    return finished_;
}


// ============================================================================
// STOP
// ============================================================================

void DancingDuckAnimation::stop() {
    startedAt_ = 0u;
    lastFrameAt_ = 0u;

    frameIndex_ = 0u;

    finished_ = true;
}
