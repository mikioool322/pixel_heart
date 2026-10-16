#include <unity.h>

#define private public
#include "animations/LighthouseAnimation.h"
#undef private

#include "config/LedPowerBudget.h"
#include "control/AnimationCatalog.h"
#include "core/AnimationId.h"
#include "core/AnimationNames.h"
#include "core/AnimationRegistry.h"
#include "core/PixelMatrix.h"

// LIGHTHOUSE - a still night scene with a sweeping beam.
//
// What this file protects:
//   - the lighthouse never moves: every tower pixel is identical at every step,
//   - the sea never moves either, except for the three beam reflections,
//   - the beam starts at the beacon and widens with distance,
//   - the sweep is the 12-step ping-pong 0 1 2 3 4 5 6 5 4 3 2 1, so the
//     reversal has no jump,
//   - the night stays dark and nothing is pure white,
//   - t=0 and t=loopDuration are the same logical frame.

namespace {

constexpr uint8_t kPanel = 16u;

Color pixelAt(const PixelMatrix& matrix, uint8_t x, uint8_t y) {
    const uint32_t packed =
        matrix.pixelColorAt(PixelMatrix::xyToIndexStatic(x, y));
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

uint8_t stepAtElapsed(uint32_t elapsedMs) {
    return static_cast<uint8_t>(
        (elapsedMs / LighthouseAnimation::kStepIntervalMs) % LighthouseAnimation::kStepCount);
}

bool isBeamColor(const Color& c) {
    // The three beam bands, plus the scaled reflections which share the hue.
    return c.r > c.g && c.g >= c.b && c.r > 40u;
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

void test_lighthouse_is_registered(void) {
    TEST_ASSERT_NOT_NULL(AnimationRegistry::getAnimation(AnimationId::LIGHTHOUSE));
    TEST_ASSERT_TRUE(AnimationRegistry::isRenderable(AnimationId::LIGHTHOUSE));
    TEST_ASSERT_TRUE(AnimationCatalog::isUserSelectable(AnimationId::LIGHTHOUSE));
    TEST_ASSERT_EQUAL_UINT8(24u, static_cast<uint8_t>(AnimationId::LIGHTHOUSE));
}

void test_lighthouse_ui_name_is_polish(void) {
    TEST_ASSERT_EQUAL_STRING("Latarnia morska", animationNameOf(AnimationId::LIGHTHOUSE));
}

void test_lighthouse_is_static_and_on_the_left(void) {
    int16_t minX = static_cast<int16_t>(kPanel);
    int16_t maxX = -1;

    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            if (!LighthouseAnimation::towerContains(x, y)) {
                continue;
            }
            if (static_cast<int16_t>(x) < minX) minX = static_cast<int16_t>(x);
            if (static_cast<int16_t>(x) > maxX) maxX = static_cast<int16_t>(x);
        }
    }

    TEST_ASSERT_TRUE(minX >= 0);
    TEST_ASSERT_TRUE(maxX <= 6);   // the lighthouse lives on the left
    TEST_ASSERT_TRUE(maxX - minX + 1 >= 4);

    // The beacon is the beam's origin and sits at the top of the tower.
    TEST_ASSERT_TRUE(LighthouseAnimation::beaconContains(3u, 6u));
    TEST_ASSERT_TRUE(LighthouseAnimation::beaconContains(4u, 6u));

    // Every tower pixel is identical at every step.
    LighthouseAnimation animation;
    PixelMatrix reference;
    PixelMatrix frame;

    animation.renderFrame(reference, 0u);

    for (uint8_t step = 1u; step < LighthouseAnimation::kStepCount; ++step) {
        animation.renderFrame(frame, step);

        for (uint8_t y = 0u; y < kPanel; ++y) {
            for (uint8_t x = 0u; x < kPanel; ++x) {
                if (!LighthouseAnimation::towerContains(x, y) &&
                    !LighthouseAnimation::baseContains(x, y)) {
                    continue;
                }
                TEST_ASSERT_TRUE(sameColor(pixelAt(reference, x, y), pixelAt(frame, x, y)));
            }
        }
    }
}

void test_lighthouse_sea_is_static_except_the_reflections(void) {
    const uint8_t reflectionRow = 15u;

    LighthouseAnimation animation;
    PixelMatrix reference;
    PixelMatrix frame;

    animation.renderFrame(reference, 0u);

    for (uint8_t step = 1u; step < LighthouseAnimation::kStepCount; ++step) {
        animation.renderFrame(frame, step);

        for (uint8_t x = 8u; x < kPanel; ++x) {
            bool isReflection = false;
            for (uint8_t i = 0u; i < LighthouseAnimation::reflectionCount(); ++i) {
                if (LighthouseAnimation::reflectionX(i) == x) {
                    isReflection = true;
                }
            }

            if (isReflection) {
                continue;
            }

            TEST_ASSERT_TRUE(sameColor(
                pixelAt(reference, x, reflectionRow), pixelAt(frame, x, reflectionRow)));
        }
    }
}

void test_lighthouse_beam_starts_at_the_beacon(void) {
    // At the column next to the beacon the beam is only three pixels tall and
    // centred on the beacon height, for every elevation.
    for (uint8_t elevation = 0u; elevation < LighthouseAnimation::kBeamElevationCount; ++elevation) {
        TEST_ASSERT_EQUAL_UINT8(
            LighthouseAnimation::kBeamOriginY,
            LighthouseAnimation::beamCentreY(elevation, LighthouseAnimation::kBeamOriginX + 1u));
    }

    LighthouseAnimation animation;
    PixelMatrix matrix;
    animation.renderFrame(matrix, 0u);

    const uint8_t centre = LighthouseAnimation::beamCentreY(0u, LighthouseAnimation::kBeamOriginX + 1u);
    TEST_ASSERT_TRUE(isBeamColor(pixelAt(matrix, LighthouseAnimation::kBeamOriginX + 1u, centre)));

    // The tower columns are never reached by the beam: the beacon keeps its own
    // colour at every elevation.
    for (uint8_t step = 0u; step < LighthouseAnimation::kStepCount; ++step) {
        animation.renderFrame(matrix, step);
        for (uint8_t x = 2u; x <= 5u; ++x) {
            for (uint8_t y = 6u; y <= 7u; ++y) {
                const Color c = pixelAt(matrix, x, y);
                TEST_ASSERT_TRUE(c.r == 255u && c.g == 185u && c.b == 55u);
            }
        }
    }
}

void test_lighthouse_beam_widens_with_distance(void) {
    TEST_ASSERT_EQUAL_UINT8(1u, LighthouseAnimation::beamHalfWidth(LighthouseAnimation::kBeamOriginX + 1u));
    TEST_ASSERT_EQUAL_UINT8(3u, LighthouseAnimation::beamHalfWidth(LighthouseAnimation::kBeamFarX));

    uint8_t previous = 0u;
    for (uint8_t x = 0u; x < kPanel; ++x) {
        const uint8_t width = LighthouseAnimation::beamHalfWidth(x);
        TEST_ASSERT_TRUE(width >= previous);
        previous = width;
    }

    // Rendered: at the far edge a centred beam covers 7 rows, not a line.
    LighthouseAnimation animation;
    PixelMatrix matrix;
    animation.renderFrame(matrix, 3u);   // elevation 3 => offset 0

    uint8_t beamPixels = 0u;
    for (uint8_t y = 0u; y < kPanel; ++y) {
        if (isBeamColor(pixelAt(matrix, LighthouseAnimation::kBeamFarX, y))) {
            ++beamPixels;
        }
    }
    TEST_ASSERT_TRUE(beamPixels >= 5u);
    TEST_ASSERT_TRUE(beamPixels <= 9u);
}

void test_lighthouse_beam_sweeps_both_ways_without_teleporting(void) {
    bool sawUp = false;
    bool sawDown = false;

    for (uint8_t step = 0u; step < LighthouseAnimation::kStepCount; ++step) {
        const uint8_t elevation = LighthouseAnimation::elevationAtStep(step);
        const int8_t offset = LighthouseAnimation::elevationOffset(elevation);

        if (offset < 0) sawUp = true;
        if (offset > 0) sawDown = true;

        const uint8_t nextStep = static_cast<uint8_t>((step + 1u) % LighthouseAnimation::kStepCount);
        const uint8_t nextElevation = LighthouseAnimation::elevationAtStep(nextStep);

        const int16_t farHere = LighthouseAnimation::beamCentreY(elevation, LighthouseAnimation::kBeamFarX);
        const int16_t farNext = LighthouseAnimation::beamCentreY(nextElevation, LighthouseAnimation::kBeamFarX);

        // No teleport, including across the wrap.
        TEST_ASSERT_TRUE(farNext - farHere <= 2);
        TEST_ASSERT_TRUE(farNext - farHere >= -2);
    }

    TEST_ASSERT_TRUE(sawUp);
    TEST_ASSERT_TRUE(sawDown);
}

void test_lighthouse_beam_sequence_is_the_designed_ping_pong(void) {
    const uint8_t expected[12] = { 0u, 1u, 2u, 3u, 4u, 5u, 6u, 5u, 4u, 3u, 2u, 1u };

    for (uint8_t step = 0u; step < LighthouseAnimation::kStepCount; ++step) {
        TEST_ASSERT_EQUAL_UINT8(expected[step], LighthouseAnimation::elevationAtStep(step));
    }

    // Wrap: the last step is elevation 1, the first is 0 - one step, no jump.
    TEST_ASSERT_EQUAL_UINT8(
        1u, LighthouseAnimation::elevationAtStep(static_cast<uint8_t>(LighthouseAnimation::kStepCount - 1u)));
    TEST_ASSERT_EQUAL_UINT8(0u, LighthouseAnimation::elevationAtStep(0u));
}

void test_lighthouse_night_stays_dark(void) {
    LighthouseAnimation animation;
    PixelMatrix matrix;

    for (uint8_t step = 0u; step < LighthouseAnimation::kStepCount; ++step) {
        animation.renderFrame(matrix, step);

        // A sky corner the beam can never reach.
        const Color sky = pixelAt(matrix, 0u, 0u);
        TEST_ASSERT_TRUE(isLit(sky));
        TEST_ASSERT_TRUE(static_cast<uint16_t>(sky.r) + sky.g + sky.b < 120u);

        // Nothing anywhere is pure white.
        for (uint8_t y = 0u; y < kPanel; ++y) {
            for (uint8_t x = 0u; x < kPanel; ++x) {
                const Color c = pixelAt(matrix, x, y);
                TEST_ASSERT_FALSE(c.r == 255u && c.g == 255u && c.b == 255u);
            }
        }
    }
}

void test_lighthouse_loop_is_seamless(void) {
    TEST_ASSERT_EQUAL_UINT8(0u, stepAtElapsed(0u));
    TEST_ASSERT_EQUAL_UINT8(0u, stepAtElapsed(LighthouseAnimation::kLoopDurationMs));
    TEST_ASSERT_EQUAL_UINT8(
        0u,
        static_cast<uint8_t>(
            (LighthouseAnimation::kLoopDurationMs / LighthouseAnimation::kStepIntervalMs) %
            LighthouseAnimation::kStepCount));

    LighthouseAnimation animation;
    PixelMatrix atZero;
    PixelMatrix atLoop;
    animation.renderFrame(atZero, stepAtElapsed(0u));
    animation.renderFrame(atLoop, stepAtElapsed(LighthouseAnimation::kLoopDurationMs));

    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            TEST_ASSERT_TRUE(sameColor(pixelAt(atZero, x, y), pixelAt(atLoop, x, y)));
        }
    }
}

