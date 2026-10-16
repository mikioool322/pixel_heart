#include <stdio.h>
#include <string.h>

#include <unity.h>

#define private public
#include "animations/SunOverMountainsAnimation.h"
#undef private

#include "core/AnimationId.h"
#include "core/AnimationRegistry.h"
#include "core/PixelMatrix.h"

void test_sun_over_mountains_registry_and_constants(void) {
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(AnimationId::FALLING_STARS) + 1u,
        static_cast<uint8_t>(AnimationId::SUN_OVER_MOUNTAINS)
    );
    TEST_ASSERT_NOT_NULL(AnimationRegistry::getAnimation(AnimationId::SUN_OVER_MOUNTAINS));

    TEST_ASSERT_EQUAL_UINT16(120u, SunOverMountainsAnimation::kFrameIntervalMs);
    TEST_ASSERT_EQUAL_UINT8(64u, SunOverMountainsAnimation::kLoopFrameCount);
    TEST_ASSERT_EQUAL_UINT8(32u, SunOverMountainsAnimation::kPhaseCount);
}

void test_sun_over_mountains_seamless_loop_progression(void) {
    SunOverMountainsAnimation animation;
    PixelMatrix matrix;

    animation.start(1000u);
    TEST_ASSERT_EQUAL_UINT8(0u, animation.phase_);
    TEST_ASSERT_EQUAL_UINT8(0u, animation.renderedFrameCount_);
    TEST_ASSERT_FALSE(animation.finished_);

    for (uint8_t step = 0u; step < 64u; ++step) {
        const uint32_t now = 1000u + static_cast<uint32_t>(step) * 120u;
        animation.update(now, matrix);

        const uint8_t expectedPhase = static_cast<uint8_t>((step + 1u) % 32u);
        TEST_ASSERT_EQUAL_UINT8(expectedPhase, animation.phase_);

        if (step < 63u) {
            TEST_ASSERT_FALSE(animation.finished_);
        }
    }

    TEST_ASSERT_EQUAL_UINT8(0u, animation.phase_);
    TEST_ASSERT_EQUAL_UINT8(64u, animation.renderedFrameCount_);
    TEST_ASSERT_TRUE(animation.finished_);
}

void test_sun_over_mountains_manual_scene_bounds_and_palette(void) {
    SunOverMountainsAnimation animation;
    PixelMatrix matrix;

    animation.start(1000u);
    animation.update(1120u, matrix);

    const uint8_t sunPixels[] = {
        7u, 1u, 8u, 1u, 6u, 2u, 7u, 2u, 8u, 2u, 9u, 2u,
        5u, 3u, 6u, 3u, 7u, 3u, 8u, 3u, 9u, 3u, 10u, 3u,
        5u, 4u, 6u, 4u, 7u, 4u, 8u, 4u, 9u, 4u, 10u, 4u,
        6u, 5u, 7u, 5u, 8u, 5u, 9u, 5u, 7u, 6u, 8u, 6u
    };

    for (uint8_t i = 0u; i < (sizeof(sunPixels) / sizeof(sunPixels[0])); i += 2u) {
        const uint8_t x = sunPixels[i];
        const uint8_t y = sunPixels[i + 1u];
        TEST_ASSERT_TRUE(x < PixelMatrix::WIDTH);
        TEST_ASSERT_TRUE(y < PixelMatrix::HEIGHT);

        const uint32_t packed = matrix.pixels.getPixelColor(PixelMatrix::xyToIndexStatic(x, y));
        TEST_ASSERT_TRUE(packed != 0u);
    }

    for (uint8_t x = 5u; x <= 10u; ++x) {
        for (uint8_t y = 3u; y <= 4u; ++y) {
            const uint32_t packed = matrix.pixels.getPixelColor(PixelMatrix::xyToIndexStatic(x, y));
            const uint8_t r = static_cast<uint8_t>((packed >> 16) & 0xFFu);
            const uint8_t g = static_cast<uint8_t>((packed >> 8) & 0xFFu);
            const uint8_t b = static_cast<uint8_t>(packed & 0xFFu);
            TEST_ASSERT_TRUE(r > 180u);
            TEST_ASSERT_TRUE(g > 180u);
            TEST_ASSERT_TRUE(b < 200u);
        }
    }

    const uint32_t skyPacked = matrix.pixels.getPixelColor(PixelMatrix::xyToIndexStatic(0u, 0u));
    const uint8_t skyR = static_cast<uint8_t>((skyPacked >> 16) & 0xFFu);
    const uint8_t skyG = static_cast<uint8_t>((skyPacked >> 8) & 0xFFu);
    const uint8_t skyB = static_cast<uint8_t>(skyPacked & 0xFFu);

    TEST_ASSERT_TRUE(skyB > skyG);
    TEST_ASSERT_TRUE(skyG > skyR);
}

