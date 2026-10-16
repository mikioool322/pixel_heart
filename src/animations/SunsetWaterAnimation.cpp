#include "animations/SunsetWaterAnimation.h"

#include "assets/PixelMask.h"
#include "core/Color.h"
#include "core/PixelMatrix.h"

#if defined(ARDUINO_ARCH_AVR)
#include <avr/pgmspace.h>
#endif

namespace {

constexpr uint16_t kFrameIntervalMs = 160u;

constexpr uint8_t kWavePhaseCount = 32u;

constexpr uint8_t kLoopFrameCount = 64u;

static_assert(
    kLoopFrameCount % kWavePhaseCount == 0u,
    "SunsetWater loop must contain complete shimmer cycles"
);

// The upper sky is deliberately the darkest part of the scene, but it must not
// vanish: the brightness limiter scales every channel by (raw + 1) / 256, so the
// old top value of 24 landed at 2/255 and the whole first row read as unlit.
// These two stops are lifted just enough that the top row is genuinely visible,
// while the gradient still climbs towards the horizon.
constexpr Color kSkyTop{
    56,
    19,
    46
};

constexpr Color kSkyUpper{
    76,
    26,
    54
};

constexpr Color kSkyMid{
    88,
    28,
    58
};

constexpr Color kSkyLower{
    138,
    48,
    68
};

constexpr Color kSkyHorizon{
    185,
    72,
    55
};

constexpr Color kSunCore{
    235,
    120,
    38
};

constexpr Color kSunMid{
    235,
    120,
    38
};

constexpr Color kSunEdge{
    235,
    120,
    38
};

constexpr Color kSunGlow{
    205,
    88,
    32
};

constexpr Color kWaterBase{
    18,
    26,
    58
};

constexpr Color kWaterWarm{
    48,
    38,
    66
};

constexpr Color kWaterReflection{
    210,
    110,
    95
};

constexpr Color kWaterReflectionBright{
    245,
    205,
    120
};

}  // namespace

SunsetWaterAnimation::SunsetWaterAnimation()
    : lastFrameAt_(0u),
      phase_(0u),
      renderedFrameCount_(0u),
      finished_(false) {
}

void SunsetWaterAnimation::start(uint32_t now) {
    phase_ = 0u;

    renderedFrameCount_ = 0u;

    finished_ = false;

    lastFrameAt_ =
        static_cast<uint32_t>(
            now - kFrameIntervalMs
        );
}

void SunsetWaterAnimation::update(
    uint32_t now,
    PixelMatrix& matrix
) {
    if (finished_) {
        return;
    }

    const uint32_t elapsed =
        static_cast<uint32_t>(
            now - lastFrameAt_
        );

    if (
        elapsed <
        kFrameIntervalMs
    ) {
        return;
    }

    renderFrame(
        matrix,
        phase_
    );

    phase_ =
        static_cast<uint8_t>(
            (phase_ + 1u)
            % kWavePhaseCount
        );

    ++renderedFrameCount_;

    lastFrameAt_ = now;

    if (
        renderedFrameCount_ >=
        kLoopFrameCount
    ) {
        finished_ = true;
    }
}

bool SunsetWaterAnimation::isFinished() const {
    return finished_;
}

void SunsetWaterAnimation::stop() {
    lastFrameAt_ = 0u;

    phase_ = 0u;

    renderedFrameCount_ = 0u;

    finished_ = true;
}

Color SunsetWaterAnimation::blend(
    Color a,
    Color b,
    uint8_t amount
) {
    return Color{
        static_cast<uint8_t>(
            (
                static_cast<uint16_t>(a.r) * (255u - amount)
                + static_cast<uint16_t>(b.r) * amount
            ) / 255u
        ),
        static_cast<uint8_t>(
            (
                static_cast<uint16_t>(a.g) * (255u - amount)
                + static_cast<uint16_t>(b.g) * amount
            ) / 255u
        ),
        static_cast<uint8_t>(
            (
                static_cast<uint16_t>(a.b) * (255u - amount)
                + static_cast<uint16_t>(b.b) * amount
            ) / 255u
        )
    };
}

void SunsetWaterAnimation::renderFrame(
    PixelMatrix& matrix,
    uint8_t phase
) const {
    matrix.clear();

    renderSky(matrix);

    renderSun(matrix);

    renderWater(
        matrix,
        phase
    );
}

void SunsetWaterAnimation::renderSky(
    PixelMatrix& matrix
) const {
    for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
        matrix.setPixel(
            x,
            0u,
            kSkyTop
        );
        matrix.setPixel(
            x,
            1u,
            blend(kSkyTop, kSkyUpper, 120u)
        );
        matrix.setPixel(
            x,
            2u,
            kSkyUpper
        );
        matrix.setPixel(
            x,
            3u,
            blend(kSkyUpper, kSkyMid, 120u)
        );
        matrix.setPixel(
            x,
            4u,
            kSkyMid
        );
        matrix.setPixel(
            x,
            5u,
            blend(kSkyMid, kSkyLower, 145u)
        );
        matrix.setPixel(
            x,
            6u,
            blend(kSkyLower, kSkyHorizon, 150u)
        );
        matrix.setPixel(
            x,
            7u,
            blend(kSkyHorizon, kWaterWarm, 75u)
        );
    }
}

