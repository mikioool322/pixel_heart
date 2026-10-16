#include "animations/HalloweenGhostAnimation.h"

#include "assets/PixelMask.h"
#include "core/Color.h"
#include "core/PixelMatrix.h"

#if defined(ARDUINO_ARCH_AVR)
#include <avr/pgmspace.h>
#endif

namespace {

#if defined(ARDUINO_ARCH_AVR)
#define GHOST_MASK_STORAGE PROGMEM
#else
#define GHOST_MASK_STORAGE
#endif

// One logical pixel every 450 ms: on a 16 px panel a single pixel is already a
// large physical movement, so anything faster would read as hurrying.
constexpr uint16_t kFrameIntervalMs = 150u;

// Timeline (26 frames, 3900 ms per loop):
//   MOVE_LEFT  frames  0..8   +1 -> 0 -> -1, gaze LEFT,   hem waves
//   STOP_LEFT  frames  9..13  -1, gaze CENTER, hem still
//   MOVE_RIGHT frames 14..22  -1 -> 0 -> +1, gaze RIGHT,  hem waves
//   TURNAROUND frames 23..25  +1, gaze RIGHT -> CENTER -> LEFT, hem still
//
// Frame 25 (pos +1, gaze LEFT, hem 0) is pixel-identical to frame 0, which is
// what makes the loop seamless.
constexpr uint8_t kMoveLeftFrames = 9u; //
constexpr uint8_t kStopLeftFrames = 5u;
constexpr uint8_t kMoveRightFrames = 9u;
constexpr uint8_t kTurnaroundFrames = 3u;
constexpr uint8_t kLoopFrameCount =
    static_cast<uint8_t>(kMoveLeftFrames + kStopLeftFrames + kMoveRightFrames + kTurnaroundFrames);

static_assert(kLoopFrameCount == 26u, "ghost loop must be 26 frames");

// Restrained greys: the diffuser blooms full white badly, so the body stays
// well below RGB(255,255,255).
constexpr Color kGhostGrey{145u, 155u, 170u};
constexpr Color kGhostHighlight{180u, 188u, 200u};
constexpr Color kEyeWhite{220u, 225u, 230u};
constexpr Color kPupil{0u, 0u, 0u};

// Eye sclera regions (body coordinates, before xOffset).
constexpr int16_t kLeftEyeX = 3;
constexpr int16_t kRightEyeX = 9;
constexpr int16_t kEyeY = 5;
constexpr uint8_t kEyeSize = 3u;
constexpr uint8_t kPupilHeight = 2u;

// Body rows 0..13 are shared by both hem phases; only the fringe rows differ,
// so the hem animation costs two extra rows instead of a second full sprite.
static const PixelMask16 kGhostBody GHOST_MASK_STORAGE = {{
    0x0000,  // y0
    0x03C0,  // y1
    0x0FF0,  // y2
    0x1FF8,  // y3
    0x3FFC,  // y4
    0x3FFC,  // y5
    0x3FFC,  // y6
    0x3FFC,  // y7
    0x3FFC,  // y8
    0x3FFC,  // y9
    0x3FFC,  // y10
    0x3FFC,  // y11
    0x3FFC,  // y12
    0x3FFC,  // y13
    0x0000,  // y14 (hem, see below)
    0x0000   // y15 (hem, see below)
}};

// Hem fringe. The reference masks each contained ONE tatter pixel with no lit
// orthogonal neighbour (0x1248 -> x12, 0x1BB4 -> x13), which would render a
// floating dot under the ghost. Those two bits are dropped so every tatter
// stays attached to the body.
static const PixelMaskRow kHemRow14[2] GHOST_MASK_STORAGE = { 0x3774, 0x3BB8 };
static const PixelMaskRow kHemRow15[2] GHOST_MASK_STORAGE = { 0x1240, 0x1BB0 };

#undef GHOST_MASK_STORAGE

PixelMaskRow readHemRow(const PixelMaskRow* rows, uint8_t hem) {
#if defined(ARDUINO_ARCH_AVR)
    return pgm_read_word(&rows[hem & 1u]);
#else
    return rows[hem & 1u];
#endif
}

void setPixelShifted(PixelMatrix& matrix, int16_t x, int16_t y, Color color) {
    if (x < 0 || y < 0 ||
        x >= static_cast<int16_t>(PixelMatrix::WIDTH) ||
        y >= static_cast<int16_t>(PixelMatrix::HEIGHT)) {
        return;
    }
    matrix.setPixel(static_cast<uint8_t>(x), static_cast<uint8_t>(y), color);
}

bool bodyRowHas(const PixelMask16* mask, uint8_t x, uint8_t y) {
    return maskHasPixel(mask, x, y);
}

}  // namespace

HalloweenGhostAnimation::HalloweenGhostAnimation()
    : lastFrameAt_(0u),
      phase_(0u),
      renderedFrameCount_(0u),
      finished_(false) {
}

void HalloweenGhostAnimation::start(uint32_t now) {
    phase_ = 0u;
    renderedFrameCount_ = 0u;
    finished_ = false;
    lastFrameAt_ = static_cast<uint32_t>(now - kFrameIntervalMs);
}

