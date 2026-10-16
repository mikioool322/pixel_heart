#include <unity.h>

#include "core/PixelMatrix.h"

int g_neo_show_count = 0;

void setUp(void) {
    g_neo_show_count = 0;
}

void test_matrix_starts_clean_and_tracks_dirty_frames(void) {
    PixelMatrix matrix;

    TEST_ASSERT_FALSE(matrix.isDirty());

    matrix.clear();
    TEST_ASSERT_TRUE(matrix.isDirty());

    matrix.show();
    TEST_ASSERT_FALSE(matrix.isDirty());

    matrix.setPixel(0, 0, Colors::Red);
    TEST_ASSERT_TRUE(matrix.isDirty());

    matrix.showIfDirty();
    TEST_ASSERT_FALSE(matrix.isDirty());
    TEST_ASSERT_EQUAL_UINT(1, static_cast<uint32_t>(g_neo_show_count));

    matrix.showIfDirty();
    TEST_ASSERT_FALSE(matrix.isDirty());
    TEST_ASSERT_EQUAL_UINT(1, static_cast<uint32_t>(g_neo_show_count));

    matrix.fill(Colors::Blue);
    TEST_ASSERT_TRUE(matrix.isDirty());

    matrix.setBrightness(10);
    TEST_ASSERT_TRUE(matrix.isDirty());

    matrix.show();
    TEST_ASSERT_FALSE(matrix.isDirty());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_matrix_starts_clean_and_tracks_dirty_frames);
    return UNITY_END();
}
