#include <unity.h>

#define private public
#include "animations/FallingStarsAnimation.h"
#undef private

#include "core/AnimationId.h"
#include "core/AnimationRegistry.h"
#include "core/PixelMatrix.h"

void test_falling_stars_id_and_registry(void) {
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(AnimationId::SUNSET_WATER) + 1u,
        static_cast<uint8_t>(AnimationId::FALLING_STARS)
    );
    TEST_ASSERT_NOT_NULL(AnimationRegistry::getAnimation(AnimationId::FALLING_STARS));
}

void test_falling_stars_constants_and_loop_progression(void) {
    FallingStarsAnimation animation;
    PixelMatrix matrix;

    TEST_ASSERT_EQUAL_UINT16(90u, FallingStarsAnimation::kFrameIntervalMs);
    TEST_ASSERT_EQUAL_UINT8(64u, FallingStarsAnimation::kLoopFrameCount);

    animation.start(1000u);
    TEST_ASSERT_EQUAL_UINT8(0u, animation.phase_);
    TEST_ASSERT_EQUAL_UINT8(0u, animation.renderedFrameCount_);
    TEST_ASSERT_FALSE(animation.finished_);

    for (uint8_t step = 0u; step < 64u; ++step) {
        const uint32_t now = 1000u + static_cast<uint32_t>(step) * 90u;
        animation.update(now, matrix);

        const uint8_t expectedFrame = static_cast<uint8_t>((step + 1u) % 64u);
        TEST_ASSERT_EQUAL_UINT8(expectedFrame, animation.phase_);

        if (step < 63u) {
            TEST_ASSERT_FALSE(animation.finished_);
        }
    }

    TEST_ASSERT_EQUAL_UINT8(64u, animation.renderedFrameCount_);
    TEST_ASSERT_TRUE(animation.finished_);
}

void test_falling_stars_move_diagonally_with_tail(void) {
    FallingStarsAnimation animation;
    PixelMatrix matrix;

    animation.renderFrame(matrix, 0u);

    const uint8_t headX = 15u;
    const uint8_t headY = 1u;
    const uint32_t headPacked = matrix.pixels.getPixelColor(PixelMatrix::xyToIndexStatic(headX, headY));
    TEST_ASSERT_TRUE(headPacked != 0u);

    animation.renderFrame(matrix, 6u);
    const uint32_t laterHeadPacked = matrix.pixels.getPixelColor(PixelMatrix::xyToIndexStatic(9u, 7u));
    const uint32_t laterTrailPacked = matrix.pixels.getPixelColor(PixelMatrix::xyToIndexStatic(10u, 6u));
    TEST_ASSERT_TRUE(laterHeadPacked != 0u);
    TEST_ASSERT_TRUE(laterTrailPacked != 0u);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_falling_stars_id_and_registry);
    RUN_TEST(test_falling_stars_constants_and_loop_progression);
    RUN_TEST(test_falling_stars_move_diagonally_with_tail);
    return UNITY_END();
}
