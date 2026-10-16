#include <unity.h>

#define private public
#include "animations/MoonStarsAnimation.h"
#undef private

#include "config/LedPowerBudget.h"
#include "core/AnimationId.h"
#include "core/AnimationRegistry.h"
#include "core/PixelMatrix.h"

namespace {

constexpr uint8_t kPanel = 16u;

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

// Warm gold: red strongest, then green, then a little blue. Never white.
bool isWarmGold(const Color& c) {
    return c.r > c.g && c.g > c.b;
}

struct StarBox {
    int16_t minX;
    int16_t maxX;
    int16_t minY;
    int16_t maxY;
    bool valid;
};

StarBox starBox(uint8_t index) {
    StarBox box{static_cast<int16_t>(kPanel), -1, static_cast<int16_t>(kPanel), -1, false};
    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            if (!MoonStarsAnimation::starContains(index, x, y)) {
                continue;
            }
            box.valid = true;
            if (static_cast<int16_t>(x) < box.minX) box.minX = static_cast<int16_t>(x);
            if (static_cast<int16_t>(x) > box.maxX) box.maxX = static_cast<int16_t>(x);
            if (static_cast<int16_t>(y) < box.minY) box.minY = static_cast<int16_t>(y);
            if (static_cast<int16_t>(y) > box.maxY) box.maxY = static_cast<int16_t>(y);
        }
    }
    return box;
}

int16_t chebyshev(uint8_t ax, uint8_t ay, uint8_t bx, uint8_t by) {
    const int16_t dx = static_cast<int16_t>(ax) - static_cast<int16_t>(bx);
    const int16_t dy = static_cast<int16_t>(ay) - static_cast<int16_t>(by);
    const int16_t adx = (dx < 0) ? static_cast<int16_t>(-dx) : dx;
    const int16_t ady = (dy < 0) ? static_cast<int16_t>(-dy) : dy;
    return (adx > ady) ? adx : ady;
}

}  // namespace

void test_moon_stars_id_and_registry(void) {
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(AnimationId::CHRISTMAS_TREE) + 1u,
        static_cast<uint8_t>(AnimationId::MOON_STARS)
    );
    TEST_ASSERT_NOT_NULL(AnimationRegistry::getAnimation(AnimationId::MOON_STARS));
}

void test_moon_stars_phase_progression_and_loop_boundary(void) {
    MoonStarsAnimation animation;
    PixelMatrix matrix;

    animation.start(1000u);
    TEST_ASSERT_EQUAL_UINT8(0u, animation.phase_);
    TEST_ASSERT_EQUAL_UINT8(0u, animation.renderedFrameCount_);
    TEST_ASSERT_FALSE(animation.finished_);

    for (uint8_t step = 0u; step < 64u; ++step) {
        const uint32_t now = 1000u + (static_cast<uint32_t>(step) * 180u);
        animation.update(now, matrix);

        const uint8_t expectedPhase =
            static_cast<uint8_t>((step + 1u) % 32u);

        TEST_ASSERT_EQUAL_UINT8(expectedPhase, animation.phase_);

        if (step < 63u) {
            TEST_ASSERT_FALSE(animation.finished_);
        }
    }

    TEST_ASSERT_EQUAL_UINT8(0u, animation.phase_);
    TEST_ASSERT_EQUAL_UINT8(64u, animation.renderedFrameCount_);
    TEST_ASSERT_TRUE(animation.finished_);
}

void test_moon_stars_twinkle_table_seam_is_smooth(void) {
    const uint8_t brightness0 = MoonStarsAnimation::twinkleScale(0u);
    const uint8_t brightness31 = MoonStarsAnimation::twinkleScale(31u);

    TEST_ASSERT_EQUAL_UINT8(96u, brightness0);
    TEST_ASSERT_EQUAL_UINT8(95u, brightness31);
    TEST_ASSERT_EQUAL_UINT8(1u, static_cast<uint8_t>(brightness0 - brightness31));
}

// ---------------------------------------------------------------------------
// A1/A2: the moon must be dominant and genuinely crescent shaped.
// ---------------------------------------------------------------------------

void test_moon_stars_moon_is_large(void) {
    uint16_t count = 0u;
    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            if (MoonStarsAnimation::moonContains(x, y)) {
                ++count;
            }
        }
    }

    // The previous moon was 35 pixels across a 6x9 box and read as too small
    // through the frosted diffuser.
    TEST_ASSERT_TRUE(count >= 45u);
    TEST_ASSERT_TRUE(count <= 60u);
}

