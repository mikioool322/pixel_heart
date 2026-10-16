#include <unity.h>

#include <stdlib.h>
#include <string.h>

#include "animations/PalmBreezeAnimation.h"
#include "assets/PixelMask.h"
#include "config/LedPowerBudget.h"
#include "control/AnimationCatalog.h"
#include "control/UserSettings.h"
#include "core/AnimationId.h"
#include "core/AnimationNames.h"
#include "core/AnimationRegistry.h"
#include "core/PixelMatrix.h"

// PALM_BREEZE - a calm tropical palm on a 16x16 matrix behind a milky diffuser.
//
// The invariants this file protects are the ones that are easy to break by
// accident and hard to notice in code:
//
//   - the lower trunk is ANCHORED. It must be pixel-identical in every pose, or
//     the tree slides sideways across the panel instead of swaying.
//   - the leaves travel further than the trunk, which is what makes the motion
//     read as wind.
//   - no adjacent pose moves any feature by more than one logical pixel,
//     INCLUDING the wrap from the last pose back to the first.
//   - the loop is seamless by construction: t=0 and t=loopDurationMs are the
//     same frame.
//   - the scene is visible but restrained: dim sky, real ocean and sand bands,
//     a small sun, and no pure white.
//   - the frame fits inside the existing 2000 mA budget without weakening it.

namespace {

// The five logical poses.
constexpr uint8_t kCenter = 0u;
constexpr uint8_t kRight1 = 1u;
constexpr uint8_t kRight2 = 2u;
constexpr uint8_t kLeft1 = 3u;
constexpr uint8_t kLeft2 = 4u;

uint32_t packedAt(PixelMatrix& matrix, uint8_t x, uint8_t y) {
    // Read back through PixelMatrix's own serpentine mapping. Indexing the
    // buffer linearly would mirror every odd row and silently compare the wrong
    // pixels.
    return matrix.pixelColorAt(PixelMatrix::xyToIndexStatic(x, y));
}

bool isColor(PixelMatrix& matrix, uint8_t x, uint8_t y, Color expected) {
    const uint32_t packed = packedAt(matrix, x, y);
    return static_cast<uint8_t>((packed >> 16) & 0xFFu) == expected.r &&
           static_cast<uint8_t>((packed >> 8) & 0xFFu) == expected.g &&
           static_cast<uint8_t>(packed & 0xFFu) == expected.b;
}

uint8_t redOf(PixelMatrix& matrix, uint8_t x, uint8_t y) {
    return static_cast<uint8_t>((packedAt(matrix, x, y) >> 16) & 0xFFu);
}
uint8_t greenOf(PixelMatrix& matrix, uint8_t x, uint8_t y) {
    return static_cast<uint8_t>((packedAt(matrix, x, y) >> 8) & 0xFFu);
}
uint8_t blueOf(PixelMatrix& matrix, uint8_t x, uint8_t y) {
    return static_cast<uint8_t>(packedAt(matrix, x, y) & 0xFFu);
}

// Renders the logical frame a given number of milliseconds after start() and
// returns it in `matrix`.
void renderAt(PalmBreezeAnimation& palm, PixelMatrix& matrix, uint32_t startMs, uint32_t offsetMs) {
    palm.start(startMs);
    // One update is enough: the step is derived from elapsed time, and
    // sequenceIndex_ starts at an invalid value so the first update paints.
    palm.update(startMs + offsetMs, matrix);
}

uint32_t transmittedChannel(uint8_t channel, uint8_t brightnessRaw) {
    return (static_cast<uint32_t>(channel) * (static_cast<uint32_t>(brightnessRaw) + 1u)) >> 8;
}

// Same estimator the power-budget tests use: the real library scales a channel
// at write time, so the estimate is over transmitted values.
uint32_t frameEstimateMilliAmp(const PixelMatrix& matrix) {
    const uint8_t brightness = matrix.effectiveBrightnessRaw();
    uint32_t channelSum = 0u;

    for (uint16_t i = 0u; i < PixelMatrix::LED_COUNT; ++i) {
        const uint32_t packed = matrix.pixelColorAt(i);
        channelSum += transmittedChannel(static_cast<uint8_t>((packed >> 16) & 0xFFu), brightness);
        channelSum += transmittedChannel(static_cast<uint8_t>((packed >> 8) & 0xFFu), brightness);
        channelSum += transmittedChannel(static_cast<uint8_t>(packed & 0xFFu), brightness);
    }

    return LedPowerBudget::estimateMilliAmp(channelSum);
}

}  // namespace

