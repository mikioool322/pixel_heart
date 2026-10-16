#include <unity.h>

#include <stdio.h>
#include <string.h>

#include "control/AnimationCatalog.h"
#include "control/ControlPipeline.h"
#include "control/PanelRenderer.h"
#include "core/AnimationId.h"
#include "core/AnimationManager.h"
#include "core/AnimationNames.h"
#include "scheduling/ClockSnapshot.h"
#include "scheduling/ManualTimeProvider.h"
#include "scheduling/Scheduler.h"

// Phase B regression: ONE authenticated selection request must start EVERY
// user-selectable animation, from both MANUAL and AUTO, with no second request.
//
// The production ordering rule this pins down is:
//
//     apply(request)  ->  settled playback  ->  publish panel  ->  serve HTTP
//
// ControlPipeline::apply() performs the first two steps as one unit, and App
// publishes the panel only afterwards. That is what makes the 303 redirect's
// GET render the animation the click just selected. When the panel was
// published *before* playback settled, the GET rendered the previous
// animation, which is exactly why the first click appeared to do nothing.

namespace {

constexpr uint8_t kAnimationCount =
    static_cast<uint8_t>(AnimationId::ANIMATION_ID_COUNT);

AnimationId nthUserSelectable(uint8_t nth) {
    uint8_t seen = 0u;
    for (uint8_t raw = 1u; raw < kAnimationCount; ++raw) {
        const AnimationId id = static_cast<AnimationId>(raw);
        if (!AnimationCatalog::isUserSelectable(id)) {
            continue;
        }
        if (seen == nth) {
            return id;
        }
        ++seen;
    }
    return AnimationId::NONE;
}

AnimationId userSelectableOtherThan(AnimationId other) {
    const AnimationId first = nthUserSelectable(0u);
    return (first == other) ? nthUserSelectable(1u) : first;
}

// A native fixture that composes the SAME objects production does, in the same
// order, and drives them through ControlPipeline exactly the way App does.
struct Fixture {
    ManualTimeProvider clock;
    ManualOverride manual;
    Scheduler scheduler;
    AnimationManager manager;
    PixelMatrix matrix;
    PlaybackPolicy policy;
    UserSettings settings;
    ControlPipeline pipeline;
    ControlPipeline::Outcome lastOutcome;

    Fixture()
        : clock(),
          manual(),
          scheduler(clock, manual, SchedulerConfig()),
          manager(),
          matrix(),
          policy(scheduler, manager, matrix),
          settings(UserSettingsDefaults::make()),
          pipeline(settings, manual, policy, manager),
          lastOutcome() {
        // No trustworthy clock, so the scheduler resolves to its documented
        // fallback. That is still a genuine scheduler decision, which is what
        // the AUTO-path race test needs.
        ClockSnapshot invalid;
        invalid.year = 0u;
        invalid.month = 1u;
        invalid.day = 1u;
        invalid.hour = 0u;
        invalid.minute = 0u;
        invalid.second = 0u;
        invalid.weekday = 0u;
        invalid.valid = false;
        clock.set(invalid);
    }

    // One production tick with no queued request: settles playback.
    void tick(uint32_t now) {
        lastOutcome = pipeline.apply(makeIdleControlRequest(), now);
    }

    // One HTTP /animation selection, delivered as a single ControlRequest.
    void select(AnimationId id, uint32_t now) {
        ControlRequest request = makeIdleControlRequest();
        request.action = ControlAction::SELECT_ANIMATION;
        request.animation = id;
        lastOutcome = pipeline.apply(request, now);
    }

    AnimationId visible() const { return pipeline.visibleAnimation(); }
};

uint8_t idOf(AnimationId id) { return static_cast<uint8_t>(id); }
uint8_t modeOf(ControlMode mode) { return static_cast<uint8_t>(mode); }

struct BufferSink {
    char* buffer;
    size_t capacity;
    size_t used;

    void append(const char* text) {
        while (*text != '\0' && used + 1u < capacity) {
            buffer[used++] = *text++;
        }
        buffer[used] = '\0';
    }
};

}  // namespace

// ---------------------------------------------------------------------------
// B10: the required regression test, for the complete animation set.
// ---------------------------------------------------------------------------

