#include "animations/MoonStarsAnimation.h"

#include "assets/PixelMask.h"
#include "core/Color.h"
#include "core/PixelMatrix.h"

#if defined(ARDUINO_ARCH_AVR)
#include <avr/pgmspace.h>
#endif

namespace {

#if defined(ARDUINO_ARCH_AVR)
#define MOON_MASK_STORAGE PROGMEM
#else
#define MOON_MASK_STORAGE
#endif

constexpr uint16_t kFrameIntervalMs = 180u;

constexpr uint8_t kTwinklePeriod = 32u;

constexpr uint8_t kLoopFrameCount = 64u;

static_assert(
    kLoopFrameCount % kTwinklePeriod == 0u,
    "MoonStars loop must contain complete twinkle cycles"
);

// -----------------------------------------------------------------------------
// WARM MOONLIGHT
//
// The finished lamp has a frosted diffuser that blooms pure white badly, so the
// moon is deliberately gold - never RGB(255,255,255). Only three flat shades
// are used, so no framebuffer, gradient or per-pixel blend state is needed.
// -----------------------------------------------------------------------------
constexpr Color kMoonEdge{205u, 145u, 45u};        // darker warm rim
constexpr Color kMoonBody{255u, 205u, 95u};        // main warm gold
constexpr Color kMoonHighlight{255u, 225u, 135u};  // pale warm highlight

// Stars are secondary and stay dim/cool so they frame the moon instead of
// competing with it.
constexpr Color kStarCool{150u, 180u, 225u};
constexpr Color kStarWarm{225u, 210u, 170u};

struct StarDefinition {
    uint8_t x;
    uint8_t y;
    uint8_t type;  // 0 = single pixel, 1 = plus (5 px)
    uint8_t phaseOffset;
};

// -----------------------------------------------------------------------------
// THE MOON
//
// One dominant crescent, opening to the RIGHT. 8 px wide (x=4..11), 12 px tall
// (y=2..13), 50 illuminated pixels - large enough that the diffuser cannot
// erase the shape, and thick enough (4..5 px at the body) that it does not read
// as a thin "C" outline.
//
// The right edge is carved inward to x=7 in the middle (versus x=11 at the
// horns), which is what makes it read as a crescent rather than a disc.
//
// Coordinates are FINAL: always drawn at offset 0,0.
//
//   0123456789ABCDEF
// 2 ..........##....
// 3 ........####....
// 4 ......#####.....
// 5 .....#####......
// 6 ....#####.......
// 7 ....####........
// 8 ....####........
// 9 ....#####.......
// A ......#####.....
// B .....#####......
// C ........####....
// D ..........##....
// -----------------------------------------------------------------------------
static const PixelMask16 kMoonMask MOON_MASK_STORAGE = {{
    0x0000,  // y0
    0x0000,  // y1
    0x0030,  // y2  x10,x11
    0x00f0,  // y3  x8..x11
    0x03e0,  // y4  x6..x10
    0x07c0,  // y5  x5..x9
    0x0f80,  // y6  x4..x8
    0x0f00,  // y7  x4..x7
    0x0f00,  // y8  x4..x7
    0x0f80,  // y9  x4..x8
    0x07c0,  // y10 x5..x9
    0x03e0,  // y11 x6..x10
    0x00f0,  // y12 x8..x11
    0x0030,  // y13 x10,x11
    0x0000,  // y14
    0x0000   // y15
}};

#undef MOON_MASK_STORAGE

// -----------------------------------------------------------------------------
// THE STARS
//
// Five stars: three sit in the negative space on the far side of the moon's
// opening, two are small accents. The moon reserves a one-pixel halo, and the
// star anchors are laid out to respect it with room to spare, so a star can
// never touch or bleed into the moon through the diffuser.
//
// Exactly one star is a "plus"; the rest are single pixels. Several large
// crosses at once would read as a blob on the real panel.
// -----------------------------------------------------------------------------
#if defined(ARDUINO_ARCH_AVR)
static const StarDefinition kStars[5] PROGMEM = {
#else
static const StarDefinition kStars[5] = {
#endif
    {1u, 2u, 0u, 0u},
    {2u, 7u, 0u, 10u},
    {1u, 13u, 0u, 20u},
    {14u, 3u, 0u, 28u},
    {14u, 7u, 1u, 4u}
};

constexpr uint8_t kStarCount = 5u;

#if defined(ARDUINO_ARCH_AVR)
static const uint8_t kTwinkleTable[32] PROGMEM = {
#else
static const uint8_t kTwinkleTable[32] = {
#endif
    96u,
    98u,
    102u,
    108u,
    116u,
    126u,
    138u,
    152u,
    166u,
    180u,
    192u,
    202u,
    210u,
    216u,
    220u,
    222u,
    220u,
    216u,
    210u,
    202u,
    192u,
    180u,
    166u,
    152u,
    138u,
    126u,
    116u,
    108u,
    102u,
    98u,
    96u,
    95u
};

Color skyColor(uint8_t y) {
    switch (y) {
        case 0u:
            return Color{2u, 3u, 8u};
        case 1u:
            return Color{3u, 4u, 10u};
        case 2u:
            return Color{4u, 5u, 12u};
        case 3u:
            return Color{5u, 6u, 14u};
        case 4u:
            return Color{6u, 7u, 16u};
        case 5u:
            return Color{7u, 8u, 18u};
        case 6u:
            return Color{8u, 9u, 20u};
        case 7u:
            return Color{9u, 10u, 22u};
        case 8u:
            return Color{8u, 9u, 20u};
        case 9u:
            return Color{7u, 8u, 18u};
        case 10u:
            return Color{6u, 7u, 16u};
        case 11u:
            return Color{5u, 6u, 14u};
        case 12u:
            return Color{4u, 5u, 12u};
        case 13u:
            return Color{3u, 4u, 10u};
        case 14u:
            return Color{2u, 3u, 8u};
        case 15u:
        default:
            return Color{2u, 2u, 6u};
    }
}

StarDefinition readStar(uint8_t index) {
#if defined(ARDUINO_ARCH_AVR)
    StarDefinition star;
    star.x = pgm_read_byte(&kStars[index].x);
    star.y = pgm_read_byte(&kStars[index].y);
    star.type = pgm_read_byte(&kStars[index].type);
    star.phaseOffset = pgm_read_byte(&kStars[index].phaseOffset);
    return star;
#else
    return kStars[index];
#endif
}

// Moon membership in logical coordinates. Bounds-safe, so callers can probe
// neighbours without underflowing the coordinate space.
bool moonMaskAt(int16_t x, int16_t y) {
    if (x < 0 || y < 0 ||
        x >= static_cast<int16_t>(PixelMatrix::WIDTH) ||
        y >= static_cast<int16_t>(PixelMatrix::HEIGHT)) {
        return false;
    }

    return maskHasPixel(
        &kMoonMask,
        static_cast<uint8_t>(x),
        static_cast<uint8_t>(y)
    );
}

// Moon plus its one-pixel exclusion halo: the region stars may never occupy.
bool moonHaloAt(int16_t x, int16_t y) {
    for (int8_t dy = -1; dy <= 1; ++dy) {
        for (int8_t dx = -1; dx <= 1; ++dx) {
            if (moonMaskAt(
                    static_cast<int16_t>(x + dx),
                    static_cast<int16_t>(y + dy))) {
                return true;
            }
        }
    }

    return false;
}

void drawStarPixel(
    PixelMatrix& matrix,
    int16_t x,
    int16_t y,
    Color color
) {
    if (x < 0 || y < 0 ||
        x >= static_cast<int16_t>(PixelMatrix::WIDTH) ||
        y >= static_cast<int16_t>(PixelMatrix::HEIGHT)) {
        return;
    }

    // The composition reserves the halo, and this is the hard guarantee that
    // even a future edit to the star table cannot place a star on the moon or
    // inside its bloom margin. Drawing the moon last would only hide the
    // overlap, not prevent it.
    if (moonHaloAt(x, y)) {
        return;
    }

    matrix.setPixel(
        static_cast<uint8_t>(x),
        static_cast<uint8_t>(y),
        color
    );
}

}  // namespace

MoonStarsAnimation::MoonStarsAnimation()
    : lastFrameAt_(0u),
      phase_(0u),
      renderedFrameCount_(0u),
      finished_(false) {
}

void MoonStarsAnimation::start(uint32_t now) {
    phase_ = 0u;
    renderedFrameCount_ = 0u;
    finished_ = false;
    lastFrameAt_ = static_cast<uint32_t>(now - kFrameIntervalMs);
}

void MoonStarsAnimation::update(uint32_t now, PixelMatrix& matrix) {
    if (finished_) {
        return;
    }

    const uint32_t elapsed = static_cast<uint32_t>(now - lastFrameAt_);
    if (elapsed < kFrameIntervalMs) {
        return;
    }

    renderFrame(matrix, phase_);

    phase_ = static_cast<uint8_t>((phase_ + 1u) % kTwinklePeriod);
    ++renderedFrameCount_;
    lastFrameAt_ = now;

    if (renderedFrameCount_ >= kLoopFrameCount) {
        finished_ = true;
    }
}

bool MoonStarsAnimation::isFinished() const {
    return finished_;
}

void MoonStarsAnimation::stop() {
    lastFrameAt_ = 0u;
    phase_ = 0u;
    renderedFrameCount_ = 0u;
    finished_ = true;
}

void MoonStarsAnimation::renderFrame(PixelMatrix& matrix, uint8_t phase) const {
    matrix.clear();

    for (uint8_t y = 0u; y < PixelMatrix::HEIGHT; ++y) {
        const Color color = skyColor(y);
        for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
            matrix.setPixel(x, y, color);
        }
    }

    renderMoon(matrix);
    renderStars(matrix, phase);
}

void MoonStarsAnimation::renderMoon(PixelMatrix& matrix) const {
    for (uint8_t y = 0u; y < PixelMatrix::HEIGHT; ++y) {
        for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
            if (!moonContains(x, y)) {
                continue;
            }

            matrix.setPixel(x, y, moonShadeAt(x, y));
        }
    }
}

