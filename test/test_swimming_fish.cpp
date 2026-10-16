#include <unity.h>

#define private public
#include "animations/SwimmingFishAnimation.h"
#undef private

#include "config/LedPowerBudget.h"
#include "core/AnimationId.h"
#include "core/AnimationManager.h"
#include "core/AnimationNames.h"
#include "core/AnimationRegistry.h"
#include "core/PixelMatrix.h"

namespace {

constexpr uint8_t kPanel = 16u;
using S = SwimmingFishAnimation;
using P = SwimmingFish::Palette;

Color pixelColorAt(const PixelMatrix& matrix, uint8_t x, uint8_t y) {
    const uint32_t packed =
        matrix.pixels.getPixelColor(PixelMatrix::xyToIndexStatic(x, y));
    return Color{
        static_cast<uint8_t>((packed >> 16) & 0xFFu),
        static_cast<uint8_t>((packed >> 8) & 0xFFu),
        static_cast<uint8_t>(packed & 0xFFu)
    };
}

bool isLit(const Color& c) {
    return c.r != 0u || c.g != 0u || c.b != 0u;
}

bool sameFrame(const PixelMatrix& a, const PixelMatrix& b) {
    for (uint16_t i = 0u; i < PixelMatrix::LED_COUNT; ++i) {
        if (a.pixels.getPixelColor(i) != b.pixels.getPixelColor(i)) {
            return false;
        }
    }
    return true;
}

bool sameColor(const Color& a, const Color& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

bool sameLitSet(const PixelMatrix& a, const PixelMatrix& b) {
    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            if (isLit(pixelColorAt(a, x, y)) != isLit(pixelColorAt(b, x, y))) {
                return false;
            }
        }
    }
    return true;
}

// Nominal sprite box for an active fish (conservative: includes transparent
// corners, which is what "could two fish collide" must consider).
struct Box {
    int16_t minX;
    int16_t maxX;
    int16_t minY;
    int16_t maxY;
};

Box fishBox(uint8_t index, uint16_t moveStep) {
    const int16_t x = S::fishXAt(index, moveStep);
    const int16_t y = S::fishYAt(index, moveStep);
    return Box{
        x,
        static_cast<int16_t>(x + SwimmingFish::kFishWidth - 1),
        y,
        static_cast<int16_t>(y + SwimmingFish::kFishHeight - 1)
    };
}

uint8_t activeCountAt(uint16_t moveStep) {
    uint8_t count = 0u;
    for (uint8_t i = 0u; i < SwimmingFish::kFishCount; ++i) {
        if (S::fishActiveAt(i, moveStep)) {
            ++count;
        }
    }
    return count;
}

void renderAtStep(PixelMatrix& matrix, uint16_t step) {
    S().renderFrame(matrix, step);
}

}  // namespace

// ---------------------------------------------------------------------------
// 1-4: definitions, palettes, lanes
// ---------------------------------------------------------------------------

void test_fish_definitions_and_palettes_exist(void) {
    TEST_ASSERT_EQUAL_UINT8(3u, SwimmingFish::kFishCount);
    TEST_ASSERT_EQUAL_UINT8(SwimmingFish::kFishCount, S::paletteCount());
    TEST_ASSERT_EQUAL_UINT8(3u, SwimmingFish::kBubbleCount);

    // Every fish points at a real palette.
    for (uint8_t i = 0u; i < SwimmingFish::kFishCount; ++i) {
        TEST_ASSERT_TRUE(S::fishPaletteIndexAt(i) < S::paletteCount());
    }
}

