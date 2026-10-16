#include <unity.h>

#define private public
#include "animations/OrangeCatAnimation.h"
#undef private

#include "config/LedPowerBudget.h"
#include "control/AnimationCatalog.h"
#include "core/AnimationId.h"
#include "core/AnimationNames.h"
#include "core/AnimationRegistry.h"
#include "core/PixelMatrix.h"

namespace {

constexpr uint8_t kPanel = 16u;

// The previous (too small) cat, kept here so "significantly larger" is an
// assertion rather than a claim: 12 x 14 bounding box, 144 body pixels,
// 2 highlight pixels per lens.
constexpr uint8_t kOldCatWidth = 12u;
constexpr uint8_t kOldCatHeight = 14u;
constexpr uint16_t kOldCatPixels = 144u;
constexpr uint8_t kOldLensPixelsPerSide = 2u;

Color pixelColorAt(const PixelMatrix& matrix, uint8_t x, uint8_t y) {
    const uint32_t packed =
        matrix.pixels.getPixelColor(PixelMatrix::xyToIndexStatic(x, y));
    return Color{
        static_cast<uint8_t>((packed >> 16) & 0xFFu),
        static_cast<uint8_t>((packed >> 8) & 0xFFu),
        static_cast<uint8_t>(packed & 0xFFu)
    };
}

bool sameColor(const Color& a, const Color& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

bool isLit(const Color& c) {
    return c.r != 0u || c.g != 0u || c.b != 0u;
}

bool isBlueLens(const Color& c) {
    return c.b > c.r && c.b > c.g && c.b > 120u;
}

uint16_t countBodyPixels() {
    uint16_t count = 0u;
    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            if (OrangeCatAnimation::bodyContains(x, y)) {
                ++count;
            }
        }
    }
    return count;
}

uint16_t countLensPixels(bool rightSide) {
    uint16_t count = 0u;
    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            if (!OrangeCatAnimation::lensContains(x, y)) {
                continue;
            }
            const bool onRight = x >= 8u;
            if (onRight == rightSide) {
                ++count;
            }
        }
    }
    return count;
}

void renderPhase(PixelMatrix& matrix, uint8_t phase) {
    OrangeCatAnimation().renderFrame(matrix, phase);
}

}  // namespace

// ---------------------------------------------------------------------------
// 1-2: identity is untouched
// ---------------------------------------------------------------------------

void test_orange_cat_is_registered_and_selectable(void) {
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(AnimationId::FOX) + 1u,
        static_cast<uint8_t>(AnimationId::ORANGE_CAT));
    TEST_ASSERT_EQUAL_UINT8(21u, static_cast<uint8_t>(AnimationId::ORANGE_CAT));
    TEST_ASSERT_NOT_NULL(AnimationRegistry::getAnimation(AnimationId::ORANGE_CAT));
    TEST_ASSERT_TRUE(AnimationCatalog::isUserSelectable(AnimationId::ORANGE_CAT));
}

void test_orange_cat_label_is_polish(void) {
    TEST_ASSERT_EQUAL_STRING("Rudy kot", animationNameOf(AnimationId::ORANGE_CAT));
}

// ---------------------------------------------------------------------------
// 3-6: the face now fills the panel
// ---------------------------------------------------------------------------

