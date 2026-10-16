#include "animations/SunOverMountainsAnimation.h"

#include "core/Color.h"
#include "core/PixelMatrix.h"

namespace {
constexpr uint8_t kShimmerTable[32] = {
    96u, 98u, 102u, 108u, 116u, 126u, 138u, 152u,
    166u, 180u, 192u, 202u, 210u, 216u, 220u, 222u,
    220u, 216u, 210u, 202u, 192u, 180u, 166u, 152u,
    138u, 126u, 116u, 108u, 102u, 98u, 96u, 95u
};

constexpr Color kSkyRow0{16u, 54u, 138u};
constexpr Color kSkyRow1{20u, 66u, 156u};
constexpr Color kSkyRow2{24u, 80u, 176u};
constexpr Color kSkyRow3{30u, 96u, 198u};
constexpr Color kSkyRow4{40u, 116u, 216u};
constexpr Color kSkyRow5{56u, 138u, 232u};
constexpr Color kSkyRow6{74u, 160u, 240u};
constexpr Color kSkyRow7{92u, 178u, 245u};
constexpr Color kSkyRow8{108u, 192u, 248u};

constexpr Color kRayOuter{255u, 142u, 52u};
constexpr Color kRayMedium{235u, 128u, 44u};
constexpr Color kRayDim{185u, 92u, 30u};
constexpr Color kSunOuter{250u, 232u, 42u};
constexpr Color kSunInner{250u, 232u, 42u};
constexpr Color kSunCore{250u, 232u, 42u};

constexpr Color kMountainLeft{30u, 52u, 72u};
constexpr Color kMountainCenter{20u, 38u, 54u};
constexpr Color kMountainRight{36u, 60u, 80u};

struct SunPixel {
    uint8_t x;
    uint8_t y;
    Color color;
};

void setPixelSafe(PixelMatrix& matrix, uint8_t x, uint8_t y, Color color) {
    if (x >= PixelMatrix::WIDTH || y >= PixelMatrix::HEIGHT) {
        return;
    }
    matrix.setPixel(x, y, color);
}

uint8_t shimmerLookup(uint8_t phase) {
    return kShimmerTable[phase & 31u];
}

void setSunBody(PixelMatrix& matrix) {
    static const SunPixel kOuterBody[] = {
        {6u, 1u, kSunOuter}, {7u, 1u, kSunOuter}, {8u, 1u, kSunOuter}, {9u, 1u, kSunOuter},
        {5u, 2u, kSunOuter}, {6u, 2u, kSunOuter}, {7u, 2u, kSunOuter}, {8u, 2u, kSunOuter}, {9u, 2u, kSunOuter}, {10u, 2u, kSunOuter},
        {5u, 3u, kSunOuter}, {6u, 3u, kSunOuter}, {7u, 3u, kSunOuter}, {8u, 3u, kSunOuter}, {9u, 3u, kSunOuter}, {10u, 3u, kSunOuter},
        {5u, 4u, kSunOuter}, {6u, 4u, kSunOuter}, {7u, 4u, kSunOuter}, {8u, 4u, kSunOuter}, {9u, 4u, kSunOuter}, {10u, 4u, kSunOuter},
        {5u, 5u, kSunOuter}, {6u, 5u, kSunOuter}, {7u, 5u, kSunOuter}, {8u, 5u, kSunOuter}, {9u, 5u, kSunOuter}, {10u, 5u, kSunOuter},
        {6u, 6u, kSunOuter}, {7u, 6u, kSunOuter}, {8u, 6u, kSunOuter}, {9u, 6u, kSunOuter}
    };

    static const SunPixel kInnerBody[] = {
        {6u, 2u, kSunInner}, {7u, 2u, kSunInner}, {8u, 2u, kSunInner}, {9u, 2u, kSunInner},
        {6u, 3u, kSunInner}, {7u, 3u, kSunInner}, {8u, 3u, kSunInner}, {9u, 3u, kSunInner},
        {6u, 4u, kSunInner}, {7u, 4u, kSunInner}, {8u, 4u, kSunInner}, {9u, 4u, kSunInner},
        {6u, 5u, kSunInner}, {7u, 5u, kSunInner}, {8u, 5u, kSunInner}, {9u, 5u, kSunInner}
    };

    static const SunPixel kCore[] = {
        {7u, 3u, kSunCore}, {8u, 3u, kSunCore},
        {7u, 4u, kSunCore}, {8u, 4u, kSunCore}
    };

    for (uint8_t i = 0u; i < sizeof(kOuterBody) / sizeof(kOuterBody[0]); ++i) {
        setPixelSafe(matrix, kOuterBody[i].x, kOuterBody[i].y, kOuterBody[i].color);
    }
    for (uint8_t i = 0u; i < sizeof(kInnerBody) / sizeof(kInnerBody[0]); ++i) {
        setPixelSafe(matrix, kInnerBody[i].x, kInnerBody[i].y, kInnerBody[i].color);
    }
    for (uint8_t i = 0u; i < sizeof(kCore) / sizeof(kCore[0]); ++i) {
        setPixelSafe(matrix, kCore[i].x, kCore[i].y, kCore[i].color);
    }
}

void setMountainPixels(PixelMatrix& matrix) {
    static const uint8_t leftX[] = {
        0u, 1u, 2u, 3u, 4u, 5u,
        0u, 1u, 2u, 3u, 4u, 5u,
        1u, 2u, 3u, 4u, 5u,
        1u, 2u, 3u, 4u,
        2u, 3u, 4u,
        3u
    };
    static const uint8_t leftY[] = {
        15u, 15u, 15u, 15u, 15u, 15u,
        14u, 14u, 14u, 14u, 14u, 14u,
        13u, 13u, 13u, 13u, 13u,
        12u, 12u, 12u, 12u,
        11u, 11u, 11u,
        10u
    };

    static const uint8_t centerX[] = {
        4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u,
        4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u,
        5u, 6u, 7u, 8u, 9u, 10u, 11u,
        5u, 6u, 7u, 8u, 9u, 10u, 11u,
        6u, 7u, 8u, 9u, 10u,
        7u, 8u, 9u,
        8u
    };
    static const uint8_t centerY[] = {
        15u, 15u, 15u, 15u, 15u, 15u, 15u, 15u, 15u,
        14u, 14u, 14u, 14u, 14u, 14u, 14u, 14u, 14u,
        13u, 13u, 13u, 13u, 13u, 13u, 13u,
        12u, 12u, 12u, 12u, 12u, 12u, 12u,
        11u, 11u, 11u, 11u, 11u,
        10u, 10u, 10u,
        9u
    };

    static const uint8_t rightX[] = {
        10u, 11u, 12u, 13u, 14u, 15u,
        10u, 11u, 12u, 13u, 14u, 15u,
        10u, 11u, 12u, 13u, 14u,
        11u, 12u, 13u, 14u,
        12u, 13u, 14u,
        13u
    };
    static const uint8_t rightY[] = {
        15u, 15u, 15u, 15u, 15u, 15u,
        14u, 14u, 14u, 14u, 14u, 14u,
        13u, 13u, 13u, 13u, 13u,
        12u, 12u, 12u, 12u,
        11u, 11u, 11u,
        10u
    };

    for (uint8_t i = 0u; i < sizeof(leftX) / sizeof(leftX[0]); ++i) {
        setPixelSafe(matrix, leftX[i], leftY[i], kMountainLeft);
    }
    for (uint8_t i = 0u; i < sizeof(centerX) / sizeof(centerX[0]); ++i) {
        setPixelSafe(matrix, centerX[i], centerY[i], kMountainCenter);
    }
    for (uint8_t i = 0u; i < sizeof(rightX) / sizeof(rightX[0]); ++i) {
        setPixelSafe(matrix, rightX[i], rightY[i], kMountainRight);
    }
}

void renderSunRays(PixelMatrix& matrix, uint8_t phase) {
    static const uint8_t kTopGroup[][2] = {
        {6u, 0u}, {7u, 0u}, {8u, 0u}, {9u, 0u}
    };
    static const uint8_t kRightGroup[][2] = {
        {10u, 1u}, {11u, 2u}, {11u, 3u}, {11u, 4u}, {10u, 6u}
    };
    static const uint8_t kBottomGroup[][2] = {
        {6u, 7u}, {7u, 7u}, {8u, 7u}, {9u, 7u}
    };
    static const uint8_t kLeftGroup[][2] = {
        {5u, 6u}, {4u, 4u}, {4u, 3u}, {4u, 2u}, {5u, 1u}
    };
    static const uint8_t kUpperDiag[][2] = {
        {5u, 2u}, {10u, 2u}
    };
    static const uint8_t kLowerDiag[][2] = {
        {5u, 5u}, {10u, 5u}
    };

    const uint8_t phase16 = phase % 16u;
    const uint8_t activeGroup = phase16 / 4u;
    const uint8_t previousGroup = (activeGroup + 3u) % 4u;
    const uint8_t nextGroup = (activeGroup + 1u) % 4u;

    auto paintGroup = [&](const uint8_t points[][2], uint8_t count, uint8_t groupIndex) {
        Color color = kRayDim;
        if (groupIndex == activeGroup) {
            color = kRayOuter;
        } else if (groupIndex == previousGroup || groupIndex == nextGroup) {
            color = kRayMedium;
        }
        for (uint8_t i = 0u; i < count; ++i) {
            setPixelSafe(matrix, points[i][0u], points[i][1u], color);
        }
    };

    paintGroup(kTopGroup, sizeof(kTopGroup) / sizeof(kTopGroup[0]), 0u);
    paintGroup(kRightGroup, sizeof(kRightGroup) / sizeof(kRightGroup[0]), 1u);
    paintGroup(kBottomGroup, sizeof(kBottomGroup) / sizeof(kBottomGroup[0]), 2u);
    paintGroup(kLeftGroup, sizeof(kLeftGroup) / sizeof(kLeftGroup[0]), 3u);

    const bool upperActive = (activeGroup == 0u || activeGroup == 1u);
    const bool upperAdjacent = (previousGroup == 0u || previousGroup == 1u || nextGroup == 0u || nextGroup == 1u);
    for (uint8_t i = 0u; i < sizeof(kUpperDiag) / sizeof(kUpperDiag[0]); ++i) {
        const Color color = upperActive ? kRayOuter : (upperAdjacent ? kRayMedium : kRayDim);
        setPixelSafe(matrix, kUpperDiag[i][0u], kUpperDiag[i][1u], color);
    }

    const bool lowerActive = (activeGroup == 2u || activeGroup == 3u);
    const bool lowerAdjacent = (previousGroup == 2u || previousGroup == 3u || nextGroup == 2u || nextGroup == 3u);
    for (uint8_t i = 0u; i < sizeof(kLowerDiag) / sizeof(kLowerDiag[0]); ++i) {
        const Color color = lowerActive ? kRayOuter : (lowerAdjacent ? kRayMedium : kRayDim);
        setPixelSafe(matrix, kLowerDiag[i][0u], kLowerDiag[i][1u], color);
    }
}
}  // namespace