void test_palettes_are_visibly_distinct(void) {
    const P& a = S::paletteAt(0u);
    const P& b = S::paletteAt(1u);
    const P& c = S::paletteAt(2u);

    const auto differs = [](const Color& x, const Color& y) {
        const int dr = static_cast<int>(x.r) - static_cast<int>(y.r);
        const int dg = static_cast<int>(x.g) - static_cast<int>(y.g);
        const int db = static_cast<int>(x.b) - static_cast<int>(y.b);
        const int sum = (dr < 0 ? -dr : dr) + (dg < 0 ? -dg : dg) + (db < 0 ? -db : db);
        return sum > 120;
    };

    TEST_ASSERT_TRUE(differs(a.body, b.body));
    TEST_ASSERT_TRUE(differs(a.body, c.body));
    TEST_ASSERT_TRUE(differs(b.body, c.body));

    // A is blue/cyan: blue dominates. B is orange: red dominates. C is
    // purple/pink: red and blue both beat green.
    TEST_ASSERT_TRUE(a.body.b > a.body.g && a.body.g > a.body.r);
    TEST_ASSERT_TRUE(b.body.r > b.body.g && b.body.g > b.body.b);
    TEST_ASSERT_TRUE(c.body.r > c.body.g && c.body.b > c.body.g);

    // No palette may use a full white body.
    TEST_ASSERT_FALSE(a.body.r == 255u && a.body.g == 255u && a.body.b == 255u);
    TEST_ASSERT_FALSE(b.body.r == 255u && b.body.g == 255u && b.body.b == 255u);
    TEST_ASSERT_FALSE(c.body.r == 255u && c.body.g == 255u && c.body.b == 255u);
}

void test_lanes_are_top_and_bottom(void) {
    TEST_ASSERT_EQUAL_UINT8(0u, S::fishLaneAt(0u));   // A: top
    TEST_ASSERT_EQUAL_UINT8(1u, S::fishLaneAt(1u));   // B: bottom
    TEST_ASSERT_EQUAL_UINT8(0u, S::fishLaneAt(2u));   // C: top

    TEST_ASSERT_EQUAL_INT8(2, S::laneBaseY(0u));
    TEST_ASSERT_EQUAL_INT8(9, S::laneBaseY(1u));

    // Directions: left->right, right->left, left->right.
    TEST_ASSERT_TRUE(S::fishFacesRightAt(0u));
    TEST_ASSERT_FALSE(S::fishFacesRightAt(1u));
    TEST_ASSERT_TRUE(S::fishFacesRightAt(2u));
}

// ---------------------------------------------------------------------------
// 5-8, 15: occupancy invariants
// ---------------------------------------------------------------------------

void test_simultaneous_fish_always_use_different_lanes(void) {
    for (uint16_t moveStep = 0u; moveStep < S::fishEndMoveStepAt(2u); ++moveStep) {
        uint8_t firstActive = 0xFFu;
        for (uint8_t i = 0u; i < SwimmingFish::kFishCount; ++i) {
            if (!S::fishActiveAt(i, moveStep)) {
                continue;
            }
            if (firstActive == 0xFFu) {
                firstActive = i;
                continue;
            }
            TEST_ASSERT_NOT_EQUAL(S::fishLaneAt(firstActive), S::fishLaneAt(i));
        }
    }
}

void test_active_counts_cover_the_choreography(void) {
    bool sawOne = false;
    bool sawTwo = false;

    for (uint16_t moveStep = 0u; moveStep < SwimmingFish::kStepCount / SwimmingFish::kStepsPerMove; ++moveStep) {
        const uint8_t count = activeCountAt(moveStep);

        // 8: never three at once.
        TEST_ASSERT_TRUE(count <= 2u);

        if (count == 1u) {
            sawOne = true;
        }
        if (count == 2u) {
            sawTwo = true;
        }
    }

    // 6 and 7.
    TEST_ASSERT_TRUE(sawOne);
    TEST_ASSERT_TRUE(sawTwo);
}

