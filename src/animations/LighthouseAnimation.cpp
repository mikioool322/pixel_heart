#include "animations/LighthouseAnimation.h"

#include "assets/PixelMask.h"
#include "core/Color.h"
#include "core/PixelMatrix.h"

#if defined(ARDUINO_ARCH_AVR)
#include <avr/pgmspace.h>
#endif

namespace {

#if defined(ARDUINO_ARCH_AVR)
#define LIGHT_MASK_STORAGE PROGMEM
#else
#define LIGHT_MASK_STORAGE
#endif

// ---------------------------------------------------------------------------
// Palette. The whole scene except the beam stays very dark, so the warm wedge
// is the only thing that carries the eye - which is what survives a diffuser.
// ---------------------------------------------------------------------------
constexpr Color kNightSky{5u, 12u, 38u};
constexpr Color kSkyHighlight{8u, 22u, 60u};
constexpr Color kSea{5u, 40u, 95u};
constexpr Color kSeaHighlight{15u, 75u, 150u};

constexpr Color kRock{18u, 20u, 28u};
constexpr Color kTowerWhite{180u, 170u, 155u};
constexpr Color kTowerRed{170u, 45u, 32u};
constexpr Color kRoof{35u, 22u, 18u};
constexpr Color kBeacon{255u, 185u, 55u};
constexpr Color kStar{120u, 145u, 195u};

constexpr Color kBeamDim{80u, 55u, 18u};
constexpr Color kBeamMedium{155u, 105u, 25u};
constexpr Color kBeamBright{235u, 165u, 45u};

// ---------------------------------------------------------------------------
// Scene bands
// ---------------------------------------------------------------------------
constexpr uint8_t kSkyHighlightFirstRow = 8u;   // rows 8..12 lighter near the horizon
constexpr uint8_t kSeaHighlightRow = 13u;

// ---------------------------------------------------------------------------
// Lighthouse geometry (logical x=0 -> bit 15, matching PixelMask.h)
//   tower  : x 2..5, rows 5..13
//   roof   : rows 4..5
//   beacon : rows 6..7
//   bands  : rows 8..13, alternating white and red
//   base   : rows 14..15, a rock footing that widens to x 0..7
// ---------------------------------------------------------------------------
static const PixelMask16 kRoofDarkMask LIGHT_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0000,
    0x1800, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
}};

static const PixelMask16 kRoofRedMask LIGHT_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x3C00, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
}};

static const PixelMask16 kBeaconMask LIGHT_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x3C00, 0x3C00,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
}};

static const PixelMask16 kBandWhiteMask LIGHT_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x3C00, 0x0000, 0x3C00, 0x0000,
    0x3C00, 0x3C00, 0x0000, 0x0000
}};

static const PixelMask16 kBandRedMask LIGHT_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x3C00, 0x0000, 0x3C00,
    0x0000, 0x0000, 0x0000, 0x0000
}};

static const PixelMask16 kBaseMask LIGHT_MASK_STORAGE = {{
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x7E00, 0xFF00
}};

#undef LIGHT_MASK_STORAGE

// Five fixed stars in the upper sky.
constexpr uint8_t kStarX[5] = { 1u, 10u, 13u, 0u, 15u };
constexpr uint8_t kStarY[5] = { 1u, 0u, 2u, 4u, 3u };
constexpr uint8_t kStarCount = static_cast<uint8_t>(sizeof(kStarX) / sizeof(kStarX[0]));

// Three water reflections under the beam, strongest first.
constexpr uint8_t kReflectionX[3] = { 9u, 11u, 13u };
constexpr uint8_t kReflectionScale[3] = { 255u, 200u, 150u };
constexpr uint8_t kReflectionCount =
    static_cast<uint8_t>(sizeof(kReflectionX) / sizeof(kReflectionX[0]));

// The beam never reaches the last row, which is what keeps the "sea static
// except the reflections" invariant testable.
constexpr uint8_t kReflectionRow = 15u;

// Elevation 0..6 maps to a far-edge displacement from the beacon. Negative is
// up, positive is down. Consecutive elevations move by one band, so the sweep
// never jumps.
constexpr int8_t kElevationOffset[7] = { -5, -3, -2, 0, 2, 3, 5 };