SunOverMountainsAnimation::SunOverMountainsAnimation()
    : lastFrameAt_(0u),
      phase_(0u),
      renderedFrameCount_(0u),
      finished_(false) {
}

void SunOverMountainsAnimation::start(uint32_t now) {
    lastFrameAt_ = static_cast<uint32_t>(now - kFrameIntervalMs);
    phase_ = 0u;
    renderedFrameCount_ = 0u;
    finished_ = false;
}

void SunOverMountainsAnimation::update(uint32_t now, PixelMatrix& matrix) {
    if (finished_) {
        return;
    }

    const uint32_t elapsed = now - lastFrameAt_;
    if (elapsed < kFrameIntervalMs) {
        return;
    }

    renderFrame(matrix, phase_);

    phase_ = static_cast<uint8_t>((phase_ + 1u) % kPhaseCount);
    ++renderedFrameCount_;
    lastFrameAt_ = now;

    if (renderedFrameCount_ >= kLoopFrameCount) {
        finished_ = true;
    }
}

bool SunOverMountainsAnimation::isFinished() const {
    return finished_;
}

void SunOverMountainsAnimation::stop() {
    lastFrameAt_ = 0u;
    phase_ = 0u;
    renderedFrameCount_ = 0u;
    finished_ = true;
}

void SunOverMountainsAnimation::renderFrame(PixelMatrix& matrix, uint8_t phase) const {
    matrix.clear();
    renderSky(matrix, phase);
    renderSun(matrix, phase);
    renderMountains(matrix);
}

