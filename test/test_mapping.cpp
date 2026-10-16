#include <unity.h>

#include "core/PixelMatrix.h"

void test_serpentine_mapping(void) {
    TEST_ASSERT_EQUAL_UINT16(0, PixelMatrix::xyToIndexStatic(0, 0));
    TEST_ASSERT_EQUAL_UINT16(15, PixelMatrix::xyToIndexStatic(15, 0));
    TEST_ASSERT_EQUAL_UINT16(31, PixelMatrix::xyToIndexStatic(0, 1));
    TEST_ASSERT_EQUAL_UINT16(16, PixelMatrix::xyToIndexStatic(15, 1));
    TEST_ASSERT_EQUAL_UINT16(32, PixelMatrix::xyToIndexStatic(0, 2));
    TEST_ASSERT_EQUAL_UINT16(47, PixelMatrix::xyToIndexStatic(15, 2));
    TEST_ASSERT_EQUAL_UINT16(255, PixelMatrix::xyToIndexStatic(15, 15));
    TEST_ASSERT_EQUAL_UINT16(0, PixelMatrix::xyToIndexStatic(16, 0));
    TEST_ASSERT_EQUAL_UINT16(0, PixelMatrix::xyToIndexStatic(0, 16));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_serpentine_mapping);
    return UNITY_END();
}