void test_moon_stars_moon_bounding_box_is_large_and_on_panel(void) {
    int16_t minX = static_cast<int16_t>(kPanel);
    int16_t maxX = -1;
    int16_t minY = static_cast<int16_t>(kPanel);
    int16_t maxY = -1;

    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            if (!MoonStarsAnimation::moonContains(x, y)) {
                continue;
            }
            if (static_cast<int16_t>(x) < minX) minX = static_cast<int16_t>(x);
            if (static_cast<int16_t>(x) > maxX) maxX = static_cast<int16_t>(x);
            if (static_cast<int16_t>(y) < minY) minY = static_cast<int16_t>(y);
            if (static_cast<int16_t>(y) > maxY) maxY = static_cast<int16_t>(y);
        }
    }

    TEST_ASSERT_TRUE(minX >= 0 && maxX < static_cast<int16_t>(kPanel));
    TEST_ASSERT_TRUE(minY >= 0 && maxY < static_cast<int16_t>(kPanel));

    // 8 wide x 12 tall: the requested dominant size.
    TEST_ASSERT_EQUAL_INT16(8, static_cast<int16_t>(maxX - minX + 1));
    TEST_ASSERT_EQUAL_INT16(12, static_cast<int16_t>(maxY - minY + 1));
}

void test_moon_stars_crescent_opens_to_the_right(void) {
    int16_t topRight = -1;
    int16_t midRight = -1;
    int16_t bottomRight = -1;

    for (uint8_t y = 0u; y < kPanel; ++y) {
        int16_t rowRight = -1;
        for (uint8_t x = 0u; x < kPanel; ++x) {
            if (MoonStarsAnimation::moonContains(x, y)) {
                rowRight = static_cast<int16_t>(x);
            }
        }
        if (rowRight < 0) {
            continue;
        }

        if (y <= 3u && rowRight > topRight) topRight = rowRight;
        if (y >= 7u && y <= 8u && rowRight > midRight) midRight = rowRight;
        if (y >= 12u && rowRight > bottomRight) bottomRight = rowRight;
    }

    TEST_ASSERT_TRUE(topRight > 0);
    TEST_ASSERT_TRUE(midRight > 0);
    TEST_ASSERT_TRUE(bottomRight > 0);

    // The illuminated right edge is carved inward in the middle, so the shape
    // reads as a crescent opening to the right rather than a disc or a C.
    TEST_ASSERT_TRUE(midRight < topRight);
    TEST_ASSERT_TRUE(midRight < bottomRight);
    TEST_ASSERT_TRUE(static_cast<int16_t>(topRight - midRight) >= 3);
}

// ---------------------------------------------------------------------------
// A4: warm moonlight, never pure white.
// ---------------------------------------------------------------------------

void test_moon_stars_moon_is_warm_gold_not_white(void) {
    Color seen[8];
    uint8_t seenCount = 0u;

    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            if (!MoonStarsAnimation::moonContains(x, y)) {
                continue;
            }

            const Color shade = MoonStarsAnimation::moonShadeAt(x, y);

            // Not pure white, and not merely pale: warm gold ordering.
            TEST_ASSERT_FALSE(shade.r == 255u && shade.g == 255u && shade.b == 255u);
            TEST_ASSERT_TRUE(isWarmGold(shade));
            // Moonlight gold, not orange: green stays substantial.
            TEST_ASSERT_TRUE(shade.g >= 140u);

            bool tracked = false;
            for (uint8_t i = 0u; i < seenCount; ++i) {
                if (sameColor(seen[i], shade)) {
                    tracked = true;
                    break;
                }
            }
            if (!tracked && seenCount < 8u) {
                seen[seenCount] = shade;
                ++seenCount;
            }
        }
    }

    // Exactly the three intended shades: rim, body, highlight.
    TEST_ASSERT_EQUAL_UINT8(3u, seenCount);
    for (uint8_t i = 0u; i < seenCount; ++i) {
        TEST_ASSERT_TRUE(isWarmGold(seen[i]));
    }
}

