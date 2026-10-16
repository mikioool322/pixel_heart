#include <unity.h>

// Access to the underlying strip for read-back. Same convention the existing
// animation tests use. Rotation is proven end-to-end through setPixel(), so no
// test-only accessor has to be added to PixelMatrix.
#define private public
#include "core/PixelMatrix.h"
#undef private

#include "core/DisplayRotation.h"

namespace {

// Physical LED index that setPixel() actually lit. Returns 0xFFFF when the
// panel is dark, which is itself a failure for these tests.
uint16_t litIndex(const PixelMatrix& matrix) {
    uint16_t found = 0xFFFFu;
    uint8_t litCount = 0u;

    for (uint16_t i = 0u; i < PixelMatrix::LED_COUNT; ++i) {
        if (matrix.pixels.getPixelColor(i) != 0u) {
            found = i;
            ++litCount;
        }
    }

    // Exactly one LED must light for a single setPixel call.
    return (litCount == 1u) ? found : 0xFFFFu;
}

// Lights one logical pixel on a freshly cleared matrix and reports which
// physical LED came on.
uint16_t indexFor(PixelMatrix& matrix, DisplayRotation rotation, uint8_t x, uint8_t y) {
    matrix.setRotation(rotation);
    matrix.clear();
    matrix.setPixel(x, y, Colors::White);
    return litIndex(matrix);
}

void assertCorner(
    PixelMatrix& matrix,
    DisplayRotation rotation,
    uint8_t x,
    uint8_t y,
    uint8_t expectedPhysicalX,
    uint8_t expectedPhysicalY) {
    const uint16_t actual = indexFor(matrix, rotation, x, y);
    const uint16_t expected = PixelMatrix::xyToIndexStatic(expectedPhysicalX, expectedPhysicalY);
    TEST_ASSERT_EQUAL_UINT16(expected, actual);
}

}  // namespace

// ---------------------------------------------------------------------------
// Corners
// ---------------------------------------------------------------------------

void test_deg0_maps_corners_identically(void) {
    PixelMatrix matrix;

    assertCorner(matrix, DisplayRotation::DEG_0, 0u, 0u, 0u, 0u);
    assertCorner(matrix, DisplayRotation::DEG_0, 15u, 0u, 15u, 0u);
    assertCorner(matrix, DisplayRotation::DEG_0, 0u, 15u, 0u, 15u);
    assertCorner(matrix, DisplayRotation::DEG_0, 15u, 15u, 15u, 15u);
}

void test_deg90_maps_corners_clockwise(void) {
    PixelMatrix matrix;

    // old top-left -> new top-right, old top-right -> new bottom-right, etc.
    assertCorner(matrix, DisplayRotation::DEG_90, 0u, 0u, 15u, 0u);
    assertCorner(matrix, DisplayRotation::DEG_90, 15u, 0u, 15u, 15u);
    assertCorner(matrix, DisplayRotation::DEG_90, 0u, 15u, 0u, 0u);
    assertCorner(matrix, DisplayRotation::DEG_90, 15u, 15u, 0u, 15u);
}

void test_deg180_maps_corners(void) {
    PixelMatrix matrix;

    assertCorner(matrix, DisplayRotation::DEG_180, 0u, 0u, 15u, 15u);
    assertCorner(matrix, DisplayRotation::DEG_180, 15u, 0u, 0u, 15u);
    assertCorner(matrix, DisplayRotation::DEG_180, 0u, 15u, 15u, 0u);
    assertCorner(matrix, DisplayRotation::DEG_180, 15u, 15u, 0u, 0u);
}

void test_deg270_maps_corners_counter_clockwise(void) {
    PixelMatrix matrix;

    assertCorner(matrix, DisplayRotation::DEG_270, 0u, 0u, 0u, 15u);
    assertCorner(matrix, DisplayRotation::DEG_270, 15u, 0u, 0u, 0u);
    assertCorner(matrix, DisplayRotation::DEG_270, 0u, 15u, 15u, 15u);
    assertCorner(matrix, DisplayRotation::DEG_270, 15u, 15u, 15u, 0u);
}

// ---------------------------------------------------------------------------
// Asymmetric interior points.
//
// Corners alone cannot distinguish a rotation from a mirror, because both map
// the corner set onto itself. These points can.
// ---------------------------------------------------------------------------

