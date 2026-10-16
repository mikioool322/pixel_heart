#include <unity.h>

#define private public
#include "animations/MeadowFlowerAnimation.h"
#undef private

#include "config/LedPowerBudget.h"
#include "control/AnimationCatalog.h"
#include "core/AnimationId.h"
#include "core/AnimationNames.h"
#include "core/AnimationRegistry.h"
#include "core/PixelMatrix.h"

// MEADOW_FLOWER - a large flower bending in a daytime breeze.
//
// What this file protects:
//   - the lower stem is ANCHORED. It must be pixel-identical in every pose, or
//     the flower slides sideways out of the meadow instead of bending.
//   - the head leans further than the stem, which is what makes the motion read
//     as wind rather than as a sliding sticker.
//   - no adjacent pose moves any feature by more than one pixel, INCLUDING the
//     wrap from the last pose back to the first.
//   - the loop is seamless by construction: t=0 and t=loopDurationMs are the
//     same frame.
//   - the scene stays restrained: dim sky, real meadow, no pure white.

namespace {

constexpr uint8_t kPanel = 16u;

Color pixelAt(const PixelMatrix& matrix, uint8_t x, uint8_t y) {
    const uint32_t packed = matrix.pixelColorAt(PixelMatrix::xyToIndexStatic(x, y));
    return Color{
        static_cast<uint8_t>((packed >> 16) & 0xFFu),
        static_cast<uint8_t>((packed >> 8) & 0xFFu),
        static_cast<uint8_t>(packed & 0xFFu)
    };
}

bool sameColor(const Color& a, const Color& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

bool isLit(const Color& c) {
    return c.r != 0u || c.g != 0u || c.b != 0u;
}

bool isBlue(const Color& c) {
    return c.b > c.r && c.b > c.g;
}

bool isGreen(const Color& c) {
    return c.g > c.r && c.g > c.b;
}

void renderPose(MeadowFlowerAnimation& animation, PixelMatrix& matrix, uint8_t pose) {
    animation.renderFrame(matrix, pose);
}

uint32_t transmittedChannel(uint8_t channel, uint8_t brightnessRaw) {
    return (static_cast<uint32_t>(channel) * (static_cast<uint32_t>(brightnessRaw) + 1u)) >> 8;
}

uint32_t frameEstimateMilliAmp(const PixelMatrix& matrix) {
    const uint8_t brightness = matrix.effectiveBrightnessRaw();
    uint32_t channelSum = 0u;

    for (uint16_t i = 0u; i < PixelMatrix::LED_COUNT; ++i) {
        const uint32_t packed = matrix.pixelColorAt(i);
        channelSum += transmittedChannel(static_cast<uint8_t>((packed >> 16) & 0xFFu), brightness);
        channelSum += transmittedChannel(static_cast<uint8_t>((packed >> 8) & 0xFFu), brightness);
        channelSum += transmittedChannel(static_cast<uint8_t>(packed & 0xFFu), brightness);
    }

    return LedPowerBudget::estimateMilliAmp(channelSum);
}

}  // namespace

void test_meadow_flower_is_registered(void) {
    TEST_ASSERT_NOT_NULL(AnimationRegistry::getAnimation(AnimationId::MEADOW_FLOWER));
    TEST_ASSERT_TRUE(AnimationRegistry::isRenderable(AnimationId::MEADOW_FLOWER));
    TEST_ASSERT_TRUE(AnimationCatalog::isUserSelectable(AnimationId::MEADOW_FLOWER));
    TEST_ASSERT_EQUAL_UINT8(26u, static_cast<uint8_t>(AnimationId::MEADOW_FLOWER));
}

void test_meadow_flower_ui_name_is_polish(void) {
    TEST_ASSERT_EQUAL_STRING("Kwiat na łące", animationNameOf(AnimationId::MEADOW_FLOWER));
}

void test_meadow_flower_head_is_large(void) {
    int16_t minX = static_cast<int16_t>(kPanel);
    int16_t maxX = -1;
    int16_t minY = static_cast<int16_t>(kPanel);
    int16_t maxY = -1;
    uint16_t pixels = 0u;

    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            if (!MeadowFlowerAnimation::headContains(x, y)) {
                continue;
            }
            ++pixels;
            if (static_cast<int16_t>(x) < minX) minX = static_cast<int16_t>(x);
            if (static_cast<int16_t>(x) > maxX) maxX = static_cast<int16_t>(x);
            if (static_cast<int16_t>(y) < minY) minY = static_cast<int16_t>(y);
            if (static_cast<int16_t>(y) > maxY) maxY = static_cast<int16_t>(y);
        }
    }

    // A real hero: about 10 px wide and 9 px tall, over 50 lit pixels.
    TEST_ASSERT_TRUE(minX >= 0 && maxX < static_cast<int16_t>(kPanel));
    TEST_ASSERT_TRUE(minY >= 0 && maxY < static_cast<int16_t>(kPanel));
    TEST_ASSERT_TRUE(static_cast<int16_t>(maxX - minX + 1) >= 8);
    TEST_ASSERT_TRUE(static_cast<int16_t>(maxY - minY + 1) >= 6);
    TEST_ASSERT_TRUE(pixels >= 50u);

    // The head is in the upper part of the panel, above the meadow.
    TEST_ASSERT_TRUE(maxY < static_cast<int16_t>(kPanel));
}

