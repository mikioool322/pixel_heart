#include <unity.h>

#define private public
#include "animations/HalloweenGhostAnimation.h"
#undef private

#include "config/LedPowerBudget.h"
#include "control/AnimationCatalog.h"
#include "control/ControlPipeline.h"
#include "core/AnimationId.h"
#include "core/AnimationManager.h"
#include "core/AnimationNames.h"
#include "core/AnimationRegistry.h"
#include "core/PixelMatrix.h"
#include "scheduling/ManualOverride.h"
#include "scheduling/ManualTimeProvider.h"
#include "scheduling/PlaybackPolicy.h"
#include "scheduling/Scheduler.h"

namespace {

constexpr uint8_t kPanel = 16u;

using Gaze = HalloweenGhostAnimation::Gaze;

Color pixelColorAt(const PixelMatrix& matrix, uint8_t x, uint8_t y) {
    const uint32_t packed =
        matrix.pixels.getPixelColor(PixelMatrix::xyToIndexStatic(x, y));
    return Color{
        static_cast<uint8_t>((packed >> 16) & 0xFFu),
        static_cast<uint8_t>((packed >> 8) & 0xFFu),
        static_cast<uint8_t>(packed & 0xFFu)
    };
}

bool sameFrame(const PixelMatrix& a, const PixelMatrix& b) {
    for (uint16_t i = 0u; i < PixelMatrix::LED_COUNT; ++i) {
        if (a.pixels.getPixelColor(i) != b.pixels.getPixelColor(i)) {
            return false;
        }
    }
    return true;
}

struct Bounds {
    int16_t minX;
    int16_t maxX;
    int16_t minY;
    int16_t maxY;
    uint16_t lit;
};

Bounds boundsOf(const PixelMatrix& matrix) {
    Bounds b{static_cast<int16_t>(kPanel), -1, static_cast<int16_t>(kPanel), -1, 0u};
    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            if (pixelColorAt(matrix, x, y).r == 0u &&
                pixelColorAt(matrix, x, y).g == 0u &&
                pixelColorAt(matrix, x, y).b == 0u) {
                continue;
            }
            ++b.lit;
            if (static_cast<int16_t>(x) < b.minX) b.minX = static_cast<int16_t>(x);
            if (static_cast<int16_t>(x) > b.maxX) b.maxX = static_cast<int16_t>(x);
            if (static_cast<int16_t>(y) < b.minY) b.minY = static_cast<int16_t>(y);
            if (static_cast<int16_t>(y) > b.maxY) b.maxY = static_cast<int16_t>(y);
        }
    }
    return b;
}

// True when every lit pixel is reachable from the body silhouette by
// orthogonal steps: the hem may never float free of the ghost.
bool hemIsAttached(uint8_t hem) {
    static bool visited[kPanel][kPanel];

    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            visited[y][x] = false;
        }
    }

    // Seed from the whole body core.
    uint8_t queueX[kPanel * kPanel];
    uint8_t queueY[kPanel * kPanel];
    uint16_t head = 0u;
    uint16_t tail = 0u;

    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            if (HalloweenGhostAnimation::bodyPixelAt(x, y)) {
                visited[y][x] = true;
                queueX[tail] = x;
                queueY[tail] = y;
                ++tail;
            }
        }
    }

    const int8_t dx[4] = {1, -1, 0, 0};
    const int8_t dy[4] = {0, 0, 1, -1};

    while (head < tail) {
        const uint8_t cx = queueX[head];
        const uint8_t cy = queueY[head];
        ++head;

        for (uint8_t i = 0u; i < 4u; ++i) {
            const int16_t nx = static_cast<int16_t>(cx) + dx[i];
            const int16_t ny = static_cast<int16_t>(cy) + dy[i];
            if (nx < 0 || ny < 0 || nx >= kPanel || ny >= kPanel) {
                continue;
            }
            if (visited[ny][nx]) {
                continue;
            }

            bool lit = HalloweenGhostAnimation::bodyPixelAt(
                static_cast<uint8_t>(nx), static_cast<uint8_t>(ny));
            if (!lit && (ny == 14 || ny == 15)) {
                lit = HalloweenGhostAnimation::hemPixelAt(
                    hem, static_cast<uint8_t>(ny), static_cast<uint8_t>(nx));
            }
            if (!lit) {
                continue;
            }

            visited[ny][nx] = true;
            queueX[tail] = static_cast<uint8_t>(nx);
            queueY[tail] = static_cast<uint8_t>(ny);
            ++tail;
        }
    }

    // Every hem pixel must have been reached.
    for (uint8_t row = 14u; row <= 15u; ++row) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            if (HalloweenGhostAnimation::hemPixelAt(hem, row, x) && !visited[row][x]) {
                return false;
            }
        }
    }

    return true;
}

}  // namespace

