#include "animations/AuroraAnimation.h"

#include "core/Color.h"
#include "core/PixelMatrix.h"

namespace {
constexpr uint16_t kFrameIntervalMs = 50u;
constexpr uint16_t kPhasePeriod = 512u;
constexpr uint16_t kPhaseStep = 3u;
constexpr uint16_t kLoopFrameCount = 512u;

inline uint8_t triangleWave(uint8_t value) {
    if (value < 128u) {
        return static_cast<uint8_t>(value * 2u);
    }

    return static_cast<uint8_t>((255u - value) * 2u);
}

Color paletteStop(AuroraPaletteId palette, uint8_t stop) {
    switch (palette) {
        case AuroraPaletteId::ARCTIC:
            switch (stop) {
                case 0: return {5, 10, 55};
                case 1: return {15, 65, 255};
                case 2: return {20, 220, 255};
                case 3: return {25, 255, 150};
                case 4: return {125, 55, 255};
                case 5: return {240, 55, 185};
                default: return {0, 0, 0};
            }
        case AuroraPaletteId::SUNSET:
            switch (stop) {
                case 0: return {45, 5, 30};
                case 1: return {105, 25, 145};
                case 2: return {220, 35, 155};
                case 3: return {255, 55, 65};
                case 4: return {255, 120, 25};
                case 5: return {255, 215, 55};
                default: return {0, 0, 0};
            }
        case AuroraPaletteId::NEON:
            switch (stop) {
                case 0: return {20, 5, 70};
                case 1: return {45, 65, 255};
                case 2: return {15, 235, 255};
                case 3: return {145, 255, 45};
                case 4: return {255, 55, 170};
                case 5: return {220, 30, 255};
                default: return {0, 0, 0};
            }
        default:
            return {0, 0, 0};
    }
}
}  // namespace

AuroraAnimation::AuroraAnimation(AuroraPaletteId palette)
    : palette_(palette),
      startedAt_(0u),
      lastFrameAt_(0u),
      phase_(0u),
      renderedFrameCount_(0u),
      finished_(false) {
}

void AuroraAnimation::start(uint32_t now) {
    startedAt_ = now;
    lastFrameAt_ = static_cast<uint32_t>(now - kFrameIntervalMs);
    phase_ = 0u;
    renderedFrameCount_ = 0u;
    finished_ = false;
}

void AuroraAnimation::update(uint32_t now, PixelMatrix& matrix) {
    if (finished_) {
        return;
    }

    const uint32_t elapsed =
        static_cast<uint32_t>(now - lastFrameAt_);

    if (elapsed < kFrameIntervalMs) {
        return;
    }

    renderFrame(matrix, phase_);

    phase_ = static_cast<uint16_t>((phase_ + kPhaseStep) % kPhasePeriod);
    ++renderedFrameCount_;
    lastFrameAt_ = now;

    if (renderedFrameCount_ >= kLoopFrameCount) {
        finished_ = true;
    }
}

bool AuroraAnimation::isFinished() const {
    return finished_;
}

void AuroraAnimation::stop() {
    finished_ = true;
}

Color AuroraAnimation::lerpColor(
    Color a,
    Color b,
    uint8_t amount
) {
    const uint16_t amount16 = static_cast<uint16_t>(amount);

    return {
        static_cast<uint8_t>(
            (static_cast<uint16_t>(a.r) * (255u - amount16) +
             static_cast<uint16_t>(b.r) * amount16) /
            255u
        ),
        static_cast<uint8_t>(
            (static_cast<uint16_t>(a.g) * (255u - amount16) +
             static_cast<uint16_t>(b.g) * amount16) /
            255u
        ),
        static_cast<uint8_t>(
            (static_cast<uint16_t>(a.b) * (255u - amount16) +
             static_cast<uint16_t>(b.b) * amount16) /
            255u
        )
    };
}

Color AuroraAnimation::auroraColor(uint8_t value) const {
    const Color c0 = paletteStop(palette_, 0);
    const Color c1 = paletteStop(palette_, 1);
    const Color c2 = paletteStop(palette_, 2);
    const Color c3 = paletteStop(palette_, 3);
    const Color c4 = paletteStop(palette_, 4);
    const Color c5 = paletteStop(palette_, 5);

    if (value < 51u) {
        return lerpColor(c0, c1, value * 5u);
    }

    if (value < 101u) {
        return lerpColor(c1, c2, (value - 51u) * 5u);
    }

    if (value < 151u) {
        return lerpColor(c2, c3, (value - 101u) * 5u);
    }

    if (value < 201u) {
        return lerpColor(c3, c4, (value - 151u) * 5u);
    }

    return lerpColor(c4, c5, (value - 201u) * 5u);
}

void AuroraAnimation::renderFrame(
    PixelMatrix& matrix,
    uint16_t phase
) const {
    matrix.clear();

    for (uint8_t y = 0u; y < PixelMatrix::HEIGHT; ++y) {
        for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
            const uint16_t base =
                phase +
                static_cast<uint16_t>(x) * 17u +
                static_cast<uint16_t>(y) * 11u;

            const uint16_t secondary =
                static_cast<uint16_t>(phase / 2u + x * 7u + y * 19u);

            const uint8_t a = static_cast<uint8_t>(base & 0xFFu);
            const uint8_t b = static_cast<uint8_t>(secondary & 0xFFu);

            const uint8_t waveA = triangleWave(a);
            const uint8_t waveB = triangleWave(b);

            uint16_t value =
                static_cast<uint16_t>(waveA) * 3u +
                static_cast<uint16_t>(waveB) * 2u;
            value /= 5u;

            const uint8_t centerDistance =
                static_cast<uint8_t>(
                    (y < 7u)
                        ? (7u - y)
                        : (y - 7u)
                );

            uint16_t brightnessScale = 255u - static_cast<uint16_t>(centerDistance) * 8u;
            if (brightnessScale < 199u) {
                brightnessScale = 199u;
            }

            const uint8_t fieldValue = static_cast<uint8_t>(value);

            Color color = auroraColor(fieldValue);
            color = scaleColor(color, static_cast<uint8_t>(brightnessScale));
            matrix.setPixel(x, y, color);
        }
    }
}
