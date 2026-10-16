#include "animations/DiscoBallAnimation.h"

#include "assets/PixelMask.h"
#include "core/Color.h"
#include "core/PixelMatrix.h"

#if defined(ARDUINO_ARCH_AVR)
#include <avr/pgmspace.h>
#endif

namespace {

#if defined(ARDUINO_ARCH_AVR)
#define DISCO_MASK_STORAGE PROGMEM
#else
#define DISCO_MASK_STORAGE
#endif

// ---------------------------------------------------------------------------
// Palette. The background is very dark so the silver-blue ball carries the
// frame; the coloured beams are deliberately dimmer than the ball so they stay
// secondary behind a diffuser.
// ---------------------------------------------------------------------------
constexpr Color kBackground{12u, 5u, 28u};

constexpr Color kBallDark{45u, 55u, 85u};
constexpr Color kBallBlue{70u, 105u, 165u};
constexpr Color kBallViolet{105u, 75u, 160u};
constexpr Color kBallSilver{150u, 165u, 190u};
constexpr Color kHighlight{220u, 230u, 245u};

constexpr Color kBeamPink{190u, 35u, 180u};
constexpr Color kBeamBlue{25u, 110u, 235u};
constexpr Color kBeamOrange{235u, 90u, 25u};
constexpr Color kBeamCyan{35u, 190u, 220u};

// ---------------------------------------------------------------------------
// Geometry (logical x=0 -> bit 15, matching PixelMask.h)
//   suspension : x = 7, rows 1..3
//   ball       : x 3..12, rows 4..12  (78 pixels, never moves)
// ---------------------------------------------------------------------------
// The BALL only (rows 4..12). The hanging wire is a separate, one-column
// element, so "ballContains" always means the sphere itself.
static const PixelMask16 kBallMask DISCO_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0000,
    0x07E0, 0x0FF0, 0x1FF8, 0x1FF8,
    0x1FF8, 0x1FF8, 0x1FF8, 0x0FF0,
    0x07E0, 0x0000, 0x0000, 0x0000
}};

#undef DISCO_MASK_STORAGE

// The suspension wire is the same column as the two ball pixels in rows 1..3.
constexpr uint8_t kSuspensionX = 7u;
constexpr uint8_t kSuspensionFirstRow = 1u;
constexpr uint8_t kSuspensionLastRow = 3u;

// The ball's centre, used only to place the travelling highlight.
constexpr uint8_t kBallCentreX = 7u;
constexpr uint8_t kBallCentreY = 8u;

// Eight highlight anchors, walking clockwise around the ball. Each pair is the
// anchor itself plus one neighbour, so the glint is two bright pixels - large
// enough to read through the diffuser, small enough not to bloom.
constexpr uint8_t kHighlightAnchorX[8] = { 5u, 7u, 10u, 11u, 10u, 7u, 5u, 4u };
constexpr uint8_t kHighlightAnchorY[8] = { 6u, 5u, 6u, 8u, 10u, 11u, 10u, 8u };
constexpr uint8_t kHighlightPixelCount = 2u;

// Four diagonal traces in the corners.
struct CornerBeam {
    uint8_t startX;
    uint8_t startY;
    int8_t dx;
    int8_t dy;
    uint8_t colorIndex;
    uint8_t phaseOffset;
};

constexpr CornerBeam kCornerBeams[4] = {
    { 0u, 0u, 1, 1, 0u, 0u },     // pink, upper-left
    { 15u, 0u, -1, 1, 1u, 2u },   // blue, upper-right
    { 15u, 15u, -1, -1, 2u, 4u }, // orange, lower-right
    { 0u, 15u, 1, -1, 3u, 6u }    // cyan, lower-left
};
constexpr uint8_t kCornerBeamCountSize = 4u;
constexpr uint8_t kCornerBeamLengthSize = 3u;

// A smooth, cyclic brightness curve: the wrap from the last entry to the first
// is the same size as every other step, so nothing ever blinks.
constexpr uint8_t kBeamLevels[8] = { 60u, 90u, 130u, 170u, 200u, 170u, 130u, 90u };

// Floor reflections, in a row the ball never reaches.
struct FloorPixel {
    uint8_t x;
    uint8_t y;
    uint8_t colorIndex;
    uint8_t phaseOffset;
};

constexpr FloorPixel kFloor[6] = {
    { 5u, 14u, 0u, 0u },
    { 7u, 14u, 1u, 1u },
    { 9u, 14u, 2u, 2u },
    { 11u, 14u, 3u, 3u },
    { 6u, 15u, 1u, 1u },
    { 10u, 15u, 2u, 2u }
};
constexpr uint8_t kFloorCountSize = 6u;

// A second curve, offset from the beam curve so the floor and the walls are
// never in lock step.
constexpr uint8_t kFloorLevels[8] = { 200u, 170u, 130u, 90u, 60u, 90u, 130u, 170u };

