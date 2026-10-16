#include <unity.h>

#define private public
#include "animations/DiscoBallAnimation.h"
#undef private

#include "config/LedPowerBudget.h"
#include "control/AnimationCatalog.h"
#include "core/AnimationId.h"
#include "core/AnimationNames.h"
#include "core/AnimationRegistry.h"
#include "core/PixelMatrix.h"

// DISCO_BALL - a big silver-blue sphere that never changes shape.
//
// What this file protects:
//   - the ball geometry is IDENTICAL at every phase: the rotation is an
//     illusion built only from tile colours, one travelling highlight and the
//     changing brightness of the corner beams and the floor,
//   - the tile classes really do shift with the phase,
//   - the highlight walks around the ball and returns exactly to the start,
//   - nothing ever strobes and no frame is washed out white,
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

uint8_t phaseAtElapsed(uint32_t elapsedMs) {
    return static_cast<uint8_t>(
        (elapsedMs / DiscoBallAnimation::kStepIntervalMs) % DiscoBallAnimation::kPhaseCount);
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

void test_disco_ball_is_registered(void) {
    TEST_ASSERT_NOT_NULL(AnimationRegistry::getAnimation(AnimationId::DISCO_BALL));
    TEST_ASSERT_TRUE(AnimationRegistry::isRenderable(AnimationId::DISCO_BALL));
    TEST_ASSERT_TRUE(AnimationCatalog::isUserSelectable(AnimationId::DISCO_BALL));
    TEST_ASSERT_EQUAL_UINT8(25u, static_cast<uint8_t>(AnimationId::DISCO_BALL));
}

void test_disco_ball_ui_name_is_polish(void) {
    TEST_ASSERT_EQUAL_STRING("Kula disco", animationNameOf(AnimationId::DISCO_BALL));
}

void test_disco_ball_is_a_large_centred_sphere(void) {
    int16_t minX = static_cast<int16_t>(kPanel);
    int16_t maxX = -1;
    int16_t minY = static_cast<int16_t>(kPanel);
    int16_t maxY = -1;
    uint16_t pixels = 0u;

    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            if (!DiscoBallAnimation::ballContains(x, y)) {
                continue;
            }
            ++pixels;
            if (static_cast<int16_t>(x) < minX) minX = static_cast<int16_t>(x);
            if (static_cast<int16_t>(x) > maxX) maxX = static_cast<int16_t>(x);
            if (static_cast<int16_t>(y) < minY) minY = static_cast<int16_t>(y);
            if (static_cast<int16_t>(y) > maxY) maxY = static_cast<int16_t>(y);
        }
    }

    TEST_ASSERT_TRUE(minX >= 2 && maxX <= 13);
    TEST_ASSERT_TRUE(minY >= 3 && maxY <= 13);
    TEST_ASSERT_TRUE(static_cast<int16_t>(maxX - minX + 1) >= 9);
    TEST_ASSERT_TRUE(static_cast<int16_t>(maxY - minY + 1) >= 8);
    TEST_ASSERT_TRUE(pixels >= 60u);

    // A hanging wire above it.
    TEST_ASSERT_TRUE(DiscoBallAnimation::suspensionContains(7u, 1u));
    TEST_ASSERT_TRUE(DiscoBallAnimation::suspensionContains(7u, 3u));
    TEST_ASSERT_FALSE(DiscoBallAnimation::suspensionContains(7u, 5u));
}

void test_disco_ball_geometry_is_identical_at_every_phase(void) {
    DiscoBallAnimation animation;
    PixelMatrix reference;
    PixelMatrix frame;

    animation.renderFrame(reference, 0u);

    for (uint8_t phase = 1u; phase < DiscoBallAnimation::kPhaseCount; ++phase) {
        animation.renderFrame(frame, phase);

        for (uint8_t y = 0u; y < kPanel; ++y) {
            for (uint8_t x = 0u; x < kPanel; ++x) {
                // The lit set never changes: not one pixel appears or vanishes.
                TEST_ASSERT_EQUAL_UINT8(
                    isLit(pixelAt(reference, x, y)) ? 1u : 0u,
                    isLit(pixelAt(frame, x, y)) ? 1u : 0u);

                // The ball itself is always exactly the ball mask.
                if (!DiscoBallAnimation::ballContains(x, y)) {
                    continue;
                }
                TEST_ASSERT_EQUAL_UINT8(1u, isLit(pixelAt(frame, x, y)) ? 1u : 0u);
            }
        }
    }
}