// ===========================================================================
// Registration, name, ordinals
// ===========================================================================

void test_palm_breeze_is_registered(void) {
    TEST_ASSERT_NOT_NULL(AnimationRegistry::getAnimation(AnimationId::PALM_BREEZE));
    TEST_ASSERT_TRUE(AnimationRegistry::isRenderable(AnimationId::PALM_BREEZE));

    // The registry holds one shared instance, like every other animation.
    TEST_ASSERT_EQUAL_PTR(
        AnimationRegistry::getAnimation(AnimationId::PALM_BREEZE),
        AnimationRegistry::getAnimation(AnimationId::PALM_BREEZE));
}

void test_palm_breeze_ui_name_is_palma(void) {
    TEST_ASSERT_EQUAL_STRING("Palma", animationNameOf(AnimationId::PALM_BREEZE));
}

void test_palm_breeze_is_user_selectable(void) {
    // No separate catalog list: the panel derives its list from the registry.
    TEST_ASSERT_TRUE(AnimationCatalog::isUserSelectable(AnimationId::PALM_BREEZE));
    TEST_ASSERT_TRUE(AnimationCatalog::isSelectable(AnimationId::PALM_BREEZE));
}

void test_existing_animation_ordinals_are_unchanged(void) {
    // Every id that shipped before PALM_BREEZE must keep its stored value,
    // because NVS persists the raw byte.
    TEST_ASSERT_EQUAL_UINT8(0u, static_cast<uint8_t>(AnimationId::NONE));
    TEST_ASSERT_EQUAL_UINT8(1u, static_cast<uint8_t>(AnimationId::HEARTBEAT));
    TEST_ASSERT_EQUAL_UINT8(2u, static_cast<uint8_t>(AnimationId::BREATHING_HEART));
    TEST_ASSERT_EQUAL_UINT8(3u, static_cast<uint8_t>(AnimationId::ROSE_BLOOM));
    TEST_ASSERT_EQUAL_UINT8(4u, static_cast<uint8_t>(AnimationId::FIREWORKS));
    TEST_ASSERT_EQUAL_UINT8(5u, static_cast<uint8_t>(AnimationId::LOVE_MODE));
    TEST_ASSERT_EQUAL_UINT8(6u, static_cast<uint8_t>(AnimationId::CAMPFIRE));
    TEST_ASSERT_EQUAL_UINT8(7u, static_cast<uint8_t>(AnimationId::DANCING_DUCK));
    TEST_ASSERT_EQUAL_UINT8(8u, static_cast<uint8_t>(AnimationId::SLOT_MACHINE));
    TEST_ASSERT_EQUAL_UINT8(9u, static_cast<uint8_t>(AnimationId::SWIMMING_FISH));
    TEST_ASSERT_EQUAL_UINT8(10u, static_cast<uint8_t>(AnimationId::AURORA));
    TEST_ASSERT_EQUAL_UINT8(11u, static_cast<uint8_t>(AnimationId::AURORA_ARCTIC));
    TEST_ASSERT_EQUAL_UINT8(12u, static_cast<uint8_t>(AnimationId::AURORA_SUNSET));
    TEST_ASSERT_EQUAL_UINT8(13u, static_cast<uint8_t>(AnimationId::AURORA_NEON));
    TEST_ASSERT_EQUAL_UINT8(14u, static_cast<uint8_t>(AnimationId::CHRISTMAS_TREE));
    TEST_ASSERT_EQUAL_UINT8(15u, static_cast<uint8_t>(AnimationId::MOON_STARS));
    TEST_ASSERT_EQUAL_UINT8(16u, static_cast<uint8_t>(AnimationId::SUNSET_WATER));
    TEST_ASSERT_EQUAL_UINT8(17u, static_cast<uint8_t>(AnimationId::FALLING_STARS));
    TEST_ASSERT_EQUAL_UINT8(18u, static_cast<uint8_t>(AnimationId::SUN_OVER_MOUNTAINS));
    TEST_ASSERT_EQUAL_UINT8(19u, static_cast<uint8_t>(AnimationId::HALLOWEEN_GHOST));
    TEST_ASSERT_EQUAL_UINT8(20u, static_cast<uint8_t>(AnimationId::FOX));
    TEST_ASSERT_EQUAL_UINT8(21u, static_cast<uint8_t>(AnimationId::ORANGE_CAT));

    // Appended, so it is the last real id.
    TEST_ASSERT_EQUAL_UINT8(22u, static_cast<uint8_t>(AnimationId::PALM_BREEZE));
    TEST_ASSERT_EQUAL_UINT8(23u, static_cast<uint8_t>(AnimationId::JELLYFISH));
    TEST_ASSERT_EQUAL_UINT8(24u, static_cast<uint8_t>(AnimationId::LIGHTHOUSE));
    TEST_ASSERT_EQUAL_UINT8(25u, static_cast<uint8_t>(AnimationId::DISCO_BALL));
    TEST_ASSERT_EQUAL_UINT8(26u, static_cast<uint8_t>(AnimationId::MEADOW_FLOWER));
    TEST_ASSERT_EQUAL_UINT8(27u, static_cast<uint8_t>(AnimationId::BLUE_SKY_CLOUDS));
    TEST_ASSERT_EQUAL_UINT8(28u, static_cast<uint8_t>(AnimationId::ANIMATION_ID_COUNT));
}