// 0 -> 6 -> 1, so the wrap from the last step back to the first is a single
// band change and the beacon never appears to snap.
constexpr uint8_t kBeamSequence[12] = { 0u, 1u, 2u, 3u, 4u, 5u, 6u, 5u, 4u, 3u, 2u, 1u };

// The beam widens by one pixel per four columns of travel: 3 px at the beacon,
// 7 px at the far edge.
constexpr uint8_t kBeamWidthDivisor = 4u;

// A downward beam brightens the water more than an upward one.
constexpr uint8_t kReflectionLevel[7] = { 50u, 80u, 100u, 130u, 160u, 190u, 220u };

void setPixelSafe(PixelMatrix& matrix, int16_t x, int16_t y, Color color) {
    if (x < 0 || y < 0 ||
        x >= static_cast<int16_t>(PixelMatrix::WIDTH) ||
        y >= static_cast<int16_t>(PixelMatrix::HEIGHT)) {
        return;
    }
    matrix.setPixel(static_cast<uint8_t>(x), static_cast<uint8_t>(y), color);
}

}  // namespace

LighthouseAnimation::LighthouseAnimation()
    : startedAt_(0u),
      lastStep_(0xFFu),
      finished_(false) {
}

void LighthouseAnimation::start(uint32_t now) {
    startedAt_ = now;
    lastStep_ = 0xFFu;
    finished_ = false;
}

void LighthouseAnimation::update(uint32_t now, PixelMatrix& matrix) {
    if (finished_) {
        return;
    }

    const uint32_t elapsed = static_cast<uint32_t>(now - startedAt_);
    if (elapsed >= kLoopDurationMs) {
        finished_ = true;
        return;
    }

    const uint8_t step = static_cast<uint8_t>((elapsed / kStepIntervalMs) % kStepCount);

    if (step == lastStep_) {
        return;
    }

    lastStep_ = step;
    renderFrame(matrix, step);
}

bool LighthouseAnimation::isFinished() const {
    return finished_;
}

void LighthouseAnimation::stop() {
    startedAt_ = 0u;
    lastStep_ = 0xFFu;
    finished_ = true;
}

void LighthouseAnimation::renderFrame(PixelMatrix& matrix, uint8_t step) const {
    matrix.clear();

    drawSky(matrix);
    drawSea(matrix);
    drawStars(matrix);
    drawLighthouse(matrix);

    const uint8_t elevation = elevationAtStep(step);
    drawBeam(matrix, elevation);
    drawReflections(matrix, elevation);
}

void LighthouseAnimation::drawSky(PixelMatrix& matrix) const {
    for (uint8_t y = 0u; y < kSeaHighlightRow; ++y) {
        const Color shade = (y >= kSkyHighlightFirstRow) ? kSkyHighlight : kNightSky;
        for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
            matrix.setPixel(x, y, shade);
        }
    }
}

void LighthouseAnimation::drawSea(PixelMatrix& matrix) const {
    for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
        matrix.setPixel(x, kSeaHighlightRow, kSeaHighlight);
        for (uint8_t y = static_cast<uint8_t>(kSeaHighlightRow + 1u);
             y < PixelMatrix::HEIGHT; ++y) {
            matrix.setPixel(x, y, kSea);
        }
    }
}

void LighthouseAnimation::drawStars(PixelMatrix& matrix) const {
    for (uint8_t i = 0u; i < kStarCount; ++i) {
        matrix.setPixel(kStarX[i], kStarY[i], kStar);
    }
}

void LighthouseAnimation::drawLighthouse(PixelMatrix& matrix) const {
    drawMask(&kRoofDarkMask, 0, 0, kRoof, matrix);
    drawMask(&kRoofRedMask, 0, 0, kTowerRed, matrix);
    drawMask(&kBeaconMask, 0, 0, kBeacon, matrix);
    drawMask(&kBandWhiteMask, 0, 0, kTowerWhite, matrix);
    drawMask(&kBandRedMask, 0, 0, kTowerRed, matrix);
    drawMask(&kBaseMask, 0, 0, kRock, matrix);
}

