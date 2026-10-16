#include <unity.h>

#define private public
#include "animations/SunsetWaterAnimation.h"
#undef private

#include "core/AnimationId.h"
#include "core/AnimationRegistry.h"
#include "core/PixelMatrix.h"

namespace {

Color pixelColorAt(const PixelMatrix& matrix, uint8_t x, uint8_t y) {
    const uint32_t packed = matrix.pixels.getPixelColor(matrix.xyToIndex(x, y));
    return Color{
        static_cast<uint8_t>((packed >> 16) & 0xFFu),
        static_cast<uint8_t>((packed >> 8) & 0xFFu),
        static_cast<uint8_t>(packed & 0xFFu)
    };
}

uint8_t brightness(const Color& color) {
    return static_cast<uint8_t>(
        static_cast<uint16_t>(color.r)
        + static_cast<uint16_t>(color.g)
        + static_cast<uint16_t>(color.b)
    );
}

// The real library scales a channel at write time, so "is this row visible" has
// to be judged on the transmitted value, not on the raw colour.
uint32_t transmittedChannel(uint8_t channel, uint8_t brightnessRaw) {
    return (static_cast<uint32_t>(channel) * (static_cast<uint32_t>(brightnessRaw) + 1u)) >> 8;
}

}  // namespace

void test_sunset_water_id_and_registry(void) {
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(AnimationId::MOON_STARS) + 1u,
        static_cast<uint8_t>(AnimationId::SUNSET_WATER)
    );
    TEST_ASSERT_NOT_NULL(AnimationRegistry::getAnimation(AnimationId::SUNSET_WATER));
}

void test_sunset_water_phase_progression_and_loop_boundary(void) {
    SunsetWaterAnimation animation;
    PixelMatrix matrix;

    animation.start(1000u);
    TEST_ASSERT_EQUAL_UINT8(0u, animation.phase_);
    TEST_ASSERT_EQUAL_UINT8(0u, animation.renderedFrameCount_);
    TEST_ASSERT_FALSE(animation.finished_);

    for (uint8_t step = 0u; step < 64u; ++step) {
        const uint32_t now = 1000u + static_cast<uint32_t>(step) * 160u;
        animation.update(now, matrix);

        const uint8_t expectedPhase =
            static_cast<uint8_t>((step + 1u) % 32u);

        TEST_ASSERT_EQUAL_UINT8(expectedPhase, animation.phase_);

        if (step < 63u) {
            TEST_ASSERT_FALSE(animation.finished_);
        }
    }

    TEST_ASSERT_EQUAL_UINT8(0u, animation.phase_);
    TEST_ASSERT_EQUAL_UINT8(64u, animation.renderedFrameCount_);
    TEST_ASSERT_TRUE(animation.finished_);
}

void test_sunset_water_shimmer_table_seam_is_smooth(void) {
    const uint8_t brightness0 = SunsetWaterAnimation::shimmerValue(0u);
    const uint8_t brightness31 = SunsetWaterAnimation::shimmerValue(31u);

    TEST_ASSERT_EQUAL_UINT8(108u, brightness0);
    TEST_ASSERT_EQUAL_UINT8(109u, brightness31);
    TEST_ASSERT_EQUAL_UINT8(1u, static_cast<uint8_t>(brightness0 - brightness31));
}