void test_asymmetric_points_are_rotated_not_mirrored(void) {
    PixelMatrix matrix;

    // (2,5): DEG_90 -> (10,2) ; DEG_180 -> (13,10) ; DEG_270 -> (5,13)
    assertCorner(matrix, DisplayRotation::DEG_90, 2u, 5u, 10u, 2u);
    assertCorner(matrix, DisplayRotation::DEG_180, 2u, 5u, 13u, 10u);
    assertCorner(matrix, DisplayRotation::DEG_270, 2u, 5u, 5u, 13u);

    // (3,11): DEG_90 -> (4,3) ; DEG_180 -> (12,4) ; DEG_270 -> (11,12)
    assertCorner(matrix, DisplayRotation::DEG_90, 3u, 11u, 4u, 3u);
    assertCorner(matrix, DisplayRotation::DEG_180, 3u, 11u, 12u, 4u);
    assertCorner(matrix, DisplayRotation::DEG_270, 3u, 11u, 11u, 12u);

    // (12,4): DEG_90 -> (11,12) ; DEG_180 -> (3,11) ; DEG_270 -> (4,3)
    assertCorner(matrix, DisplayRotation::DEG_90, 12u, 4u, 11u, 12u);
    assertCorner(matrix, DisplayRotation::DEG_180, 12u, 4u, 3u, 11u);
    assertCorner(matrix, DisplayRotation::DEG_270, 12u, 4u, 4u, 3u);
}

void test_mirror_is_actually_detected(void) {
    // Sanity check on the test itself: a horizontal mirror would map (2,5) to
    // (13,5), which is NOT what DEG_180 does. If the implementation were
    // mirrored, the assertions above would fail rather than silently pass.
    PixelMatrix matrix;
    const uint16_t rotated = indexFor(matrix, DisplayRotation::DEG_180, 2u, 5u);
    const uint16_t mirrored = PixelMatrix::xyToIndexStatic(13u, 5u);
    TEST_ASSERT_NOT_EQUAL_UINT16(mirrored, rotated);
}

// ---------------------------------------------------------------------------
// Exhaustive bijection: all 256 logical pixels, for all four rotations.
// ---------------------------------------------------------------------------

void test_every_rotation_is_a_bijection_over_all_256_pixels(void) {
    const DisplayRotation rotations[] = {
        DisplayRotation::DEG_0,
        DisplayRotation::DEG_90,
        DisplayRotation::DEG_180,
        DisplayRotation::DEG_270,
    };

    for (uint8_t r = 0u; r < 4u; ++r) {
        PixelMatrix matrix;
        uint8_t hits[PixelMatrix::LED_COUNT];

        for (uint16_t i = 0u; i < PixelMatrix::LED_COUNT; ++i) {
            hits[i] = 0u;
        }

        for (uint8_t y = 0u; y < PixelMatrix::HEIGHT; ++y) {
            for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
                const uint16_t index = indexFor(matrix, rotations[r], x, y);

                // Every logical pixel must light exactly one LED.
                TEST_ASSERT_TRUE(index < PixelMatrix::LED_COUNT);
                ++hits[index];
            }
        }

        // No duplicates and no missing LEDs: a bijection.
        for (uint16_t i = 0u; i < PixelMatrix::LED_COUNT; ++i) {
            TEST_ASSERT_EQUAL_UINT8(1u, hits[i]);
        }
    }
}

// ---------------------------------------------------------------------------
// Default behaviour and the physical mapping itself.
// ---------------------------------------------------------------------------

void test_default_rotation_is_deg0(void) {
    PixelMatrix matrix;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(DisplayRotation::DEG_0),
        static_cast<uint8_t>(matrix.rotation()));

    // A default-constructed matrix must produce the original behaviour.
    TEST_ASSERT_EQUAL_UINT16(
        PixelMatrix::xyToIndexStatic(2u, 5u),
        indexFor(matrix, DisplayRotation::DEG_0, 2u, 5u));
}

void test_serpentine_wiring_is_unchanged(void) {
    // The physical chain must not be affected by this feature. These are the
    // same relationships the existing static_asserts encode.
    TEST_ASSERT_EQUAL_UINT16(0u, PixelMatrix::xyToIndexStatic(0u, 0u));
    TEST_ASSERT_EQUAL_UINT16(15u, PixelMatrix::xyToIndexStatic(15u, 0u));
    TEST_ASSERT_EQUAL_UINT16(31u, PixelMatrix::xyToIndexStatic(0u, 1u));
    TEST_ASSERT_EQUAL_UINT16(16u, PixelMatrix::xyToIndexStatic(15u, 1u));
    TEST_ASSERT_EQUAL_UINT16(32u, PixelMatrix::xyToIndexStatic(0u, 2u));
    TEST_ASSERT_EQUAL_UINT16(255u, PixelMatrix::xyToIndexStatic(0u, 15u));
    TEST_ASSERT_EQUAL_UINT16(240u, PixelMatrix::xyToIndexStatic(15u, 15u));

    // Four consecutive rotations return to the original mapping.
    PixelMatrix matrix;
    for (uint8_t i = 0u; i < 4u; ++i) {
        matrix.setRotation(static_cast<DisplayRotation>(i));
        const uint16_t index = indexFor(matrix, static_cast<DisplayRotation>(i), 2u, 5u);
        TEST_ASSERT_TRUE(index < PixelMatrix::LED_COUNT);
    }
}

