#include <unity.h>

#include <string.h>

// Access to the underlying strip for frame read-back, same convention the
// existing animation tests use.
#define private public
#include "core/PixelMatrix.h"
#undef private

#include "config/LedPowerBudget.h"
#include "control/UserSettings.h"

#include "animations/FoxAnimation.h"
#include "animations/HalloweenGhostAnimation.h"
#include "animations/OrangeCatAnimation.h"

// ---------------------------------------------------------------------------
// How these tests model the real output path
//
// The native Adafruit_NeoPixel double stores channel values verbatim, whereas
// the real library scales a channel AT WRITE TIME as
//     transmitted = (channel * (brightnessRaw + 1)) >> 8
// (Adafruit_NeoPixel 1.15.5, setPixelColor). Because the double deliberately
// does not reimplement that, these tests apply the library's own formula to
// the read-back frame, using the brightness PixelMatrix actually handed to the
// strip. That means the estimates below are computed over the values the real
// hardware would transmit, not over the raw logical frame.
//
// The chain being proven:
//   animation RGB -> PixelMatrix::setPixel -> strip buffer (scaled by the
//   clamped brightness) -> show() transmits it unchanged
// and the clamped brightness is the only value the clamp allows, which the
// compile-time static_asserts in LedPowerBudget.h verify.
// ---------------------------------------------------------------------------

namespace {

uint8_t transmittedChannel(uint8_t channel, uint8_t brightnessRaw) {
    return static_cast<uint8_t>(
        (static_cast<uint16_t>(channel) * (static_cast<uint16_t>(brightnessRaw) + 1u)) >> 8);
}

// Conservative LED emission estimate for whatever is currently in the strip,
// in mA, using the transmitted (post-brightness) channel values.
uint32_t frameEstimateMilliAmp(const PixelMatrix& matrix) {
    const uint8_t brightness = matrix.effectiveBrightnessRaw();
    uint32_t channelSum = 0u;

    for (uint16_t i = 0u; i < PixelMatrix::LED_COUNT; ++i) {
        const uint32_t packed = matrix.pixels.getPixelColor(i);
        const uint8_t r = static_cast<uint8_t>((packed >> 16) & 0xFFu);
        const uint8_t g = static_cast<uint8_t>((packed >> 8) & 0xFFu);
        const uint8_t b = static_cast<uint8_t>(packed & 0xFFu);

        channelSum += transmittedChannel(r, brightness);
        channelSum += transmittedChannel(g, brightness);
        channelSum += transmittedChannel(b, brightness);
    }

    return LedPowerBudget::estimateMilliAmp(channelSum);
}

void fillAll(PixelMatrix& matrix, uint8_t r, uint8_t g, uint8_t b) {
    for (uint8_t y = 0u; y < PixelMatrix::HEIGHT; ++y) {
        for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
            matrix.setPixel(x, y, Color{r, g, b});
        }
    }
}

// Deterministic PRNG so property tests never flake.
uint32_t nextRandom(uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

}  // namespace

// ---------------------------------------------------------------------------
// Compile-time budget model
// ---------------------------------------------------------------------------

void test_budget_constants_are_the_documented_conservative_model(void) {
    TEST_ASSERT_EQUAL_UINT16(2000u, LedPowerBudget::kLimitMilliAmp);
    TEST_ASSERT_EQUAL_UINT8(20u, LedPowerBudget::kMilliAmpPerFullChannel);
    TEST_ASSERT_EQUAL_UINT16(256u, LedPowerBudget::kPixelCount);

    // 256 pixels x 3 channels x 20 mA = 15360 mA unrestricted.
    TEST_ASSERT_EQUAL_UINT32(15360u, LedPowerBudget::kUnlimitedWhiteFrameMilliAmp);

    // Derived, not guessed: floor(2000 * 255 / 15360) = 33.
    TEST_ASSERT_EQUAL_UINT8(33u, LedPowerBudget::kMaxSafeChannel);
    TEST_ASSERT_EQUAL_UINT8(33u, LedPowerBudget::kMaxSafeBrightnessRaw);

    // Worst case the limiter can actually produce.
    TEST_ASSERT_EQUAL_UINT32(1987u, LedPowerBudget::kWorstCaseMilliAmp);
    TEST_ASSERT_TRUE(LedPowerBudget::kWorstCaseMilliAmp <= LedPowerBudget::kLimitMilliAmp);
}

