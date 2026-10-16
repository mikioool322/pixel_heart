#include <unity.h>

#define private public
#include "animations/BlueSkyCloudsAnimation.h"
#undef private

#include "config/LedPowerBudget.h"
#include "control/AnimationCatalog.h"
#include "core/AnimationId.h"
#include "core/AnimationNames.h"
#include "core/AnimationRegistry.h"
#include "core/PixelMatrix.h"

// BLUE_SKY_CLOUDS - a bright daytime sky with three slowly drifting clouds.
//
// What this file protects:
//   - the whole scene is derived from ONE 32-step cycle, so t=0 and
//     t=loopDurationMs are the same frame and a cloud can never drift.
//   - each cloud stays in its own vertical lane.
//   - a cloud never moves more than one pixel per step, and it is always fully
//     off the panel when its phase wraps.
//   - the sky stays blue and nothing is pure white.

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

bool isBlue(const Color& c) {
    return c.b > c.r && c.b > c.g;
}

bool isCloudy(const Color& c) {
    // Cloud pixels are pale and desaturated compared with the sky.
    return c.g > 100u && c.r > 90u && !isBlue(c);
}

void renderPhase(BlueSkyCloudsAnimation& animation, PixelMatrix& matrix, uint8_t phase) {
    animation.renderFrame(matrix, phase);
}

// The cloud's mask width, measured from its own definition.
uint8_t cloudWidthOf(uint8_t index) {
    uint8_t width = 0u;
    for (uint8_t localY = 0u; localY < BlueSkyCloudsAnimation::cloudHeight(index); ++localY) {
        for (uint8_t localX = 0u; localX < kPanel; ++localX) {
            if (BlueSkyCloudsAnimation::cloudContains(index, localX, localY) &&
                static_cast<uint8_t>(localX + 1u) > width) {
                width = static_cast<uint8_t>(localX + 1u);
            }
        }
    }
    return width;
}