Color tileColor(uint8_t tileClass) {
    switch (tileClass) {
        case DiscoBallAnimation::kTileBlue:
            return kBallBlue;
        case DiscoBallAnimation::kTileViolet:
            return kBallViolet;
        case DiscoBallAnimation::kTileSilver:
            return kBallSilver;
        case DiscoBallAnimation::kTileDark:
        default:
            return kBallDark;
    }
}

Color beamColor(uint8_t colorIndex) {
    switch (colorIndex) {
        case 1u:
            return kBeamBlue;
        case 2u:
            return kBeamOrange;
        case 3u:
            return kBeamCyan;
        case 0u:
        default:
            return kBeamPink;
    }
}

void setPixelSafe(PixelMatrix& matrix, int16_t x, int16_t y, Color color) {
    if (x < 0 || y < 0 ||
        x >= static_cast<int16_t>(PixelMatrix::WIDTH) ||
        y >= static_cast<int16_t>(PixelMatrix::HEIGHT)) {
        return;
    }
    matrix.setPixel(static_cast<uint8_t>(x), static_cast<uint8_t>(y), color);
}

}  // namespace

DiscoBallAnimation::DiscoBallAnimation()
    : startedAt_(0u),
      lastPhase_(0xFFu),
      finished_(false) {
}

void DiscoBallAnimation::start(uint32_t now) {
    startedAt_ = now;
    lastPhase_ = 0xFFu;
    finished_ = false;
}

void DiscoBallAnimation::update(uint32_t now, PixelMatrix& matrix) {
    if (finished_) {
        return;
    }

    const uint32_t elapsed = static_cast<uint32_t>(now - startedAt_);
    if (elapsed >= kLoopDurationMs) {
        finished_ = true;
        return;
    }

    const uint8_t phase = phaseAtStep(static_cast<uint8_t>(elapsed / kStepIntervalMs));

    if (phase == lastPhase_) {
        return;
    }

    lastPhase_ = phase;
    renderFrame(matrix, phase);
}

bool DiscoBallAnimation::isFinished() const {
    return finished_;
}

void DiscoBallAnimation::stop() {
    startedAt_ = 0u;
    lastPhase_ = 0xFFu;
    finished_ = true;
}

void DiscoBallAnimation::renderFrame(PixelMatrix& matrix, uint8_t phase) const {
    matrix.clear();

    drawBackground(matrix);
    drawCornerBeams(matrix, phase);
    drawBall(matrix, phase);
    drawHighlight(matrix, phase);
    drawFloor(matrix, phase);
}

void DiscoBallAnimation::drawBackground(PixelMatrix& matrix) const {
    for (uint8_t y = 0u; y < PixelMatrix::HEIGHT; ++y) {
        for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
            matrix.setPixel(x, y, kBackground);
        }
    }
}

void DiscoBallAnimation::drawCornerBeams(PixelMatrix& matrix, uint8_t phase) const {
    for (uint8_t i = 0u; i < kCornerBeamCountSize; ++i) {
        const CornerBeam& beam = kCornerBeams[i];
        const Color color = scaleColor(beamColor(beam.colorIndex), cornerBeamLevel(i, phase));

        for (uint8_t step = 0u; step < kCornerBeamLengthSize; ++step) {
            setPixelSafe(
                matrix,
                static_cast<int16_t>(beam.startX) + static_cast<int16_t>(beam.dx) * step,
                static_cast<int16_t>(beam.startY) + static_cast<int16_t>(beam.dy) * step,
                color);
        }
    }
}

void DiscoBallAnimation::drawBall(PixelMatrix& matrix, uint8_t phase) const {
    // The wire first, so the ball's top row covers where it attaches.
    for (uint8_t y = kSuspensionFirstRow; y <= kSuspensionLastRow; ++y) {
        matrix.setPixel(kSuspensionX, y, kBallSilver);
    }

    // Every ball pixel is repainted every frame from the same table; only the
    // class it lands in changes with the phase. The shape never moves.
    for (uint8_t y = 0u; y < PixelMatrix::HEIGHT; ++y) {
        for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
            if (!ballContains(x, y)) {
                continue;
            }
            matrix.setPixel(x, y, tileColor(tileClassAt(x, y, phase)));
        }
    }
}

