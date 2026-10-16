#include <unity.h>

#define private public
#include "animations/SlotMachineAnimation.h"
#undef private

#include "core/AnimationId.h"
#include "core/AnimationRegistry.h"
#include "core/PixelMatrix.h"

void test_slot_machine_id_and_registry(void) {
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(AnimationId::SLOT_MACHINE),
        static_cast<uint8_t>(AnimationId::DANCING_DUCK) + 1u
    );
    TEST_ASSERT_NOT_NULL(AnimationRegistry::getAnimation(AnimationId::SLOT_MACHINE));
}

void test_slot_machine_start_and_stop(void) {
    SlotMachineAnimation animation;
    PixelMatrix matrix;

    animation.start(1000u);
    TEST_ASSERT_FALSE(animation.isFinished());

    animation.update(1000u + 200u, matrix);
    TEST_ASSERT_FALSE(animation.isFinished());

    animation.stop();
    TEST_ASSERT_TRUE(animation.isFinished());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_slot_machine_id_and_registry);
    RUN_TEST(test_slot_machine_start_and_stop);
    return UNITY_END();
}
