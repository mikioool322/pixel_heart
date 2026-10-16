#include "animations/JellyfishAnimation.h"

#include "assets/PixelMask.h"
#include "core/Color.h"
#include "core/PixelMatrix.h"

#if defined(ARDUINO_ARCH_AVR)
#include <avr/pgmspace.h>
#endif

namespace {

#if defined(ARDUINO_ARCH_AVR)
#define JELLY_MASK_STORAGE PROGMEM
#else
#define JELLY_MASK_STORAGE
#endif

// ---------------------------------------------------------------------------
// Palette. Restrained on purpose: the diffuser blooms bright neighbours, so the
// bell is the strongest shape and the tentacles sit well below it in brightness.
// ---------------------------------------------------------------------------
constexpr Color kDeepBlue{20u, 55u, 150u};
constexpr Color kBlue{40u, 110u, 235u};
constexpr Color kCyan{70u, 190u, 255u};
constexpr Color kViolet{125u, 65u, 220u};
constexpr Color kPinkViolet{190u, 95u, 235u};
constexpr Color kHighlight{185u, 220u, 255u};
constexpr Color kParticle{50u, 90u, 190u};

// Dimmer versions of the bell colours, so the trailing tentacles never compete
// with the bell for attention.
constexpr Color kTentacleCyan{40u, 120u, 175u};
constexpr Color kTentacleViolet{90u, 60u, 150u};

// ---------------------------------------------------------------------------
// Bell. Rows 1..8 of the mask, drawn at a vertical offset of at most one pixel.
// ---------------------------------------------------------------------------
static const PixelMask16 kBellMask JELLY_MASK_STORAGE = {{
    0x0000, 0x03C0, 0x0FF0, 0x1FF8,
    0x3FFC, 0x3FFC, 0x3FFC, 0x1FF8,
    0x0C30, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
}};

// Brighter inner dome: the "lit from inside" reading.
static const PixelMask16 kBellCoreMask JELLY_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x07E0,
    0x07E0, 0x07E0, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
}};

// Violet lower rim, and its pink-violet tatters underneath.
static const PixelMask16 kBellRimMask JELLY_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x1FF8,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
}};

static const PixelMask16 kBellTatterMask JELLY_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0C30, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
}};

// Three specular pixels on the upper dome.
constexpr uint8_t kHighlightX[3] = { 8u, 6u, 9u };
constexpr uint8_t kHighlightY[3] = { 2u, 4u, 4u };
constexpr uint8_t kHighlightCount =
    static_cast<uint8_t>(sizeof(kHighlightX) / sizeof(kHighlightX[0]));

#undef JELLY_MASK_STORAGE

// ---------------------------------------------------------------------------
// Motion tables
// ---------------------------------------------------------------------------

// CENTER -> UP -> CENTER -> DOWN. One entry per three render ticks (600 ms).
constexpr int8_t kVertical[4] = { 0, -1, 0, 1 };

// The bell breathes by scaling ONLY its own colours; global brightness is never
// touched.
constexpr uint8_t kPulse[4] = { 180u, 205u, 230u, 205u };

// The tentacle wave. Consecutive table entries - and therefore consecutive
// poses - differ by at most one pixel, so no pose ever snaps sideways.
constexpr int8_t kWave[4] = { 0, 1, 0, -1 };
constexpr uint8_t kWaveLength = 4u;

struct Tentacle {
    uint8_t anchorX;
    uint8_t length;   // number of rows it occupies
    uint8_t phase;
    uint8_t violet;   // 0 = cyan shade, 1 = violet shade
};

// Four tentacles hanging from the bell's row-8 tatters (x = 4, 5, 10, 11).
// Alternate lengths and colours so the fringe does not read as a comb.
constexpr Tentacle kTentacles[4] = {
    { 4u, 7u, 0u, 0u },
    { 5u, 6u, 1u, 1u },
    { 10u, 7u, 2u, 1u },
    { 11u, 6u, 3u, 0u }
};
constexpr uint8_t kTentacleCount =
    static_cast<uint8_t>(sizeof(kTentacles) / sizeof(kTentacles[0]));

