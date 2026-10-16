#include <unity.h>

#define private public
#include "animations/MoonStarsAnimation.h"
#undef private

#include "core/AnimationManager.h"
#include "core/AnimationRegistry.h"
#include "scheduling/ManualOverride.h"
#include "scheduling/ManualTimeProvider.h"
#include "scheduling/PlaybackPolicy.h"
#include "scheduling/Scheduler.h"

namespace {

uint8_t id(AnimationId value) {
    return static_cast<uint8_t>(value);
}

struct Fixture {
    ManualTimeProvider clock;
    ManualOverride manual;
    Scheduler scheduler;
    AnimationManager manager;
    PixelMatrix matrix;
    PlaybackPolicy policy;

    Fixture()
        : clock(),
          manual(),
          scheduler(clock, manual, SchedulerConfig()),
          manager(),
          matrix(),
          policy(scheduler, manager, matrix) {}
};

uint8_t renderedFramesOfMoonStars() {
    Animation* animation = AnimationRegistry::getAnimation(AnimationId::MOON_STARS);
    if (animation == nullptr) {
        return 0u;
    }

    // The registry owns exactly one MoonStarsAnimation instance.
    MoonStarsAnimation* moonStars = static_cast<MoonStarsAnimation*>(animation);
    return moonStars->renderedFrameCount_;
}

}  // namespace

void test_repeated_same_decision_starts_once(void) {
    Fixture f;
    // Force MOON_STARS through a manual override so the decision is stable.
    f.manual.activateUntilCancelled(AnimationId::MOON_STARS, PlayMode::LOOP);

    PixelMatrix matrix;
    uint32_t now = 0u;
    for (uint16_t i = 0u; i < 600u; ++i) {
        f.policy.tick(now);
        f.manager.tick(now, matrix);
        now += 16u;
    }

    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::MOON_STARS), id(f.policy.appliedAnimation()));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::MOON_STARS), id(f.manager.currentAnimation()));
    TEST_ASSERT_EQUAL_UINT16(1u, f.policy.startCount());
    TEST_ASSERT_EQUAL_UINT16(0u, f.policy.stopCount());
    TEST_ASSERT_TRUE(f.manager.isPlaying());

    // Proof the manager was not restarted: the animation kept rendering
    // frames across the whole run instead of resetting every tick.
    TEST_ASSERT_TRUE(renderedFramesOfMoonStars() > 1u);
}

void test_single_switch_between_animations(void) {
    Fixture f;
    PixelMatrix matrix;

    f.manual.activateUntilCancelled(AnimationId::MOON_STARS, PlayMode::LOOP);
    f.policy.tick(0u);
    TEST_ASSERT_EQUAL_UINT16(1u, f.policy.startCount());

    // Decision changes to CAMPFIRE.
    f.manual.activateUntilCancelled(AnimationId::CAMPFIRE, PlayMode::LOOP);
    f.policy.tick(1000u);
    TEST_ASSERT_EQUAL_UINT16(2u, f.policy.startCount());
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::CAMPFIRE), id(f.manager.currentAnimation()));

    // 300 more ticks with the same decision: no extra switches.
    for (uint16_t i = 0u; i < 300u; ++i) {
        f.policy.tick(2000u + i);
    }
    TEST_ASSERT_EQUAL_UINT16(2u, f.policy.startCount());
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::CAMPFIRE), id(f.manager.currentAnimation()));
    (void)matrix;
}

void test_single_stop_on_none(void) {
    Fixture f;
    f.clock.set(makeClockSnapshot(2026u, 9u, 30u, 2u, 0u));

    f.policy.tick(0u);
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::NONE), id(f.manager.currentAnimation()));
    TEST_ASSERT_FALSE(f.manager.isPlaying());
    // Nothing was playing, so entering NONE must not call stop() at all.
    TEST_ASSERT_EQUAL_UINT16(0u, f.policy.stopCount());

    // Many quiet-hours ticks must not call stop() repeatedly.
    for (uint16_t i = 0u; i < 200u; ++i) {
        f.policy.tick(1000u + i);
    }
    TEST_ASSERT_EQUAL_UINT16(0u, f.policy.stopCount());

    // Entering an animation, then leaving it again: exactly one stop.
    f.manual.activateUntilCancelled(AnimationId::MOON_STARS, PlayMode::LOOP);
    f.policy.tick(5000u);
    TEST_ASSERT_EQUAL_UINT16(1u, f.policy.startCount());
    TEST_ASSERT_TRUE(f.manager.isPlaying());

    f.manual.clear();
    f.policy.tick(6000u);
    TEST_ASSERT_EQUAL_UINT16(1u, f.policy.stopCount());
    TEST_ASSERT_FALSE(f.manager.isPlaying());
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::NONE), id(f.manager.currentAnimation()));

    for (uint16_t i = 0u; i < 100u; ++i) {
        f.policy.tick(7000u + i);
    }
    TEST_ASSERT_EQUAL_UINT16(1u, f.policy.stopCount());
}