void test_sun_over_mountains_mountain_shape_and_height(void) {
    SunOverMountainsAnimation animation;
    PixelMatrix matrix;

    animation.renderFrame(matrix, 0u);

    const uint8_t leftPeakX = 3u;
    const uint8_t centerPeakX = 8u;
    const uint8_t rightPeakX = 13u;

    uint8_t leftHeight = 0u;
    uint8_t centerHeight = 0u;
    uint8_t rightHeight = 0u;

    for (uint8_t y = 0u; y < PixelMatrix::HEIGHT; ++y) {
        if (matrix.pixels.getPixelColor(PixelMatrix::xyToIndexStatic(leftPeakX, y)) != 0u) {
            leftHeight = y;
        }
        if (matrix.pixels.getPixelColor(PixelMatrix::xyToIndexStatic(centerPeakX, y)) != 0u) {
            centerHeight = y;
        }
        if (matrix.pixels.getPixelColor(PixelMatrix::xyToIndexStatic(rightPeakX, y)) != 0u) {
            rightHeight = y;
        }
    }

    TEST_ASSERT_TRUE(leftHeight >= 10u);
    TEST_ASSERT_TRUE(centerHeight >= 9u);
    TEST_ASSERT_TRUE(rightHeight >= 10u);
    TEST_ASSERT_TRUE(centerHeight > leftHeight);
    TEST_ASSERT_TRUE(centerHeight > rightHeight);

    uint8_t mountainCount = 0u;
    for (uint8_t y = 9u; y < PixelMatrix::HEIGHT; ++y) {
        for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
            const uint32_t packed = matrix.pixels.getPixelColor(PixelMatrix::xyToIndexStatic(x, y));
            if (packed != 0u) {
                const uint8_t r = static_cast<uint8_t>((packed >> 16) & 0xFFu);
                const uint8_t g = static_cast<uint8_t>((packed >> 8) & 0xFFu);
                const uint8_t b = static_cast<uint8_t>(packed & 0xFFu);
                if (r <= 80u && g <= 100u && b <= 120u) {
                    ++mountainCount;
                }
            }
        }
    }

    TEST_ASSERT_TRUE(mountainCount > 30u);
    TEST_ASSERT_TRUE(mountainCount < 200u);
}

void test_sun_over_mountains_has_no_forbidden_runtime_calls(void) {
    FILE* file = fopen("src/animations/SunOverMountainsAnimation.cpp", "rb");
    TEST_ASSERT_NOT_NULL(file);

    char buffer[8192];
    const size_t bytesRead = fread(buffer, 1u, sizeof(buffer) - 1u, file);
    fclose(file);
    buffer[bytesRead] = '\0';

    TEST_ASSERT_NULL(strstr(buffer, "delay("));
    TEST_ASSERT_NULL(strstr(buffer, "random("));
    TEST_ASSERT_NULL(strstr(buffer, "rand("));
    TEST_ASSERT_NULL(strstr(buffer, "sin("));
    TEST_ASSERT_NULL(strstr(buffer, "cos("));
    TEST_ASSERT_NULL(strstr(buffer, "new "));
    TEST_ASSERT_NULL(strstr(buffer, "delete"));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_sun_over_mountains_registry_and_constants);
    RUN_TEST(test_sun_over_mountains_seamless_loop_progression);
    RUN_TEST(test_sun_over_mountains_manual_scene_bounds_and_palette);
    RUN_TEST(test_sun_over_mountains_mountain_shape_and_height);
    RUN_TEST(test_sun_over_mountains_has_no_forbidden_runtime_calls);
    return UNITY_END();
}