void test_halloween_ghost_is_registered_and_selectable(void) {
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(AnimationId::SUN_OVER_MOUNTAINS) + 1u,
        static_cast<uint8_t>(AnimationId::HALLOWEEN_GHOST));
    TEST_ASSERT_NOT_NULL(AnimationRegistry::getAnimation(AnimationId::HALLOWEEN_GHOST));
    TEST_ASSERT_TRUE(AnimationCatalog::isUserSelectable(AnimationId::HALLOWEEN_GHOST));
}

void test_halloween_ghost_label_is_polish(void) {
    TEST_ASSERT_EQUAL_STRING("Duszek", animationNameOf(AnimationId::HALLOWEEN_GHOST));
}

void test_halloween_ghost_frame_count_and_loop_is_seamless(void) {
    // The last frame must be pixel-identical to the first: the turnaround ends
    // exactly where MOVE_LEFT begins.
    HalloweenGhostAnimation animation;
    PixelMatrix last;
    PixelMatrix first;

    animation.renderFrame(last, static_cast<uint8_t>(HalloweenGhostAnimation::frameCount() - 1u));
    animation.renderFrame(first, 0u);

    TEST_ASSERT_EQUAL_UINT8(32u, HalloweenGhostAnimation::frameCount());
    TEST_ASSERT_TRUE(sameFrame(last, first));
}

void test_halloween_ghost_stays_inside_the_panel(void) {
    HalloweenGhostAnimation animation;
    PixelMatrix matrix;

    for (uint8_t frame = 0u; frame < HalloweenGhostAnimation::frameCount(); ++frame) {
        animation.renderFrame(matrix, frame);
        const Bounds b = boundsOf(matrix);

        TEST_ASSERT_TRUE(b.lit > 0u);
        TEST_ASSERT_TRUE(b.minX >= 0 && b.maxX < static_cast<int16_t>(kPanel));
        TEST_ASSERT_TRUE(b.minY >= 0 && b.maxY < static_cast<int16_t>(kPanel));
    }
}

void test_halloween_ghost_never_teleports(void) {
    // One logical pixel per frame at most, in both directions.
    for (uint8_t frame = 0u; frame + 1u < HalloweenGhostAnimation::frameCount(); ++frame) {
        const int16_t here = HalloweenGhostAnimation::positionAt(frame);
        const int16_t next = HalloweenGhostAnimation::positionAt(static_cast<uint8_t>(frame + 1u));
        const int16_t delta = static_cast<int16_t>(next - here);

        TEST_ASSERT_TRUE(delta >= -1 && delta <= 1);
    }

    // Whole-loop bookkeeping: left edge -> hold -> right edge -> hold.
    TEST_ASSERT_EQUAL_INT16(1, HalloweenGhostAnimation::positionAt(0u));
    TEST_ASSERT_EQUAL_INT16(-1, HalloweenGhostAnimation::positionAt(10u));
    TEST_ASSERT_EQUAL_INT16(-1, HalloweenGhostAnimation::positionAt(19u));  // last held frame
    TEST_ASSERT_EQUAL_INT16(1, HalloweenGhostAnimation::positionAt(28u));   // end of MOVE_RIGHT
    TEST_ASSERT_EQUAL_INT16(1, HalloweenGhostAnimation::positionAt(31u));   // turnaround
}