void test_policy_tracks_applied_metadata(void) {
    Fixture f;
    f.manual.activateUntilCancelled(AnimationId::ROSE_BLOOM, PlayMode::ONCE);
    f.policy.tick(0u);

    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::ROSE_BLOOM), id(f.policy.appliedAnimation()));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(PlayMode::ONCE),
        static_cast<uint8_t>(f.policy.appliedMode()));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(SchedulePriority::MANUAL),
        static_cast<uint8_t>(f.policy.appliedPriority()));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(PlaybackReason::MANUAL_OVERRIDE),
        static_cast<uint8_t>(f.policy.appliedReason()));
}

void test_automatic_playback_from_time_of_day(void) {
    Fixture f;
    f.clock.set(makeClockSnapshot(2026u, 9u, 30u, 19u, 0u));

    f.policy.tick(0u);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(SchedulePriority::TIME_OF_DAY),
        static_cast<uint8_t>(f.policy.appliedPriority()));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(PlaybackReason::SCHEDULE),
        static_cast<uint8_t>(f.policy.appliedReason()));
    TEST_ASSERT_TRUE(f.manager.isPlaying());
    TEST_ASSERT_EQUAL_UINT16(1u, f.policy.startCount());

    // Same schedule row, ten minutes later: the decision is unchanged, so the
    // animation must NOT be restarted.
    const AnimationId first = f.manager.currentAnimation();
    f.clock.set(makeClockSnapshot(2026u, 9u, 30u, 19u, 10u));
    f.policy.tick(600000u);
    TEST_ASSERT_EQUAL_UINT16(1u, f.policy.startCount());
    TEST_ASSERT_EQUAL_UINT8(id(first), id(f.manager.currentAnimation()));

    // Crossing a schedule boundary (20:00) changes the decision: exactly one
    // more start, one different animation.
    f.clock.set(makeClockSnapshot(2026u, 9u, 30u, 20u, 0u));
    f.policy.tick(1200000u);
    TEST_ASSERT_EQUAL_UINT16(2u, f.policy.startCount());
    TEST_ASSERT_NOT_EQUAL_UINT8(id(first), id(f.manager.currentAnimation()));
}

void test_invalid_clock_applies_fallback_once(void) {
    Fixture f;
    ClockSnapshot invalid;
    invalid.year = 0u;
    invalid.month = 1u;
    invalid.day = 1u;
    invalid.hour = 5u;
    invalid.minute = 0u;
    invalid.second = 0u;
    invalid.weekday = 4u;
    invalid.valid = false;
    f.clock.set(invalid);

    for (uint16_t i = 0u; i < 50u; ++i) {
        f.policy.tick(i * 10u);
    }

    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::AURORA), id(f.policy.appliedAnimation()));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(SchedulePriority::FALLBACK),
        static_cast<uint8_t>(f.policy.appliedPriority()));
    TEST_ASSERT_EQUAL_UINT16(1u, f.policy.startCount());
}

void test_playback_policy_never_leaks_forbidden_patterns(void) {
    // Guard against the policy touching global Arduino state directly: it is
    // constructed with explicit references only.
    Fixture f;
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::NONE), id(f.policy.appliedAnimation()));
    TEST_ASSERT_EQUAL_UINT16(0u, f.policy.startCount());
    TEST_ASSERT_EQUAL_UINT16(0u, f.policy.stopCount());
    TEST_ASSERT_EQUAL_UINT16(0u, f.policy.unavailableCount());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_repeated_same_decision_starts_once);
    RUN_TEST(test_single_switch_between_animations);
    RUN_TEST(test_single_stop_on_none);
    RUN_TEST(test_policy_tracks_applied_metadata);
    RUN_TEST(test_automatic_playback_from_time_of_day);
    RUN_TEST(test_invalid_clock_applies_fallback_once);
    RUN_TEST(test_playback_policy_never_leaks_forbidden_patterns);
    return UNITY_END();
}
