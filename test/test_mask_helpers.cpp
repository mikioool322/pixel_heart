#include <unity.h>

#include "assets/PixelMask.h"
#include "core/Color.h"

void test_mask_has_pixel_and_bounds(void) {
    PixelMask16 mask{};
    mask.rows[0] = static_cast<PixelMaskRow>(1u) << 15u;
    mask.rows[2] = static_cast<PixelMaskRow>(1u) << 8u;

    TEST_ASSERT_TRUE(maskHasPixel(mask, 0, 0));
    TEST_ASSERT_FALSE(maskHasPixel(mask, 1, 0));
    TEST_ASSERT_TRUE(maskHasPixel(mask, 7, 2));
    TEST_ASSERT_FALSE(maskHasPixel(mask, 8, 2));
    TEST_ASSERT_FALSE(maskHasPixel(mask, 0, 16));
}

void test_color_scaling_preserves_channel_order(void) {
    const Color result = scaleColor(Colors::RosePink, 128u);

    TEST_ASSERT_EQUAL_UINT8(128u, result.r);
    TEST_ASSERT_EQUAL_UINT8(60u, result.g);
    TEST_ASSERT_EQUAL_UINT8(85u, result.b);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_mask_has_pixel_and_bounds);
    RUN_TEST(test_color_scaling_preserves_channel_order);
    return UNITY_END();
}