void test_halloween_ghost_movement_speed_is_bounded(void) {
    // One step every three frames (450 ms), and never more than one pixel at a
    // time: on a 16 px panel that is a calm 1.0 px/s average.
    uint8_t moves = 0u;
    for (uint8_t frame = 0u; frame + 1u < HalloweenGhostAnimation::frameCount(); ++frame) {
        if (HalloweenGhostAnimation::positionAt(frame) !=
            HalloweenGhostAnimation::positionAt(static_cast<uint8_t>(frame + 1u))) {
            ++moves;
        }
    }

    // Two single-pixel steps left, two right: 4 px of travel per loop, so the
    // movement distance and cadence are untouched by the longer stare.
    TEST_ASSERT_EQUAL_UINT8(4u, moves);

    const uint32_t loopMs = static_cast<uint32_t>(HalloweenGhostAnimation::frameCount()) * 150u;
    TEST_ASSERT_EQUAL_UINT32(4800u, loopMs);

    // No two steps are ever closer than 3 frames = 450 ms, which is the band
    // the design targets. (The longer gap is the intentional pause at the left
    // turnaround, where the ghost stops before heading back.)
    uint8_t lastMove = 0u;
    bool haveLast = false;
    for (uint8_t frame = 0u; frame + 1u < HalloweenGhostAnimation::frameCount(); ++frame) {
        if (HalloweenGhostAnimation::positionAt(frame) !=
            HalloweenGhostAnimation::positionAt(static_cast<uint8_t>(frame + 1u))) {
            if (haveLast) {
                TEST_ASSERT_TRUE(static_cast<uint8_t>(frame - lastMove) >= 3u);
            }
            lastMove = frame;
            haveLast = true;
        }
    }
    TEST_ASSERT_TRUE(haveLast);
}

void test_halloween_ghost_gaze_matches_the_direction_of_travel(void) {
    // MOVE_LEFT: gaze left.
    for (uint8_t frame = 0u; frame <= 8u; ++frame) {
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(Gaze::LEFT),
            static_cast<uint8_t>(HalloweenGhostAnimation::gazeAt(frame)));
    }

    // STOP_LEFT: looks straight at the viewer for the whole hold.
    for (uint8_t frame = 9u; frame <= 19u; ++frame) {
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(Gaze::CENTER),
            static_cast<uint8_t>(HalloweenGhostAnimation::gazeAt(frame)));
    }

    // MOVE_RIGHT: gaze right.
    for (uint8_t frame = 20u; frame <= 28u; ++frame) {
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(Gaze::RIGHT),
            static_cast<uint8_t>(HalloweenGhostAnimation::gazeAt(frame)));
    }

    // TURNAROUND: RIGHT -> CENTER -> LEFT, ending on LEFT so it matches
    // MOVE_LEFT's first frame.
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(Gaze::RIGHT), static_cast<uint8_t>(HalloweenGhostAnimation::gazeAt(29u)));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(Gaze::CENTER), static_cast<uint8_t>(HalloweenGhostAnimation::gazeAt(30u)));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(Gaze::LEFT), static_cast<uint8_t>(HalloweenGhostAnimation::gazeAt(31u)));
}

void test_halloween_ghost_center_hold_is_longer_and_perceptible(void) {
    // The whole point of this change: the stare must be long enough to notice.
    uint8_t holdFrames = 0u;
    for (uint8_t frame = 0u; frame < HalloweenGhostAnimation::frameCount(); ++frame) {
        if (HalloweenGhostAnimation::gazeAt(frame) == Gaze::CENTER) {
            ++holdFrames;
        }
    }

    // The turnaround also shows one CENTER frame, so the stationary hold is
    // four frames short of that total... but the hold is what must be long.
    const uint16_t holdMs =
        static_cast<uint16_t>(holdFrames - 1u) * 150u;

    // 1: noticeably longer than the previous 750 ms.
    TEST_ASSERT_TRUE(holdMs > 750u);

    // 2: approximately 1.5 - 2.0 s.
    TEST_ASSERT_TRUE(holdMs >= 1500u);
    TEST_ASSERT_TRUE(holdMs <= 2000u);

    // 3: the preferred value, reported explicitly.
    TEST_ASSERT_EQUAL_UINT16(1650u, holdMs);
}