void test_lighthouse_frame_is_inside_the_power_budget(void) {
    LighthouseAnimation animation;
    PixelMatrix matrix;
    matrix.setBrightness(255u);

    uint32_t worst = 0u;
    for (uint8_t step = 0u; step < LighthouseAnimation::kStepCount; ++step) {
        animation.renderFrame(matrix, step);
        const uint32_t estimate = frameEstimateMilliAmp(matrix);
        TEST_ASSERT_TRUE(estimate <= LedPowerBudget::kLimitMilliAmp);
        if (estimate > worst) {
            worst = estimate;
        }
    }

    TEST_ASSERT_EQUAL_UINT16(2000u, LedPowerBudget::kLimitMilliAmp);
    TEST_ASSERT_TRUE(worst < LedPowerBudget::kLimitMilliAmp);
}

void test_lighthouse_is_non_blocking_and_bounded(void) {
    LighthouseAnimation animation;
    PixelMatrix matrix;

    animation.start(0u);
    for (uint16_t i = 1u; i <= 20u; ++i) {
        animation.update(static_cast<uint32_t>(i) * LighthouseAnimation::kStepIntervalMs, matrix);
    }

    TEST_ASSERT_TRUE(animation.isFinished());
    TEST_ASSERT_TRUE(sizeof(LighthouseAnimation) <= 32u);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_lighthouse_is_registered);
    RUN_TEST(test_lighthouse_ui_name_is_polish);
    RUN_TEST(test_lighthouse_is_static_and_on_the_left);
    RUN_TEST(test_lighthouse_sea_is_static_except_the_reflections);
    RUN_TEST(test_lighthouse_beam_starts_at_the_beacon);
    RUN_TEST(test_lighthouse_beam_widens_with_distance);
    RUN_TEST(test_lighthouse_beam_sweeps_both_ways_without_teleporting);
    RUN_TEST(test_lighthouse_beam_sequence_is_the_designed_ping_pong);
    RUN_TEST(test_lighthouse_night_stays_dark);
    RUN_TEST(test_lighthouse_loop_is_seamless);
    RUN_TEST(test_lighthouse_frame_is_inside_the_power_budget);
    RUN_TEST(test_lighthouse_is_non_blocking_and_bounded);
    return UNITY_END();
}