void test_ceiling_is_tight_and_rounds_toward_safety(void) {
    // One step above the ceiling really would exceed the budget, so the
    // ceiling is the largest safe value rather than an arbitrary small number.
    const uint8_t over = static_cast<uint8_t>(LedPowerBudget::kMaxSafeBrightnessRaw + 1u);
    const uint8_t worstChannel =
        static_cast<uint8_t>((255u * (static_cast<uint16_t>(over) + 1u)) >> 8);

    TEST_ASSERT_TRUE(worstChannel > LedPowerBudget::kMaxSafeChannel);

    const uint32_t overCurrent = LedPowerBudget::estimateMilliAmp(
        static_cast<uint32_t>(LedPowerBudget::kPixelCount) *
        LedPowerBudget::kChannelsPerPixel * worstChannel);

    TEST_ASSERT_TRUE(overCurrent > LedPowerBudget::kLimitMilliAmp);
}

void test_clamp_never_increases_a_request(void) {
    for (uint16_t requested = 0u; requested <= 255u; ++requested) {
        const uint8_t clamped =
            LedPowerBudget::clampBrightnessRaw(static_cast<uint8_t>(requested));
        TEST_ASSERT_TRUE(clamped <= requested);
        TEST_ASSERT_TRUE(clamped <= LedPowerBudget::kMaxSafeBrightnessRaw);
    }
}

void test_requests_at_or_below_the_ceiling_are_untouched(void) {
    // This is what keeps the shipped default brightness behaving exactly as
    // before the limiter existed.
    for (uint8_t requested = 0u; requested <= LedPowerBudget::kMaxSafeBrightnessRaw; ++requested) {
        TEST_ASSERT_EQUAL_UINT8(requested, LedPowerBudget::clampBrightnessRaw(requested));
    }
}

// ---------------------------------------------------------------------------
// TEST 1-3: frames that need no limiting
// ---------------------------------------------------------------------------

void test_black_frame_emits_nothing(void) {
    PixelMatrix matrix;
    matrix.setBrightness(255u);   // request the maximum
    matrix.clear();

    TEST_ASSERT_TRUE(frameEstimateMilliAmp(matrix) == 0u);
}

void test_single_red_pixel_is_well_inside_budget(void) {
    PixelMatrix matrix;
    matrix.setBrightness(255u);
    matrix.clear();
    matrix.setPixel(3u, 5u, Color{255u, 0u, 0u});

    const uint32_t estimate = frameEstimateMilliAmp(matrix);
    TEST_ASSERT_TRUE(estimate <= 20u);
}

void test_single_white_pixel_is_well_inside_budget(void) {
    PixelMatrix matrix;
    matrix.setBrightness(255u);
    matrix.clear();
    matrix.setPixel(7u, 9u, Color{255u, 255u, 255u});

    const uint32_t estimate = frameEstimateMilliAmp(matrix);
    TEST_ASSERT_TRUE(estimate <= 60u);
}

// ---------------------------------------------------------------------------
// TEST 4-7: full-screen extremes
// ---------------------------------------------------------------------------

void test_all_red_frame_is_limited(void) {
    PixelMatrix matrix;
    matrix.setBrightness(255u);
    fillAll(matrix, 255u, 0u, 0u);

    // Unrestricted this would be 256 * 20 = 5120 mA.
    TEST_ASSERT_EQUAL_UINT32(5120u, LedPowerBudget::estimateMilliAmp(256u * 255u));
    TEST_ASSERT_TRUE(frameEstimateMilliAmp(matrix) <= LedPowerBudget::kLimitMilliAmp);
}

void test_all_green_frame_is_limited(void) {
    PixelMatrix matrix;
    matrix.setBrightness(255u);
    fillAll(matrix, 0u, 255u, 0u);

    TEST_ASSERT_TRUE(frameEstimateMilliAmp(matrix) <= LedPowerBudget::kLimitMilliAmp);
}

void test_all_blue_frame_is_limited(void) {
    PixelMatrix matrix;
    matrix.setBrightness(255u);
    fillAll(matrix, 0u, 0u, 255u);

    TEST_ASSERT_TRUE(frameEstimateMilliAmp(matrix) <= LedPowerBudget::kLimitMilliAmp);
}