// True when any part of a cloud would be visible at this x for the given width.
bool cloudVisible(int16_t x, uint8_t width) {
    return (x + static_cast<int16_t>(width)) > 0 && x < static_cast<int16_t>(kPanel);
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

void test_blue_sky_clouds_is_registered(void) {
    TEST_ASSERT_NOT_NULL(AnimationRegistry::getAnimation(AnimationId::BLUE_SKY_CLOUDS));
    TEST_ASSERT_TRUE(AnimationRegistry::isRenderable(AnimationId::BLUE_SKY_CLOUDS));
    TEST_ASSERT_TRUE(AnimationCatalog::isUserSelectable(AnimationId::BLUE_SKY_CLOUDS));
    TEST_ASSERT_EQUAL_UINT8(27u, static_cast<uint8_t>(AnimationId::BLUE_SKY_CLOUDS));
}

void test_blue_sky_clouds_ui_name_is_polish(void) {
    TEST_ASSERT_EQUAL_STRING("Błękitne niebo", animationNameOf(AnimationId::BLUE_SKY_CLOUDS));
}

void test_blue_sky_clouds_background_is_blue(void) {
    BlueSkyCloudsAnimation animation;
    PixelMatrix matrix;

    for (uint8_t phase = 0u; phase < BlueSkyCloudsAnimation::kPhaseCount; ++phase) {
        renderPhase(animation, matrix, phase);

        for (uint8_t y = 0u; y < kPanel; ++y) {
            for (uint8_t x = 0u; x < kPanel; ++x) {
                const Color c = pixelAt(matrix, x, y);

                // Every pixel is either sky (blue) or cloud (pale), never black.
                TEST_ASSERT_TRUE(isBlue(c) || isCloudy(c));
                TEST_ASSERT_FALSE(c.r == 0u && c.g == 0u && c.b == 0u);

                // No pure white.
                TEST_ASSERT_FALSE(c.r == 255u && c.g == 255u && c.b == 255u);
            }
        }
    }

    // A corner the clouds never reach stays sky blue.
    renderPhase(animation, matrix, 0u);
    TEST_ASSERT_TRUE(isBlue(pixelAt(matrix, 15u, 15u)));
}

void test_blue_sky_clouds_has_exactly_three_clouds(void) {
    TEST_ASSERT_EQUAL_UINT8(3u, BlueSkyCloudsAnimation::kCloudCount);

    // Each definition is real: a non-empty mask inside its own lane.
    for (uint8_t i = 0u; i < BlueSkyCloudsAnimation::kCloudCount; ++i) {
        const uint8_t height = BlueSkyCloudsAnimation::cloudHeight(i);
        const uint8_t baseY = BlueSkyCloudsAnimation::cloudBaseY(i);

        TEST_ASSERT_TRUE(height >= 3u);
        TEST_ASSERT_TRUE(baseY + height <= kPanel);

        uint8_t pixels = 0u;
        for (uint8_t localY = 0u; localY < height; ++localY) {
            for (uint8_t localX = 0u; localX < kPanel; ++localX) {
                if (BlueSkyCloudsAnimation::cloudContains(i, localX, localY)) {
                    ++pixels;
                }
            }
        }
        TEST_ASSERT_TRUE(pixels >= 8u);
    }
}

void test_blue_sky_clouds_lanes_do_not_collide(void) {
    // Bounding boxes must be vertically disjoint, so two clouds never merge
    // into one blob on the diffuser.
    for (uint8_t i = 0u; i < BlueSkyCloudsAnimation::kCloudCount; ++i) {
        const uint8_t topI = BlueSkyCloudsAnimation::cloudBaseY(i);
        const uint8_t bottomI = static_cast<uint8_t>(topI + BlueSkyCloudsAnimation::cloudHeight(i) - 1u);

        for (uint8_t j = static_cast<uint8_t>(i + 1u); j < BlueSkyCloudsAnimation::kCloudCount; ++j) {
            const uint8_t topJ = BlueSkyCloudsAnimation::cloudBaseY(j);
            const uint8_t bottomJ = static_cast<uint8_t>(topJ + BlueSkyCloudsAnimation::cloudHeight(j) - 1u);

            TEST_ASSERT_TRUE(bottomI < topJ || bottomJ < topI);
        }
    }
}

void test_blue_sky_clouds_move_in_mixed_directions(void) {
    uint8_t right = 0u;
    uint8_t left = 0u;

    for (uint8_t i = 0u; i < BlueSkyCloudsAnimation::kCloudCount; ++i) {
        if (BlueSkyCloudsAnimation::cloudMovesRight(i)) {
            ++right;
        } else {
            ++left;
        }
    }

    TEST_ASSERT_TRUE(right >= 1u);
    TEST_ASSERT_TRUE(left >= 1u);
}

void test_blue_sky_clouds_positions_change_slowly_and_never_teleport(void) {
    for (uint8_t i = 0u; i < BlueSkyCloudsAnimation::kCloudCount; ++i) {
        const uint8_t width = cloudWidthOf(i);
        const uint8_t offset = BlueSkyCloudsAnimation::cloudPhaseOffset(i);
        TEST_ASSERT_TRUE(width > 0u);

        // Walk the cloud's OWN local phase, so its single wrap is unambiguous
        // even when the cloud carries a phase offset.
        const auto xAtLocal = [&](uint8_t local) -> int16_t {
            const uint8_t global = static_cast<uint8_t>(
                (local + BlueSkyCloudsAnimation::kPhaseCount - offset) %
                BlueSkyCloudsAnimation::kPhaseCount);
            return BlueSkyCloudsAnimation::cloudXAt(i, global);
        };

        int16_t previous = xAtLocal(0u);

        // Every step of the crossing moves by at most one pixel.
        for (uint8_t local = 1u; local < BlueSkyCloudsAnimation::kPhaseCount; ++local) {
            const int16_t here = xAtLocal(local);
            const int16_t delta = static_cast<int16_t>(here - previous);
            TEST_ASSERT_TRUE(delta >= -1 && delta <= 1);
            previous = here;
        }

        // ...and wherever the cloud can actually be seen, the move is that
        // small too: the only big jump is the wrap, which happens off panel.
        for (uint8_t local = 0u; local < BlueSkyCloudsAnimation::kPhaseCount; ++local) {
            const int16_t here = xAtLocal(local);
            const int16_t before = xAtLocal(static_cast<uint8_t>(
                (local + BlueSkyCloudsAnimation::kPhaseCount - 1u) % BlueSkyCloudsAnimation::kPhaseCount));

            if (cloudVisible(before, width) || cloudVisible(here, width)) {
                const int16_t delta = static_cast<int16_t>(here - before);
                TEST_ASSERT_TRUE(delta >= -1 && delta <= 1);
            }
        }

        // Both ends of the wrap are fully outside the panel.
        TEST_ASSERT_FALSE(cloudVisible(xAtLocal(static_cast<uint8_t>(BlueSkyCloudsAnimation::kPhaseCount - 1u)), width));
        TEST_ASSERT_FALSE(cloudVisible(xAtLocal(0u), width));
    }
}

void test_blue_sky_clouds_clipping_is_safe_at_the_edges(void) {
    BlueSkyCloudsAnimation animation;
    PixelMatrix matrix;

    // Drive every phase, including the ones where a cloud hangs half off the
    // panel on either side.
    for (uint8_t phase = 0u; phase < BlueSkyCloudsAnimation::kPhaseCount; ++phase) {
        renderPhase(animation, matrix, phase);

        uint16_t lit = 0u;
        for (uint8_t y = 0u; y < kPanel; ++y) {
            for (uint8_t x = 0u; x < kPanel; ++x) {
                const Color c = pixelAt(matrix, x, y);
                if (c.r != 0u || c.g != 0u || c.b != 0u) {
                    ++lit;
                }
            }
        }
        TEST_ASSERT_TRUE(lit > 0u);
        TEST_ASSERT_TRUE(lit <= PixelMatrix::LED_COUNT);
    }
}

void test_blue_sky_clouds_offsets_are_deterministic_and_distinct(void) {
    const uint8_t a = BlueSkyCloudsAnimation::cloudPhaseOffset(0u);
    const uint8_t b = BlueSkyCloudsAnimation::cloudPhaseOffset(1u);
    const uint8_t c = BlueSkyCloudsAnimation::cloudPhaseOffset(2u);

    TEST_ASSERT_TRUE(a != b);
    TEST_ASSERT_TRUE(b != c);
    TEST_ASSERT_TRUE(a != c);

    // Same query, same answer, always.
    for (uint8_t i = 0u; i < BlueSkyCloudsAnimation::kCloudCount; ++i) {
        TEST_ASSERT_EQUAL_UINT8(
            BlueSkyCloudsAnimation::cloudPhaseOffset(i),
            BlueSkyCloudsAnimation::cloudPhaseOffset(i));
        for (uint8_t phase = 0u; phase < BlueSkyCloudsAnimation::kPhaseCount; ++phase) {
            TEST_ASSERT_EQUAL_INT16(
                BlueSkyCloudsAnimation::cloudXAt(i, phase),
                BlueSkyCloudsAnimation::cloudXAt(i, phase));
        }
    }
}

void test_blue_sky_clouds_loop_is_seamless(void) {
    // One shared cycle: t=0 and t=loopDuration are the same phase.
    TEST_ASSERT_EQUAL_UINT32(
        BlueSkyCloudsAnimation::kLoopDurationMs,
        static_cast<uint32_t>(BlueSkyCloudsAnimation::kPhaseCount) * BlueSkyCloudsAnimation::kStepMs);
    TEST_ASSERT_EQUAL_UINT8(0u, BlueSkyCloudsAnimation::phaseAtElapsed(0u));
    TEST_ASSERT_EQUAL_UINT8(0u, BlueSkyCloudsAnimation::phaseAtElapsed(BlueSkyCloudsAnimation::kLoopDurationMs));

    BlueSkyCloudsAnimation animation;
    PixelMatrix atZero;
    PixelMatrix atLoop;
    renderPhase(animation, atZero, BlueSkyCloudsAnimation::phaseAtElapsed(0u));
    renderPhase(animation, atLoop, BlueSkyCloudsAnimation::phaseAtElapsed(BlueSkyCloudsAnimation::kLoopDurationMs));

    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            TEST_ASSERT_TRUE(sameColor(pixelAt(atZero, x, y), pixelAt(atLoop, x, y)));
        }
    }

    // The last step must lead into the first with no cloud jump WHILE VISIBLE.
    // A cloud whose own wrap lands on the global boundary may legitimately sit
    // at opposite off-screen edges on either side of it.
    const uint8_t last = static_cast<uint8_t>(BlueSkyCloudsAnimation::kPhaseCount - 1u);
    for (uint8_t i = 0u; i < BlueSkyCloudsAnimation::kCloudCount; ++i) {
        const uint8_t width = cloudWidthOf(i);
        const int16_t before = BlueSkyCloudsAnimation::cloudXAt(i, last);
        const int16_t after = BlueSkyCloudsAnimation::cloudXAt(i, 0u);
        const int16_t delta = static_cast<int16_t>(after - before);

        if (cloudVisible(before, width) || cloudVisible(after, width)) {
            TEST_ASSERT_TRUE(delta >= -1 && delta <= 1);
        }
    }
}