void LighthouseAnimation::drawBeam(PixelMatrix& matrix, uint8_t elevation) const {
    for (uint8_t x = static_cast<uint8_t>(kBeamOriginX + 1u); x <= kBeamFarX; ++x) {
        const uint8_t centreY = beamCentreY(elevation, x);
        const uint8_t halfWidth = beamHalfWidth(x);

        for (uint8_t band = 0u; band <= halfWidth; ++band) {
            Color color = kBeamDim;
            if (band == 0u) {
                color = kBeamBright;
            } else if (band == 1u) {
                color = kBeamMedium;
            }

            setPixelSafe(matrix, x, static_cast<int16_t>(centreY) - band, color);
            if (band != 0u) {
                setPixelSafe(matrix, x, static_cast<int16_t>(centreY) + band, color);
            }
        }
    }
}

void LighthouseAnimation::drawReflections(PixelMatrix& matrix, uint8_t elevation) const {
    const uint8_t level = reflectionLevel(elevation, 0u);
    if (level == 0u) {
        return;
    }

    for (uint8_t i = 0u; i < kReflectionCount; ++i) {
        setPixelSafe(
            matrix,
            static_cast<int16_t>(kReflectionX[i]),
            kReflectionRow,
            scaleColor(kBeamBright, reflectionLevel(elevation, i)));
    }
}

// ---------------------------------------------------------------------------
// Derived scene queries
// ---------------------------------------------------------------------------

uint8_t LighthouseAnimation::elevationAtStep(uint8_t step) {
    return kBeamSequence[step % kStepCount];
}

int8_t LighthouseAnimation::elevationOffset(uint8_t elevation) {
    return kElevationOffset[elevation % kBeamElevationCount];
}

uint8_t LighthouseAnimation::beamCentreY(uint8_t elevation, uint8_t x) {
    const int16_t distance = static_cast<int16_t>(x) - static_cast<int16_t>(kBeamOriginX);
    if (distance <= 0) {
        return kBeamOriginY;
    }

    const int16_t span = static_cast<int16_t>(kBeamFarX) - static_cast<int16_t>(kBeamOriginX);
    const int16_t offset = static_cast<int16_t>(elevationOffset(elevation)) * distance / span;

    const int16_t centre = static_cast<int16_t>(kBeamOriginY) + offset;
    if (centre < 0) {
        return 0u;
    }
    if (centre > static_cast<int16_t>(PixelMatrix::HEIGHT - 1u)) {
        return static_cast<uint8_t>(PixelMatrix::HEIGHT - 1u);
    }
    return static_cast<uint8_t>(centre);
}

uint8_t LighthouseAnimation::beamHalfWidth(uint8_t x) {
    const int16_t distance = static_cast<int16_t>(x) - static_cast<int16_t>(kBeamOriginX);
    if (distance <= 0) {
        return 1u;
    }
    return static_cast<uint8_t>(1u + static_cast<uint16_t>(distance) / kBeamWidthDivisor);
}

bool LighthouseAnimation::towerContains(uint8_t x, uint8_t y) {
    return maskHasPixel(&kRoofDarkMask, x, y) ||
           maskHasPixel(&kRoofRedMask, x, y) ||
           maskHasPixel(&kBeaconMask, x, y) ||
           maskHasPixel(&kBandWhiteMask, x, y) ||
           maskHasPixel(&kBandRedMask, x, y);
}

bool LighthouseAnimation::beaconContains(uint8_t x, uint8_t y) {
    return maskHasPixel(&kBeaconMask, x, y);
}

bool LighthouseAnimation::baseContains(uint8_t x, uint8_t y) {
    return maskHasPixel(&kBaseMask, x, y);
}

uint8_t LighthouseAnimation::starCount() {
    return kStarCount;
}

uint8_t LighthouseAnimation::starX(uint8_t index) {
    return kStarX[index % kStarCount];
}

uint8_t LighthouseAnimation::starY(uint8_t index) {
    return kStarY[index % kStarCount];
}

uint8_t LighthouseAnimation::reflectionCount() {
    return kReflectionCount;
}

uint8_t LighthouseAnimation::reflectionX(uint8_t index) {
    return kReflectionX[index % kReflectionCount];
}

uint8_t LighthouseAnimation::reflectionLevel(uint8_t elevation, uint8_t index) {
    const uint8_t base = kReflectionLevel[elevation % kBeamElevationCount];
    const uint16_t scaled =
        static_cast<uint16_t>(base) * kReflectionScale[index % kReflectionCount] / 255u;
    return static_cast<uint8_t>(scaled);
}