void test_disco_ball_tile_colours_shift_with_the_phase(void) {
    // The class really is a function of the phase.
    bool classChanged = false;
    for (uint8_t y = 4u; y <= 12u; ++y) {
        for (uint8_t x = 3u; x <= 12u; ++x) {
            if (!DiscoBallAnimation::ballContains(x, y)) {
                continue;
            }
            if (DiscoBallAnimation::tileClassAt(x, y, 0u) !=
                DiscoBallAnimation::tileClassAt(x, y, 1u)) {
                classChanged = true;
            }
        }
    }
    TEST_ASSERT_TRUE(classChanged);

    // ...and the rendered frame really does change, while staying a ball.
    DiscoBallAnimation animation;
    PixelMatrix first;
    PixelMatrix second;
    animation.renderFrame(first, 0u);
    animation.renderFrame(second, 1u);

    bool sawDifferentColour = false;
    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            if (!DiscoBallAnimation::ballContains(x, y)) {
                continue;
            }
            if (!sameColor(pixelAt(first, x, y), pixelAt(second, x, y))) {
                sawDifferentColour = true;
            }
        }
    }
    TEST_ASSERT_TRUE(sawDifferentColour);
}

void test_disco_ball_highlight_travels_around_the_ball(void) {
    TEST_ASSERT_EQUAL_UINT8(2u, DiscoBallAnimation::highlightPixelCount());

    // Every anchor is inside the ball...
    for (uint8_t phase = 0u; phase < DiscoBallAnimation::kPhaseCount; ++phase) {
        const uint8_t ax = DiscoBallAnimation::highlightAnchorX(phase);
        const uint8_t ay = DiscoBallAnimation::highlightAnchorY(phase);
        TEST_ASSERT_TRUE(DiscoBallAnimation::ballContains(ax, ay));
    }

    // ...the anchor actually moves between phases...
    bool moves = false;
    for (uint8_t phase = 0u; phase < DiscoBallAnimation::kPhaseCount; ++phase) {
        const uint8_t next = static_cast<uint8_t>((phase + 1u) % DiscoBallAnimation::kPhaseCount);
        if (DiscoBallAnimation::highlightAnchorX(phase) !=
                DiscoBallAnimation::highlightAnchorX(next) ||
            DiscoBallAnimation::highlightAnchorY(phase) !=
                DiscoBallAnimation::highlightAnchorY(next)) {
            moves = true;
        }
    }
    TEST_ASSERT_TRUE(moves);

    // ...and returns exactly to where it started after a full loop.
    TEST_ASSERT_EQUAL_UINT8(
        DiscoBallAnimation::highlightAnchorX(0u),
        DiscoBallAnimation::highlightAnchorX(DiscoBallAnimation::kPhaseCount));
    TEST_ASSERT_EQUAL_UINT8(
        DiscoBallAnimation::highlightAnchorY(0u),
        DiscoBallAnimation::highlightAnchorY(DiscoBallAnimation::kPhaseCount));
}

void test_disco_ball_beams_and_floor_animate_deterministically(void) {
    // Corner beams: four of them, and their brightness cycles with the phase.
    TEST_ASSERT_EQUAL_UINT8(4u, DiscoBallAnimation::cornerBeamCount());
    TEST_ASSERT_TRUE(DiscoBallAnimation::cornerBeamLength() >= 2u);

    for (uint8_t i = 0u; i < DiscoBallAnimation::cornerBeamCount(); ++i) {
        bool changed = false;
        for (uint8_t phase = 0u; phase < DiscoBallAnimation::kPhaseCount; ++phase) {
            if (DiscoBallAnimation::cornerBeamLevel(i, phase) !=
                DiscoBallAnimation::cornerBeamLevel(i, 0u)) {
                changed = true;
            }
        }
        TEST_ASSERT_TRUE(changed);

        // Every step of a beam stays inside the panel.
        for (uint8_t step = 0u; step < DiscoBallAnimation::cornerBeamLength(); ++step) {
            TEST_ASSERT_TRUE(DiscoBallAnimation::cornerBeamX(i, step) < kPanel);
            TEST_ASSERT_TRUE(DiscoBallAnimation::cornerBeamY(i, step) < kPanel);
        }
    }

    // Floor reflections: present, in the bottom rows, animated.
    TEST_ASSERT_TRUE(DiscoBallAnimation::floorCount() >= 4u);
    for (uint8_t i = 0u; i < DiscoBallAnimation::floorCount(); ++i) {
        TEST_ASSERT_TRUE(DiscoBallAnimation::floorY(i) >= 14u);

        bool changed = false;
        for (uint8_t phase = 0u; phase < DiscoBallAnimation::kPhaseCount; ++phase) {
            if (DiscoBallAnimation::floorLevel(i, phase) !=
                DiscoBallAnimation::floorLevel(i, 0u)) {
                changed = true;
            }
        }
        TEST_ASSERT_TRUE(changed);
    }
}

