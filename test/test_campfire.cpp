#include <unity.h>

#define private public
#include "animations/CampfireAnimation.h"
#undef private

#include "core/AnimationId.h"
#include "core/AnimationRegistry.h"
#include "core/PixelMatrix.h"

void test_animation_id_and_registry(void) {
    TEST_ASSERT_TRUE(static_cast<uint8_t>(AnimationId::CAMPFIRE) > static_cast<uint8_t>(AnimationId::NONE));
    TEST_ASSERT_NOT_NULL(AnimationRegistry::getAnimation(AnimationId::CAMPFIRE));
}

void test_campfire_constants_and_start_reset(void) {
    CampfireAnimation animation;

    TEST_ASSERT_EQUAL_UINT8(4u, CampfireAnimation::kFrameCount);
    TEST_ASSERT_EQUAL_UINT16(140u, CampfireAnimation::kFrameDurationMs);
    TEST_ASSERT_EQUAL_UINT32(12000u, CampfireAnimation::kAnimationDurationMs);

    PixelMatrix matrix;
    animation.start(1000u);
    TEST_ASSERT_FALSE(animation.isFinished());

    animation.update(1000u + CampfireAnimation::kFrameDurationMs, matrix);
    TEST_ASSERT_FALSE(animation.isFinished());

    animation.stop();
    TEST_ASSERT_TRUE(animation.isFinished());
}

void test_campfire_advances_one_frame_only(void) {
    CampfireAnimation animation;
    PixelMatrix matrix;

    animation.start(0u);
    animation.update(140u, matrix);
    animation.update(280u, matrix);
    animation.update(420u, matrix);

    TEST_ASSERT_FALSE(animation.isFinished());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_animation_id_and_registry);
    RUN_TEST(test_campfire_constants_and_start_reset);
    RUN_TEST(test_campfire_advances_one_frame_only);
    return UNITY_END();
}