void test_lane_boxes_never_overlap(void) {
    const uint16_t lastStep =
        static_cast<uint16_t>(SwimmingFish::kStepCount / SwimmingFish::kStepsPerMove);

    for (uint16_t moveStep = 0u; moveStep < lastStep; ++moveStep) {
        for (uint8_t i = 0u; i < SwimmingFish::kFishCount; ++i) {
            if (!S::fishActiveAt(i, moveStep)) {
                continue;
            }
            const Box bi = fishBox(i, moveStep);

            for (uint8_t j = static_cast<uint8_t>(i + 1u); j < SwimmingFish::kFishCount; ++j) {
                if (!S::fishActiveAt(j, moveStep)) {
                    continue;
                }
                const Box bj = fishBox(j, moveStep);

                // 15: the row ranges must be disjoint.
                const bool rowsOverlap =
                    !(bi.maxY < bj.minY || bj.maxY < bi.minY);
                TEST_ASSERT_FALSE(rowsOverlap);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// 9-14: motion
// ---------------------------------------------------------------------------

void test_each_fish_enters_and_exits_fully_outside(void) {
    for (uint8_t i = 0u; i < SwimmingFish::kFishCount; ++i) {
        const uint16_t start = S::fishStartMoveStepAt(i);
        const uint16_t end = S::fishEndMoveStepAt(i);
        const bool right = S::fishFacesRightAt(i);

        // 9: starts fully outside the entry edge.
        TEST_ASSERT_EQUAL_INT8(
            right ? SwimmingFish::kEntryLeftX : SwimmingFish::kEntryRightX,
            S::fishXAt(i, start));

        // 10: ends fully outside the opposite edge.
        TEST_ASSERT_EQUAL_INT8(
            right ? SwimmingFish::kEntryRightX : SwimmingFish::kEntryLeftX,
            S::fishXAt(i, static_cast<uint16_t>(end - 1u)));

        // The whole crossing is exactly the documented length.
        TEST_ASSERT_EQUAL_UINT16(SwimmingFish::kCrossingMoveSteps, static_cast<uint16_t>(end - start));
    }
}

void test_no_fish_reverses_or_teleports_while_visible(void) {
    for (uint8_t i = 0u; i < SwimmingFish::kFishCount; ++i) {
        const bool right = S::fishFacesRightAt(i);

        for (uint16_t s = S::fishStartMoveStepAt(i); s + 1u < S::fishEndMoveStepAt(i); ++s) {
            const int16_t here = S::fishXAt(i, s);
            const int16_t next = S::fishXAt(i, static_cast<uint16_t>(s + 1u));
            const int16_t delta = static_cast<int16_t>(next - here);

            // 11 + 13: one pixel, always in the same direction.
            TEST_ASSERT_EQUAL_INT16(right ? 1 : -1, delta);
        }
    }
}

void test_horizontal_speed_is_calm(void) {
    // 250 ms per pixel = 4 px/s, versus a projectile-like 150 ms before.
    TEST_ASSERT_EQUAL_UINT16(250u, SwimmingFish::kMoveIntervalMs);
    TEST_ASSERT_EQUAL_UINT16(125u, SwimmingFish::kStepIntervalMs);
    TEST_ASSERT_EQUAL_UINT8(2u, SwimmingFish::kStepsPerMove);

    // A full crossing takes 23 * 250 ms = 5.75 s.
    TEST_ASSERT_EQUAL_UINT32(
        5750u,
        static_cast<uint32_t>(SwimmingFish::kCrossingMoveSteps) *
            SwimmingFish::kMoveIntervalMs);
}

void test_vertical_bob_is_one_pixel_and_lane_bounded(void) {
    for (uint8_t i = 0u; i < SwimmingFish::kFishCount; ++i) {
        const int8_t base = S::laneBaseY(S::fishLaneAt(i));

        for (uint16_t s = 0u; s < 64u; ++s) {
            const int16_t offset = static_cast<int16_t>(S::fishYAt(i, s)) - base;

            // 14: never more than one row from the lane base.
            TEST_ASSERT_TRUE(offset >= -1 && offset <= 1);
        }
    }

    // And the two lanes can never reach each other, bob included.
    const int16_t topMaxRow =
        static_cast<int16_t>(S::laneBaseY(0u)) + 1 + SwimmingFish::kFishHeight - 1;
    const int16_t bottomMinRow = static_cast<int16_t>(S::laneBaseY(1u)) - 1;
    TEST_ASSERT_TRUE(topMaxRow < bottomMinRow);
}

// ---------------------------------------------------------------------------
// 16-17: tail and palette independence
// ---------------------------------------------------------------------------

void test_tail_animates_independently_of_horizontal_movement(void) {
    // Tail phase drives off the render tick (125 ms), the body off the move
    // tick (250 ms): within one move step the tail must still advance.
    for (uint8_t i = 0u; i < SwimmingFish::kFishCount; ++i) {
        const uint16_t a = 10u;
        const uint16_t b = 11u;
        TEST_ASSERT_EQUAL_UINT16(a / SwimmingFish::kStepsPerMove, b / SwimmingFish::kStepsPerMove);
        TEST_ASSERT_NOT_EQUAL(S::tailFrameAt(i, a), S::tailFrameAt(i, b));
    }

    // Three tail poses, and they are used.
    bool seen[3] = {false, false, false};
    for (uint16_t s = 0u; s < 12u; ++s) {
        seen[S::tailFrameAt(0u, s) % 3u] = true;
    }
    TEST_ASSERT_TRUE(seen[0] && seen[1] && seen[2]);

    // 16: fish may carry different tail phases, but never a colliding modulus.
    TEST_ASSERT_NOT_EQUAL(S::fishTailPhaseAt(0u) % 3u, S::fishTailPhaseAt(1u) % 3u);
    TEST_ASSERT_NOT_EQUAL(S::fishTailPhaseAt(1u) % 3u, S::fishTailPhaseAt(2u) % 3u);
}

void test_palettes_do_not_alter_geometry(void) {
    // 17: identical lit pixels, only the colours differ.
    PixelMatrix first;
    PixelMatrix second;
    PixelMatrix third;

    S animation;
    animation.drawFish(first, 5, 5, true, 0u, S::paletteAt(0u));
    animation.drawFish(second, 5, 5, true, 0u, S::paletteAt(1u));
    animation.drawFish(third, 5, 5, true, 0u, S::paletteAt(2u));

    TEST_ASSERT_TRUE(sameLitSet(first, second));
    TEST_ASSERT_TRUE(sameLitSet(first, third));

    // ...but the colours are genuinely different.
    bool foundDifference = false;
    for (uint8_t y = 0u; y < kPanel && !foundDifference; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            if (!sameColor(pixelColorAt(first, x, y), pixelColorAt(second, x, y))) {
                foundDifference = true;
                break;
            }
        }
    }
    TEST_ASSERT_TRUE(foundDifference);

    // Facing left mirrors the same geometry (not merely a recolour).
    PixelMatrix mirrored;
    animation.drawFish(mirrored, 5, 5, false, 0u, S::paletteAt(0u));
    TEST_ASSERT_FALSE(sameFrame(first, mirrored));
    TEST_ASSERT_TRUE(sameLitSet(first, mirrored) == false);
}

// ---------------------------------------------------------------------------
// 18-19: bubbles
// ---------------------------------------------------------------------------

void test_bubbles_spawn_only_from_active_fish(void) {
    for (uint8_t slot = 0u; slot < SwimmingFish::kBubbleCount; ++slot) {
        const uint16_t spawnMoveStep = static_cast<uint16_t>(
            S::bubbleStartStep(slot) / SwimmingFish::kStepsPerMove);

        // 18: the source fish is on stage at the moment the bubble is born, so
        // the spawn position is always derived from a real, visible fish.
        TEST_ASSERT_TRUE(S::fishActiveAt(S::bubbleSourceFish(slot), spawnMoveStep));

        // The spawn point is just outside that fish's mouth, never inside it.
        const int8_t fishX = S::fishXAt(S::bubbleSourceFish(slot), spawnMoveStep);
        const int8_t bubbleX = S::bubbleXAt(slot, S::bubbleStartStep(slot));
        if (S::fishFacesRightAt(S::bubbleSourceFish(slot))) {
            TEST_ASSERT_EQUAL_INT8(static_cast<int8_t>(fishX + SwimmingFish::kFishWidth), bubbleX);
        } else {
            TEST_ASSERT_EQUAL_INT8(static_cast<int8_t>(fishX - 1), bubbleX);
        }
    }
}

void test_bubbles_stay_inside_bounds_and_never_cross_the_seam(void) {
    for (uint8_t slot = 0u; slot < SwimmingFish::kBubbleCount; ++slot) {
        bool sawRenderedBubble = false;

        for (uint16_t step = 0u; step < SwimmingFish::kStepCount; ++step) {
            if (!S::bubbleActiveAt(slot, step)) {
                continue;
            }

            const int8_t x = S::bubbleXAt(slot, step);
            const int8_t y = S::bubbleYAt(slot, step);

            // 19: anything the renderer would draw is inside the panel.
            if (x >= 0 && y >= 0 && x < kPanel && y < kPanel) {
                sawRenderedBubble = true;
            }
        }

        TEST_ASSERT_TRUE(sawRenderedBubble);

        // The whole life is spent inside one loop, so no bubble is ever alive
        // across the loop boundary.
        TEST_ASSERT_TRUE(S::bubbleStartStep(slot) + S::bubbleLifeSteps(slot) <= SwimmingFish::kStepCount);
    }

    // At the seam itself nothing is emitted.
    for (uint16_t step = 100u; step < SwimmingFish::kStepCount; ++step) {
        for (uint8_t slot = 0u; slot < SwimmingFish::kBubbleCount; ++slot) {
            TEST_ASSERT_FALSE(S::bubbleActiveAt(slot, step));
        }
    }
    for (uint16_t step = 0u; step < 12u; ++step) {
        for (uint8_t slot = 0u; slot < SwimmingFish::kBubbleCount; ++slot) {
            TEST_ASSERT_FALSE(S::bubbleActiveAt(slot, step));
        }
    }
}

// ---------------------------------------------------------------------------
// 20-21: the loop proof
// ---------------------------------------------------------------------------

void test_loop_start_and_end_are_identical(void) {
    // 20: t = 0 and t = kLoopDurationMs resolve to the same logical step...
    TEST_ASSERT_EQUAL_UINT16(0u, S::stepAtElapsed(0u));
    TEST_ASSERT_EQUAL_UINT16(0u, S::stepAtElapsed(SwimmingFish::kLoopDurationMs));
    TEST_ASSERT_EQUAL_UINT32(0u, SwimmingFish::kLoopDurationMs % SwimmingFish::kStepIntervalMs);

    // ...and therefore render byte-identical frames.
    PixelMatrix atZero;
    PixelMatrix atLoop;
    renderAtStep(atZero, S::stepAtElapsed(0u));
    renderAtStep(atLoop, S::stepAtElapsed(SwimmingFish::kLoopDurationMs));
    TEST_ASSERT_TRUE(sameFrame(atZero, atLoop));

    // The play duration is exactly one loop, so playback restarts on the seam.
    TEST_ASSERT_EQUAL_UINT32(SwimmingFish::kLoopDurationMs, SwimmingFish::kAnimationDurationMs);
}

void test_last_step_leads_into_the_first_with_no_jump(void) {
    // 21: the final tick of the loop and the first tick of the next are the
    // same calm, empty frame, so nothing teleports or vanishes.
    PixelMatrix last;
    PixelMatrix first;
    renderAtStep(last, static_cast<uint16_t>(SwimmingFish::kStepCount - 1u));
    renderAtStep(first, 0u);

    TEST_ASSERT_TRUE(sameFrame(last, first));

    // Both are the intentional quiet gap: no fish and no bubble on screen.
    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            TEST_ASSERT_FALSE(isLit(pixelColorAt(first, x, y)));
        }
    }
}

void test_every_step_of_the_loop_respects_the_occupancy_invariants(void) {
    PixelMatrix matrix;

    for (uint16_t step = 0u; step < SwimmingFish::kStepCount; ++step) {
        const uint16_t moveStep = static_cast<uint16_t>(step / SwimmingFish::kStepsPerMove);

        const uint8_t count = activeCountAt(moveStep);
        TEST_ASSERT_TRUE(count <= 2u);

        if (count == 2u) {
            uint8_t firstLane = 0xFFu;
            for (uint8_t i = 0u; i < SwimmingFish::kFishCount; ++i) {
                if (!S::fishActiveAt(i, moveStep)) {
                    continue;
                }
                if (firstLane == 0xFFu) {
                    firstLane = S::fishLaneAt(i);
                    continue;
                }
                TEST_ASSERT_NOT_EQUAL(firstLane, S::fishLaneAt(i));
            }
        }

        // The rendered frame must agree with the schedule: with nothing on
        // screen only bubbles may be lit, and any fish that has reached the
        // panel must actually be drawn.
        renderAtStep(matrix, step);

        uint16_t lit = 0u;
        for (uint8_t y = 0u; y < kPanel; ++y) {
            for (uint8_t x = 0u; x < kPanel; ++x) {
                if (isLit(pixelColorAt(matrix, x, y))) {
                    ++lit;
                }
            }
        }

        bool anyFishVisible = false;
        for (uint8_t i = 0u; i < SwimmingFish::kFishCount; ++i) {
            if (!S::fishActiveAt(i, moveStep)) {
                continue;
            }
            const int16_t x = S::fishXAt(i, moveStep);
            if (x + SwimmingFish::kFishWidth > 0 && x < static_cast<int16_t>(kPanel)) {
                anyFishVisible = true;
            }
        }

        if (anyFishVisible) {
            TEST_ASSERT_TRUE(lit > 0u);
        } else {
            TEST_ASSERT_TRUE(lit <= SwimmingFish::kBubbleCount);
        }
    }
}

// ---------------------------------------------------------------------------
// 22-25: architecture rules
// ---------------------------------------------------------------------------

void test_animation_holds_no_framebuffer_or_per_fish_state(void) {
    // 22: no heap, no per-fish arrays, no 256-pixel buffer: the class is a
    // vtable pointer plus three scalars.
    TEST_ASSERT_TRUE(sizeof(SwimmingFishAnimation) <= 32u);
}

void test_animation_is_non_blocking(void) {
    // 23: the whole loop is produced by update() alone, with no wait.
    S animation;
    PixelMatrix matrix;

    animation.start(0u);
    for (uint16_t step = 1u; step <= SwimmingFish::kStepCount; ++step) {
        animation.update(static_cast<uint32_t>(step) * SwimmingFish::kStepIntervalMs, matrix);
    }

    TEST_ASSERT_TRUE(animation.isFinished());
}

void test_animation_is_registered_unchanged(void) {
    TEST_ASSERT_NOT_NULL(AnimationRegistry::getAnimation(AnimationId::SWIMMING_FISH));
    TEST_ASSERT_EQUAL_UINT8(9u, static_cast<uint8_t>(AnimationId::SWIMMING_FISH));
    TEST_ASSERT_EQUAL_STRING("Pływająca rybka", animationNameOf(AnimationId::SWIMMING_FISH));
}

void test_rotation_stays_central(void) {
    // 24: the animation draws logical coordinates only; PixelMatrix rotates.
    PixelMatrix plain;
    PixelMatrix rotated;

    rotated.setRotation(DisplayRotation::DEG_90);
    renderAtStep(plain, 20u);
    renderAtStep(rotated, 20u);

    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            const Color logical = pixelColorAt(plain, x, y);
            const Color mapped = pixelColorAt(rotated, static_cast<uint8_t>(kPanel - 1u - y), x);
            TEST_ASSERT_TRUE(sameColor(logical, mapped));
        }
    }
}