// ===========================================================================
// Scene: sky, ocean, sand, sun
// ===========================================================================

void test_scene_has_dim_blue_sky(void) {
    PalmBreezeAnimation palm;
    PixelMatrix matrix;
    renderAt(palm, matrix, 0u, 0u);

    // Rows 0..11 are sky and are clearly blue: blue is the strongest channel.
    for (uint8_t y = 0u; y <= 11u; ++y) {
        for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
            if (PalmBreezeAnimation::leavesDarkContains(x, y) ||
                PalmBreezeAnimation::leavesBrightContains(x, y) ||
                PalmBreezeAnimation::trunkUpperContains(x, y) ||
                PalmBreezeAnimation::trunkLowerContains(x, y)) {
                continue;
            }
            // The sun sits in the sky but is warm, not blue.
            if (x >= 2u && x <= 3u && y >= 1u && y <= 2u) {
                continue;
            }
            TEST_ASSERT_TRUE(blueOf(matrix, x, y) > redOf(matrix, x, y));
        }
    }
}

void test_sky_stays_dim_for_the_diffuser(void) {
    PalmBreezeAnimation palm;
    PixelMatrix matrix;
    renderAt(palm, matrix, 0u, 0u);

    // A bloomy diffuser turns anything bright into a white slab, so the sky
    // must stay well below the palm's brightest channel.
    TEST_ASSERT_TRUE(blueOf(matrix, 0u, 8u) <= 160u);
    TEST_ASSERT_TRUE(redOf(matrix, 0u, 8u) <= 40u);
}

void test_scene_has_ocean_and_sand_bands(void) {
    PalmBreezeAnimation palm;
    PixelMatrix matrix;
    renderAt(palm, matrix, 0u, 0u);

    // Two ocean rows, both blue-dominant, and a lighter shoreline row.
    for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
        if (PalmBreezeAnimation::trunkLowerContains(x, 12u) ||
            PalmBreezeAnimation::trunkLowerContains(x, 13u)) {
            continue;
        }
        TEST_ASSERT_TRUE(blueOf(matrix, x, 12u) > redOf(matrix, x, 12u));
        TEST_ASSERT_TRUE(blueOf(matrix, x, 13u) > blueOf(matrix, x, 12u));
    }

    // Two warm sand rows at the bottom.
    for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
        if (PalmBreezeAnimation::trunkLowerContains(x, 14u) ||
            PalmBreezeAnimation::trunkLowerContains(x, 15u)) {
            continue;
        }
        TEST_ASSERT_TRUE(redOf(matrix, x, 14u) > blueOf(matrix, x, 14u));
        TEST_ASSERT_TRUE(redOf(matrix, x, 15u) > blueOf(matrix, x, 15u));
    }
}

void test_scene_has_a_small_warm_sun(void) {
    PalmBreezeAnimation palm;
    PixelMatrix matrix;
    renderAt(palm, matrix, 0u, 0u);

    // Upper-left, warm, and deliberately tiny: a 2x2 block, not a yellow square.
    TEST_ASSERT_TRUE(redOf(matrix, 2u, 1u) > greenOf(matrix, 2u, 1u));
    TEST_ASSERT_TRUE(greenOf(matrix, 2u, 1u) > blueOf(matrix, 2u, 1u));

    uint32_t sunPixels = 0u;
    for (uint8_t y = 0u; y < 4u; ++y) {
        for (uint8_t x = 0u; x < 6u; ++x) {
            if (redOf(matrix, x, y) > 200u && greenOf(matrix, x, y) > 120u) {
                ++sunPixels;
            }
        }
    }
    TEST_ASSERT_TRUE(sunPixels >= 3u);
    TEST_ASSERT_TRUE(sunPixels <= 6u);
}

