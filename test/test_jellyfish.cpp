#include <unity.h>

#define private public
#include "animations/JellyfishAnimation.h"
#undef private

#include "config/LedPowerBudget.h"
#include "control/AnimationCatalog.h"
#include "core/AnimationId.h"
#include "core/AnimationNames.h"
#include "core/AnimationRegistry.h"
#include "core/PixelMatrix.h"

// JELLYFISH - a large glowing bell with slowly rippling tentacles.
//
// What this file protects:
//   - the bell is the hero and is ALWAYS present,
//   - the whole body never moves more than one pixel,
//   - the pulse only rescales the jellyfish's own colours and never touches the
//     global brightness authority,
//   - every tentacle step moves any pixel by at most one column, including the
//     wrap from the last pose back to the first,
//   - particles are fully deterministic and never touch the bell,
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

bool isLit(const Color& c) {
    return c.r != 0u || c.g != 0u || c.b != 0u;
}

uint8_t stepAtElapsed(uint32_t elapsedMs) {
    return static_cast<uint8_t>(
        (elapsedMs / JellyfishAnimation::kStepIntervalMs) % JellyfishAnimation::kStepCount);
}

void renderStep(JellyfishAnimation& animation, PixelMatrix& matrix, uint8_t step) {
    animation.renderFrame(matrix, step);
}

uint16_t litCount(const PixelMatrix& matrix) {
    uint16_t count = 0u;
    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            if (isLit(pixelAt(matrix, x, y))) {
                ++count;
            }
        }
    }
    return count;
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

void test_jellyfish_is_registered(void) {
    TEST_ASSERT_NOT_NULL(AnimationRegistry::getAnimation(AnimationId::JELLYFISH));
    TEST_ASSERT_TRUE(AnimationRegistry::isRenderable(AnimationId::JELLYFISH));
    TEST_ASSERT_TRUE(AnimationCatalog::isUserSelectable(AnimationId::JELLYFISH));
    TEST_ASSERT_EQUAL_UINT8(23u, static_cast<uint8_t>(AnimationId::JELLYFISH));
}

void test_jellyfish_ui_name_is_meduza(void) {
    TEST_ASSERT_EQUAL_STRING("Meduza", animationNameOf(AnimationId::JELLYFISH));
}

void test_jellyfish_bell_is_large(void) {
    // Bounding box of the bell silhouette.
    int16_t minX = static_cast<int16_t>(kPanel);
    int16_t maxX = -1;
    int16_t minY = static_cast<int16_t>(kPanel);
    int16_t maxY = -1;
    uint16_t pixels = 0u;

    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            if (!JellyfishAnimation::bellContains(x, y)) {
                continue;
            }
            ++pixels;
            if (static_cast<int16_t>(x) < minX) minX = static_cast<int16_t>(x);
            if (static_cast<int16_t>(x) > maxX) maxX = static_cast<int16_t>(x);
            if (static_cast<int16_t>(y) < minY) minY = static_cast<int16_t>(y);
            if (static_cast<int16_t>(y) > maxY) maxY = static_cast<int16_t>(y);
        }
    }

    TEST_ASSERT_TRUE(minX >= 0 && maxX < static_cast<int16_t>(kPanel));
    TEST_ASSERT_TRUE(minY >= 0 && maxY < static_cast<int16_t>(kPanel));

    // A real hero: ~12 px wide and ~8 px tall, over 70 lit pixels.
    TEST_ASSERT_TRUE(static_cast<int16_t>(maxX - minX + 1) >= 10);
    TEST_ASSERT_TRUE(static_cast<int16_t>(maxY - minY + 1) >= 6);
    TEST_ASSERT_TRUE(pixels >= 60u);
}