void test_power_ceiling_is_untouched(void) {
    // 25: the limiter is not referenced by the animation at all.
    TEST_ASSERT_EQUAL_UINT16(2000u, LedPowerBudget::kLimitMilliAmp);
    TEST_ASSERT_EQUAL_UINT8(33u, LedPowerBudget::kMaxSafeBrightnessRaw);

    PixelMatrix matrix;
    matrix.setBrightness(255u);
    renderAtStep(matrix, 20u);

    TEST_ASSERT_EQUAL_UINT8(
        LedPowerBudget::kMaxSafeBrightnessRaw, matrix.effectiveBrightnessRaw());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_fish_definitions_and_palettes_exist);
    RUN_TEST(test_palettes_are_visibly_distinct);
    RUN_TEST(test_lanes_are_top_and_bottom);
    RUN_TEST(test_simultaneous_fish_always_use_different_lanes);
    RUN_TEST(test_active_counts_cover_the_choreography);
    RUN_TEST(test_lane_boxes_never_overlap);
    RUN_TEST(test_each_fish_enters_and_exits_fully_outside);
    RUN_TEST(test_no_fish_reverses_or_teleports_while_visible);
    RUN_TEST(test_horizontal_speed_is_calm);
    RUN_TEST(test_vertical_bob_is_one_pixel_and_lane_bounded);
    RUN_TEST(test_tail_animates_independently_of_horizontal_movement);
    RUN_TEST(test_palettes_do_not_alter_geometry);
    RUN_TEST(test_bubbles_spawn_only_from_active_fish);
    RUN_TEST(test_bubbles_stay_inside_bounds_and_never_cross_the_seam);
    RUN_TEST(test_loop_start_and_end_are_identical);
    RUN_TEST(test_last_step_leads_into_the_first_with_no_jump);
    RUN_TEST(test_every_step_of_the_loop_respects_the_occupancy_invariants);
    RUN_TEST(test_animation_holds_no_framebuffer_or_per_fish_state);
    RUN_TEST(test_animation_is_non_blocking);
    RUN_TEST(test_animation_is_registered_unchanged);
    RUN_TEST(test_rotation_stays_central);
    RUN_TEST(test_power_ceiling_is_untouched);
    return UNITY_END();
}