void HalloweenGhostAnimation::update(uint32_t now, PixelMatrix& matrix) {
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

bool HalloweenGhostAnimation::isFinished() const {
    return finished_;
}

void HalloweenGhostAnimation::stop() {
    lastFrameAt_ = 0u;
    phase_ = 0u;
    renderedFrameCount_ = 0u;
    finished_ = true;
}

void HalloweenGhostAnimation::renderFrame(PixelMatrix& matrix, uint8_t frame) const {
    matrix.clear();

    const int8_t xOffset = positionAt(frame);
    const Gaze gaze = gazeAt(frame);
    const uint8_t hem = hemAt(frame);

    const PixelMaskRow hem14 = readHemRow(kHemRow14, hem);
    const PixelMaskRow hem15 = readHemRow(kHemRow15, hem);

    // Body rows 0..13 plus the current hem fringe, all shifted by xOffset.
    for (uint8_t y = 0u; y < PixelMatrix::HEIGHT; ++y) {
        for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
            bool lit = false;
            if (y <= 13u) {
                lit = bodyRowHas(&kGhostBody, x, y);
            } else if (y == 14u) {
                lit = (hem14 & (static_cast<PixelMaskRow>(1u) << (15u - x))) != 0u;
            } else {
                lit = (hem15 & (static_cast<PixelMaskRow>(1u) << (15u - x))) != 0u;
            }
            if (!lit) {
                continue;
            }
            const int16_t px = static_cast<int16_t>(x) + xOffset;
            if (px < 0 || px >= static_cast<int16_t>(PixelMatrix::WIDTH)) {
                continue;
            }
            matrix.setPixel(static_cast<uint8_t>(px), y, shadeAt(x, y));
        }
    }

    // Eyes: white sclera first, then the dark pupil. Both take the same
    // xOffset as the body, so they can never detach from the ghost.
    const uint8_t leftPupil = pupilX(false, gaze);
    const uint8_t rightPupil = pupilX(true, gaze);

    for (uint8_t dy = 0u; dy < kEyeSize; ++dy) {
        for (uint8_t dx = 0u; dx < kEyeSize; ++dx) {
            const int16_t lx = kLeftEyeX + static_cast<int16_t>(dx) + xOffset;
            const int16_t rx = kRightEyeX + static_cast<int16_t>(dx) + xOffset;
            const int16_t ey = kEyeY + static_cast<int16_t>(dy);
            setPixelShifted(matrix, lx, ey, kEyeWhite);
            setPixelShifted(matrix, rx, ey, kEyeWhite);
        }
    }

    for (uint8_t dy = 0u; dy < kPupilHeight; ++dy) {
        const int16_t py = static_cast<int16_t>(6 + dy);
        setPixelShifted(
            matrix,
            static_cast<int16_t>(leftPupil) + xOffset,
            py, kPupil);
        setPixelShifted(
            matrix,
            static_cast<int16_t>(rightPupil) + xOffset,
            py, kPupil);
    }
}

int8_t HalloweenGhostAnimation::positionAt(uint8_t frame) {
    if (frame <= 2u) {
        return 1;
    }
    if (frame <= 5u) {
        return 0;
    }
    if (frame <= 16u) {
        return -1;
    }
    if (frame <= 19u) {
        return 0;
    }
    return 1;
}

HalloweenGhostAnimation::Gaze HalloweenGhostAnimation::gazeAt(uint8_t frame) {
    if (frame <= 8u) {
        return Gaze::LEFT;
    }
    if (frame <= 13u) {
        return Gaze::CENTER;
    }
    if (frame <= 22u) {
        return Gaze::RIGHT;
    }
    if (frame == 23u) {
        return Gaze::RIGHT;
    }
    if (frame == 24u) {
        return Gaze::CENTER;
    }
    return Gaze::LEFT;
}

uint8_t HalloweenGhostAnimation::hemAt(uint8_t frame) {
    uint8_t moving = 0u;
    uint8_t indexInPhase = 0u;
    if (frame <= 8u) {
        moving = 1u;
        indexInPhase = frame;
    } else if (frame >= 14u && frame <= 22u) {
        moving = 1u;
        indexInPhase = static_cast<uint8_t>(frame - 14u);
    }
    if (moving == 0u) {
        return 0u;
    }
    return static_cast<uint8_t>((indexInPhase >> 1) & 1u);
}

bool HalloweenGhostAnimation::bodyPixelAt(uint8_t x, uint8_t y) {
    if (y > 13u) {
        return false;
    }
    return bodyRowHas(&kGhostBody, x, y);
}

bool HalloweenGhostAnimation::hemPixelAt(uint8_t hem, uint8_t row, uint8_t x) {
    if (row == 14u) {
        const PixelMaskRow bits = readHemRow(kHemRow14, hem);
        return (bits & (static_cast<PixelMaskRow>(1u) << (15u - x))) != 0u;
    }
    if (row == 15u) {
        const PixelMaskRow bits = readHemRow(kHemRow15, hem);
        return (bits & (static_cast<PixelMaskRow>(1u) << (15u - x))) != 0u;
    }
    return false;
}

Color HalloweenGhostAnimation::shadeAt(uint8_t x, uint8_t y) {
    (void)x;
    // Pale cap over the dome, main grey below. Both restrained, neither white.
    if (y <= 4u) {
        return kGhostHighlight;
    }
    return kGhostGrey;
}

uint8_t HalloweenGhostAnimation::pupilX(bool rightEye, Gaze gaze) {
    const int16_t base = rightEye ? kRightEyeX : kLeftEyeX;
    switch (gaze) {
        case Gaze::LEFT:
            return static_cast<uint8_t>(base);
        case Gaze::RIGHT:
            return static_cast<uint8_t>(base + 2);
        case Gaze::CENTER:
        default:
            return static_cast<uint8_t>(base + 1);
    }
}

uint8_t HalloweenGhostAnimation::frameCount() {
    return kLoopFrameCount;
}