void test_orange_cat_body_fills_the_panel(void) {
    // 3: significantly larger than the previous cat.
    int16_t minX = static_cast<int16_t>(kPanel);
    int16_t maxX = -1;
    int16_t minY = static_cast<int16_t>(kPanel);
    int16_t maxY = -1;
    uint8_t rows = 0u;

    for (uint8_t y = 0u; y < kPanel; ++y) {
        bool rowHasBody = false;
        for (uint8_t x = 0u; x < kPanel; ++x) {
            if (!OrangeCatAnimation::bodyContains(x, y)) {
                continue;
            }
            rowHasBody = true;
            if (static_cast<int16_t>(x) < minX) minX = static_cast<int16_t>(x);
            if (static_cast<int16_t>(x) > maxX) maxX = static_cast<int16_t>(x);
            if (static_cast<int16_t>(y) < minY) minY = static_cast<int16_t>(y);
            if (static_cast<int16_t>(y) > maxY) maxY = static_cast<int16_t>(y);
        }
        if (rowHasBody) {
            ++rows;
        }
    }

    const int16_t width = static_cast<int16_t>(maxX - minX + 1);
    const int16_t height = static_cast<int16_t>(maxY - minY + 1);

    TEST_ASSERT_EQUAL_INT16(16, width);
    TEST_ASSERT_EQUAL_INT16(16, height);
    TEST_ASSERT_TRUE(width > static_cast<int16_t>(kOldCatWidth));
    TEST_ASSERT_TRUE(height > static_cast<int16_t>(kOldCatHeight));

    // 5: at least 12 rows tall (the whole panel, in fact).
    TEST_ASSERT_TRUE(rows >= 12u);
    TEST_ASSERT_EQUAL_UINT8(16u, rows);

    // And substantially more illuminated pixels than before.
    const uint16_t pixels = countBodyPixels();
    TEST_ASSERT_TRUE(pixels > kOldCatPixels);
    TEST_ASSERT_TRUE(pixels >= 200u);
}

void test_orange_cat_body_reaches_both_side_edges(void) {
    // 4: the fur touches x=0 and x=15.
    bool touchesLeft = false;
    bool touchesRight = false;

    for (uint8_t y = 0u; y < kPanel; ++y) {
        if (OrangeCatAnimation::bodyContains(0u, y)) {
            touchesLeft = true;
        }
        if (OrangeCatAnimation::bodyContains(15u, y)) {
            touchesRight = true;
        }
    }

    TEST_ASSERT_TRUE(touchesLeft);
    TEST_ASSERT_TRUE(touchesRight);
}

void test_orange_cat_ears_reach_the_top(void) {
    // 6: two separate ears in the top three rows.
    for (uint8_t y = 0u; y <= 2u; ++y) {
        bool leftEar = false;
        bool rightEar = false;
        for (uint8_t x = 0u; x < kPanel; ++x) {
            if (!OrangeCatAnimation::bodyContains(x, y)) {
                continue;
            }
            if (x < 8u) {
                leftEar = true;
            } else {
                rightEar = true;
            }
        }
        TEST_ASSERT_TRUE(leftEar);
        TEST_ASSERT_TRUE(rightEar);
    }

    // The very top row is two distinct ear tips with a gap between them.
    TEST_ASSERT_TRUE(OrangeCatAnimation::bodyContains(2u, 0u));
    TEST_ASSERT_TRUE(OrangeCatAnimation::bodyContains(3u, 0u));
    TEST_ASSERT_TRUE(OrangeCatAnimation::bodyContains(4u, 0u));
    TEST_ASSERT_TRUE(OrangeCatAnimation::bodyContains(11u, 0u));
    TEST_ASSERT_TRUE(OrangeCatAnimation::bodyContains(12u, 0u));
    TEST_ASSERT_TRUE(OrangeCatAnimation::bodyContains(13u, 0u));
    TEST_ASSERT_FALSE(OrangeCatAnimation::bodyContains(7u, 0u));
    TEST_ASSERT_FALSE(OrangeCatAnimation::bodyContains(8u, 0u));
}

// ---------------------------------------------------------------------------
// 7-10: much larger sunglasses
// ---------------------------------------------------------------------------