void test_jellyfish_bell_is_always_drawn(void) {
    JellyfishAnimation animation;
    PixelMatrix matrix;

    for (uint8_t step = 0u; step < JellyfishAnimation::kStepCount; ++step) {
        renderStep(animation, matrix, step);

        const int8_t dy = JellyfishAnimation::verticalOffsetAt(step);
        TEST_ASSERT_TRUE(dy >= -1 && dy <= 1);

        for (uint8_t y = 0u; y < kPanel; ++y) {
            for (uint8_t x = 0u; x < kPanel; ++x) {
                if (!JellyfishAnimation::bellContains(x, y)) {
                    continue;
                }
                const int16_t py = static_cast<int16_t>(y) + dy;
                TEST_ASSERT_TRUE(py >= 0 && py < static_cast<int16_t>(kPanel));
                TEST_ASSERT_TRUE(isLit(pixelAt(matrix, x, static_cast<uint8_t>(py))));
            }
        }
    }
}

void test_jellyfish_body_never_moves_more_than_one_pixel(void) {
    for (uint8_t step = 0u; step < JellyfishAnimation::kStepCount; ++step) {
        const int8_t here = JellyfishAnimation::verticalOffsetAt(step);
        TEST_ASSERT_TRUE(here >= -1 && here <= 1);

        const uint8_t next = static_cast<uint8_t>((step + 1u) % JellyfishAnimation::kStepCount);
        const int16_t delta = static_cast<int16_t>(JellyfishAnimation::verticalOffsetAt(next)) -
                              static_cast<int16_t>(here);
        TEST_ASSERT_TRUE(delta >= -1 && delta <= 1);
    }

    // A full float cycle must be present, not one static offset.
    bool sawUp = false;
    bool sawDown = false;
    for (uint8_t step = 0u; step < JellyfishAnimation::kStepCount; ++step) {
        if (JellyfishAnimation::verticalOffsetAt(step) < 0) sawUp = true;
        if (JellyfishAnimation::verticalOffsetAt(step) > 0) sawDown = true;
    }
    TEST_ASSERT_TRUE(sawUp);
    TEST_ASSERT_TRUE(sawDown);
}

void test_jellyfish_tentacles_move_but_never_snap(void) {
    const uint8_t count = JellyfishAnimation::tentacleCount();
    TEST_ASSERT_TRUE(count >= 4u && count <= 5u);

    bool sawMove = false;

    for (uint8_t t = 0u; t < count; ++t) {
        const uint8_t length = JellyfishAnimation::tentacleLength(t);
        TEST_ASSERT_TRUE(length > 0u);

        for (uint8_t row = 0u; row < length; ++row) {
            for (uint8_t pose = 0u; pose < JellyfishAnimation::kTentaclePoseCount; ++pose) {
                const int8_t here = JellyfishAnimation::tentacleOffsetAt(t, row, pose);
                const uint8_t nextPose = static_cast<uint8_t>(
                    (pose + 1u) % JellyfishAnimation::kTentaclePoseCount);
                const int16_t delta = static_cast<int16_t>(
                    JellyfishAnimation::tentacleOffsetAt(t, row, nextPose)) -
                    static_cast<int16_t>(here);

                // No tentacle pixel ever moves more than one column, and the
                // wrap from the last pose to the first is included above.
                TEST_ASSERT_TRUE(delta >= -1 && delta <= 1);
                if (delta != 0) {
                    sawMove = true;
                }
            }
        }
    }

    TEST_ASSERT_TRUE(sawMove);
}

void test_jellyfish_tentacles_reach_towards_the_bottom(void) {
    // The longest tentacle must cover most of the way to the last row.
    uint8_t longest = 0u;
    for (uint8_t t = 0u; t < JellyfishAnimation::tentacleCount(); ++t) {
        if (JellyfishAnimation::tentacleLength(t) > longest) {
            longest = JellyfishAnimation::tentacleLength(t);
        }
    }

    TEST_ASSERT_TRUE(static_cast<uint16_t>(longest) >= 6u);

    // Rendered: the bottom rows must contain tentacle pixels for every step.
    JellyfishAnimation animation;
    PixelMatrix matrix;

    for (uint8_t step = 0u; step < JellyfishAnimation::kStepCount; ++step) {
        renderStep(animation, matrix, step);

        bool bottomLit = false;
        for (uint8_t x = 0u; x < kPanel; ++x) {
            if (isLit(pixelAt(matrix, x, 15u)) || isLit(pixelAt(matrix, x, 14u))) {
                bottomLit = true;
            }
        }
        TEST_ASSERT_TRUE(bottomLit);
    }
}