void test_blue_sky_clouds_is_reproducible(void) {
    // No randomness anywhere: two independent instances agree on every frame.
    BlueSkyCloudsAnimation a;
    BlueSkyCloudsAnimation b;
    PixelMatrix first;
    PixelMatrix second;

    for (uint8_t phase = 0u; phase < BlueSkyCloudsAnimation::kPhaseCount; ++phase) {
        renderPhase(a, first, phase);
        renderPhase(b, second, phase);

        for (uint8_t y = 0u; y < kPanel; ++y) {
            for (uint8_t x = 0u; x < kPanel; ++x) {
                TEST_ASSERT_TRUE(sameColor(pixelAt(first, x, y), pixelAt(second, x, y)));
            }
        }
    }
}

void test_blue_sky_clouds_is_non_blocking_and_bounded(void) {
    BlueSkyCloudsAnimation animation;
    PixelMatrix matrix;

    animation.start(0u);
    for (uint16_t i = 1u; i <= 40u; ++i) {
        animation.update(static_cast<uint32_t>(i) * BlueSkyCloudsAnimation::kStepMs, matrix);
    }

    TEST_ASSERT_TRUE(animation.isFinished());
    TEST_ASSERT_TRUE(sizeof(BlueSkyCloudsAnimation) <= 32u);
}