void test_orange_cat_lenses_are_large_on_both_sides(void) {
    // 7: a lens exists on each side.
    TEST_ASSERT_TRUE(OrangeCatAnimation::lensContains(3u, 7u));
    TEST_ASSERT_TRUE(OrangeCatAnimation::lensContains(12u, 7u));

    // 8: at least 10 illuminated lens pixels per side, and far more than the
    // old 2-pixel highlight.
    const uint16_t left = countLensPixels(false);
    const uint16_t right = countLensPixels(true);

    TEST_ASSERT_TRUE(left >= 10u);
    TEST_ASSERT_TRUE(right >= 10u);
    TEST_ASSERT_TRUE(left > kOldLensPixelsPerSide);
    TEST_ASSERT_TRUE(right > kOldLensPixelsPerSide);
    TEST_ASSERT_EQUAL_UINT16(12u, left);
    TEST_ASSERT_EQUAL_UINT16(12u, right);

    // Each lens is 4 px wide and 3 rows tall.
    for (uint8_t y = 6u; y <= 8u; ++y) {
        uint8_t leftWidth = 0u;
        uint8_t rightWidth = 0u;
        for (uint8_t x = 0u; x < kPanel; ++x) {
            if (!OrangeCatAnimation::lensContains(x, y)) {
                continue;
            }
            if (x < 8u) {
                ++leftWidth;
            } else {
                ++rightWidth;
            }
        }
        TEST_ASSERT_EQUAL_UINT8(4u, leftWidth);
        TEST_ASSERT_EQUAL_UINT8(4u, rightWidth);
    }
}

void test_orange_cat_glasses_span_the_face_with_a_bridge(void) {
    // 10: a dark bridge sits strictly between the two lenses.
    for (uint8_t y = 6u; y <= 8u; ++y) {
        TEST_ASSERT_TRUE(OrangeCatAnimation::darkContains(7u, y));
        TEST_ASSERT_TRUE(OrangeCatAnimation::darkContains(8u, y));
        TEST_ASSERT_FALSE(OrangeCatAnimation::lensContains(7u, y));
        TEST_ASSERT_FALSE(OrangeCatAnimation::lensContains(8u, y));
    }

    // The frame wraps the whole lens band and spans most of the face width.
    uint8_t frameMinX = kPanel;
    uint8_t frameMaxX = 0u;
    for (uint8_t x = 0u; x < kPanel; ++x) {
        if (OrangeCatAnimation::darkContains(x, 6u) ||
            OrangeCatAnimation::darkContains(x, 7u) ||
            OrangeCatAnimation::darkContains(x, 8u)) {
            if (x < frameMinX) frameMinX = x;
            if (x > frameMaxX) frameMaxX = x;
        }
    }
    TEST_ASSERT_TRUE(frameMinX <= 1u);
    TEST_ASSERT_TRUE(frameMaxX >= 14u);

    // The nose/mouth structure survives below the glasses.
    TEST_ASSERT_TRUE(OrangeCatAnimation::darkContains(7u, 10u));
    TEST_ASSERT_TRUE(OrangeCatAnimation::darkContains(8u, 10u));
    TEST_ASSERT_TRUE(OrangeCatAnimation::darkContains(7u, 13u));
}

void test_orange_cat_glint_pixels_sit_inside_the_lenses(void) {
    // Every reflection must land on blue lens, never on the dark frame.
    TEST_ASSERT_EQUAL_UINT8(8u, OrangeCatAnimation::glintPixelCount());

    bool sawLeft = false;
    bool sawRight = false;

    for (uint8_t i = 0u; i < OrangeCatAnimation::glintPixelCount(); ++i) {
        const uint8_t x = OrangeCatAnimation::glintPixelX(i);
        const uint8_t y = OrangeCatAnimation::glintPixelY(i);

        TEST_ASSERT_TRUE(OrangeCatAnimation::lensContains(x, y));
        TEST_ASSERT_FALSE(OrangeCatAnimation::darkContains(x, y));
        TEST_ASSERT_TRUE(OrangeCatAnimation::bodyContains(x, y));

        if (OrangeCatAnimation::glintOnRight(i)) {
            sawRight = true;
            TEST_ASSERT_TRUE(x >= 8u);
        } else {
            sawLeft = true;
            TEST_ASSERT_TRUE(x < 8u);
        }
    }

    TEST_ASSERT_TRUE(sawLeft);
    TEST_ASSERT_TRUE(sawRight);
}

// ---------------------------------------------------------------------------
// 9, 11-15: everything except the reflections is frozen
// ---------------------------------------------------------------------------