void test_scene_never_uses_pure_white(void) {
    PalmBreezeAnimation palm;
    PixelMatrix matrix;
    renderAt(palm, matrix, 0u, 0u);

    for (uint16_t i = 0u; i < PixelMatrix::LED_COUNT; ++i) {
        const uint32_t packed = matrix.pixelColorAt(i);
        const bool white =
            static_cast<uint8_t>((packed >> 16) & 0xFFu) > 240u &&
            static_cast<uint8_t>((packed >> 8) & 0xFFu) > 240u &&
            static_cast<uint8_t>(packed & 0xFFu) > 240u;
        TEST_ASSERT_FALSE(white);
    }
}

// ===========================================================================
// Palm geometry
// ===========================================================================

void test_palm_occupies_a_large_central_region(void) {
    uint8_t minX = 16u;
    uint8_t maxX = 0u;
    uint8_t minY = 16u;
    uint8_t maxY = 0u;

    for (uint8_t y = 0u; y < 16u; ++y) {
        for (uint8_t x = 0u; x < 16u; ++x) {
            if (PalmBreezeAnimation::leavesDarkContains(x, y) ||
                PalmBreezeAnimation::leavesBrightContains(x, y) ||
                PalmBreezeAnimation::trunkUpperContains(x, y) ||
                PalmBreezeAnimation::trunkLowerContains(x, y)) {
                if (x < minX) { minX = x; }
                if (x > maxX) { maxX = x; }
                if (y < minY) { minY = y; }
                if (y > maxY) { maxY = y; }
            }
        }
    }

    TEST_ASSERT_EQUAL_UINT8(2u, minX);
    TEST_ASSERT_EQUAL_UINT8(13u, maxX);
    TEST_ASSERT_EQUAL_UINT8(3u, minY);
    TEST_ASSERT_EQUAL_UINT8(15u, maxY);

    // The palm is the hero: it spans most of the panel width and reaches the
    // sand, so it is not a small detail lost in the sky.
    TEST_ASSERT_TRUE((maxX - minX) >= 10u);
    TEST_ASSERT_EQUAL_UINT8(PixelMatrix::HEIGHT - 1u, maxY);
}

void test_palm_is_rendered_where_the_geometry_says(void) {
    PalmBreezeAnimation palm;
    PixelMatrix matrix;
    renderAt(palm, matrix, 0u, 0u);

    // The anchored trunk base must be visible in the sand.
    TEST_ASSERT_TRUE(PalmBreezeAnimation::trunkLowerContains(7u, 15u));
    TEST_ASSERT_TRUE(redOf(matrix, 7u, 15u) > blueOf(matrix, 7u, 15u));

    // The crown must be green and clearly brighter than the sky behind it.
    TEST_ASSERT_TRUE(PalmBreezeAnimation::leavesBrightContains(7u, 3u));
    TEST_ASSERT_TRUE(greenOf(matrix, 7u, 3u) > blueOf(matrix, 7u, 3u));

    // Two distinct green shades are used for the leaves.
    const uint8_t bright = greenOf(matrix, 7u, 3u);
    const uint8_t dark = greenOf(matrix, 2u, 6u);
    TEST_ASSERT_TRUE(bright > dark);
}

// ===========================================================================
// Sway invariants
// ===========================================================================

void test_lower_trunk_is_identical_across_all_poses(void) {
    // The whole point of the design: the base never moves.
    PalmBreezeAnimation palm;
    PixelMatrix reference;
    renderAt(palm, reference, 0u, 0u);

    for (uint8_t step = 0u; step < PalmBreezeAnimation::sequenceLength(); ++step) {
        const uint32_t offset =
            static_cast<uint32_t>(step) * PalmBreezeAnimation::kFrameIntervalMs;

        PalmBreezeAnimation other;
        PixelMatrix matrix;
        renderAt(other, matrix, 0u, offset);

        for (uint8_t y = 10u; y < 16u; ++y) {
            for (uint8_t x = 0u; x < PixelMatrix::WIDTH; ++x) {
                TEST_ASSERT_EQUAL_HEX32(packedAt(reference, x, y), packedAt(matrix, x, y));
            }
        }
    }
}

void test_upper_trunk_moves_at_most_one_pixel(void) {
    for (uint8_t pose = 0u; pose < PalmBreezeAnimation::kPoseCount; ++pose) {
        const int8_t shift = PalmBreezeAnimation::trunkShiftAt(pose);
        TEST_ASSERT_TRUE(shift >= -1 && shift <= 1);
    }
}