void test_jellyfish_particles_are_deterministic_and_clear_of_the_bell(void) {
    const uint8_t count = JellyfishAnimation::particleCount();
    TEST_ASSERT_TRUE(count >= 4u && count <= 6u);

    for (uint8_t i = 0u; i < count; ++i) {
        const uint8_t x = JellyfishAnimation::particleAtX(i);

        // Particles live only in the columns outside the bell (x 2..13), so
        // they can never merge with it.
        TEST_ASSERT_TRUE(x < 2u || x > 13u);

        for (uint8_t step = 0u; step < JellyfishAnimation::kStepCount; ++step) {
            const uint8_t y = JellyfishAnimation::particleAtY(i, step);
            TEST_ASSERT_TRUE(y < kPanel);
            TEST_ASSERT_TRUE(JellyfishAnimation::particleAtLevel(i, step) > 0u);
        }
    }

    // Same step => same values, always.
    for (uint8_t step = 0u; step < JellyfishAnimation::kStepCount; ++step) {
        for (uint8_t i = 0u; i < count; ++i) {
            TEST_ASSERT_EQUAL_UINT8(
                JellyfishAnimation::particleAtY(i, step),
                JellyfishAnimation::particleAtY(i, step));
            TEST_ASSERT_EQUAL_UINT8(
                JellyfishAnimation::particleAtLevel(i, step),
                JellyfishAnimation::particleAtLevel(i, step));
        }
    }
}

void test_jellyfish_render_is_reproducible(void) {
    // No randomness anywhere: two independent instances produce identical
    // frames for every step.
    JellyfishAnimation a;
    JellyfishAnimation b;
    PixelMatrix first;
    PixelMatrix second;

    for (uint8_t step = 0u; step < JellyfishAnimation::kStepCount; ++step) {
        a.renderFrame(first, step);
        b.renderFrame(second, step);

        for (uint8_t y = 0u; y < kPanel; ++y) {
            for (uint8_t x = 0u; x < kPanel; ++x) {
                const Color ca = pixelAt(first, x, y);
                const Color cb = pixelAt(second, x, y);
                TEST_ASSERT_TRUE(ca.r == cb.r && ca.g == cb.g && ca.b == cb.b);
            }
        }
    }
}

void test_jellyfish_pulse_never_touches_global_brightness(void) {
    JellyfishAnimation animation;
    PixelMatrix matrix;

    matrix.setBrightness(255u);
    const uint8_t before = matrix.effectiveBrightnessRaw();

    for (uint8_t step = 0u; step < JellyfishAnimation::kStepCount; ++step) {
        renderStep(animation, matrix, step);
        TEST_ASSERT_EQUAL_UINT8(before, matrix.effectiveBrightnessRaw());
    }

    TEST_ASSERT_EQUAL_UINT8(LedPowerBudget::kMaxSafeBrightnessRaw, before);

    // The pulse really does change the bell's local brightness, though.
    bool sawPulseChange = false;
    const uint8_t firstLevel = JellyfishAnimation::pulseLevelAt(0u);
    for (uint8_t step = 0u; step < JellyfishAnimation::kStepCount; ++step) {
        if (JellyfishAnimation::pulseLevelAt(step) != firstLevel) {
            sawPulseChange = true;
        }
    }
    TEST_ASSERT_TRUE(sawPulseChange);
}