void test_orange_cat_lit_geometry_is_identical_across_all_frames(void) {
    PixelMatrix reference;
    PixelMatrix frame;
    renderPhase(reference, 0u);

    const uint8_t frames = OrangeCatAnimation::frameCount();
    TEST_ASSERT_TRUE(frames >= OrangeCatAnimation::phaseCount());

    for (uint8_t f = 1u; f < frames; ++f) {
        renderPhase(frame, f);

        for (uint8_t y = 0u; y < kPanel; ++y) {
            for (uint8_t x = 0u; x < kPanel; ++x) {
                // 11 + 12: the lit set never changes at all.
                TEST_ASSERT_EQUAL_UINT8(
                    isLit(pixelColorAt(reference, x, y)) ? 1u : 0u,
                    isLit(pixelColorAt(frame, x, y)) ? 1u : 0u);
            }
        }
    }
}

void test_orange_cat_only_glint_pixels_change(void) {
    PixelMatrix reference;
    PixelMatrix frame;
    renderPhase(reference, 0u);

    for (uint8_t f = 1u; f < OrangeCatAnimation::frameCount(); ++f) {
        renderPhase(frame, f);

        for (uint8_t y = 0u; y < kPanel; ++y) {
            for (uint8_t x = 0u; x < kPanel; ++x) {
                if (sameColor(pixelColorAt(reference, x, y), pixelColorAt(frame, x, y))) {
                    continue;
                }
                // 13: the only pixels allowed to differ are the reflections.
                TEST_ASSERT_TRUE(OrangeCatAnimation::glintContains(x, y));
            }
        }
    }
}

void test_orange_cat_body_and_frame_never_change_brightness(void) {
    PixelMatrix reference;
    PixelMatrix frame;
    renderPhase(reference, 0u);

    for (uint8_t f = 1u; f < OrangeCatAnimation::frameCount(); ++f) {
        renderPhase(frame, f);

        for (uint8_t y = 0u; y < kPanel; ++y) {
            for (uint8_t x = 0u; x < kPanel; ++x) {
                const bool isFrame = OrangeCatAnimation::darkContains(x, y);
                const bool isLens = OrangeCatAnimation::lensContains(x, y);
                const bool isFur =
                    OrangeCatAnimation::bodyContains(x, y) && !isFrame && !isLens;

                // 9 + 14 + 15: the dark frame and the plain fur are static.
                if (isFrame || isFur) {
                    TEST_ASSERT_TRUE(sameColor(
                        pixelColorAt(reference, x, y), pixelColorAt(frame, x, y)));
                }

                // 12: lens pixels that are not reflections keep their blue.
                if (isLens && !OrangeCatAnimation::glintContains(x, y)) {
                    TEST_ASSERT_TRUE(sameColor(
                        pixelColorAt(reference, x, y), pixelColorAt(frame, x, y)));
                }
            }
        }
    }
}

void test_orange_cat_body_is_orange_and_lenses_are_blue(void) {
    PixelMatrix matrix;
    renderPhase(matrix, 0u);

    uint16_t orange = 0u;
    uint16_t blue = 0u;
    uint16_t grey = 0u;

    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            const Color c = pixelColorAt(matrix, x, y);
            if (!isLit(c)) {
                continue;
            }
            if (c.r > 180u && c.g > 60u && c.g < 150u && c.b < 90u) {
                ++orange;
            }
            if (isBlueLens(c)) {
                ++blue;
            }
            if (c.r == c.g && c.g == c.b) {
                ++grey;
            }
        }
    }

    TEST_ASSERT_TRUE(orange > 100u);
    TEST_ASSERT_TRUE(blue > 12u);
    TEST_ASSERT_EQUAL_UINT16(0u, grey);

    // The lucid blue must dominate the lenses; the frame is dark, not grey.
    TEST_ASSERT_TRUE(OrangeCatAnimation::darkContains(1u, 6u));
    const Color framePixel = pixelColorAt(matrix, 1u, 6u);
    TEST_ASSERT_TRUE(framePixel.r < 60u && framePixel.g < 60u && framePixel.b < 60u);
}