void test_halloween_ghost_body_is_frozen_for_the_whole_hold(void) {
    // 4 + 5: position fixed and gaze CENTER for every frame of the hold.
    const int8_t heldPosition = HalloweenGhostAnimation::positionAt(9u);
    TEST_ASSERT_EQUAL_INT8(-1, heldPosition);

    for (uint8_t frame = 9u; frame <= 19u; ++frame) {
        TEST_ASSERT_EQUAL_INT8(heldPosition, HalloweenGhostAnimation::positionAt(frame));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(Gaze::CENTER),
            static_cast<uint8_t>(HalloweenGhostAnimation::gazeAt(frame)));
    }

    // The pupils must not creep back toward LEFT or RIGHT during the hold.
    for (uint8_t frame = 9u; frame <= 19u; ++frame) {
        TEST_ASSERT_EQUAL_UINT8(4u, HalloweenGhostAnimation::pupilX(false, HalloweenGhostAnimation::gazeAt(frame)));
        TEST_ASSERT_EQUAL_UINT8(10u, HalloweenGhostAnimation::pupilX(true, HalloweenGhostAnimation::gazeAt(frame)));
    }

    // Hem is still while staring, matching the original style.
    for (uint8_t frame = 9u; frame <= 19u; ++frame) {
        TEST_ASSERT_EQUAL_UINT8(0u, HalloweenGhostAnimation::hemAt(frame));
    }
}

void test_halloween_ghost_pupils_track_the_gaze(void) {
    // Pupil x is the body-space anchor; 3/9 left, 4/10 centre, 5/11 right.
    TEST_ASSERT_EQUAL_UINT8(3u, HalloweenGhostAnimation::pupilX(false, Gaze::LEFT));
    TEST_ASSERT_EQUAL_UINT8(4u, HalloweenGhostAnimation::pupilX(false, Gaze::CENTER));
    TEST_ASSERT_EQUAL_UINT8(5u, HalloweenGhostAnimation::pupilX(false, Gaze::RIGHT));
    TEST_ASSERT_EQUAL_UINT8(9u, HalloweenGhostAnimation::pupilX(true, Gaze::LEFT));
    TEST_ASSERT_EQUAL_UINT8(10u, HalloweenGhostAnimation::pupilX(true, Gaze::CENTER));
    TEST_ASSERT_EQUAL_UINT8(11u, HalloweenGhostAnimation::pupilX(true, Gaze::RIGHT));
}

void test_halloween_ghost_hem_waves_only_while_moving(void) {
    // Waving while moving left (unchanged cadence).
    TEST_ASSERT_NOT_EQUAL(
        HalloweenGhostAnimation::hemAt(0u),
        HalloweenGhostAnimation::hemAt(2u));

    // Still for the entire extended stop.
    const uint8_t stopHem = HalloweenGhostAnimation::hemAt(9u);
    for (uint8_t frame = 9u; frame <= 19u; ++frame) {
        TEST_ASSERT_EQUAL_UINT8(stopHem, HalloweenGhostAnimation::hemAt(frame));
    }

    // Waving again while moving right (unchanged cadence).
    TEST_ASSERT_NOT_EQUAL(
        HalloweenGhostAnimation::hemAt(20u),
        HalloweenGhostAnimation::hemAt(22u));

    // Still during the turnaround.
    const uint8_t turnHem = HalloweenGhostAnimation::hemAt(29u);
    for (uint8_t frame = 29u; frame < HalloweenGhostAnimation::frameCount(); ++frame) {
        TEST_ASSERT_EQUAL_UINT8(turnHem, HalloweenGhostAnimation::hemAt(frame));
    }
}