// A7: every moon pixel is written through PixelMatrix, unchanged.
void test_moon_stars_moon_pixels_render_through_the_matrix(void) {
    MoonStarsAnimation animation;
    PixelMatrix matrix;

    animation.start(1000u);
    animation.update(1180u, matrix);

    uint16_t checked = 0u;
    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            if (!MoonStarsAnimation::moonContains(x, y)) {
                continue;
            }

            const Color expected = MoonStarsAnimation::moonShadeAt(x, y);
            const Color actual = pixelColorAt(matrix, x, y);
            TEST_ASSERT_TRUE(sameColor(expected, actual));
            ++checked;
        }
    }

    TEST_ASSERT_TRUE(checked >= 45u);
}

// ---------------------------------------------------------------------------
// A5/A6/A7: star exclusion, spacing and bounds.
// ---------------------------------------------------------------------------

void test_moon_stars_stars_never_touch_the_moon_or_its_halo(void) {
    const uint8_t count = MoonStarsAnimation::starCount();

    for (uint8_t i = 0u; i < count; ++i) {
        for (uint8_t y = 0u; y < kPanel; ++y) {
            for (uint8_t x = 0u; x < kPanel; ++x) {
                if (!MoonStarsAnimation::starContains(i, x, y)) {
                    continue;
                }
                TEST_ASSERT_FALSE(MoonStarsAnimation::moonContains(x, y));
                TEST_ASSERT_FALSE(MoonStarsAnimation::moonHaloContains(x, y));
            }
        }
    }
}

void test_moon_stars_stars_do_not_overlap_each_other(void) {
    const uint8_t count = MoonStarsAnimation::starCount();

    for (uint8_t i = 0u; i < count; ++i) {
        for (uint8_t j = static_cast<uint8_t>(i + 1u); j < count; ++j) {
            for (uint8_t y = 0u; y < kPanel; ++y) {
                for (uint8_t x = 0u; x < kPanel; ++x) {
                    TEST_ASSERT_FALSE(
                        MoonStarsAnimation::starContains(i, x, y) &&
                        MoonStarsAnimation::starContains(j, x, y));
                }
            }
        }
    }
}

void test_moon_stars_star_centres_keep_chebyshev_clearance(void) {
    const uint8_t count = MoonStarsAnimation::starCount();

    for (uint8_t i = 0u; i < count; ++i) {
        for (uint8_t j = static_cast<uint8_t>(i + 1u); j < count; ++j) {
            const uint8_t radiusI = MoonStarsAnimation::starFootprintRadius(i);
            const uint8_t radiusJ = MoonStarsAnimation::starFootprintRadius(j);

            // Star sprites plus at least one fully empty pixel between them.
            const int16_t required =
                static_cast<int16_t>(radiusI + radiusJ + 2u);

            const int16_t distance = chebyshev(
                MoonStarsAnimation::starAnchorX(i),
                MoonStarsAnimation::starAnchorY(i),
                MoonStarsAnimation::starAnchorX(j),
                MoonStarsAnimation::starAnchorY(j));

            TEST_ASSERT_TRUE(distance >= required);
        }
    }
}

void test_moon_stars_star_footprints_stay_on_the_panel(void) {
    const uint8_t count = MoonStarsAnimation::starCount();

    for (uint8_t i = 0u; i < count; ++i) {
        const StarBox box = starBox(i);
        TEST_ASSERT_TRUE(box.valid);
        TEST_ASSERT_TRUE(box.minX >= 0 && box.maxX < static_cast<int16_t>(kPanel));
        TEST_ASSERT_TRUE(box.minY >= 0 && box.maxY < static_cast<int16_t>(kPanel));

        // The full sprite is representable: nothing is clipped at an edge.
        const uint8_t radius = MoonStarsAnimation::starFootprintRadius(i);
        TEST_ASSERT_EQUAL_INT16(
            static_cast<int16_t>((2u * radius) + 1u),
            static_cast<int16_t>(box.maxX - box.minX + 1));
        TEST_ASSERT_EQUAL_INT16(
            static_cast<int16_t>((2u * radius) + 1u),
            static_cast<int16_t>(box.maxY - box.minY + 1));
    }
}

void test_moon_stars_star_count_and_shape_are_bounded(void) {
    const uint8_t count = MoonStarsAnimation::starCount();

    // A calm night sky: a handful of stars, not a full field.
    TEST_ASSERT_TRUE(count >= 3u);
    TEST_ASSERT_TRUE(count <= 6u);

    // At most one large plus; the rest must stay small for the diffuser.
    uint8_t plusCount = 0u;
    for (uint8_t i = 0u; i < count; ++i) {
        if (MoonStarsAnimation::starFootprintRadius(i) > 0u) {
            ++plusCount;
        }
    }
    TEST_ASSERT_TRUE(plusCount <= 1u);
}