void SunOverMountainsAnimation::renderSky(PixelMatrix& matrix, uint8_t phase) const {
    for (uint8_t y = 0u; y < PixelMatrix::HEIGHT; ++y) {
        const uint8_t rowIndex = y < 9u ? y : 8u;
        const Color base = rowIndex == 0u ? kSkyRow0
            : rowIndex == 1u ? kSkyRow1
            : rowIndex == 2u ? kSkyRow2
            : rowIndex == 3u ? kSkyRow3
            : rowIndex == 4u ? kSkyRow4
            : rowIndex == 5u ? kSkyRow5
            : rowIndex == 6u ? kSkyRow6
            : rowIndex == 7u ? kSkyRow7
            : kSkyRow8;

        const uint8_t shimmer = shimmerValue(static_cast<uint8_t>(phase + (y * 3u)));
        const uint8_t brightness = static_cast<uint8_t>(220u + ((shimmer > 160u) ? ((shimmer - 160u) / 2u) : 0u));
        const Color shade = scaleColor(base, brightness);

        for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
            matrix.setPixel(x, y, shade);
        }
    }
}

void SunOverMountainsAnimation::renderSun(PixelMatrix& matrix, uint8_t phase) const {
    renderSunRays(matrix, phase);
    setSunBody(matrix);
}

void SunOverMountainsAnimation::renderMountains(PixelMatrix& matrix) const {
    setMountainPixels(matrix);
}

uint8_t SunOverMountainsAnimation::shimmerValue(uint8_t phase) {
    return shimmerLookup(phase);
}