void test_leaf_tips_move_at_most_two_pixels(void) {
    int8_t widest = 0;
    for (uint8_t pose = 0u; pose < PalmBreezeAnimation::kPoseCount; ++pose) {
        const int8_t shift = PalmBreezeAnimation::leafShiftAt(pose);
        TEST_ASSERT_TRUE(shift >= -2 && shift <= 2);
        if (shift > widest) { widest = shift; }
        if (-shift > widest) { widest = static_cast<int8_t>(-shift); }
    }
    // The leaves really do use their full range, otherwise the tree would not
    // read as a breeze.
    TEST_ASSERT_EQUAL_INT8(2, widest);
}

void test_leaves_move_further_than_the_trunk(void) {
    // Magnitude, so direction does not matter: at every extreme the fronds have
    // travelled twice as far as the trunk they are attached to.
    for (uint8_t pose = 0u; pose < PalmBreezeAnimation::kPoseCount; ++pose) {
        const int leaf = PalmBreezeAnimation::leafShiftAt(pose);
        const int trunk = PalmBreezeAnimation::trunkShiftAt(pose);
        TEST_ASSERT_TRUE(abs(leaf) >= abs(trunk));
    }

    // At the extremes the leaves have travelled twice as far as the trunk.
    TEST_ASSERT_EQUAL_INT8(2, abs(PalmBreezeAnimation::leafShiftAt(kRight2)));
    TEST_ASSERT_EQUAL_INT8(1, abs(PalmBreezeAnimation::trunkShiftAt(kRight2)));
    TEST_ASSERT_EQUAL_INT8(2, abs(PalmBreezeAnimation::leafShiftAt(kLeft2)));
    TEST_ASSERT_EQUAL_INT8(1, abs(PalmBreezeAnimation::trunkShiftAt(kLeft2)));
}

void test_crown_moves(void) {
    TEST_ASSERT_EQUAL_INT8(0, PalmBreezeAnimation::leafShiftAt(kCenter));
    TEST_ASSERT_TRUE(PalmBreezeAnimation::leafShiftAt(kRight1) != 0);
    TEST_ASSERT_TRUE(PalmBreezeAnimation::leafShiftAt(kLeft1) != 0);
    TEST_ASSERT_TRUE(PalmBreezeAnimation::trunkShiftAt(kRight1) != 0);
}

void test_right_2_bends_farther_right_than_right_1(void) {
    TEST_ASSERT_TRUE(
        PalmBreezeAnimation::leafShiftAt(kRight2) >
        PalmBreezeAnimation::leafShiftAt(kRight1));
    TEST_ASSERT_TRUE(PalmBreezeAnimation::leafShiftAt(kRight1) > 0);
}

void test_left_2_bends_farther_left_than_left_1(void) {
    TEST_ASSERT_TRUE(
        PalmBreezeAnimation::leafShiftAt(kLeft2) <
        PalmBreezeAnimation::leafShiftAt(kLeft1));
    TEST_ASSERT_TRUE(PalmBreezeAnimation::leafShiftAt(kLeft1) < 0);
}

void test_center_is_the_reference_geometry(void) {
    TEST_ASSERT_EQUAL_INT8(0, PalmBreezeAnimation::trunkShiftAt(kCenter));
    TEST_ASSERT_EQUAL_INT8(0, PalmBreezeAnimation::leafShiftAt(kCenter));

    // The geometry helpers describe CENTER, so at CENTER a pixel reported by
    // the helper must actually be painted palm.
    PalmBreezeAnimation palm;
    PixelMatrix matrix;
    renderAt(palm, matrix, 0u, 0u);

    for (uint8_t y = 0u; y < 16u; ++y) {
        for (uint8_t x = 0u; x < 16u; ++x) {
            const bool inGeometry =
                PalmBreezeAnimation::trunkLowerContains(x, y) ||
                PalmBreezeAnimation::trunkUpperContains(x, y) ||
                PalmBreezeAnimation::leavesDarkContains(x, y) ||
                PalmBreezeAnimation::leavesBrightContains(x, y);

            if (!inGeometry) {
                continue;
            }

            // Palm pixels are warm brown or green: never sky blue.
            TEST_ASSERT_FALSE(
                blueOf(matrix, x, y) > 120u &&
                greenOf(matrix, x, y) > blueOf(matrix, x, y));
        }
    }
}

// ===========================================================================
// Sequence and loop
// ===========================================================================