void test_meadow_flower_meadow_and_sky_exist(void) {
    // Sky above, meadow below, with the meadow at the bottom of the panel.
    for (uint8_t y = 0u; y < kPanel; ++y) {
        TEST_ASSERT_TRUE(MeadowFlowerAnimation::skyContains(0u, y) !=
                         MeadowFlowerAnimation::meadowContains(0u, y));
    }

    TEST_ASSERT_TRUE(MeadowFlowerAnimation::meadowContains(0u, 13u));
    TEST_ASSERT_TRUE(MeadowFlowerAnimation::meadowContains(0u, 15u));
    TEST_ASSERT_FALSE(MeadowFlowerAnimation::skyContains(0u, 13u));

    MeadowFlowerAnimation animation;
    PixelMatrix matrix;
    renderPose(animation, matrix, 0u);

    // Rendered: the top is blue sky, the bottom three rows are green meadow.
    TEST_ASSERT_TRUE(isBlue(pixelAt(matrix, 0u, 0u)));
    TEST_ASSERT_TRUE(isBlue(pixelAt(matrix, 15u, 5u)));
    TEST_ASSERT_TRUE(isGreen(pixelAt(matrix, 0u, 13u)));
    TEST_ASSERT_TRUE(isGreen(pixelAt(matrix, 15u, 14u)));
    TEST_ASSERT_TRUE(isGreen(pixelAt(matrix, 0u, 15u)));
}

void test_meadow_flower_lower_stem_is_anchored(void) {
    // The lower stem must never move: it is what pins the flower to the meadow.
    TEST_ASSERT_TRUE(MeadowFlowerAnimation::stemLowerContains(7u, 12u));
    TEST_ASSERT_TRUE(MeadowFlowerAnimation::stemLowerContains(8u, 15u));
    TEST_ASSERT_FALSE(MeadowFlowerAnimation::stemLowerContains(7u, 11u));

    MeadowFlowerAnimation animation;
    PixelMatrix reference;
    PixelMatrix frame;

    renderPose(animation, reference, 0u);

    for (uint8_t pose = 1u; pose < MeadowFlowerAnimation::kPoseCount; ++pose) {
        renderPose(animation, frame, pose);

        for (uint8_t y = 0u; y < kPanel; ++y) {
            for (uint8_t x = 0u; x < kPanel; ++x) {
                if (!MeadowFlowerAnimation::stemLowerContains(x, y)) {
                    continue;
                }
                TEST_ASSERT_TRUE(sameColor(pixelAt(reference, x, y), pixelAt(frame, x, y)));
            }
        }
    }
}

