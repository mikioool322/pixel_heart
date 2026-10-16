#include <unity.h>

#define private public
#include "animations/DancingDuckAnimation.h"
#undef private

#include "core/AnimationId.h"
#include "core/AnimationRegistry.h"
#include "core/PixelMatrix.h"

void test_dancing_duck_id_and_registry(void) {
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(AnimationId::DANCING_DUCK),
        static_cast<uint8_t>(AnimationId::CAMPFIRE) + 1u
    );
    TEST_ASSERT_NOT_NULL(AnimationRegistry::getAnimation(AnimationId::DANCING_DUCK));
}

void test_dancing_duck_constants_and_start_reset(void) {
    DancingDuckAnimation animation;

    TEST_ASSERT_EQUAL_UINT8(4u, DancingDuckAnimation::kFrameCount);
    TEST_ASSERT_EQUAL_UINT16(160u, DancingDuckAnimation::kFrameDurationMs);
    TEST_ASSERT_EQUAL_UINT32(12000u, DancingDuckAnimation::kAnimationDurationMs);

    animation.start(1000u);
    TEST_ASSERT_FALSE(animation.isFinished());
    TEST_ASSERT_EQUAL_UINT8(0u, animation.frameIndex_);

    PixelMatrix matrix;
    animation.update(1000u + DancingDuckAnimation::kFrameDurationMs, matrix);
    TEST_ASSERT_FALSE(animation.isFinished());
    TEST_ASSERT_EQUAL_UINT8(1u, animation.frameIndex_);

    animation.stop();
    TEST_ASSERT_TRUE(animation.isFinished());
}

void test_dancing_duck_advances_one_frame_only(void) {
    DancingDuckAnimation animation;
    PixelMatrix matrix;

    animation.start(0u);
    animation.update(160u, matrix);
    animation.update(320u, matrix);
    animation.update(480u, matrix);

    TEST_ASSERT_FALSE(animation.isFinished());
    TEST_ASSERT_EQUAL_UINT8(3u, animation.frameIndex_);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_dancing_duck_id_and_registry);
    RUN_TEST(test_dancing_duck_constants_and_start_reset);
    RUN_TEST(test_dancing_duck_advances_one_frame_only);
    return UNITY_END();
}