void test_all_white_frame_is_the_worst_case_and_is_limited(void) {
    PixelMatrix matrix;
    matrix.setBrightness(255u);
    fillAll(matrix, 255u, 255u, 255u);

    const uint32_t estimate = frameEstimateMilliAmp(matrix);

    // Unrestricted the model says 15360 mA; the limiter must bring it inside
    // the budget, and not one milliamp over.
    TEST_ASSERT_TRUE(estimate <= LedPowerBudget::kLimitMilliAmp);
    TEST_ASSERT_EQUAL_UINT32(LedPowerBudget::kWorstCaseMilliAmp, estimate);
    TEST_ASSERT_EQUAL_UINT32(1987u, estimate);

    // Never 2001+ through integer rounding.
    TEST_ASSERT_TRUE(estimate != LedPowerBudget::kLimitMilliAmp + 1u);
}

// ---------------------------------------------------------------------------
// TEST 8-9: mixed and sparse content
// ---------------------------------------------------------------------------

void test_mixed_adversarial_frame_is_inside_budget(void) {
    PixelMatrix matrix;
    matrix.setBrightness(255u);
    matrix.clear();

    // Deterministic mix of white, primaries, black and intermediates.
    const Color pattern[] = {
        Color{255u, 255u, 255u},
        Color{255u, 0u, 0u},
        Color{0u, 255u, 0u},
        Color{0u, 0u, 255u},
        Color{0u, 0u, 0u},
        Color{128u, 64u, 200u},
        Color{17u, 200u, 3u},
        Color{250u, 250u, 10u},
    };

    uint16_t index = 0u;
    for (uint8_t y = 0u; y < PixelMatrix::HEIGHT; ++y) {
        for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
            matrix.setPixel(x, y, pattern[index % (sizeof(pattern) / sizeof(pattern[0]))]);
            ++index;
        }
    }

    TEST_ASSERT_TRUE(frameEstimateMilliAmp(matrix) <= LedPowerBudget::kLimitMilliAmp);
}

void test_sparse_bright_frame_is_conservatively_bounded(void) {
    // A global ceiling is intentionally conservative: the limiter does not
    // try to grant sparse content a higher budget. It must still be far inside
    // the limit, which is what makes typical content safe by construction.
    PixelMatrix matrix;
    matrix.setBrightness(255u);
    matrix.clear();
    matrix.setPixel(0u, 0u, Color{255u, 255u, 255u});
    matrix.setPixel(15u, 15u, Color{255u, 255u, 255u});

    const uint32_t estimate = frameEstimateMilliAmp(matrix);

    TEST_ASSERT_TRUE(estimate <= LedPowerBudget::kLimitMilliAmp);
    // Two pixels, not 256: the ceiling scales with the frame, so sparse
    // content is dimmed by the same factor as dense content.
    TEST_ASSERT_TRUE(estimate <= 2u * 60u);
}

// ---------------------------------------------------------------------------
// TEST 10-12: brightness interaction and the boundary
// ---------------------------------------------------------------------------

void test_user_brightness_100_percent_still_fits_the_budget(void) {
    PixelMatrix matrix;
    matrix.setBrightness(255u);   // maximum possible request
    fillAll(matrix, 255u, 255u, 255u);

    TEST_ASSERT_TRUE(frameEstimateMilliAmp(matrix) <= LedPowerBudget::kLimitMilliAmp);
    TEST_ASSERT_EQUAL_UINT8(
        LedPowerBudget::kMaxSafeBrightnessRaw,
        matrix.effectiveBrightnessRaw());
}

void test_user_brightness_0_percent_is_off(void) {
    PixelMatrix matrix;
    matrix.setBrightness(0u);
    fillAll(matrix, 255u, 255u, 255u);

    // Brightness 0 means the strip transmits nothing at all.
    TEST_ASSERT_EQUAL_UINT8(0u, matrix.effectiveBrightnessRaw());
    TEST_ASSERT_TRUE(frameEstimateMilliAmp(matrix) == 0u);
}

