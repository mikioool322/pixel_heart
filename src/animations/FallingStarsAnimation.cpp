#include "animations/FallingStarsAnimation.h"

#include "core/Color.h"
#include "core/PixelMatrix.h"

namespace {
constexpr Color kSky{10u, 12u, 24u};
constexpr Color kStar{95u, 120u, 170u};
constexpr Color kHead{255u, 235u, 150u};
constexpr Color kTrail{120u, 150u, 220u};

struct MeteorSpec {
    uint8_t startX;
    uint8_t startY;
    int8_t dx;
    int8_t dy;
    uint8_t phaseOffset;
};

constexpr MeteorSpec kMeteors[] = {
    {0u, 10u, 1, -1, 0u},
    {3u, 13u, 1, -1, 12u},
    {6u, 8u, 1, -1, 24u},
    {10u, 12u, 1, -1, 38u},
};

constexpr uint8_t kStarX[] = {1u, 4u, 7u, 9u, 12u, 14u, 2u, 6u, 10u, 13u};
constexpr uint8_t kStarY[] = {1u, 3u, 2u, 6u, 4u, 8u, 11u, 13u, 9u, 12u};

void setIfInBounds(PixelMatrix& matrix, int16_t x, int16_t y, Color color) {
    if (x < 0 || y < 0 || x >= PixelMatrix::WIDTH || y >= PixelMatrix::HEIGHT) {
        return;
    }
    matrix.setPixel(static_cast<uint8_t>(x), static_cast<uint8_t>(y), color);
}
}  // namespace

FallingStarsAnimation::FallingStarsAnimation()
    : lastFrameAt_(0u),
      phase_(0u),
      renderedFrameCount_(0u),
      finished_(false) {
}

void FallingStarsAnimation::start(uint32_t now) {
    lastFrameAt_ = static_cast<uint32_t>(now - kFrameIntervalMs);
    phase_ = 0u;
    renderedFrameCount_ = 0u;
    finished_ = false;
}

void FallingStarsAnimation::update(uint32_t now, PixelMatrix& matrix) {
    if (finished_) {
        return;
    }

    const uint32_t elapsed = now - lastFrameAt_;
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

bool FallingStarsAnimation::isFinished() const {
    return finished_;
}

void FallingStarsAnimation::stop() {
    lastFrameAt_ = 0u;
    phase_ = 0u;
    renderedFrameCount_ = 0u;
    finished_ = true;
}

void FallingStarsAnimation::renderFrame(PixelMatrix& matrix, uint8_t phase) const {
    renderBackground(matrix, phase);
    for (uint8_t i = 0u; i < 4u; ++i) {
        renderMeteor(matrix, i, phase);
    }
}

void FallingStarsAnimation::renderBackground(PixelMatrix& matrix, uint8_t phase) const {
    for (uint8_t y = 0u; y < PixelMatrix::HEIGHT; ++y) {
        for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
            matrix.setPixel(x, y, kSky);
        }
    }

    for (uint8_t i = 0u; i < 10u; ++i) {
        uint8_t brightness = static_cast<uint8_t>((phase + i * 13u) % 80u + 120u);
        matrix.setPixel(kStarX[i], kStarY[i], scaleColor(kStar, brightness));
    }
}

void FallingStarsAnimation::renderMeteor(PixelMatrix& matrix, uint8_t meteorIndex, uint8_t phase) const {
    const MeteorSpec& meteor = kMeteors[meteorIndex];
    const uint8_t localPhase = static_cast<uint8_t>((phase + meteor.phaseOffset) % 16u);

    for (int8_t tail = 0; tail < 4; ++tail) {
        const int16_t currentStep = static_cast<int16_t>(localPhase) - tail;
        if (currentStep < 0) {
            continue;
        }

        const int16_t x = static_cast<int16_t>(meteor.startX) + (currentStep * meteor.dx);
        const int16_t y = static_cast<int16_t>(meteor.startY) + (currentStep * meteor.dy);

        if (x < 0 || y < 0 || x >= PixelMatrix::WIDTH || y >= PixelMatrix::HEIGHT) {
            continue;
        }

        const uint8_t brightness = tail == 0u ? 255u : tailBrightness(static_cast<uint8_t>(tail));
        const Color color = tail == 0u ? kHead : scaleColor(kTrail, brightness);
        setIfInBounds(matrix, x, y, color);
    }
}

uint8_t FallingStarsAnimation::tailBrightness(uint8_t distance) {
    switch (distance) {
        case 0u:
            return 255u;
        case 1u:
            return 180u;
        case 2u:
            return 110u;
        default:
            return 45u;
    }
}
