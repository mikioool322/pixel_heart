#include <unity.h>

#include "assets/HeartAssets.h"

void test_heart_mask_contains_core_points(void) {
    TEST_ASSERT_TRUE(HeartAssets::contains(7, 7));
    TEST_ASSERT_TRUE(HeartAssets::contains(8, 7));
    TEST_ASSERT_TRUE(HeartAssets::contains(7, 8));
    TEST_ASSERT_TRUE(HeartAssets::contains(5, 9));
    TEST_ASSERT_FALSE(HeartAssets::contains(0, 0));
    TEST_ASSERT_FALSE(HeartAssets::contains(15, 15));
}

void test_heart_mask_outline_points(void) {
    TEST_ASSERT_TRUE(HeartAssets::isOutlinePixel(0, 1));
    TEST_ASSERT_TRUE(HeartAssets::isOutlinePixel(15, 1));
    TEST_ASSERT_FALSE(HeartAssets::isOutlinePixel(7, 7));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_heart_mask_contains_core_points);
    RUN_TEST(test_heart_mask_outline_points);
    return UNITY_END();
}
