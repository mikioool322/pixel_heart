#include "animations/BlueSkyCloudsAnimation.h"

#include "assets/PixelMask.h"
#include "core/Color.h"
#include "core/PixelMatrix.h"

#if defined(ARDUINO_ARCH_AVR)
#include <avr/pgmspace.h>
#endif

namespace {

#if defined(ARDUINO_ARCH_AVR)
#define SKY_MASK_STORAGE PROGMEM
#else
#define SKY_MASK_STORAGE
#endif

// ---------------------------------------------------------------------------
// Palette. The sky is a large mid-brightness area and the clouds are close to it,
// so nothing blooms into a white plate: the cloud "highlight" is warm grey, not
// white.
// ---------------------------------------------------------------------------
constexpr Color kSkyTop{15u, 120u, 205u};   // rows 0..7, lighter
constexpr Color kSkyLow{10u, 95u, 170u};    // rows 8..15, slightly deeper

constexpr Color kCloudHighlight{225u, 230u, 225u};
constexpr Color kCloudBody{180u, 205u, 220u};
constexpr Color kCloudShadow{110u, 150u, 185u};

// ---------------------------------------------------------------------------
// Clouds (logical x=0 -> bit 15, matching PixelMask.h).
//
// Each mask is drawn with its row 0 at the cloud's baseY, so the row index also
// selects the shading band.
// ---------------------------------------------------------------------------

// Cloud A - large, upper lane, drifts LEFT -> RIGHT.
static const PixelMask16 kCloudA SKY_MASK_STORAGE = {{
    0x3000, 0x7800, 0xFC00, 0xFF00,
    0x7800, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
}};

// Cloud B - medium, middle lane, drifts RIGHT -> LEFT.
static const PixelMask16 kCloudB SKY_MASK_STORAGE = {{
    0x7800, 0xFC00, 0x7800, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
}};

// Cloud C - large, lower lane, drifts LEFT -> RIGHT.
static const PixelMask16 kCloudC SKY_MASK_STORAGE = {{
    0x3C00, 0x7F00, 0xFF80, 0x7F00,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
}};

#undef SKY_MASK_STORAGE

// ---------------------------------------------------------------------------
// Trajectories. Every cloud walks the SAME 32-step cycle, just with a different
// start, end and phase offset, which is what guarantees the closed loop.
//
//   A: right-moving, 8 px wide, -8 .. 16
//   B: left-moving,  6 px wide, 16 .. -6
//   C: right-moving, 9 px wide, -9 .. 16
//
// Start and end are both fully outside the panel, so a cloud is never visible
// when it wraps around from the last phase to the first.
// ---------------------------------------------------------------------------
struct CloudDefinition {
    uint8_t baseY;
    uint8_t height;
    uint8_t movesRight;
    uint8_t phaseOffset;
    int8_t startX;
    int8_t endX;
};

constexpr CloudDefinition kClouds[BlueSkyCloudsAnimation::kCloudCount] = {
    { 2u, 5u, 1u, 0u, -8, 16 },
    { 7u, 3u, 0u, 11u, 16, -6 },
    { 11u, 4u, 1u, 21u, -9, 16 }
};

const PixelMask16* const kCloudMasks[BlueSkyCloudsAnimation::kCloudCount] = {
    &kCloudA, &kCloudB, &kCloudC
};

// Highlight on the top row, shadow on the bottom, body in between.
Color cloudShade(uint8_t row, uint8_t height) {
    if (row == 0u) {
        return kCloudHighlight;
    }
    if (static_cast<uint8_t>(row + 1u) >= height) {
        return kCloudShadow;
    }
    return kCloudBody;
}

}  // namespace

BlueSkyCloudsAnimation::BlueSkyCloudsAnimation()
    : startedAt_(0u),
      lastPhase_(0xFFu),
      finished_(false) {
}

void BlueSkyCloudsAnimation::start(uint32_t now) {
    startedAt_ = now;
    lastPhase_ = 0xFFu;
    finished_ = false;
}

void BlueSkyCloudsAnimation::update(uint32_t now, PixelMatrix& matrix) {
    if (finished_) {
        return;
    }

    const uint32_t elapsed = static_cast<uint32_t>(now - startedAt_);
    if (elapsed >= kLoopDurationMs) {
        finished_ = true;
        return;
    }

    const uint8_t phase = phaseAtElapsed(elapsed);

    // Dirty-frame preservation: only repaint when the visible phase changes.
    if (phase == lastPhase_) {
        return;
    }

    lastPhase_ = phase;
    renderFrame(matrix, phase);
}

bool BlueSkyCloudsAnimation::isFinished() const {
    return finished_;
}

