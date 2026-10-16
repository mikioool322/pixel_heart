#include "animations/CampfireAnimation.h"

#include "assets/PixelMask.h"
#include "core/Color.h"
#include "core/PixelMatrix.h"

namespace {

#if defined(ARDUINO_ARCH_AVR)
#define CAMPFIRE_MASK_STORAGE PROGMEM
#else
#define CAMPFIRE_MASK_STORAGE
#endif


// ============================================================================
// PALETTE
// ============================================================================

constexpr Color kLogDark{
    70,
    42,
    38
};

constexpr Color kLogMid{
    140,
    82,
    65
};

constexpr Color kLogLight{
    205,
    135,
    105
};


constexpr Color kOuterRed{
    235,
    30,
    25
};

constexpr Color kOrange{
    255,
    95,
    10
};

constexpr Color kYellow{
    255,
    205,
    20
};

constexpr Color kCoreWhite{
    255,
    245,
    190
};


constexpr Color kEmberRed{
    190,
    25,
    20
};

constexpr Color kEmberOrange{
    255,
    115,
    15
};


// ============================================================================
// STATIC LOGS
//
// These masks are ABSOLUTE 16x16 coordinates.
//
// Render order:
//
// dark
// -> mid
// -> light
//
// This creates a layered wooden base.
// ============================================================================

static const PixelMask16 kLogDarkMask CAMPFIRE_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0000,
    0x0000,

    0x0000,
    0x0000,
    0x0000,
    0x0000,

    0x0000,
    0x0000,
    0x0000,

    0x381C,
    0x7FFE,
    0x3FFC,
    0x0FF0,

    0x0000
}};


static const PixelMask16 kLogMidMask CAMPFIRE_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0000,
    0x0000,

    0x0000,
    0x0000,
    0x0000,
    0x0000,

    0x0000,
    0x0000,
    0x0000,

    0x1818,
    0x3C3C,
    0x1E78,
    0x07E0,

    0x0000
}};


static const PixelMask16 kLogLightMask CAMPFIRE_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0000,
    0x0000,

    0x0000,
    0x0000,
    0x0000,
    0x0000,

    0x0000,
    0x0000,
    0x0000,

    0x0810,
    0x1818,
    0x0840,
    0x0300,

    0x0000
}};


// ============================================================================
// FLAME FRAME 0
//
// Balanced / centered.
// ============================================================================

static const PixelMask16 kFlame0Red CAMPFIRE_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0180,
    0x0380,
    0x0640,
    0x0C60,
    0x1830,
    0x1010,
    0x3018,
    0x2008,
    0x600C,
    0x1010,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};


static const PixelMask16 kFlame0Orange CAMPFIRE_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0180,
    0x0380,
    0x07C0,
    0x0CE0,
    0x0860,
    0x1830,
    0x1010,
    0x0860,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};


static const PixelMask16 kFlame0Yellow CAMPFIRE_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0300,
    0x0780,
    0x06C0,
    0x0C60,
    0x0780,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};


static const PixelMask16 kFlame0White CAMPFIRE_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0100,
    0x0380,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};


// ============================================================================
// FLAME FRAME 1
//
// Flame leans LEFT.
// ============================================================================

static const PixelMask16 kFlame1Red CAMPFIRE_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0300,
    0x0700,
    0x0C80,
    0x1840,
    0x1060,
    0x3030,
    0x2030,
    0x2010,
    0x6008,
    0x1010,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};


static const PixelMask16 kFlame1Orange CAMPFIRE_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0300,
    0x0780,
    0x0F80,
    0x0CC0,
    0x1040,
    0x1020,
    0x1030,
    0x0860,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};


static const PixelMask16 kFlame1Yellow CAMPFIRE_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0300,
    0x0F80,
    0x0CC0,
    0x0C40,
    0x0780,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};


static const PixelMask16 kFlame1White CAMPFIRE_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0300,
    0x0380,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};


// ============================================================================
// FLAME FRAME 2
//
// Tallest frame.
//
// Tip reaches y=1.
// ============================================================================

static const PixelMask16 kFlame2Red CAMPFIRE_MASK_STORAGE = {{
    0x0000,
    0x0100,
    0x0300,
    0x0380,
    0x0640,
    0x0420,
    0x0C20,
    0x1810,
    0x3018,
    0x2008,
    0x600C,
    0x1010,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};


static const PixelMask16 kFlame2Orange CAMPFIRE_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0180,
    0x03C0,
    0x03C0,
    0x0420,
    0x0820,
    0x1830,
    0x1010,
    0x0860,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};


static const PixelMask16 kFlame2Yellow CAMPFIRE_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x03C0,
    0x07C0,
    0x06C0,
    0x0C60,
    0x0780,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};


static const PixelMask16 kFlame2White CAMPFIRE_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0100,
    0x0380,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};


// ============================================================================
// FLAME FRAME 3
//
// Flame leans RIGHT.
// ============================================================================

static const PixelMask16 kFlame3Red CAMPFIRE_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x00C0,
    0x01C0,
    0x0260,
    0x0430,
    0x0C10,
    0x1810,
    0x1018,
    0x2008,
    0x300C,
    0x1010,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};


static const PixelMask16 kFlame3Orange CAMPFIRE_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0180,
    0x03C0,
    0x03E0,
    0x04E0,
    0x0860,
    0x1810,
    0x0810,
    0x0860,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};


static const PixelMask16 kFlame3Yellow CAMPFIRE_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0300,
    0x0780,
    0x0660,
    0x0620,
    0x0780,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};


static const PixelMask16 kFlame3White CAMPFIRE_MASK_STORAGE = {{
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0180,
    0x01C0,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000
}};