void test_jellyfish_loop_is_seamless(void) {
    // The modular identity: t=0 and t=loopDuration are the same step.
    TEST_ASSERT_EQUAL_UINT8(0u, stepAtElapsed(0u));
    TEST_ASSERT_EQUAL_UINT8(0u, stepAtElapsed(JellyfishAnimation::kLoopDurationMs));
    TEST_ASSERT_EQUAL_UINT8(
        0u,
        static_cast<uint8_t>(
            (JellyfishAnimation::kLoopDurationMs / JellyfishAnimation::kStepIntervalMs) %
            JellyfishAnimation::kStepCount));

    // ...and therefore render identical frames.
    JellyfishAnimation animation;
    PixelMatrix atZero;
    PixelMatrix atLoop;
    renderStep(animation, atZero, stepAtElapsed(0u));
    renderStep(animation, atLoop, stepAtElapsed(JellyfishAnimation::kLoopDurationMs));

    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            const Color a = pixelAt(atZero, x, y);
            const Color b = pixelAt(atLoop, x, y);
            TEST_ASSERT_TRUE(a.r == b.r && a.g == b.g && a.b == b.b);
        }
    }

    // The last step also leads back into the first with no jump in the body.
    const uint8_t last = static_cast<uint8_t>(JellyfishAnimation::kStepCount - 1u);
    const int16_t delta = static_cast<int16_t>(JellyfishAnimation::verticalOffsetAt(0u)) -
                          static_cast<int16_t>(JellyfishAnimation::verticalOffsetAt(last));
    TEST_ASSERT_TRUE(delta >= -1 && delta <= 1);
}

void test_jellyfish_stays_inside_the_panel(void) {
    JellyfishAnimation animation;
    PixelMatrix matrix;

    for (uint8_t step = 0u; step < JellyfishAnimation::kStepCount; ++step) {
        renderStep(animation, matrix, step);
        TEST_ASSERT_TRUE(litCount(matrix) > 0u);
        TEST_ASSERT_TRUE(litCount(matrix) <= PixelMatrix::LED_COUNT);
    }
}

void test_jellyfish_frame_is_inside_the_power_budget(void) {
    JellyfishAnimation animation;
    PixelMatrix matrix;
    matrix.setBrightness(255u);

    uint32_t worst = 0u;
    for (uint8_t step = 0u; step < JellyfishAnimation::kStepCount; ++step) {
        renderStep(animation, matrix, step);
        const uint32_t estimate = frameEstimateMilliAmp(matrix);
        TEST_ASSERT_TRUE(estimate <= LedPowerBudget::kLimitMilliAmp);
        if (estimate > worst) {
            worst = estimate;
        }
    }

    TEST_ASSERT_EQUAL_UINT16(2000u, LedPowerBudget::kLimitMilliAmp);
    TEST_ASSERT_TRUE(worst < LedPowerBudget::kLimitMilliAmp);
}

void test_jellyfish_is_non_blocking_and_bounded(void) {
    // The whole play is produced by update() alone, with no wait.
    JellyfishAnimation animation;
    PixelMatrix matrix;

    animation.start(0u);
    for (uint16_t i = 1u; i <= 40u; ++i) {
        animation.update(static_cast<uint32_t>(i) * JellyfishAnimation::kStepIntervalMs, matrix);
    }

    TEST_ASSERT_TRUE(animation.isFinished());

    // No heap, no framebuffer: a state cursor only.
    TEST_ASSERT_TRUE(sizeof(JellyfishAnimation) <= 32u);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_jellyfish_is_registered);
    RUN_TEST(test_jellyfish_ui_name_is_meduza);
    RUN_TEST(test_jellyfish_bell_is_large);
    RUN_TEST(test_jellyfish_bell_is_always_drawn);
    RUN_TEST(test_jellyfish_body_never_moves_more_than_one_pixel);
    RUN_TEST(test_jellyfish_tentacles_move_but_never_snap);
    RUN_TEST(test_jellyfish_tentacles_reach_towards_the_bottom);
    RUN_TEST(test_jellyfish_particles_are_deterministic_and_clear_of_the_bell);
    RUN_TEST(test_jellyfish_render_is_reproducible);
    RUN_TEST(test_jellyfish_pulse_never_touches_global_brightness);
    RUN_TEST(test_jellyfish_loop_is_seamless);
    RUN_TEST(test_jellyfish_stays_inside_the_panel);
    RUN_TEST(test_jellyfish_frame_is_inside_the_power_budget);
    RUN_TEST(test_jellyfish_is_non_blocking_and_bounded);
    return UNITY_END();
}