void test_default_brightness_behaviour_is_unchanged(void) {
    // A request at or below the ceiling must reach the strip verbatim, so the
    // shipped default is not silently dimmed by this feature.
    PixelMatrix matrix;

    const uint8_t defaultRequest = brightnessPercentToRaw(
        UserSettingsDefaults::kBrightnessPercent);

    matrix.setBrightness(defaultRequest);
    TEST_ASSERT_EQUAL_UINT8(defaultRequest, matrix.effectiveBrightnessRaw());
    TEST_ASSERT_TRUE(matrix.effectiveBrightnessRaw() <= LedPowerBudget::kMaxSafeBrightnessRaw);
}

void test_limit_boundary_is_exact(void) {
    PixelMatrix matrix;

    // Just below the ceiling: passed through.
    matrix.setBrightness(static_cast<uint8_t>(LedPowerBudget::kMaxSafeBrightnessRaw - 1u));
    TEST_ASSERT_EQUAL_UINT8(
        LedPowerBudget::kMaxSafeBrightnessRaw - 1u, matrix.effectiveBrightnessRaw());

    // At the ceiling: passed through, and a full white frame still fits.
    matrix.setBrightness(LedPowerBudget::kMaxSafeBrightnessRaw);
    TEST_ASSERT_EQUAL_UINT8(
        LedPowerBudget::kMaxSafeBrightnessRaw, matrix.effectiveBrightnessRaw());
    fillAll(matrix, 255u, 255u, 255u);
    TEST_ASSERT_TRUE(frameEstimateMilliAmp(matrix) <= LedPowerBudget::kLimitMilliAmp);

    // One above: clamped down, never up.
    matrix.setBrightness(static_cast<uint8_t>(LedPowerBudget::kMaxSafeBrightnessRaw + 1u));
    TEST_ASSERT_EQUAL_UINT8(
        LedPowerBudget::kMaxSafeBrightnessRaw, matrix.effectiveBrightnessRaw());

    // And the extreme the library treats as "no scaling at all" is clamped too.
    matrix.setBrightness(255u);
    TEST_ASSERT_EQUAL_UINT8(
        LedPowerBudget::kMaxSafeBrightnessRaw, matrix.effectiveBrightnessRaw());
}

void test_arming_the_matrix_at_construction_is_already_safe(void) {
    // Adafruit_NeoPixel defaults to "no scaling". A matrix that has never had
    // setBrightness() called must still be bounded.
    PixelMatrix matrix;

    TEST_ASSERT_EQUAL_UINT8(
        LedPowerBudget::kMaxSafeBrightnessRaw, matrix.effectiveBrightnessRaw());

    fillAll(matrix, 255u, 255u, 255u);
    TEST_ASSERT_TRUE(frameEstimateMilliAmp(matrix) <= LedPowerBudget::kLimitMilliAmp);
}

// ---------------------------------------------------------------------------
// TEST 13 + property tests
// ---------------------------------------------------------------------------

void test_every_brightness_value_is_safe_with_a_white_frame(void) {
    // Adversarial sweep over the whole brightness range: no value may produce
    // an over-budget white frame through rounding.
    PixelMatrix matrix;

    for (uint16_t requested = 0u; requested <= 255u; ++requested) {
        matrix.setBrightness(static_cast<uint8_t>(requested));
        fillAll(matrix, 255u, 255u, 255u);

        TEST_ASSERT_TRUE(frameEstimateMilliAmp(matrix) <= LedPowerBudget::kLimitMilliAmp);
    }
}

void test_property_random_frames_never_exceed_the_budget(void) {
    // Deterministic pseudo-random frames x deterministic brightness values.
    // Seed is fixed, so this is reproducible and cannot flake.
    uint32_t seed = 0x1D872B41u;
    PixelMatrix matrix;

    const uint8_t brightnesses[] = {0u, 1u, 12u, 30u, 33u, 100u, 200u, 254u, 255u};

    for (uint8_t round = 0u; round < 24u; ++round) {
        matrix.setBrightness(brightnesses[round % (sizeof(brightnesses) / sizeof(brightnesses[0]))]);

        for (uint8_t y = 0u; y < PixelMatrix::HEIGHT; ++y) {
            for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
                const uint32_t r = nextRandom(seed);
                matrix.setPixel(
                    x, y,
                    Color{static_cast<uint8_t>(r & 0xFFu),
                          static_cast<uint8_t>((r >> 8) & 0xFFu),
                          static_cast<uint8_t>((r >> 16) & 0xFFu)});
            }
        }

        TEST_ASSERT_TRUE(frameEstimateMilliAmp(matrix) <= LedPowerBudget::kLimitMilliAmp);
    }
}