#undef CAMPFIRE_MASK_STORAGE


}  // namespace


// ============================================================================
// CONSTRUCTION
// ============================================================================

CampfireAnimation::CampfireAnimation()
    : startedAt_(0u),
      lastFrameAt_(0u),
      frameIndex_(0u),
      finished_(false) {
}


// ============================================================================
// START
// ============================================================================

void CampfireAnimation::start(uint32_t now) {
    startedAt_ = now;

    lastFrameAt_ =
        static_cast<uint32_t>(
            now - kFrameDurationMs
        );

    frameIndex_ = 0u;

    finished_ = false;
}


// ============================================================================
// LOGS
// ============================================================================

void CampfireAnimation::renderLogs(
    PixelMatrix& matrix
) const {
    drawMask(
        &kLogDarkMask,
        0,
        0,
        kLogDark,
        matrix
    );

    drawMask(
        &kLogMidMask,
        0,
        0,
        kLogMid,
        matrix
    );

    drawMask(
        &kLogLightMask,
        0,
        0,
        kLogLight,
        matrix
    );
}


// ============================================================================
// EMBERS
// ============================================================================

void CampfireAnimation::renderEmbers(
    PixelMatrix& matrix,
    uint8_t frameIndex
) const {
    switch (frameIndex % kFrameCount) {
        case 0u:
            matrix.setPixel(
                2u,
                10u,
                kEmberRed
            );

            matrix.setPixel(
                13u,
                10u,
                kEmberOrange
            );
            break;


        case 1u:
            matrix.setPixel(
                3u,
                10u,
                kEmberOrange
            );

            matrix.setPixel(
                12u,
                9u,
                kEmberRed
            );
            break;


        case 2u:
            matrix.setPixel(
                2u,
                10u,
                kEmberOrange
            );

            matrix.setPixel(
                13u,
                10u,
                kEmberRed
            );

            matrix.setPixel(
                11u,
                9u,
                kEmberOrange
            );
            break;


        case 3u:
        default:
            matrix.setPixel(
                4u,
                10u,
                kEmberRed
            );

            matrix.setPixel(
                13u,
                9u,
                kEmberOrange
            );
            break;
    }
}


// ============================================================================
// FLAME
// ============================================================================

void CampfireAnimation::renderFlameFrame(
    PixelMatrix& matrix,
    uint8_t frameIndex
) const {
    switch (frameIndex % kFrameCount) {

        // --------------------------------------------------------------------
        // FRAME 0
        // --------------------------------------------------------------------

        case 0u:
            drawMask(
                &kFlame0Red,
                0,
                0,
                kOuterRed,
                matrix
            );

            drawMask(
                &kFlame0Orange,
                0,
                0,
                kOrange,
                matrix
            );

            drawMask(
                &kFlame0Yellow,
                0,
                0,
                kYellow,
                matrix
            );

            drawMask(
                &kFlame0White,
                0,
                0,
                kCoreWhite,
                matrix
            );
            break;


        // --------------------------------------------------------------------
        // FRAME 1
        // --------------------------------------------------------------------

        case 1u:
            drawMask(
                &kFlame1Red,
                0,
                0,
                kOuterRed,
                matrix
            );

            drawMask(
                &kFlame1Orange,
                0,
                0,
                kOrange,
                matrix
            );

            drawMask(
                &kFlame1Yellow,
                0,
                0,
                kYellow,
                matrix
            );

            drawMask(
                &kFlame1White,
                0,
                0,
                kCoreWhite,
                matrix
            );
            break;


        // --------------------------------------------------------------------
        // FRAME 2
        // --------------------------------------------------------------------

        case 2u:
            drawMask(
                &kFlame2Red,
                0,
                0,
                kOuterRed,
                matrix
            );

            drawMask(
                &kFlame2Orange,
                0,
                0,
                kOrange,
                matrix
            );

            drawMask(
                &kFlame2Yellow,
                0,
                0,
                kYellow,
                matrix
            );

            drawMask(
                &kFlame2White,
                0,
                0,
                kCoreWhite,
                matrix
            );
            break;


        // --------------------------------------------------------------------
        // FRAME 3
        // --------------------------------------------------------------------

        case 3u:
        default:
            drawMask(
                &kFlame3Red,
                0,
                0,
                kOuterRed,
                matrix
            );

            drawMask(
                &kFlame3Orange,
                0,
                0,
                kOrange,
                matrix
            );

            drawMask(
                &kFlame3Yellow,
                0,
                0,
                kYellow,
                matrix
            );

            drawMask(
                &kFlame3White,
                0,
                0,
                kCoreWhite,
                matrix
            );
            break;
    }
}


// ============================================================================
// COMPLETE FRAME
// ============================================================================

void CampfireAnimation::renderFrame(
    PixelMatrix& matrix,
    uint8_t frameIndex
) const {
    matrix.clear();

    renderLogs(
        matrix
    );

    renderEmbers(
        matrix,
        frameIndex
    );

    renderFlameFrame(
        matrix,
        frameIndex
    );
}


// ============================================================================
// UPDATE
// ============================================================================

void CampfireAnimation::update(
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


    // Render EXACTLY one frame per update.
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
        finished_ = true;

        // IMPORTANT:
        // do NOT clear matrix.
        //
        // HOLD should leave the final campfire frame visible.
    }
}


// ============================================================================
// FINISHED
// ============================================================================

bool CampfireAnimation::isFinished() const {
    return finished_;
}


// ============================================================================
// STOP
// ============================================================================

void CampfireAnimation::stop() {
    startedAt_ = 0u;
    lastFrameAt_ = 0u;

    frameIndex_ = 0u;

    finished_ = true;
}