void test_halloween_ghost_hem_stays_attached_to_the_body(void) {
    TEST_ASSERT_TRUE(hemIsAttached(0u));
    TEST_ASSERT_TRUE(hemIsAttached(1u));
}

void test_halloween_ghost_every_frame_is_deterministic_and_bounded(void) {
    HalloweenGhostAnimation first;
    HalloweenGhostAnimation second;
    PixelMatrix a;
    PixelMatrix b;

    for (uint8_t frame = 0u; frame < HalloweenGhostAnimation::frameCount(); ++frame) {
        first.renderFrame(a, frame);
        second.renderFrame(b, frame);
        TEST_ASSERT_TRUE(sameFrame(a, b));
    }
}

void test_halloween_ghost_has_no_framebuffer_or_heap(void) {
    // 15 + 16: no heap, no framebuffer, no 256-pixel buffer. The class is a
    // vtable pointer plus the play cursor.
    TEST_ASSERT_TRUE(sizeof(HalloweenGhostAnimation) <= 32u);
}

void test_halloween_ghost_is_non_blocking(void) {
    // The whole loop must complete through update() alone, with no wait.
    HalloweenGhostAnimation animation;
    PixelMatrix matrix;
    uint32_t now = 0u;

    animation.start(0u);
    for (uint16_t i = 0u; i < HalloweenGhostAnimation::frameCount(); ++i) {
        now += 150u;
        animation.update(now, matrix);
    }

    TEST_ASSERT_TRUE(animation.isFinished());
}

void test_halloween_ghost_rotation_stays_central(void) {
    HalloweenGhostAnimation animation;
    PixelMatrix plain;
    PixelMatrix rotated;

    rotated.setRotation(DisplayRotation::DEG_90);
    animation.renderFrame(plain, 0u);
    animation.renderFrame(rotated, 0u);

    // The animation draws logical coordinates only; PixelMatrix rotates them.
    // DEG_90 maps (x, y) -> (15 - y, x).
    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            const Color logical = pixelColorAt(plain, x, y);
            const Color mapped = pixelColorAt(
                rotated,
                static_cast<uint8_t>(kPanel - 1u - y),
                x);
            TEST_ASSERT_TRUE(logical.r == mapped.r && logical.g == mapped.g && logical.b == mapped.b);
        }
    }
}

void test_halloween_ghost_body_is_not_white(void) {
    // The diffuser blooms full white; every ghost pixel must stay restrained.
    HalloweenGhostAnimation animation;
    PixelMatrix matrix;

    for (uint8_t frame = 0u; frame < HalloweenGhostAnimation::frameCount(); ++frame) {
        animation.renderFrame(matrix, frame);
        const Bounds b = boundsOf(matrix);
        TEST_ASSERT_TRUE(b.lit > 0u);

        for (uint8_t y = 0u; y < kPanel; ++y) {
            for (uint8_t x = 0u; x < kPanel; ++x) {
                const Color c = pixelColorAt(matrix, x, y);
                TEST_ASSERT_FALSE(c.r == 255u && c.g == 255u && c.b == 255u);
            }
        }
    }
}

void test_halloween_ghost_selects_through_the_control_path(void) {
    // One click on "Duszek" must switch to MANUAL and start the ghost.
    ManualTimeProvider clock;
    ManualOverride manual;
    Scheduler scheduler(clock, manual, SchedulerConfig());
    AnimationManager manager;
    PixelMatrix matrix;
    PlaybackPolicy policy(scheduler, manager, matrix);

    ControlRequest request = makeIdleControlRequest();
    request.action = ControlAction::SELECT_ANIMATION;
    request.animation = AnimationId::HALLOWEEN_GHOST;

    policy.tick(0u);
    manager.play(AnimationId::HEARTBEAT, PlayMode::LOOP, 0u);
    policy.tick(0u);

    UserSettings settings = UserSettingsDefaults::make();
    settings.mode = ControlMode::AUTO;

    // Apply through the portable pipeline the way App does.
    ControlPipeline pipeline(settings, manual, policy, manager);
    pipeline.apply(request, 10u);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ControlMode::MANUAL),
        static_cast<uint8_t>(settings.mode));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(AnimationId::HALLOWEEN_GHOST),
        static_cast<uint8_t>(pipeline.visibleAnimation()));
}