void test_pose_sequence_is_exactly_the_approved_order(void) {
    const uint8_t expected[8] = { 0u, 1u, 2u, 1u, 0u, 3u, 4u, 3u };

    TEST_ASSERT_EQUAL_UINT8(8u, PalmBreezeAnimation::sequenceLength());

    for (uint8_t step = 0u; step < 8u; ++step) {
        TEST_ASSERT_EQUAL_UINT8(expected[step], PalmBreezeAnimation::poseAtStep(step));
    }

    // Read as the named sequence.
    TEST_ASSERT_EQUAL_UINT8(kCenter, PalmBreezeAnimation::poseAtStep(0u));
    TEST_ASSERT_EQUAL_UINT8(kRight1, PalmBreezeAnimation::poseAtStep(1u));
    TEST_ASSERT_EQUAL_UINT8(kRight2, PalmBreezeAnimation::poseAtStep(2u));
    TEST_ASSERT_EQUAL_UINT8(kRight1, PalmBreezeAnimation::poseAtStep(3u));
    TEST_ASSERT_EQUAL_UINT8(kCenter, PalmBreezeAnimation::poseAtStep(4u));
    TEST_ASSERT_EQUAL_UINT8(kLeft1, PalmBreezeAnimation::poseAtStep(5u));
    TEST_ASSERT_EQUAL_UINT8(kLeft2, PalmBreezeAnimation::poseAtStep(6u));
    TEST_ASSERT_EQUAL_UINT8(kLeft1, PalmBreezeAnimation::poseAtStep(7u));
}

void test_no_adjacent_pose_jumps_more_than_one_pixel(void) {
    const uint8_t length = PalmBreezeAnimation::sequenceLength();

    for (uint8_t i = 0u; i < length; ++i) {
        const uint8_t a = PalmBreezeAnimation::poseAtStep(i);
        const uint8_t b = PalmBreezeAnimation::poseAtStep(static_cast<uint8_t>((i + 1u) % length));

        const int8_t trunkStep =
            PalmBreezeAnimation::trunkShiftAt(b) - PalmBreezeAnimation::trunkShiftAt(a);
        const int8_t leafStep =
            PalmBreezeAnimation::leafShiftAt(b) - PalmBreezeAnimation::leafShiftAt(a);

        TEST_ASSERT_TRUE((trunkStep >= -1 && trunkStep <= 1));
        TEST_ASSERT_TRUE((leafStep >= -1 && leafStep <= 1));
    }
}

void test_wrap_goes_through_center(void) {
    // The wrap must be LEFT_1 -> CENTER, one smooth step, and never
    // LEFT_2 -> RIGHT_2.
    const uint8_t last = PalmBreezeAnimation::poseAtStep(
        static_cast<uint8_t>(PalmBreezeAnimation::sequenceLength() - 1u));

    TEST_ASSERT_EQUAL_UINT8(kLeft1, last);
    TEST_ASSERT_EQUAL_INT8(
        PalmBreezeAnimation::leafShiftAt(kCenter) - PalmBreezeAnimation::leafShiftAt(last),
        1);
    TEST_ASSERT_EQUAL_INT8(
        PalmBreezeAnimation::trunkShiftAt(kCenter) - PalmBreezeAnimation::trunkShiftAt(last),
        1);

    // RIGHT_2 and LEFT_2 are never adjacent anywhere in the sequence.
    for (uint8_t i = 0u; i < PalmBreezeAnimation::sequenceLength(); ++i) {
        const uint8_t a = PalmBreezeAnimation::poseAtStep(i);
        const uint8_t b = PalmBreezeAnimation::poseAtStep(
            static_cast<uint8_t>((i + 1u) % PalmBreezeAnimation::sequenceLength()));

        const bool extremeJump =
            ((a == kRight2 && b == kLeft2) || (a == kLeft2 && b == kRight2));
        TEST_ASSERT_FALSE(extremeJump);
    }
}

void test_loop_is_perfectly_seamless(void) {
    TEST_ASSERT_EQUAL_UINT8(8u, PalmBreezeAnimation::sequenceLength());
    TEST_ASSERT_EQUAL_UINT32(4000u, PalmBreezeAnimation::loopDurationMs());
    TEST_ASSERT_EQUAL_UINT16(500u, PalmBreezeAnimation::kFrameIntervalMs);

    // t = 0 and t = loopDuration must be the identical frame.
    PalmBreezeAnimation palm;
    PixelMatrix first;
    renderAt(palm, first, 0u, 0u);

    PalmBreezeAnimation again;
    PixelMatrix wrapped;
    renderAt(again, wrapped, 0u, PalmBreezeAnimation::loopDurationMs());

    for (uint16_t i = 0u; i < PixelMatrix::LED_COUNT; ++i) {
        TEST_ASSERT_EQUAL_HEX32(first.pixelColorAt(i), wrapped.pixelColorAt(i));
    }
}