// ---------------------------------------------------------------------------
// 16-19: the shimmer loop
// ---------------------------------------------------------------------------

void test_orange_cat_glints_are_phase_offset_and_never_strobe(void) {
    const uint8_t count = OrangeCatAnimation::phaseCount();
    TEST_ASSERT_EQUAL_UINT8(6u, count);

    // 17: never fully dark, and every step is small in both directions.
    for (uint8_t phase = 0u; phase < count; ++phase) {
        const uint8_t level = OrangeCatAnimation::glintLevelAt(phase, false);
        TEST_ASSERT_TRUE(level >= 90u);
        TEST_ASSERT_TRUE(level != 0u);

        const uint8_t next = OrangeCatAnimation::glintLevelAt(
            static_cast<uint8_t>(phase + 1u), false);
        const int16_t delta = static_cast<int16_t>(next) - static_cast<int16_t>(level);
        TEST_ASSERT_TRUE(delta <= 60 && delta >= -60);
    }

    // 16: the two sides are not synchronised.
    bool offsetSeen = false;
    for (uint8_t phase = 0u; phase < count; ++phase) {
        if (OrangeCatAnimation::glintLevelAt(phase, false) !=
            OrangeCatAnimation::glintLevelAt(phase, true)) {
            offsetSeen = true;
        }
    }
    TEST_ASSERT_TRUE(offsetSeen);
}

void test_orange_cat_glint_curve_loops_seamlessly(void) {
    const uint8_t count = OrangeCatAnimation::phaseCount();

    // 18: the wrap step is as gentle as every other step, so the curve is
    // cyclic rather than a sawtooth.
    const uint8_t last = OrangeCatAnimation::glintLevelAt(
        static_cast<uint8_t>(count - 1u), false);
    const uint8_t first = OrangeCatAnimation::glintLevelAt(0u, false);
    const int16_t wrap = static_cast<int16_t>(first) - static_cast<int16_t>(last);
    TEST_ASSERT_TRUE(wrap <= 60 && wrap >= -60);

    // ...and a whole cycle later the value is identical again.
    TEST_ASSERT_EQUAL_UINT8(first, OrangeCatAnimation::glintLevelAt(count, false));
    TEST_ASSERT_EQUAL_UINT8(
        OrangeCatAnimation::glintLevelAt(0u, true),
        OrangeCatAnimation::glintLevelAt(count, true));
}

void test_orange_cat_full_cycle_is_equivalent_to_t_zero(void) {
    // 19: the play duration is a whole number of shimmer cycles, so the state
    // at t=0 and at t=fullCycle is the same logical frame.
    const uint8_t frames = OrangeCatAnimation::frameCount();
    const uint8_t phases = OrangeCatAnimation::phaseCount();
    TEST_ASSERT_EQUAL_UINT8(0u, static_cast<uint8_t>(frames % phases));

    PixelMatrix atZero;
    PixelMatrix atCycle;
    renderPhase(atZero, 0u);
    renderPhase(atCycle, phases);

    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            TEST_ASSERT_TRUE(sameColor(
                pixelColorAt(atZero, x, y), pixelColorAt(atCycle, x, y)));
        }
    }

    // The last phase of a play leads straight back into the first.
    const uint8_t lastPhase = static_cast<uint8_t>(phases - 1u);
    const int16_t step = static_cast<int16_t>(OrangeCatAnimation::glintLevelAt(0u, false)) -
                         static_cast<int16_t>(OrangeCatAnimation::glintLevelAt(lastPhase, false));
    TEST_ASSERT_TRUE(step <= 60 && step >= -60);
}

// ---------------------------------------------------------------------------
// 20-24: architecture rules
// ---------------------------------------------------------------------------