// ---------------------------------------------------------------------------
// A10: determinism, central rotation, untouched power ceiling.
// ---------------------------------------------------------------------------

void test_moon_stars_render_is_deterministic(void) {
    MoonStarsAnimation first;
    MoonStarsAnimation second;
    PixelMatrix matrixA;
    PixelMatrix matrixB;

    first.start(1000u);
    second.start(1000u);
    first.update(1180u, matrixA);
    second.update(1180u, matrixB);

    for (uint16_t i = 0u; i < PixelMatrix::LED_COUNT; ++i) {
        TEST_ASSERT_EQUAL_UINT32(
            matrixA.pixels.getPixelColor(i),
            matrixB.pixels.getPixelColor(i));
    }
}

void test_moon_stars_rotation_is_applied_centrally(void) {
    MoonStarsAnimation plain;
    MoonStarsAnimation rotated;
    PixelMatrix matrixPlain;
    PixelMatrix matrixRotated;

    matrixRotated.setRotation(DisplayRotation::DEG_90);

    plain.start(1000u);
    rotated.start(1000u);
    plain.update(1180u, matrixPlain);
    rotated.update(1180u, matrixRotated);

    // The animation knows nothing about rotation; PixelMatrix transforms it.
    // A logical moon pixel (6,10) must appear at DEG_90's (15-y, x) = (5,6).
    const uint8_t lx = 6u;
    const uint8_t ly = 10u;
    TEST_ASSERT_TRUE(MoonStarsAnimation::moonContains(lx, ly));

    const Color expected = MoonStarsAnimation::moonShadeAt(lx, ly);
    TEST_ASSERT_TRUE(sameColor(expected, pixelColorAt(matrixPlain, lx, ly)));
    TEST_ASSERT_TRUE(sameColor(
        expected,
        pixelColorAt(matrixRotated, static_cast<uint8_t>(kPanel - 1u - ly), lx)));

    // ...and the unrotated frame is genuinely different.
    TEST_ASSERT_FALSE(sameColor(expected, pixelColorAt(matrixRotated, lx, ly)));
}

void test_moon_stars_output_stays_under_the_power_ceiling(void) {
    // The single LED safety authority is unchanged by this animation.
    TEST_ASSERT_EQUAL_UINT16(2000u, LedPowerBudget::kLimitMilliAmp);

    PixelMatrix matrix;
    matrix.setBrightness(255u);  // an impossible request
    TEST_ASSERT_EQUAL_UINT8(
        LedPowerBudget::kMaxSafeBrightnessRaw,
        matrix.effectiveBrightnessRaw());

    MoonStarsAnimation animation;
    animation.start(1000u);
    animation.update(1180u, matrix);

    // Rendering a large moon must not touch the brightness authority.
    TEST_ASSERT_EQUAL_UINT8(
        LedPowerBudget::kMaxSafeBrightnessRaw,
        matrix.effectiveBrightnessRaw());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_moon_stars_id_and_registry);
    RUN_TEST(test_moon_stars_phase_progression_and_loop_boundary);
    RUN_TEST(test_moon_stars_twinkle_table_seam_is_smooth);
    RUN_TEST(test_moon_stars_moon_is_large);
    RUN_TEST(test_moon_stars_moon_bounding_box_is_large_and_on_panel);
    RUN_TEST(test_moon_stars_crescent_opens_to_the_right);
    RUN_TEST(test_moon_stars_moon_is_warm_gold_not_white);
    RUN_TEST(test_moon_stars_moon_pixels_render_through_the_matrix);
    RUN_TEST(test_moon_stars_stars_never_touch_the_moon_or_its_halo);
    RUN_TEST(test_moon_stars_stars_do_not_overlap_each_other);
    RUN_TEST(test_moon_stars_star_centres_keep_chebyshev_clearance);
    RUN_TEST(test_moon_stars_star_footprints_stay_on_the_panel);
    RUN_TEST(test_moon_stars_star_count_and_shape_are_bounded);
    RUN_TEST(test_moon_stars_render_is_deterministic);
    RUN_TEST(test_moon_stars_rotation_is_applied_centrally);
    RUN_TEST(test_moon_stars_output_stays_under_the_power_ceiling);
    return UNITY_END();
}