void DiscoBallAnimation::drawHighlight(PixelMatrix& matrix, uint8_t phase) const {
    const uint8_t ax = highlightAnchorX(phase);
    const uint8_t ay = highlightAnchorY(phase);

    // A silver halo under the glint keeps it from being a hard single pixel.
    for (uint8_t i = 0u; i < 4u; ++i) {
        const int16_t nx = static_cast<int16_t>(ax) + ((i == 0u) ? 1 : (i == 1u) ? -1 : 0);
        const int16_t ny = static_cast<int16_t>(ay) + ((i == 2u) ? 1 : (i == 3u) ? -1 : 0);
        if (nx < 0 || ny < 0) {
            continue;
        }
        if (ballContains(static_cast<uint8_t>(nx), static_cast<uint8_t>(ny))) {
            matrix.setPixel(static_cast<uint8_t>(nx), static_cast<uint8_t>(ny), kBallSilver);
        }
    }

    // Two bright pixels: the anchor and the neighbour on the ball-centre side
    // of it, so the glint always points inward.
    setPixelSafe(matrix, ax, ay, kHighlight);

    const int16_t secondX = static_cast<int16_t>(ax) +
        ((ax > kBallCentreX) ? -1 : (ax < kBallCentreX ? 1 : 0));
    const int16_t secondY = static_cast<int16_t>(ay) +
        ((ay > kBallCentreY) ? -1 : (ay < kBallCentreY ? 1 : 0));
    if (ballContains(static_cast<uint8_t>(secondX), static_cast<uint8_t>(secondY))) {
        setPixelSafe(matrix, secondX, secondY, kHighlight);
    }
}

void DiscoBallAnimation::drawFloor(PixelMatrix& matrix, uint8_t phase) const {
    for (uint8_t i = 0u; i < kFloorCountSize; ++i) {
        const FloorPixel& pixel = kFloor[i];
        setPixelSafe(
            matrix,
            pixel.x,
            pixel.y,
            scaleColor(beamColor(pixel.colorIndex), floorLevel(i, phase)));
    }
}

// ---------------------------------------------------------------------------
// Derived scene queries
// ---------------------------------------------------------------------------

uint8_t DiscoBallAnimation::phaseAtStep(uint8_t step) {
    return static_cast<uint8_t>(step % kPhaseCount);
}

uint8_t DiscoBallAnimation::phaseCount() {
    return kPhaseCount;
}

bool DiscoBallAnimation::ballContains(uint8_t x, uint8_t y) {
    return maskHasPixel(&kBallMask, x, y);
}

bool DiscoBallAnimation::suspensionContains(uint8_t x, uint8_t y) {
    return x == kSuspensionX &&
           y >= kSuspensionFirstRow && y <= kSuspensionLastRow;
}

uint8_t DiscoBallAnimation::tileClassAt(uint8_t x, uint8_t y, uint8_t phase) {
    return static_cast<uint8_t>((x + y + phase) % kTileClassCount);
}

uint8_t DiscoBallAnimation::highlightAnchorX(uint8_t phase) {
    return kHighlightAnchorX[phase % kPhaseCount];
}

uint8_t DiscoBallAnimation::highlightAnchorY(uint8_t phase) {
    return kHighlightAnchorY[phase % kPhaseCount];
}

uint8_t DiscoBallAnimation::highlightPixelCount() {
    return kHighlightPixelCount;
}

uint8_t DiscoBallAnimation::cornerBeamCount() {
    return kCornerBeamCountSize;
}

uint8_t DiscoBallAnimation::cornerBeamLength() {
    return kCornerBeamLengthSize;
}

uint8_t DiscoBallAnimation::cornerBeamX(uint8_t index, uint8_t step) {
    const CornerBeam& beam = kCornerBeams[index % kCornerBeamCountSize];
    return static_cast<uint8_t>(
        static_cast<int16_t>(beam.startX) + static_cast<int16_t>(beam.dx) * (step % kCornerBeamLengthSize));
}

uint8_t DiscoBallAnimation::cornerBeamY(uint8_t index, uint8_t step) {
    const CornerBeam& beam = kCornerBeams[index % kCornerBeamCountSize];
    return static_cast<uint8_t>(
        static_cast<int16_t>(beam.startY) + static_cast<int16_t>(beam.dy) * (step % kCornerBeamLengthSize));
}

uint8_t DiscoBallAnimation::cornerBeamLevel(uint8_t index, uint8_t phase) {
    const uint8_t offset = kCornerBeams[index % kCornerBeamCountSize].phaseOffset;
    return kBeamLevels[(phase + offset) % kPhaseCount];
}

uint8_t DiscoBallAnimation::floorCount() {
    return kFloorCountSize;
}

uint8_t DiscoBallAnimation::floorX(uint8_t index) {
    return kFloor[index % kFloorCountSize].x;
}

uint8_t DiscoBallAnimation::floorY(uint8_t index) {
    return kFloor[index % kFloorCountSize].y;
}

uint8_t DiscoBallAnimation::floorLevel(uint8_t index, uint8_t phase) {
    const uint8_t offset = kFloor[index % kFloorCountSize].phaseOffset;
    return kFloorLevels[(phase + offset) % kPhaseCount];
}