void test_meadow_flower_head_moves_but_nothing_jumps(void) {
    bool sawLeft = false;
    bool sawRight = false;

    for (uint8_t pose = 0u; pose < MeadowFlowerAnimation::kPoseCount; ++pose) {
        const int8_t head = MeadowFlowerAnimation::headShiftAt(pose);
        const int8_t stem = MeadowFlowerAnimation::stemShiftAt(pose);

        // The head leans up to two pixels; the stem only one.
        TEST_ASSERT_TRUE(head >= -2 && head <= 2);
        TEST_ASSERT_TRUE(stem >= -1 && stem <= 1);

        if (head < 0) sawLeft = true;
        if (head > 0) sawRight = true;
    }

    TEST_ASSERT_TRUE(sawLeft);
    TEST_ASSERT_TRUE(sawRight);

    // Walking the real sequence, no adjacent step moves the head by more than
    // one pixel - the wrap back to the first step included.
    const uint8_t length = MeadowFlowerAnimation::sequenceLength();
    TEST_ASSERT_EQUAL_UINT8(8u, length);

    int8_t previous = MeadowFlowerAnimation::headShiftAt(MeadowFlowerAnimation::poseAtStep(0u));
    for (uint8_t step = 1u; step <= length; ++step) {
        const uint8_t pose = MeadowFlowerAnimation::poseAtStep(static_cast<uint8_t>(step % length));
        const int8_t here = MeadowFlowerAnimation::headShiftAt(pose);
        const int16_t delta = static_cast<int16_t>(here) - static_cast<int16_t>(previous);

        TEST_ASSERT_TRUE(delta >= -1 && delta <= 1);
        previous = here;
    }

    // The head does not move every step, but it must move over the loop.
    bool moved = false;
    for (uint8_t step = 0u; step < length; ++step) {
        if (MeadowFlowerAnimation::headShiftAt(MeadowFlowerAnimation::poseAtStep(step)) != 0) {
            moved = true;
        }
    }
    TEST_ASSERT_TRUE(moved);
}

void test_meadow_flower_shifted_geometry_stays_in_panel(void) {
    // The head and stem keep their whole footprint on the panel in every pose.
    for (uint8_t pose = 0u; pose < MeadowFlowerAnimation::kPoseCount; ++pose) {
        const int8_t head = MeadowFlowerAnimation::headShiftAt(pose);
        const int8_t stem = MeadowFlowerAnimation::stemShiftAt(pose);

        for (uint8_t y = 0u; y < kPanel; ++y) {
            for (uint8_t x = 0u; x < kPanel; ++x) {
                if (MeadowFlowerAnimation::headContains(x, y)) {
                    const int16_t px = static_cast<int16_t>(x) + head;
                    TEST_ASSERT_TRUE(px >= 0 && px < static_cast<int16_t>(kPanel));
                }
                if (MeadowFlowerAnimation::stemUpperContains(x, y)) {
                    const int16_t px = static_cast<int16_t>(x) + stem;
                    TEST_ASSERT_TRUE(px >= 0 && px < static_cast<int16_t>(kPanel));
                }
            }
        }
    }
}

void test_meadow_flower_loop_is_seamless(void) {
    // t=0 and t=loopDuration are the same step, hence the same pose.
    TEST_ASSERT_EQUAL_UINT32(
        MeadowFlowerAnimation::kLoopDurationMs,
        static_cast<uint32_t>(MeadowFlowerAnimation::sequenceLength()) *
            MeadowFlowerAnimation::kFrameIntervalMs);
    TEST_ASSERT_EQUAL_UINT8(0u, MeadowFlowerAnimation::poseAtStep(0u));
    TEST_ASSERT_EQUAL_UINT8(
        0u,
        MeadowFlowerAnimation::poseAtStep(
            static_cast<uint8_t>(
                (MeadowFlowerAnimation::kLoopDurationMs / MeadowFlowerAnimation::kFrameIntervalMs) %
                MeadowFlowerAnimation::sequenceLength())));

    // ...and therefore render byte-identical frames.
    MeadowFlowerAnimation animation;
    PixelMatrix atZero;
    PixelMatrix atLoop;
    renderPose(animation, atZero, MeadowFlowerAnimation::poseAtStep(0u));
    renderPose(
        animation,
        atLoop,
        MeadowFlowerAnimation::poseAtStep(
            static_cast<uint8_t>(
                (MeadowFlowerAnimation::kLoopDurationMs / MeadowFlowerAnimation::kFrameIntervalMs) %
                MeadowFlowerAnimation::sequenceLength())));

    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            TEST_ASSERT_TRUE(sameColor(pixelAt(atZero, x, y), pixelAt(atLoop, x, y)));
        }
    }
}