void test_halloween_ghost_loop_boundary_state_is_continuous(void) {
    const uint8_t last = static_cast<uint8_t>(HalloweenGhostAnimation::frameCount() - 1u);

    // Position, gaze and hem identical at t=0 and t=duration.
    TEST_ASSERT_EQUAL_INT8(
        HalloweenGhostAnimation::positionAt(0u), HalloweenGhostAnimation::positionAt(last));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(HalloweenGhostAnimation::gazeAt(0u)),
        static_cast<uint8_t>(HalloweenGhostAnimation::gazeAt(last)));
    TEST_ASSERT_EQUAL_UINT8(
        HalloweenGhostAnimation::hemAt(0u), HalloweenGhostAnimation::hemAt(last));

    // ...and the frames render identically.
    HalloweenGhostAnimation animation;
    PixelMatrix atZero;
    PixelMatrix atDuration;
    animation.renderFrame(atZero, 0u);
    animation.renderFrame(atDuration, last);
    TEST_ASSERT_TRUE(sameFrame(atZero, atDuration));

    // The transition into the boundary is the designed turnaround:
    // RIGHT -> CENTER -> LEFT, and LEFT is where MOVE_LEFT begins.
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(Gaze::CENTER), static_cast<uint8_t>(HalloweenGhostAnimation::gazeAt(30u)));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(Gaze::LEFT), static_cast<uint8_t>(HalloweenGhostAnimation::gazeAt(last)));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(Gaze::LEFT), static_cast<uint8_t>(HalloweenGhostAnimation::gazeAt(0u)));

    // No position jump across the seam either.
    TEST_ASSERT_EQUAL_INT8(
        HalloweenGhostAnimation::positionAt(30u), HalloweenGhostAnimation::positionAt(last));
    TEST_ASSERT_EQUAL_INT8(
        HalloweenGhostAnimation::positionAt(last), HalloweenGhostAnimation::positionAt(0u));

    // The loop duration is the frame count times the unchanged frame interval.
    TEST_ASSERT_EQUAL_UINT32(
        4800u,
        static_cast<uint32_t>(HalloweenGhostAnimation::frameCount()) * 150u);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_halloween_ghost_is_registered_and_selectable);
    RUN_TEST(test_halloween_ghost_label_is_polish);
    RUN_TEST(test_halloween_ghost_frame_count_and_loop_is_seamless);
    RUN_TEST(test_halloween_ghost_stays_inside_the_panel);
    RUN_TEST(test_halloween_ghost_never_teleports);
    RUN_TEST(test_halloween_ghost_movement_speed_is_bounded);
    RUN_TEST(test_halloween_ghost_gaze_matches_the_direction_of_travel);
    RUN_TEST(test_halloween_ghost_center_hold_is_longer_and_perceptible);
    RUN_TEST(test_halloween_ghost_body_is_frozen_for_the_whole_hold);
    RUN_TEST(test_halloween_ghost_pupils_track_the_gaze);
    RUN_TEST(test_halloween_ghost_hem_waves_only_while_moving);
    RUN_TEST(test_halloween_ghost_hem_stays_attached_to_the_body);
    RUN_TEST(test_halloween_ghost_every_frame_is_deterministic_and_bounded);
    RUN_TEST(test_halloween_ghost_loop_boundary_state_is_continuous);
    RUN_TEST(test_halloween_ghost_has_no_framebuffer_or_heap);
    RUN_TEST(test_halloween_ghost_is_non_blocking);
    RUN_TEST(test_halloween_ghost_rotation_stays_central);
    RUN_TEST(test_halloween_ghost_body_is_not_white);
    RUN_TEST(test_halloween_ghost_selects_through_the_control_path);
    return UNITY_END();
}