void test_sunset_water_sun_footprint_and_sky_contrast(void) {
    SunsetWaterAnimation animation;
    PixelMatrix matrix;

    animation.start(1000u);
    animation.update(1160u, matrix);

    const Color sun4_7 = pixelColorAt(matrix, 7u, 4u);
    const Color sun4_8 = pixelColorAt(matrix, 8u, 4u);
    const Color sun5_5 = pixelColorAt(matrix, 5u, 5u);
    const Color sun6_6 = pixelColorAt(matrix, 6u, 6u);
    const Color sun6_11 = pixelColorAt(matrix, 11u, 6u);
    const Color sun7_7 = pixelColorAt(matrix, 7u, 7u);
    const Color sun7_11 = pixelColorAt(matrix, 11u, 7u);

    TEST_ASSERT_EQUAL_UINT8(255u, sun4_7.r);
    TEST_ASSERT_EQUAL_UINT8(210u, sun4_7.g);
    TEST_ASSERT_EQUAL_UINT8(70u, sun4_7.b);
    TEST_ASSERT_EQUAL_UINT8(255u, sun4_8.r);
    TEST_ASSERT_EQUAL_UINT8(210u, sun4_8.g);
    TEST_ASSERT_EQUAL_UINT8(70u, sun4_8.b);

    TEST_ASSERT_EQUAL_UINT8(235u, sun5_5.r);
    TEST_ASSERT_EQUAL_UINT8(120u, sun5_5.g);
    TEST_ASSERT_EQUAL_UINT8(38u, sun5_5.b);
    TEST_ASSERT_EQUAL_UINT8(255u, sun6_6.r);
    TEST_ASSERT_EQUAL_UINT8(175u, sun6_6.g);
    TEST_ASSERT_EQUAL_UINT8(55u, sun6_6.b);
    TEST_ASSERT_EQUAL_UINT8(205u, sun6_11.r);
    TEST_ASSERT_EQUAL_UINT8(88u, sun6_11.g);
    TEST_ASSERT_EQUAL_UINT8(32u, sun6_11.b);

    TEST_ASSERT_EQUAL_UINT8(255u, sun7_7.r);
    TEST_ASSERT_EQUAL_UINT8(175u, sun7_7.g);
    TEST_ASSERT_EQUAL_UINT8(55u, sun7_7.b);
    TEST_ASSERT_EQUAL_UINT8(205u, sun7_11.r);
    TEST_ASSERT_EQUAL_UINT8(88u, sun7_11.g);
    TEST_ASSERT_EQUAL_UINT8(32u, sun7_11.b);

    const Color skyTop = pixelColorAt(matrix, 0u, 0u);
    const Color skyRow4 = pixelColorAt(matrix, 0u, 4u);
    const Color skyRow5 = pixelColorAt(matrix, 0u, 5u);
    const Color skyHorizon = pixelColorAt(matrix, 0u, 7u);
    const Color waterTop = pixelColorAt(matrix, 0u, 8u);

    TEST_ASSERT_TRUE(brightness(skyTop) < brightness(skyRow4));
    TEST_ASSERT_TRUE(brightness(skyRow4) < brightness(skyRow5));
    TEST_ASSERT_TRUE(brightness(skyRow5) < brightness(skyHorizon));
    TEST_ASSERT_TRUE(brightness(waterTop) < brightness(skyHorizon));

    const Color sunCore = pixelColorAt(matrix, 7u, 6u);
    const Color horizonCenter = pixelColorAt(matrix, 7u, 7u);
    TEST_ASSERT_TRUE(brightness(sunCore) > brightness(horizonCenter));

    const Color brightReflect = pixelColorAt(matrix, 7u, 8u);
    const Color softReflect = pixelColorAt(matrix, 6u, 8u);
    TEST_ASSERT_TRUE(brightReflect.r > 200u);
    TEST_ASSERT_TRUE(brightReflect.g > brightReflect.b);
    TEST_ASSERT_TRUE(softReflect.b < 150u);
    TEST_ASSERT_FALSE(brightReflect.r == 255u && brightReflect.g == 255u && brightReflect.b == 255u);

    for (uint8_t y = 8u; y < PixelMatrix::HEIGHT; ++y) {
        for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
            const Color c = pixelColorAt(matrix, x, y);
            TEST_ASSERT_FALSE(c.r == 255u && c.g == 255u && c.b == 255u);
            TEST_ASSERT_FALSE(c.r > 220u && c.g > 220u && c.b > 220u);
        }
    }

    TEST_ASSERT_TRUE(animation.phase_ == 1u || animation.phase_ == 0u);
}

void test_sunset_water_top_row_is_visible(void) {
    SunsetWaterAnimation animation;
    PixelMatrix matrix;
    matrix.setBrightness(255u);   // request the maximum; the limiter clamps it

    animation.start(1000u);
    animation.update(1160u, matrix);

    const uint8_t raw = matrix.effectiveBrightnessRaw();
    TEST_ASSERT_TRUE(raw > 0u);

    // Every pixel of the first row must actually light up. Before the fix the
    // whole row transmitted at 2/0/3 and read as an unlit band above the view.
    for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
        const Color c = pixelColorAt(matrix, x, 0u);
        const uint32_t transmitted =
            transmittedChannel(c.r, raw) +
            transmittedChannel(c.g, raw) +
            transmittedChannel(c.b, raw);

        TEST_ASSERT_TRUE(transmitted >= 8u);
    }

    // It must still be the darkest row, so the dusk gradient is preserved.
    // (A local 16-bit sum: the shared helper returns uint8_t and overflows on
    // the brighter rows.)
    const auto sumOf = [](const Color& c) -> uint16_t {
        return static_cast<uint16_t>(
            static_cast<uint16_t>(c.r) + static_cast<uint16_t>(c.g) + static_cast<uint16_t>(c.b));
    };

    const Color row0 = pixelColorAt(matrix, 0u, 0u);
    const Color row4 = pixelColorAt(matrix, 0u, 4u);
    const Color row7 = pixelColorAt(matrix, 0u, 7u);
    TEST_ASSERT_TRUE(sumOf(row0) < sumOf(row4));
    TEST_ASSERT_TRUE(sumOf(row4) < sumOf(row7));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_sunset_water_id_and_registry);
    RUN_TEST(test_sunset_water_phase_progression_and_loop_boundary);
    RUN_TEST(test_sunset_water_shimmer_table_seam_is_smooth);
    RUN_TEST(test_sunset_water_sun_footprint_and_sky_contrast);
    RUN_TEST(test_sunset_water_top_row_is_visible);
    return UNITY_END();
}