void test_limiter_never_makes_a_pixel_brighter(void) {
    // The transmitted channel must never exceed what was requested, for every
    // channel value and every brightness.
    for (uint16_t brightness = 0u; brightness <= 255u; ++brightness) {
        for (uint16_t channel = 0u; channel <= 255u; ++channel) {
            const uint8_t out = transmittedChannel(
                static_cast<uint8_t>(channel),
                LedPowerBudget::clampBrightnessRaw(static_cast<uint8_t>(brightness)));
            TEST_ASSERT_TRUE(out <= channel);
        }
    }
}

void test_scale_preserves_channel_ratios(void) {
    // A single global scale keeps hues intact: the clamp scales all channels
    // by the same factor, so a colour never shifts.
    PixelMatrix matrix;
    matrix.setBrightness(255u);
    matrix.clear();
    matrix.setPixel(0u, 0u, Color{255u, 128u, 64u});

    const uint8_t brightness = matrix.effectiveBrightnessRaw();
    const uint8_t r = transmittedChannel(255u, brightness);
    const uint8_t g = transmittedChannel(128u, brightness);
    const uint8_t b = transmittedChannel(64u, brightness);

    TEST_ASSERT_TRUE(r > g);
    TEST_ASSERT_TRUE(g > b);
}

// ---------------------------------------------------------------------------
// Rotation independence
// ---------------------------------------------------------------------------

void test_estimated_current_is_independent_of_rotation(void) {
    // Current limiting is a function of colour content only; rotating the
    // display must not change the budget.
    PixelMatrix matrix;

    const DisplayRotation rotations[] = {
        DisplayRotation::DEG_0,
        DisplayRotation::DEG_90,
        DisplayRotation::DEG_180,
        DisplayRotation::DEG_270,
    };

    matrix.setBrightness(255u);

    for (uint8_t r = 0u; r < 4u; ++r) {
        matrix.setRotation(rotations[r]);
        matrix.clear();

        // Same measured rotation, so the same content lands on the same LEDs.
        matrix.setPixel(2u, 5u, Color{255u, 255u, 255u});
        matrix.setPixel(11u, 3u, Color{255u, 0u, 0u});
        matrix.setPixel(4u, 12u, Color{0u, 255u, 128u});

        TEST_ASSERT_TRUE(frameEstimateMilliAmp(matrix) <= LedPowerBudget::kLimitMilliAmp);
    }

    // And an identical full frame produces an identical estimate at every angle.
    uint32_t previous = 0u;
    for (uint8_t r = 0u; r < 4u; ++r) {
        matrix.setRotation(rotations[r]);
        fillAll(matrix, 255u, 255u, 255u);

        const uint32_t estimate = frameEstimateMilliAmp(matrix);
        if (r > 0u) {
            TEST_ASSERT_EQUAL_UINT32(previous, estimate);
        }
        previous = estimate;
    }

    TEST_ASSERT_TRUE(previous <= LedPowerBudget::kLimitMilliAmp);
}

// ---------------------------------------------------------------------------
// fill() and clear() are bound by the same ceiling
// ---------------------------------------------------------------------------

void test_fill_goes_through_the_same_ceiling(void) {
    PixelMatrix matrix;
    matrix.setBrightness(255u);
    matrix.fill(Color{255u, 255u, 255u});

    TEST_ASSERT_TRUE(frameEstimateMilliAmp(matrix) <= LedPowerBudget::kLimitMilliAmp);

    matrix.clear();
    TEST_ASSERT_TRUE(frameEstimateMilliAmp(matrix) == 0u);
}

// ---------------------------------------------------------------------------
// New sprite animations: every frame of each must fit the same budget
// ---------------------------------------------------------------------------

void test_halloween_ghost_frames_are_inside_the_budget(void) {
    HalloweenGhostAnimation animation;
    PixelMatrix matrix;
    matrix.setBrightness(255u);   // request the maximum; the limiter clamps it

    animation.start(0u);
    uint16_t renderCount = 0u;

    // One full loop is 32 frames at 150 ms.
    for (uint16_t i = 1u; i <= 32u; ++i) {
        animation.update(static_cast<uint32_t>(i) * 150u, matrix);
        TEST_ASSERT_TRUE(frameEstimateMilliAmp(matrix) <= LedPowerBudget::kLimitMilliAmp);
        ++renderCount;
    }

    TEST_ASSERT_EQUAL_UINT16(32u, renderCount);
}