void BlueSkyCloudsAnimation::stop() {
    startedAt_ = 0u;
    lastPhase_ = 0xFFu;
    finished_ = true;
}

void BlueSkyCloudsAnimation::renderFrame(PixelMatrix& matrix, uint8_t phase) const {
    matrix.clear();
    drawSky(matrix);
    drawClouds(matrix, phase);
}

void BlueSkyCloudsAnimation::drawSky(PixelMatrix& matrix) const {
    for (uint8_t y = 0u; y < PixelMatrix::HEIGHT; ++y) {
        const Color sky = (y < 8u) ? kSkyTop : kSkyLow;
        for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
            matrix.setPixel(x, y, sky);
        }
    }
}

void BlueSkyCloudsAnimation::drawClouds(PixelMatrix& matrix, uint8_t phase) const {
    for (uint8_t i = 0u; i < kCloudCount; ++i) {
        drawCloud(matrix, i, cloudXAt(i, phase));
    }
}

void BlueSkyCloudsAnimation::drawCloud(PixelMatrix& matrix, uint8_t index, int16_t x) const {
    const CloudDefinition& cloud = kClouds[index % kCloudCount];
    const PixelMask16* mask = kCloudMasks[index % kCloudCount];

    for (uint8_t row = 0u; row < cloud.height; ++row) {
        const Color color = cloudShade(row, cloud.height);

        for (uint8_t localX = 0u; localX < PixelMatrix::WIDTH; ++localX) {
            if (!maskHasPixel(mask, localX, row)) {
                continue;
            }

            const int16_t px = x + static_cast<int16_t>(localX);
            const int16_t py = static_cast<int16_t>(cloud.baseY) + static_cast<int16_t>(row);
            if (px < 0 || py < 0 ||
                px >= static_cast<int16_t>(PixelMatrix::WIDTH) ||
                py >= static_cast<int16_t>(PixelMatrix::HEIGHT)) {
                continue;
            }

            matrix.setPixel(static_cast<uint8_t>(px), static_cast<uint8_t>(py), color);
        }
    }
}

// ---------------------------------------------------------------------------
// Derived scene queries
// ---------------------------------------------------------------------------

uint8_t BlueSkyCloudsAnimation::phaseAtElapsed(uint32_t elapsedMs) {
    return static_cast<uint8_t>((elapsedMs / kStepMs) % kPhaseCount);
}

uint8_t BlueSkyCloudsAnimation::cloudBaseY(uint8_t index) {
    return kClouds[index % kCloudCount].baseY;
}

uint8_t BlueSkyCloudsAnimation::cloudHeight(uint8_t index) {
    return kClouds[index % kCloudCount].height;
}

bool BlueSkyCloudsAnimation::cloudMovesRight(uint8_t index) {
    return kClouds[index % kCloudCount].movesRight != 0u;
}

uint8_t BlueSkyCloudsAnimation::cloudPhaseOffset(uint8_t index) {
    return kClouds[index % kCloudCount].phaseOffset;
}

int8_t BlueSkyCloudsAnimation::cloudStartX(uint8_t index) {
    return kClouds[index % kCloudCount].startX;
}

int8_t BlueSkyCloudsAnimation::cloudEndX(uint8_t index) {
    return kClouds[index % kCloudCount].endX;
}

int16_t BlueSkyCloudsAnimation::cloudXAt(uint8_t index, uint8_t phase) {
    const CloudDefinition& cloud = kClouds[index % kCloudCount];

    const uint8_t offset = cloud.phaseOffset;
    const uint8_t local = static_cast<uint8_t>((phase + offset) % kPhaseCount);

    const int16_t distance = static_cast<int16_t>(cloud.endX) - static_cast<int16_t>(cloud.startX);

    // Both operands must stay SIGNED: mixing int32_t with the unsigned literal
    // form of the divisor would turn this into an unsigned division and mangle
    // every left-moving cloud's negative distance.
    const int32_t span = static_cast<int32_t>(kPhaseCount) - 1;
    const int32_t travelled =
        (static_cast<int32_t>(distance) * static_cast<int32_t>(local)) / span;

    return static_cast<int16_t>(static_cast<int16_t>(cloud.startX) + static_cast<int16_t>(travelled));
}

bool BlueSkyCloudsAnimation::cloudContains(uint8_t index, uint8_t localX, uint8_t localY) {
    if (localY >= kClouds[index % kCloudCount].height) {
        return false;
    }
    return maskHasPixel(kCloudMasks[index % kCloudCount], localX, localY);
}

bool BlueSkyCloudsAnimation::skyContains(uint8_t x, uint8_t y) {
    (void)x;
    return y < PixelMatrix::HEIGHT;
}