void MoonStarsAnimation::renderStars(PixelMatrix& matrix, uint8_t phase) const {
    for (uint8_t i = 0u; i < kStarCount; ++i) {
        const StarDefinition star = readStar(i);
        const uint8_t localPhase = static_cast<uint8_t>((phase + star.phaseOffset) % kTwinklePeriod);
        const Color color = scaleColor(starBaseColor(star.type), twinkleScale(localPhase));

        const int16_t x = star.x;
        const int16_t y = star.y;

        drawStarPixel(matrix, x, y, color);

        if (star.type == 1u) {
            // A small plus, dimmer at the arms so the real diffuser keeps the
            // shape readable instead of saturating into a square blob.
            const Color arm = scaleColor(color, 170u);
            drawStarPixel(matrix, static_cast<int16_t>(x + 1), y, arm);
            drawStarPixel(matrix, static_cast<int16_t>(x - 1), y, arm);
            drawStarPixel(matrix, x, static_cast<int16_t>(y + 1), arm);
            drawStarPixel(matrix, x, static_cast<int16_t>(y - 1), arm);
        }
    }
}

uint8_t MoonStarsAnimation::twinkleScale(uint8_t phase) {
    const uint8_t index = static_cast<uint8_t>(phase & 0x1Fu);
#if defined(ARDUINO_ARCH_AVR)
    return pgm_read_byte(&kTwinkleTable[index]);
#else
    return kTwinkleTable[index];
#endif
}