// The bell's lowest tatter row. Tentacles start on the row below it.
constexpr uint8_t kBellBottomRow = 8u;
constexpr uint8_t kTentacleTopRow = kBellBottomRow + 1u;

// ---------------------------------------------------------------------------
// Floating particles. Placed only in the columns OUTSIDE the bell (x < 2 or
// x > 13), so a particle can never touch or merge with it.
// ---------------------------------------------------------------------------
struct Particle {
    uint8_t x;
    uint8_t baseY;
    uint8_t phase;
};

constexpr Particle kParticles[5] = {
    { 0u, 6u, 0u },
    { 15u, 4u, 1u },
    { 1u, 12u, 2u },
    { 14u, 10u, 3u },
    { 0u, 2u, 1u }
};
constexpr uint8_t kParticleCount =
    static_cast<uint8_t>(sizeof(kParticles) / sizeof(kParticles[0]));

constexpr int8_t kParticleBob[4] = { 0, -1, 0, 1 };
constexpr uint8_t kParticleLevel[4] = { 110u, 150u, 110u, 70u };

constexpr uint8_t kParticleTickDivisor = 2u;   // one particle tick per 400 ms

void setPixelSafe(PixelMatrix& matrix, int16_t x, int16_t y, Color color) {
    if (x < 0 || y < 0 ||
        x >= static_cast<int16_t>(PixelMatrix::WIDTH) ||
        y >= static_cast<int16_t>(PixelMatrix::HEIGHT)) {
        return;
    }
    matrix.setPixel(static_cast<uint8_t>(x), static_cast<uint8_t>(y), color);
}

}  // namespace

JellyfishAnimation::JellyfishAnimation()
    : startedAt_(0u),
      lastStep_(0xFFu),
      finished_(false) {
}

void JellyfishAnimation::start(uint32_t now) {
    startedAt_ = now;
    lastStep_ = 0xFFu;
    finished_ = false;
}

void JellyfishAnimation::update(uint32_t now, PixelMatrix& matrix) {
    if (finished_) {
        return;
    }

    const uint32_t elapsed = static_cast<uint32_t>(now - startedAt_);
    if (elapsed >= kLoopDurationMs) {
        finished_ = true;
        return;
    }

    const uint8_t step = static_cast<uint8_t>((elapsed / kStepIntervalMs) % kStepCount);

    // Dirty-frame preservation: only repaint when the visible step changes.
    if (step == lastStep_) {
        return;
    }

    lastStep_ = step;
    renderFrame(matrix, step);
}

bool JellyfishAnimation::isFinished() const {
    return finished_;
}

void JellyfishAnimation::stop() {
    startedAt_ = 0u;
    lastStep_ = 0xFFu;
    finished_ = true;
}

void JellyfishAnimation::renderFrame(PixelMatrix& matrix, uint8_t step) const {
    matrix.clear();
    drawParticles(matrix, step);
    drawBell(matrix, step);
    drawTentacles(matrix, step);
}

void JellyfishAnimation::drawParticles(PixelMatrix& matrix, uint8_t step) const {
    for (uint8_t i = 0u; i < kParticleCount; ++i) {
        const uint8_t level = particleAtLevel(i, step);
        setPixelSafe(
            matrix,
            static_cast<int16_t>(particleAtX(i)),
            static_cast<int16_t>(particleAtY(i, step)),
            scaleColor(kParticle, level));
    }
}

void JellyfishAnimation::drawBell(PixelMatrix& matrix, uint8_t step) const {
    const int16_t dy = static_cast<int16_t>(verticalOffsetAt(step));
    const uint8_t pulse = pulseLevelAt(step);

    // Outer dome, then the brighter inner dome, then the violet rim and its
    // pink-violet tatters. Only the brightness of these local colours changes;
    // the geometry is fixed.
    drawMask(&kBellMask, 0, dy, scaleColor(kBlue, pulse), matrix);
    drawMask(&kBellCoreMask, 0, dy, scaleColor(kCyan, pulse), matrix);
    drawMask(&kBellRimMask, 0, dy, scaleColor(kViolet, pulse), matrix);
    drawMask(&kBellTatterMask, 0, dy, scaleColor(kPinkViolet, pulse), matrix);

    // A few specular pixels keep the dome from reading as a flat blob.
    for (uint8_t i = 0u; i < kHighlightCount; ++i) {
        setPixelSafe(
            matrix,
            static_cast<int16_t>(kHighlightX[i]),
            static_cast<int16_t>(kHighlightY[i]) + dy,
            scaleColor(kHighlight, pulse));
    }
}