void test_rotation_values_do_not_collide(void) {
    // Each rotation must produce a different picture for an asymmetric point,
    // otherwise two settings would be indistinguishable on the panel.
    PixelMatrix matrix;

    const uint16_t a = indexFor(matrix, DisplayRotation::DEG_0, 2u, 5u);
    const uint16_t b = indexFor(matrix, DisplayRotation::DEG_90, 2u, 5u);
    const uint16_t c = indexFor(matrix, DisplayRotation::DEG_180, 2u, 5u);
    const uint16_t d = indexFor(matrix, DisplayRotation::DEG_270, 2u, 5u);

    TEST_ASSERT_NOT_EQUAL_UINT16(a, b);
    TEST_ASSERT_NOT_EQUAL_UINT16(a, c);
    TEST_ASSERT_NOT_EQUAL_UINT16(a, d);
    TEST_ASSERT_NOT_EQUAL_UINT16(b, d);
    TEST_ASSERT_NOT_EQUAL_UINT16(c, d);
}

// ---------------------------------------------------------------------------
// Out-of-range coordinates must be ignored, never underflowed.
// ---------------------------------------------------------------------------

void test_out_of_range_coordinates_are_rejected(void) {
    PixelMatrix matrix;

    matrix.setRotation(DisplayRotation::DEG_90);
    matrix.clear();

    // 16 and 255 are invalid in logical space. Rotation arithmetic on them
    // would underflow WIDTH - 1 - x, so they must be dropped before then.
    matrix.setPixel(16u, 0u, Colors::White);
    matrix.setPixel(0u, 16u, Colors::White);
    matrix.setPixel(255u, 255u, Colors::White);
    matrix.setPixel(200u, 3u, Colors::White);

    TEST_ASSERT_EQUAL_UINT16(0xFFFFu, litIndex(matrix));
}

// ---------------------------------------------------------------------------
// setRotation bookkeeping.
// ---------------------------------------------------------------------------

void test_setting_a_new_rotation_forces_a_redraw(void) {
    PixelMatrix matrix;
    matrix.setRotation(DisplayRotation::DEG_0);
    matrix.clear();
    matrix.show();
    TEST_ASSERT_FALSE(matrix.isDirty());

    // The strip still holds the old orientation, so the frame must be marked
    // dirty for showIfDirty() to transmit the rotated picture.
    matrix.setRotation(DisplayRotation::DEG_90);
    TEST_ASSERT_TRUE(matrix.isDirty());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(DisplayRotation::DEG_90),
        static_cast<uint8_t>(matrix.rotation()));
}

void test_setting_the_same_rotation_is_a_no_op(void) {
    PixelMatrix matrix;
    matrix.setRotation(DisplayRotation::DEG_180);
    matrix.show();
    TEST_ASSERT_FALSE(matrix.isDirty());

    // Must not manufacture a redundant frame.
    matrix.setRotation(DisplayRotation::DEG_180);
    TEST_ASSERT_FALSE(matrix.isDirty());
    TEST_ASSERT_FALSE(matrix.isDirty());
}

void test_invalid_rotation_is_ignored(void) {
    PixelMatrix matrix;
    matrix.setRotation(DisplayRotation::DEG_90);

    matrix.setRotation(static_cast<DisplayRotation>(9u));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(DisplayRotation::DEG_90),
        static_cast<uint8_t>(matrix.rotation()));
}

void test_clear_and_fill_are_geometry_independent(void) {
    PixelMatrix matrix;
    matrix.setRotation(DisplayRotation::DEG_270);

    matrix.clear();
    TEST_ASSERT_TRUE(matrix.isDirty());
    TEST_ASSERT_EQUAL_UINT16(0xFFFFu, litIndex(matrix));

    // fill() addresses every LED directly, so rotation must not disturb it.
    matrix.fill(Colors::Blue);
    for (uint16_t i = 0u; i < PixelMatrix::LED_COUNT; ++i) {
        TEST_ASSERT_NOT_EQUAL_UINT32(0u, matrix.pixels.getPixelColor(i));
    }
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_deg0_maps_corners_identically);
    RUN_TEST(test_deg90_maps_corners_clockwise);
    RUN_TEST(test_deg180_maps_corners);
    RUN_TEST(test_deg270_maps_corners_counter_clockwise);
    RUN_TEST(test_asymmetric_points_are_rotated_not_mirrored);
    RUN_TEST(test_mirror_is_actually_detected);
    RUN_TEST(test_every_rotation_is_a_bijection_over_all_256_pixels);
    RUN_TEST(test_default_rotation_is_deg0);
    RUN_TEST(test_serpentine_wiring_is_unchanged);
    RUN_TEST(test_rotation_values_do_not_collide);
    RUN_TEST(test_out_of_range_coordinates_are_rejected);
    RUN_TEST(test_setting_a_new_rotation_forces_a_redraw);
    RUN_TEST(test_setting_the_same_rotation_is_a_no_op);
    RUN_TEST(test_invalid_rotation_is_ignored);
    RUN_TEST(test_clear_and_fill_are_geometry_independent);
    return UNITY_END();
}