Color MoonStarsAnimation::starBaseColor(uint8_t type) {
    // Only the small plus is warm; single stars stay cool so the eye reads the
    // gold moon as the warm focal point.
    return (type == 0u) ? kStarCool : kStarWarm;
}

bool MoonStarsAnimation::moonContains(uint8_t x, uint8_t y) {
    return moonMaskAt(x, y);
}

bool MoonStarsAnimation::moonHaloContains(uint8_t x, uint8_t y) {
    return moonHaloAt(x, y);
}

Color MoonStarsAnimation::moonShadeAt(uint8_t x, uint8_t y) {
    const int16_t xi = static_cast<int16_t>(x);
    const int16_t yi = static_cast<int16_t>(y);

    // Outline versus interior. A pixel is on the rim when any orthogonal
    // neighbour is not moon, which keeps the outer contour and the carved
    // crescent edge in the darker warm shade.
    const bool edge =
        !moonMaskAt(static_cast<int16_t>(xi - 1), yi) ||
        !moonMaskAt(static_cast<int16_t>(xi + 1), yi) ||
        !moonMaskAt(xi, static_cast<int16_t>(yi - 1)) ||
        !moonMaskAt(xi, static_cast<int16_t>(yi + 1));

    if (edge) {
        return kMoonEdge;
    }

    // The upper interior carries the pale highlight; the lower body the main
    // gold. Both are warm, neither is white.
    if (y <= 6u) {
        return kMoonHighlight;
    }

    return kMoonBody;
}

uint8_t MoonStarsAnimation::starCount() {
    return kStarCount;
}

uint8_t MoonStarsAnimation::starAnchorX(uint8_t index) {
    return readStar(index).x;
}

uint8_t MoonStarsAnimation::starAnchorY(uint8_t index) {
    return readStar(index).y;
}

uint8_t MoonStarsAnimation::starFootprintRadius(uint8_t index) {
    return (readStar(index).type == 0u) ? 0u : 1u;
}

bool MoonStarsAnimation::starContains(uint8_t index, uint8_t x, uint8_t y) {
    if (index >= kStarCount) {
        return false;
    }

    const StarDefinition star = readStar(index);
    const int16_t dx = static_cast<int16_t>(x) - static_cast<int16_t>(star.x);
    const int16_t dy = static_cast<int16_t>(y) - static_cast<int16_t>(star.y);

    if (star.type == 0u) {
        return dx == 0 && dy == 0;
    }

    // Plus: centre plus the four orthogonal arms.
    return (dx == 0 && dy >= -1 && dy <= 1) ||
           (dy == 0 && dx >= -1 && dx <= 1);
}