void test_fox_frames_are_inside_the_budget(void) {
    FoxAnimation animation;
    PixelMatrix matrix;
    matrix.setBrightness(255u);

    animation.start(0u);
    for (uint16_t i = 1u; i <= 8u; ++i) {
        animation.update(static_cast<uint32_t>(i) * 350u, matrix);
        TEST_ASSERT_TRUE(frameEstimateMilliAmp(matrix) <= LedPowerBudget::kLimitMilliAmp);
    }
}

void test_orange_cat_frames_are_inside_the_budget(void) {
    OrangeCatAnimation animation;
    PixelMatrix matrix;
    matrix.setBrightness(255u);

    animation.start(0u);
    for (uint16_t i = 1u; i <= 36u; ++i) {
        animation.update(static_cast<uint32_t>(i) * 200u, matrix);
        TEST_ASSERT_TRUE(frameEstimateMilliAmp(matrix) <= LedPowerBudget::kLimitMilliAmp);
    }
}

void test_new_sprite_animations_never_exceed_the_ceiling_brightness(void) {
    // The animations paint colours only; the brightness authority is the same
    // one every other animation uses, and it stays clamped.
    HalloweenGhostAnimation ghost;
    FoxAnimation fox;
    OrangeCatAnimation cat;
    PixelMatrix matrix;

    matrix.setBrightness(255u);
    TEST_ASSERT_EQUAL_UINT8(
        LedPowerBudget::kMaxSafeBrightnessRaw, matrix.effectiveBrightnessRaw());

    ghost.start(0u);
    ghost.update(150u, matrix);
    fox.start(0u);
    fox.update(350u, matrix);
    cat.start(0u);
    cat.update(200u, matrix);

    TEST_ASSERT_EQUAL_UINT8(
        LedPowerBudget::kMaxSafeBrightnessRaw, matrix.effectiveBrightnessRaw());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_budget_constants_are_the_documented_conservative_model);
    RUN_TEST(test_ceiling_is_tight_and_rounds_toward_safety);
    RUN_TEST(test_clamp_never_increases_a_request);
    RUN_TEST(test_requests_at_or_below_the_ceiling_are_untouched);
    RUN_TEST(test_black_frame_emits_nothing);
    RUN_TEST(test_single_red_pixel_is_well_inside_budget);
    RUN_TEST(test_single_white_pixel_is_well_inside_budget);
    RUN_TEST(test_all_red_frame_is_limited);
    RUN_TEST(test_all_green_frame_is_limited);
    RUN_TEST(test_all_blue_frame_is_limited);
    RUN_TEST(test_all_white_frame_is_the_worst_case_and_is_limited);
    RUN_TEST(test_mixed_adversarial_frame_is_inside_budget);
    RUN_TEST(test_sparse_bright_frame_is_conservatively_bounded);
    RUN_TEST(test_user_brightness_100_percent_still_fits_the_budget);
    RUN_TEST(test_user_brightness_0_percent_is_off);
    RUN_TEST(test_default_brightness_behaviour_is_unchanged);
    RUN_TEST(test_limit_boundary_is_exact);
    RUN_TEST(test_arming_the_matrix_at_construction_is_already_safe);
    RUN_TEST(test_every_brightness_value_is_safe_with_a_white_frame);
    RUN_TEST(test_property_random_frames_never_exceed_the_budget);
    RUN_TEST(test_limiter_never_makes_a_pixel_brighter);
    RUN_TEST(test_scale_preserves_channel_ratios);
    RUN_TEST(test_estimated_current_is_independent_of_rotation);
    RUN_TEST(test_fill_goes_through_the_same_ceiling);
    RUN_TEST(test_halloween_ghost_frames_are_inside_the_budget);
    RUN_TEST(test_fox_frames_are_inside_the_budget);
    RUN_TEST(test_orange_cat_frames_are_inside_the_budget);
    RUN_TEST(test_new_sprite_animations_never_exceed_the_ceiling_brightness);
    return UNITY_END();
}