void test_one_request_starts_every_animation_from_manual(void) {
    for (uint8_t raw = 1u; raw < kAnimationCount; ++raw) {
        const AnimationId b = static_cast<AnimationId>(raw);
        if (!AnimationCatalog::isUserSelectable(b)) {
            continue;
        }

        const AnimationId a = userSelectableOtherThan(b);
        TEST_ASSERT_TRUE(a != b);

        Fixture f;
        f.settings.mode = ControlMode::MANUAL;
        f.settings.animation = a;
        f.manual.activateUntilCancelled(a, PlayMode::LOOP);
        f.tick(1000u);
        TEST_ASSERT_EQUAL_UINT8(idOf(a), idOf(f.visible()));

        const uint16_t startsBefore = f.policy.startCount();

        // EXACTLY ONE request.
        f.select(b, 1000u);

        TEST_ASSERT_TRUE(f.lastOutcome.accepted);
        TEST_ASSERT_FALSE(f.lastOutcome.restarted);
        TEST_ASSERT_EQUAL_UINT8(modeOf(ControlMode::MANUAL), modeOf(f.settings.mode));
        TEST_ASSERT_EQUAL_UINT8(idOf(b), idOf(f.settings.animation));
        // The canonical/visible animation is already B when apply() returns.
        TEST_ASSERT_EQUAL_UINT8(idOf(b), idOf(f.visible()));
        TEST_ASSERT_EQUAL_UINT8(idOf(b), idOf(f.manager.currentAnimation()));
        TEST_ASSERT_TRUE(f.manager.isPlaying());
        // B was started exactly once, and no second request was needed.
        TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(startsBefore + 1u), f.policy.startCount());
    }
}

void test_one_request_starts_every_animation_from_auto(void) {
    for (uint8_t raw = 1u; raw < kAnimationCount; ++raw) {
        const AnimationId b = static_cast<AnimationId>(raw);
        if (!AnimationCatalog::isUserSelectable(b)) {
            continue;
        }

        Fixture f;
        f.settings.mode = ControlMode::AUTO;
        f.manual.clear();
        f.tick(1000u);

        // The scheduler owns playback before the click.
        const AnimationId scheduled = f.visible();
        TEST_ASSERT_TRUE(scheduled != AnimationId::NONE);

        const uint16_t startsBefore = f.policy.startCount();

        // EXACTLY ONE request, at the same instant as the scheduler decision.
        f.select(b, 1000u);

        TEST_ASSERT_TRUE(f.lastOutcome.accepted);
        TEST_ASSERT_EQUAL_UINT8(modeOf(ControlMode::MANUAL), modeOf(f.settings.mode));
        TEST_ASSERT_EQUAL_UINT8(idOf(b), idOf(f.visible()));
        TEST_ASSERT_EQUAL_UINT8(idOf(b), idOf(f.manager.currentAnimation()));
        TEST_ASSERT_TRUE(f.manager.isPlaying());
        TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(startsBefore + 1u), f.policy.startCount());
    }
}

// ---------------------------------------------------------------------------
// B6: MANUAL ownership must beat a scheduler decision from the same cycle.
// ---------------------------------------------------------------------------

void test_manual_ownership_survives_the_same_tick_scheduler_decision(void) {
    Fixture f;
    f.settings.mode = ControlMode::AUTO;
    f.manual.clear();
    f.tick(1000u);

    const AnimationId scheduled = f.visible();
    const AnimationId chosen = userSelectableOtherThan(scheduled);

    // The scheduler has just decided `scheduled` at this instant; the user's
    // click arrives on the same tick.
    f.select(chosen, 1000u);

    TEST_ASSERT_EQUAL_UINT8(modeOf(ControlMode::MANUAL), modeOf(f.settings.mode));
    TEST_ASSERT_EQUAL_UINT8(idOf(chosen), idOf(f.visible()));

    // The scheduler keeps running and must NOT steal the panel back.
    f.tick(1016u);
    TEST_ASSERT_EQUAL_UINT8(idOf(chosen), idOf(f.visible()));
    f.tick(1032u);
    TEST_ASSERT_EQUAL_UINT8(idOf(chosen), idOf(f.visible()));
}

// ---------------------------------------------------------------------------
// B11: two legitimate clicks in a row must end on the last one.
// ---------------------------------------------------------------------------

void test_rapid_selection_ends_on_the_last_intent(void) {
    const AnimationId a = nthUserSelectable(0u);
    const AnimationId b = nthUserSelectable(1u);
    const AnimationId c = nthUserSelectable(2u);
    TEST_ASSERT_TRUE(a != b && b != c && a != c);

    Fixture f;
    f.settings.mode = ControlMode::MANUAL;
    f.settings.animation = a;
    f.manual.activateUntilCancelled(a, PlayMode::LOOP);
    f.tick(1000u);

    f.select(b, 1000u);
    f.select(c, 1000u);

    TEST_ASSERT_EQUAL_UINT8(idOf(c), idOf(f.visible()));
    TEST_ASSERT_EQUAL_UINT8(idOf(c), idOf(f.settings.animation));
    TEST_ASSERT_EQUAL_UINT8(idOf(c), idOf(f.manager.currentAnimation()));

    // Later ticks must not resurrect B, nor let the scheduler overwrite C.
    f.tick(2000u);
    f.tick(20000u);
    TEST_ASSERT_EQUAL_UINT8(idOf(c), idOf(f.visible()));
}