void test_disco_ball_full_loop_is_eight_phases(void) {
    TEST_ASSERT_EQUAL_UINT8(8u, DiscoBallAnimation::phaseCount());

    for (uint8_t step = 0u; step < 16u; ++step) {
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(step % 8u),
            DiscoBallAnimation::phaseAtStep(step));
    }

    // The play duration is exactly one whole loop.
    TEST_ASSERT_EQUAL_UINT32(1760u, DiscoBallAnimation::kLoopDurationMs);
    TEST_ASSERT_EQUAL_UINT8(
        0u,
        static_cast<uint8_t>(
            (DiscoBallAnimation::kLoopDurationMs / DiscoBallAnimation::kStepIntervalMs) %
            DiscoBallAnimation::kPhaseCount));
}

void test_disco_ball_never_strobes_or_washes_out(void) {
    DiscoBallAnimation animation;
    PixelMatrix matrix;

    for (uint8_t phase = 0u; phase < DiscoBallAnimation::kPhaseCount; ++phase) {
        animation.renderFrame(matrix, phase);

        uint16_t veryBright = 0u;
        uint32_t channelSum = 0u;

        for (uint8_t y = 0u; y < kPanel; ++y) {
            for (uint8_t x = 0u; x < kPanel; ++x) {
                const Color c = pixelAt(matrix, x, y);
                if (c.r > 200u && c.g > 200u && c.b > 200u) {
                    ++veryBright;
                }
                channelSum += c.r;
                channelSum += c.g;
                channelSum += c.b;
            }
        }

        // Only the two highlight pixels may ever be that bright.
        TEST_ASSERT_TRUE(veryBright <= 3u);

        // The frame stays a dark room with a lit ball, not a white wash.
        TEST_ASSERT_TRUE(channelSum < 60000u);
    }
}

void test_disco_ball_loop_is_seamless(void) {
    TEST_ASSERT_EQUAL_UINT8(0u, phaseAtElapsed(0u));
    TEST_ASSERT_EQUAL_UINT8(0u, phaseAtElapsed(DiscoBallAnimation::kLoopDurationMs));

    DiscoBallAnimation animation;
    PixelMatrix atZero;
    PixelMatrix atLoop;
    animation.renderFrame(atZero, phaseAtElapsed(0u));
    animation.renderFrame(atLoop, phaseAtElapsed(DiscoBallAnimation::kLoopDurationMs));

    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            TEST_ASSERT_TRUE(sameColor(pixelAt(atZero, x, y), pixelAt(atLoop, x, y)));
        }
    }
}

void test_disco_ball_frame_is_inside_the_power_budget(void) {
    DiscoBallAnimation animation;
    PixelMatrix matrix;
    matrix.setBrightness(255u);

    uint32_t worst = 0u;
    for (uint8_t phase = 0u; phase < DiscoBallAnimation::kPhaseCount; ++phase) {
        animation.renderFrame(matrix, phase);
        const uint32_t estimate = frameEstimateMilliAmp(matrix);
        TEST_ASSERT_TRUE(estimate <= LedPowerBudget::kLimitMilliAmp);
        if (estimate > worst) {
            worst = estimate;
        }
    }

    TEST_ASSERT_EQUAL_UINT16(2000u, LedPowerBudget::kLimitMilliAmp);
    TEST_ASSERT_TRUE(worst < LedPowerBudget::kLimitMilliAmp);
}

void test_disco_ball_is_non_blocking_and_bounded(void) {
    DiscoBallAnimation animation;
    PixelMatrix matrix;

    animation.start(0u);
    for (uint16_t i = 1u; i <= 16u; ++i) {
        animation.update(static_cast<uint32_t>(i) * DiscoBallAnimation::kStepIntervalMs, matrix);
    }

    TEST_ASSERT_TRUE(animation.isFinished());
    TEST_ASSERT_TRUE(sizeof(DiscoBallAnimation) <= 32u);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_disco_ball_is_registered);
    RUN_TEST(test_disco_ball_ui_name_is_polish);
    RUN_TEST(test_disco_ball_is_a_large_centred_sphere);
    RUN_TEST(test_disco_ball_geometry_is_identical_at_every_phase);
    RUN_TEST(test_disco_ball_tile_colours_shift_with_the_phase);
    RUN_TEST(test_disco_ball_highlight_travels_around_the_ball);
    RUN_TEST(test_disco_ball_beams_and_floor_animate_deterministically);
    RUN_TEST(test_disco_ball_full_loop_is_eight_phases);
    RUN_TEST(test_disco_ball_never_strobes_or_washes_out);
    RUN_TEST(test_disco_ball_loop_is_seamless);
    RUN_TEST(test_disco_ball_frame_is_inside_the_power_budget);
    RUN_TEST(test_disco_ball_is_non_blocking_and_bounded);
    return UNITY_END();
}