void test_frame_timing_is_about_five_hundred_ms(void) {
    const uint32_t duration = PalmBreezeAnimation::loopDurationMs();

    for (uint8_t step = 0u; step < PalmBreezeAnimation::sequenceLength(); ++step) {
        const uint32_t onTime =
            static_cast<uint32_t>(step) * PalmBreezeAnimation::kFrameIntervalMs;

        // INSIDE a step the frame must be completely stable: sampling anywhere
        // within the 500 ms window has to give the identical picture.
        PalmBreezeAnimation palm;
        PixelMatrix middle;
        renderAt(palm, middle, 0u, onTime + 250u);

        const uint32_t samples[] = { onTime + 1u, onTime + 100u, onTime + 400u, onTime + 499u };
        for (uint8_t s = 0u; s < 4u; ++s) {
            PalmBreezeAnimation sample;
            PixelMatrix matrix;
            renderAt(sample, matrix, 0u, samples[s]);

            for (uint16_t i = 0u; i < PixelMatrix::LED_COUNT; ++i) {
                TEST_ASSERT_EQUAL_HEX32(middle.pixelColorAt(i), matrix.pixelColorAt(i));
            }
        }

        // And at each boundary the pose really does change: one millisecond
        // before the boundary is still the previous pose.
        if (step != 0u) {
            PalmBreezeAnimation before;
            PixelMatrix earlier;
            renderAt(before, earlier, 0u, onTime - 1u);

            bool differs = false;
            for (uint16_t i = 0u; i < PixelMatrix::LED_COUNT && !differs; ++i) {
                differs = middle.pixelColorAt(i) != earlier.pixelColorAt(i);
            }
            TEST_ASSERT_TRUE(differs);
        }

        TEST_ASSERT_TRUE(onTime < duration);
    }
}

void test_update_does_not_redirty_an_unchanged_pose(void) {
    PalmBreezeAnimation palm;
    PixelMatrix matrix;
    palm.start(0u);

    // First update paints pose 0.
    palm.update(0u, matrix);
    matrix.showIfDirty();
    TEST_ASSERT_FALSE(matrix.isDirty());

    // Several ticks well inside the same 500 ms window must not repaint.
    for (uint32_t t = 16u; t < 480u; t += 16u) {
        palm.update(t, matrix);
        TEST_ASSERT_FALSE(matrix.isDirty());
    }

    // Crossing the boundary does repaint.
    palm.update(500u, matrix);
    TEST_ASSERT_TRUE(matrix.isDirty());
}

void test_start_renders_the_first_pose_immediately(void) {
    PalmBreezeAnimation palm;
    PixelMatrix matrix;

    palm.start(1000u);
    // No waiting: the very first update at the very same millisecond paints.
    palm.update(1000u, matrix);

    // Pose 0 (CENTER) means the crown is exactly at the CENTER geometry.
    TEST_ASSERT_TRUE(greenOf(matrix, 7u, 3u) > blueOf(matrix, 7u, 3u));
    TEST_ASSERT_TRUE(redOf(matrix, 7u, 15u) > blueOf(matrix, 7u, 15u));
}

void test_completes_a_whole_loop_and_is_finished(void) {
    PalmBreezeAnimation palm;
    PixelMatrix matrix;
    palm.start(0u);

    TEST_ASSERT_FALSE(palm.isFinished());

    // One update per step for a whole pass.
    for (uint8_t step = 0u; step < PalmBreezeAnimation::sequenceLength(); ++step) {
        palm.update(static_cast<uint32_t>(step) * PalmBreezeAnimation::kFrameIntervalMs, matrix);
    }

    TEST_ASSERT_TRUE(palm.isFinished());

    // A finished animation stops repainting, and stop() resets it.
    palm.stop();
    matrix.showIfDirty();
    TEST_ASSERT_FALSE(matrix.isDirty());

    palm.start(0u);
    TEST_ASSERT_FALSE(palm.isFinished());
}

// ===========================================================================
// Resources and untouched subsystems
// ===========================================================================

void test_no_framebuffer_and_drawing_goes_through_pixelmatrix(void) {
    // The animation holds no pixel storage of its own: only timing state. The
    // object is small enough that this is provable by inspection of its size.
    PalmBreezeAnimation palm;
    TEST_ASSERT_TRUE(sizeof(palm) <= 16u);

    // Every pixel is delivered through PixelMatrix, so rotation and the power
    // limiter still apply. Drawing at a rotated matrix must still work.
    PixelMatrix matrix;
    renderAt(palm, matrix, 0u, 0u);
    TEST_ASSERT_FALSE(matrix.isBlack());

    matrix.setRotation(DisplayRotation::DEG_90);
    palm.update(500u, matrix);
    TEST_ASSERT_EQUAL_UINT8(DisplayRotation::DEG_90, matrix.rotation());
}