// ---------------------------------------------------------------------------
// B9 / CLICK-03: rejection is inert; re-selecting the current one restarts it.
// ---------------------------------------------------------------------------

void test_invalid_animation_selection_mutates_nothing(void) {
    Fixture f;
    f.settings.mode = ControlMode::AUTO;
    f.manual.clear();
    f.tick(1000u);

    const AnimationId visibleBefore = f.visible();
    const uint8_t animationBefore = idOf(f.settings.animation);
    const uint16_t startsBefore = f.policy.startCount();

    f.select(AnimationId::NONE, 1000u);
    TEST_ASSERT_FALSE(f.lastOutcome.accepted);

    f.select(static_cast<AnimationId>(200u), 1000u);
    TEST_ASSERT_FALSE(f.lastOutcome.accepted);

    // Not just "no animation": no mode change either.
    TEST_ASSERT_EQUAL_UINT8(modeOf(ControlMode::AUTO), modeOf(f.settings.mode));
    TEST_ASSERT_EQUAL_UINT8(animationBefore, idOf(f.settings.animation));
    TEST_ASSERT_EQUAL_UINT8(idOf(visibleBefore), idOf(f.visible()));
    TEST_ASSERT_EQUAL_UINT16(startsBefore, f.policy.startCount());
}

void test_same_animation_in_manual_restarts_it(void) {
    const AnimationId a = nthUserSelectable(0u);

    Fixture f;
    f.settings.mode = ControlMode::MANUAL;
    f.settings.animation = a;
    f.manual.activateUntilCancelled(a, PlayMode::LOOP);
    f.tick(1000u);

    const uint16_t startsBefore = f.policy.startCount();

    f.select(a, 1000u);

    TEST_ASSERT_TRUE(f.lastOutcome.accepted);
    TEST_ASSERT_TRUE(f.lastOutcome.restarted);
    TEST_ASSERT_EQUAL_UINT8(idOf(a), idOf(f.visible()));
    TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(startsBefore + 1u), f.policy.startCount());
}

// ---------------------------------------------------------------------------
// B12: the rendered panel must agree with the canonical state.
// ---------------------------------------------------------------------------

void test_panel_highlight_agrees_with_the_settled_state(void) {
    const AnimationId a = nthUserSelectable(0u);
    const AnimationId b = nthUserSelectable(1u);

    Fixture f;
    f.settings.mode = ControlMode::MANUAL;
    f.settings.animation = a;
    f.manual.activateUntilCancelled(a, PlayMode::LOOP);
    f.tick(1000u);

    f.select(b, 1000u);

    // This is exactly what App publishes after applyControl(): the canonical
    // animation, read after playback has settled.
    PanelView view;
    view.mode = f.settings.mode;
    view.currentAnimation = f.pipeline.visibleAnimation();
    view.brightnessPercent = f.settings.brightnessPercent;
    view.rotation = f.settings.rotation;
    view.connected = true;
    view.ssid = "TestNet";
    view.ipAddress = "192.168.1.10";
    view.localTime = "2026-10-02 20:00:00";

    char html[PanelRenderer::kPageCapacity];
    BufferSink sink{html, sizeof(html), 0u};
    PanelRenderer::render(sink, view);

    char activeB[64];
    snprintf(activeB, sizeof(activeB), "%s ✓", animationNameOf(b));
    char activeA[64];
    snprintf(activeA, sizeof(activeA), "%s ✓", animationNameOf(a));

    // B is highlighted once...
    TEST_ASSERT_NOT_NULL(strstr(html, activeB));
    // ...the stale animation is not...
    TEST_ASSERT_NULL(strstr(html, activeA));
    // ...and "Teraz wyświetlane" names B.
    const char* heading = strstr(html, "Teraz wyświetlane");
    TEST_ASSERT_NOT_NULL(heading);
    TEST_ASSERT_NOT_NULL(strstr(heading, animationNameOf(b)));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_one_request_starts_every_animation_from_manual);
    RUN_TEST(test_one_request_starts_every_animation_from_auto);
    RUN_TEST(test_manual_ownership_survives_the_same_tick_scheduler_decision);
    RUN_TEST(test_rapid_selection_ends_on_the_last_intent);
    RUN_TEST(test_invalid_animation_selection_mutates_nothing);
    RUN_TEST(test_same_animation_in_manual_restarts_it);
    RUN_TEST(test_panel_highlight_agrees_with_the_settled_state);
    return UNITY_END();
}