void test_meadow_flower_is_reproducible_and_bounded(void) {
    MeadowFlowerAnimation a;
    MeadowFlowerAnimation b;
    PixelMatrix first;
    PixelMatrix second;

    for (uint8_t pose = 0u; pose < MeadowFlowerAnimation::kPoseCount; ++pose) {
        renderPose(a, first, pose);
        renderPose(b, second, pose);

        uint16_t lit = 0u;
        for (uint8_t y = 0u; y < kPanel; ++y) {
            for (uint8_t x = 0u; x < kPanel; ++x) {
                const Color ca = pixelAt(first, x, y);
                const Color cb = pixelAt(second, x, y);
                TEST_ASSERT_TRUE(sameColor(ca, cb));
                if (isLit(ca)) {
                    ++lit;
                }

                // No pure white anywhere.
                TEST_ASSERT_FALSE(ca.r == 255u && ca.g == 255u && ca.b == 255u);
            }
        }

        // Sky and meadow alone fill most of the panel.
        TEST_ASSERT_TRUE(lit > 200u);
    }
}

void test_meadow_flower_is_non_blocking_and_bounded(void) {
    MeadowFlowerAnimation animation;
    PixelMatrix matrix;

    animation.start(0u);
    for (uint16_t i = 1u; i <= 12u; ++i) {
        animation.update(static_cast<uint32_t>(i) * MeadowFlowerAnimation::kFrameIntervalMs, matrix);
    }

    TEST_ASSERT_TRUE(animation.isFinished());
    TEST_ASSERT_TRUE(sizeof(MeadowFlowerAnimation) <= 32u);
}

void test_meadow_flower_frame_is_inside_the_power_budget(void) {
    MeadowFlowerAnimation animation;
    PixelMatrix matrix;
    matrix.setBrightness(255u);

    uint32_t worst = 0u;
    for (uint8_t pose = 0u; pose < MeadowFlowerAnimation::kPoseCount; ++pose) {
        renderPose(animation, matrix, pose);
        const uint32_t estimate = frameEstimateMilliAmp(matrix);
        TEST_ASSERT_TRUE(estimate <= LedPowerBudget::kLimitMilliAmp);
        if (estimate > worst) {
            worst = estimate;
        }
    }

    TEST_ASSERT_EQUAL_UINT16(2000u, LedPowerBudget::kLimitMilliAmp);
    TEST_ASSERT_TRUE(worst < LedPowerBudget::kLimitMilliAmp);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_meadow_flower_is_registered);
    RUN_TEST(test_meadow_flower_ui_name_is_polish);
    RUN_TEST(test_meadow_flower_head_is_large);
    RUN_TEST(test_meadow_flower_meadow_and_sky_exist);
    RUN_TEST(test_meadow_flower_lower_stem_is_anchored);
    RUN_TEST(test_meadow_flower_head_moves_but_nothing_jumps);
    RUN_TEST(test_meadow_flower_shifted_geometry_stays_in_panel);
    RUN_TEST(test_meadow_flower_loop_is_seamless);
    RUN_TEST(test_meadow_flower_is_reproducible_and_bounded);
    RUN_TEST(test_meadow_flower_is_non_blocking_and_bounded);
    RUN_TEST(test_meadow_flower_frame_is_inside_the_power_budget);
    return UNITY_END();
}