void SunsetWaterAnimation::renderSun(
    PixelMatrix& matrix
) const {
    matrix.setPixel(7u, 4u, kSunCore);
    matrix.setPixel(8u, 4u, kSunCore);

    matrix.setPixel(5u, 5u, kSunEdge);
    matrix.setPixel(6u, 5u, kSunMid);
    matrix.setPixel(7u, 5u, kSunCore);
    matrix.setPixel(8u, 5u, kSunCore);
    matrix.setPixel(9u, 5u, kSunMid);
    matrix.setPixel(10u, 5u, kSunEdge);

    matrix.setPixel(4u, 6u, kSunGlow);
    matrix.setPixel(5u, 6u, kSunEdge);
    matrix.setPixel(6u, 6u, kSunMid);
    matrix.setPixel(7u, 6u, kSunCore);
    matrix.setPixel(8u, 6u, kSunCore);
    matrix.setPixel(9u, 6u, kSunMid);
    matrix.setPixel(10u, 6u, kSunEdge);
    matrix.setPixel(11u, 6u, kSunGlow);

    matrix.setPixel(4u, 7u, kSunGlow);
    matrix.setPixel(5u, 7u, kSunEdge);
    matrix.setPixel(6u, 7u, kSunMid);
    matrix.setPixel(7u, 7u, kSunMid);
    matrix.setPixel(8u, 7u, kSunMid);
    matrix.setPixel(9u, 7u, kSunMid);
    matrix.setPixel(10u, 7u, kSunEdge);
    matrix.setPixel(11u, 7u, kSunGlow);
}

uint8_t SunsetWaterAnimation::shimmerValue(
    uint8_t phase
) {
    const uint8_t index =
        static_cast<uint8_t>(
            phase & 0x1Fu
        );

    static const uint8_t table[32] = {
        108u,
        112u,
        118u,
        126u,
        136u,
        148u,
        160u,
        172u,
        184u,
        194u,
        204u,
        214u,
        224u,
        232u,
        238u,
        242u,
        244u,
        242u,
        236u,
        228u,
        218u,
        206u,
        194u,
        182u,
        170u,
        158u,
        146u,
        136u,
        126u,
        118u,
        112u,
        109u
    };

    return table[index];
}

void SunsetWaterAnimation::renderWater(
    PixelMatrix& matrix,
    uint8_t phase
) const {
    const Color rowBaseColors[8] = {
        blend(kWaterWarm, kWaterBase, 65u),
        blend(kWaterWarm, kWaterBase, 95u),
        blend(kWaterWarm, kWaterBase, 120u),
        blend(kWaterWarm, kWaterBase, 145u),
        blend(kWaterWarm, kWaterBase, 168u),
        blend(kWaterWarm, kWaterBase, 188u),
        blend(kWaterWarm, kWaterBase, 206u),
        blend(kWaterWarm, kWaterBase, 222u)
    };

    for (uint8_t y = 8u; y < PixelMatrix::HEIGHT; ++y) {
        const uint8_t rowIndex = static_cast<uint8_t>(y - 8u);
        const Color baseRow = rowBaseColors[rowIndex];

        for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
            Color color = baseRow;

            const uint8_t wavePhase =
                static_cast<uint8_t>(
                    (phase + x * 3u + y * 5u) % 32u
                );

            const uint8_t wave = shimmerValue(wavePhase);
            const uint8_t waterScale =
                static_cast<uint8_t>(
                    225u + (
                        static_cast<uint16_t>(wave) * 15u
                    ) / 255u
                );

            color = scaleColor(color, waterScale);

            const uint8_t rowPhase =
                static_cast<uint8_t>(
                    (phase + (y - 8u) * 3u) % 32u
                );

            const uint8_t rowShimmer = shimmerValue(rowPhase);
            const Color softShimmer =
                scaleColor(
                    kWaterReflection,
                    static_cast<uint8_t>(
                        (static_cast<uint16_t>(rowShimmer) * 3u) / 5u
                    )
                );
            const Color brightShimmer =
                scaleColor(
                    kWaterReflectionBright,
                    static_cast<uint8_t>(
                        (static_cast<uint16_t>(rowShimmer) * 9u) / 10u
                    )
                );

            bool softZone = false;
            bool brightZone = false;

            switch (rowIndex) {
                case 0u:
                    if (x == 4u || x == 11u) {
                        softZone = true;
                    }
                    if (x >= 5u && x <= 10u) {
                        brightZone = true;
                    }
                    break;
                case 1u:
                    if (x == 5u || x == 10u) {
                        softZone = true;
                    }
                    if (x >= 6u && x <= 9u) {
                        brightZone = true;
                    }
                    break;
                case 2u:
                    if (x == 5u || x == 10u) {
                        softZone = true;
                    }
                    if (x >= 6u && x <= 9u) {
                        brightZone = true;
                    }
                    break;
                case 3u:
                    if (x == 6u || x == 9u) {
                        softZone = true;
                    }
                    if (x >= 7u && x <= 8u) {
                        brightZone = true;
                    }
                    break;
                case 4u:
                    if (x == 5u || x == 9u) {
                        softZone = true;
                    }
                    if (x >= 6u && x <= 8u) {
                        brightZone = true;
                    }
                    break;
                case 5u:
                    if (x == 6u || x == 9u) {
                        softZone = true;
                    }
                    if (x >= 7u && x <= 8u) {
                        brightZone = true;
                    }
                    break;
                case 6u:
                    if (x == 6u || x == 9u) {
                        softZone = true;
                    }
                    if (x >= 7u && x <= 8u) {
                        brightZone = true;
                    }
                    break;
                case 7u:
                    if (x == 6u || x == 8u) {
                        softZone = true;
                    }
                    if (x == 7u) {
                        brightZone = true;
                    }
                    break;
                default:
                    break;
            }

            if (softZone) {
                color = blend(color, softShimmer, 86u);
            }

            if (brightZone) {
                color = blend(color, brightShimmer, 118u);
            }

            matrix.setPixel(
                x,
                y,
                color
            );
        }
    }
}