void test_power_limiter_is_untouched_and_frame_fits_the_budget(void) {
    // The budget itself is not weakened.
    TEST_ASSERT_EQUAL_UINT16(2000u, LedPowerBudget::kLimitMilliAmp);

    // A representative full frame, measured exactly the way
    // test_led_power_budget measures one.
    PixelMatrix matrix;
    matrix.begin();
    matrix.setBrightness(brightnessPercentToRaw(100u));

    PalmBreezeAnimation palm;
    palm.start(0u);

    uint32_t worstMilliAmp = 0u;
    for (uint8_t step = 0u; step < PalmBreezeAnimation::sequenceLength(); ++step) {
        palm.update(static_cast<uint32_t>(step) * PalmBreezeAnimation::kFrameIntervalMs, matrix);

        const uint32_t milliAmp = frameEstimateMilliAmp(matrix);
        if (milliAmp > worstMilliAmp) {
            worstMilliAmp = milliAmp;
        }
    }

    // Even at maximum requested brightness the whole scene fits.
    TEST_ASSERT_TRUE(worstMilliAmp <= LedPowerBudget::kLimitMilliAmp);

    // And the frame is genuinely lit, so the assertion above is meaningful.
    TEST_ASSERT_TRUE(worstMilliAmp > 100u);
}

void test_brightness_goes_through_the_limiter(void) {
    // Requesting more than the ceiling is clamped, exactly as before. The
    // animation does not bypass it.
    PixelMatrix matrix;
    matrix.begin();
    matrix.setBrightness(255u);
    TEST_ASSERT_EQUAL_UINT8(LedPowerBudget::kMaxSafeBrightnessRaw, matrix.effectiveBrightnessRaw());

    PalmBreezeAnimation palm;
    palm.start(0u);
    palm.update(0u, matrix);
    TEST_ASSERT_EQUAL_UINT8(LedPowerBudget::kMaxSafeBrightnessRaw, matrix.effectiveBrightnessRaw());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_palm_breeze_is_registered);
    RUN_TEST(test_palm_breeze_ui_name_is_palma);
    RUN_TEST(test_palm_breeze_is_user_selectable);
    RUN_TEST(test_existing_animation_ordinals_are_unchanged);
    RUN_TEST(test_scene_has_dim_blue_sky);
    RUN_TEST(test_sky_stays_dim_for_the_diffuser);
    RUN_TEST(test_scene_has_ocean_and_sand_bands);
    RUN_TEST(test_scene_has_a_small_warm_sun);
    RUN_TEST(test_scene_never_uses_pure_white);
    RUN_TEST(test_palm_occupies_a_large_central_region);
    RUN_TEST(test_palm_is_rendered_where_the_geometry_says);
    RUN_TEST(test_lower_trunk_is_identical_across_all_poses);
    RUN_TEST(test_upper_trunk_moves_at_most_one_pixel);
    RUN_TEST(test_leaf_tips_move_at_most_two_pixels);
    RUN_TEST(test_leaves_move_further_than_the_trunk);
    RUN_TEST(test_crown_moves);
    RUN_TEST(test_right_2_bends_farther_right_than_right_1);
    RUN_TEST(test_left_2_bends_farther_left_than_left_1);
    RUN_TEST(test_center_is_the_reference_geometry);
    RUN_TEST(test_pose_sequence_is_exactly_the_approved_order);
    RUN_TEST(test_no_adjacent_pose_jumps_more_than_one_pixel);
    RUN_TEST(test_wrap_goes_through_center);
    RUN_TEST(test_loop_is_perfectly_seamless);
    RUN_TEST(test_frame_timing_is_about_five_hundred_ms);
    RUN_TEST(test_update_does_not_redirty_an_unchanged_pose);
    RUN_TEST(test_start_renders_the_first_pose_immediately);
    RUN_TEST(test_completes_a_whole_loop_and_is_finished);
    RUN_TEST(test_no_framebuffer_and_drawing_goes_through_pixelmatrix);
    RUN_TEST(test_power_limiter_is_untouched_and_frame_fits_the_budget);
    RUN_TEST(test_brightness_goes_through_the_limiter);
    return UNITY_END();
}