void test_orange_cat_stays_inside_the_panel(void) {
    // 20: nothing is ever drawn outside the sprite's own 16x16 box.
    PixelMatrix matrix;

    for (uint8_t f = 0u; f < OrangeCatAnimation::frameCount(); ++f) {
        renderPhase(matrix, f);

        for (uint8_t y = 0u; y < kPanel; ++y) {
            for (uint8_t x = 0u; x < kPanel; ++x) {
                if (!OrangeCatAnimation::bodyContains(x, y) &&
                    !OrangeCatAnimation::darkContains(x, y) &&
                    !OrangeCatAnimation::lensContains(x, y) &&
                    !OrangeCatAnimation::glintContains(x, y)) {
                    TEST_ASSERT_FALSE(isLit(pixelColorAt(matrix, x, y)));
                }
            }
        }
    }
}

void test_orange_cat_has_no_framebuffer_or_heap(void) {
    // 22: no heap, no framebuffer, no 256-pixel buffer.
    TEST_ASSERT_TRUE(sizeof(OrangeCatAnimation) <= 32u);
}

void test_orange_cat_is_non_blocking(void) {
    // 21: the whole play is produced by update() alone, with no wait.
    OrangeCatAnimation animation;
    PixelMatrix matrix;

    animation.start(0u);
    for (uint16_t i = 1u; i <= OrangeCatAnimation::frameCount(); ++i) {
        animation.update(static_cast<uint32_t>(i) * 200u, matrix);
    }

    TEST_ASSERT_TRUE(animation.isFinished());
}

void test_orange_cat_rotation_stays_central(void) {
    // 23: the animation draws logical coordinates only; PixelMatrix rotates.
    PixelMatrix plain;
    PixelMatrix rotated;

    rotated.setRotation(DisplayRotation::DEG_90);
    renderPhase(plain, 1u);
    renderPhase(rotated, 1u);

    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            const Color logical = pixelColorAt(plain, x, y);
            const Color mapped = pixelColorAt(rotated, static_cast<uint8_t>(kPanel - 1u - y), x);
            TEST_ASSERT_TRUE(sameColor(logical, mapped));
        }
    }
}

void test_orange_cat_power_ceiling_is_untouched(void) {
    // 24: the limiter is not referenced by the animation at all.
    TEST_ASSERT_EQUAL_UINT16(2000u, LedPowerBudget::kLimitMilliAmp);
    TEST_ASSERT_EQUAL_UINT8(33u, LedPowerBudget::kMaxSafeBrightnessRaw);

    PixelMatrix matrix;
    matrix.setBrightness(255u);
    renderPhase(matrix, 3u);

    TEST_ASSERT_EQUAL_UINT8(
        LedPowerBudget::kMaxSafeBrightnessRaw, matrix.effectiveBrightnessRaw());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_orange_cat_is_registered_and_selectable);
    RUN_TEST(test_orange_cat_label_is_polish);
    RUN_TEST(test_orange_cat_body_fills_the_panel);
    RUN_TEST(test_orange_cat_body_reaches_both_side_edges);
    RUN_TEST(test_orange_cat_ears_reach_the_top);
    RUN_TEST(test_orange_cat_lenses_are_large_on_both_sides);
    RUN_TEST(test_orange_cat_glasses_span_the_face_with_a_bridge);
    RUN_TEST(test_orange_cat_glint_pixels_sit_inside_the_lenses);
    RUN_TEST(test_orange_cat_lit_geometry_is_identical_across_all_frames);
    RUN_TEST(test_orange_cat_only_glint_pixels_change);
    RUN_TEST(test_orange_cat_body_and_frame_never_change_brightness);
    RUN_TEST(test_orange_cat_body_is_orange_and_lenses_are_blue);
    RUN_TEST(test_orange_cat_glints_are_phase_offset_and_never_strobe);
    RUN_TEST(test_orange_cat_glint_curve_loops_seamlessly);
    RUN_TEST(test_orange_cat_full_cycle_is_equivalent_to_t_zero);
    RUN_TEST(test_orange_cat_stays_inside_the_panel);
    RUN_TEST(test_orange_cat_has_no_framebuffer_or_heap);
    RUN_TEST(test_orange_cat_is_non_blocking);
    RUN_TEST(test_orange_cat_rotation_stays_central);
    RUN_TEST(test_orange_cat_power_ceiling_is_untouched);
    return UNITY_END();
}