void test_blue_sky_clouds_frame_is_inside_the_power_budget(void) {
    BlueSkyCloudsAnimation animation;
    PixelMatrix matrix;
    matrix.setBrightness(255u);

    uint32_t worst = 0u;
    for (uint8_t phase = 0u; phase < BlueSkyCloudsAnimation::kPhaseCount; ++phase) {
        renderPhase(animation, matrix, phase);
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
    RUN_TEST(test_blue_sky_clouds_is_registered);
    RUN_TEST(test_blue_sky_clouds_ui_name_is_polish);
    RUN_TEST(test_blue_sky_clouds_background_is_blue);
    RUN_TEST(test_blue_sky_clouds_has_exactly_three_clouds);
    RUN_TEST(test_blue_sky_clouds_lanes_do_not_collide);
    RUN_TEST(test_blue_sky_clouds_move_in_mixed_directions);
    RUN_TEST(test_blue_sky_clouds_positions_change_slowly_and_never_teleport);
    RUN_TEST(test_blue_sky_clouds_clipping_is_safe_at_the_edges);
    RUN_TEST(test_blue_sky_clouds_offsets_are_deterministic_and_distinct);
    RUN_TEST(test_blue_sky_clouds_loop_is_seamless);
    RUN_TEST(test_blue_sky_clouds_is_reproducible);
    RUN_TEST(test_blue_sky_clouds_is_non_blocking_and_bounded);
    RUN_TEST(test_blue_sky_clouds_frame_is_inside_the_power_budget);
    return UNITY_END();
}