void JellyfishAnimation::drawTentacles(PixelMatrix& matrix, uint8_t step) const {
    const uint8_t pose = tentaclePoseAt(step);
    const int16_t dy = static_cast<int16_t>(verticalOffsetAt(step));

    for (uint8_t t = 0u; t < kTentacleCount; ++t) {
        const Color color = (kTentacles[t].violet != 0u) ? kTentacleViolet : kTentacleCyan;

        for (uint8_t row = 0u; row < kTentacles[t].length; ++row) {
            const int16_t x = static_cast<int16_t>(kTentacles[t].anchorX) +
                              static_cast<int16_t>(tentacleOffsetAt(t, row, pose));
            const int16_t y = static_cast<int16_t>(kTentacleTopRow) +
                              static_cast<int16_t>(row) + dy;
            setPixelSafe(matrix, x, y, color);
        }
    }
}

// ---------------------------------------------------------------------------
// Derived scene queries
// ---------------------------------------------------------------------------

bool JellyfishAnimation::bellContains(uint8_t x, uint8_t y) {
    return maskHasPixel(&kBellMask, x, y);
}

uint8_t JellyfishAnimation::highlightCount() {
    return kHighlightCount;
}

uint8_t JellyfishAnimation::highlightX(uint8_t index) {
    return kHighlightX[index % kHighlightCount];
}

uint8_t JellyfishAnimation::highlightY(uint8_t index) {
    return kHighlightY[index % kHighlightCount];
}

int8_t JellyfishAnimation::verticalOffsetAt(uint8_t step) {
    return kVertical[(step / kMotionIntervalSteps) % kMotionCount];
}

uint8_t JellyfishAnimation::pulseLevelAt(uint8_t step) {
    return kPulse[(step / kMotionIntervalSteps) % kMotionCount];
}

uint8_t JellyfishAnimation::tentaclePoseAt(uint8_t step) {
    return static_cast<uint8_t>(step % kTentaclePoseCount);
}

uint8_t JellyfishAnimation::tentacleCount() {
    return kTentacleCount;
}

uint8_t JellyfishAnimation::tentacleAnchorX(uint8_t index) {
    return kTentacles[index % kTentacleCount].anchorX;
}

uint8_t JellyfishAnimation::tentacleLength(uint8_t index) {
    return kTentacles[index % kTentacleCount].length;
}

int8_t JellyfishAnimation::tentacleOffsetAt(uint8_t index, uint8_t rowIndex, uint8_t pose) {
    const uint8_t phase = kTentacles[index % kTentacleCount].phase;
    const uint8_t waveIndex = static_cast<uint8_t>(
        (rowIndex + pose + phase) % kWaveLength);
    return kWave[waveIndex];
}

uint8_t JellyfishAnimation::particleCount() {
    return kParticleCount;
}

uint8_t JellyfishAnimation::particleAtX(uint8_t index) {
    return kParticles[index % kParticleCount].x;
}

uint8_t JellyfishAnimation::particleAtBaseY(uint8_t index) {
    return kParticles[index % kParticleCount].baseY;
}

uint8_t JellyfishAnimation::particleAtY(uint8_t index, uint8_t step) {
    const uint8_t phase = kParticles[index % kParticleCount].phase;
    const uint8_t tick = static_cast<uint8_t>(
        (step / kParticleTickDivisor + phase) % 4u);
    const int16_t y = static_cast<int16_t>(particleAtBaseY(index)) + kParticleBob[tick];
    return static_cast<uint8_t>(y < 0 ? 0 : y);
}

uint8_t JellyfishAnimation::particleAtLevel(uint8_t index, uint8_t step) {
    const uint8_t phase = kParticles[index % kParticleCount].phase;
    const uint8_t tick = static_cast<uint8_t>(
        (step / kParticleTickDivisor + phase) % 4u);
    return kParticleLevel[tick];
}
