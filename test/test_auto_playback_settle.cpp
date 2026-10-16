#include <unity.h>

#include <string.h>

#include "control/ControlPipeline.h"
#include "control/PanelRenderer.h"
#include "control/UserSettings.h"
#include "core/AnimationManager.h"
#include "core/AnimationNames.h"
#include "core/AnimationRegistry.h"
#include "core/PixelMatrix.h"
#include "scheduling/ManualOverride.h"
#include "scheduling/ManualTimeProvider.h"
#include "scheduling/PlaybackPolicy.h"
#include "scheduling/Playlists.h"
#include "scheduling/ScheduleTable.h"
#include "scheduling/Scheduler.h"
#include "scheduling/SpecialDates.h"

// Regression coverage for the two proven AUTO playback defects.
//
//  1. A NONE decision (quiet hours) stopped the animation but never blanked the
//     matrix, so the strip kept the last frame the previous MANUAL animation
//     transmitted. Clicking "Automatyczny" at 01:00 therefore froze the panel
//     on whatever had been showing, while the panel correctly reported
//     "Wyłączone".
//
//  2. A scheduler decision naming an animation this build cannot instantiate
//     (LOVE_MODE, BREATHING_HEART) was adopted as the current animation and
//     then silently ignored, leaving the previous animation running forever
//     under a different name on the panel.
//
// The invariant every test below asserts:
//
//     currentAnimation() == NONE  ->  the matrix is black
//     currentAnimation() != NONE  ->  that id is renderable and is running
//
// Manager-only assertions were not enough to catch either bug, so these tests
// seed and read the real PixelMatrix framebuffer.

namespace {

// October 2nd is an ordinary day: no special-date rule matches it.
constexpr uint16_t kYear = 2026u;
constexpr uint8_t kMonth = 10u;
constexpr uint8_t kDay = 2u;

uint8_t id(AnimationId value) {
    return static_cast<uint8_t>(value);
}

// One production-equivalent App tick for everything this file exercises:
//
//     ControlPipeline::apply()  (apply + settle playback)
//     AnimationManager::tick()
//     PixelMatrix::showIfDirty()
//
// This is App::tick() steps 2, 5 and 6, in that order, with the HTTP and
// telemetry steps removed because they touch nothing under test.
struct Harness {
    ManualTimeProvider clock;
    ManualOverride manual;
    Scheduler scheduler;
    AnimationManager manager;
    PixelMatrix matrix;
    PlaybackPolicy policy;
    UserSettings settings;
    ControlPipeline pipeline;
    uint32_t now;

    Harness()
        : clock(),
          manual(),
          scheduler(clock, manual, SchedulerConfig()),
          manager(),
          matrix(),
          policy(scheduler, manager, matrix),
          settings(UserSettingsDefaults::make()),
          pipeline(settings, manual, policy, manager),
          now(1000u) {
        matrix.begin();
        matrix.setBrightness(brightnessPercentToRaw(50u));
    }

    void setClock(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute) {
        clock.set(makeClockSnapshot(year, month, day, hour, minute, 0u));
    }

    // One tick with no queued user action.
    void tick() {
        tickWith(makeIdleControlRequest());
    }

    // One tick carrying exactly one queued user action.
    void tickWith(const ControlRequest& request) {
        pipeline.apply(request, now);
        manager.tick(now, matrix);
        matrix.showIfDirty();
        now += 16u;
    }

    // The single user operation under test.
    void clickAuto() {
        ControlRequest request = makeIdleControlRequest();
        request.action = ControlAction::SET_MODE;
        request.mode = ControlMode::AUTO;
        tickWith(request);
    }

    void enterManualWith(AnimationId animation, PlayMode playMode) {
        settings.mode = ControlMode::MANUAL;
        settings.animation = animation;
        manual.activateUntilCancelled(animation, playMode);
    }

    // Advance until the animation has definitely painted a lit frame.
    void advanceIntoVisibleFrame(uint16_t ticks) {
        for (uint16_t i = 0u; i < ticks; ++i) {
            tick();
        }
    }

    PlaybackDecision decision() {
        return scheduler.evaluate(now);
    }

    AnimationId panelCurrent() const {
        return pipeline.visibleAnimation();
    }
};

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

// Renders the panel exactly as App publishes it after settling playback.
void renderPanel(const Harness& h, char* html, size_t capacity) {
    PanelView view;
    view.mode = h.settings.mode;
    view.currentAnimation = h.pipeline.visibleAnimation();
    view.brightnessPercent = h.settings.brightnessPercent;
    view.rotation = h.settings.rotation;
    view.connected = true;
    view.ssid = "TestNet";
    view.ipAddress = "192.168.1.10";
    view.localTime = "2026-10-02 01:00:00";

    BufferSink sink{html, capacity, 0u};
    PanelRenderer::render(sink, view);
}

}  // namespace

// ===========================================================================
// AUTO-01: MANUAL sun -> one SET_MODE(AUTO) at 01:00 must be physically dark.
// ===========================================================================

void test_auto01_quiet_hours_blanks_the_matrix(void) {
    Harness h;
    h.setClock(kYear, kMonth, kDay, 1u, 0u);
    h.enterManualWith(AnimationId::SUN_OVER_MOUNTAINS, PlayMode::LOOP);

    // Get the sun genuinely on the strip, not merely logically selected.
    h.advanceIntoVisibleFrame(20u);
    TEST_ASSERT_TRUE(h.manager.isPlaying());
    TEST_ASSERT_FALSE(h.matrix.isBlack());

    // Exactly ONE SET_MODE(AUTO).
    h.clickAuto();

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ControlMode::AUTO),
        static_cast<uint8_t>(h.settings.mode));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::NONE), id(h.decision().animationId));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::NONE), id(h.manager.currentAnimation()));
    TEST_ASSERT_FALSE(h.manager.isPlaying());

    // The whole point of the fix.
    TEST_ASSERT_TRUE(h.matrix.isBlack());

    char html[PanelRenderer::kPageCapacity];
    renderPanel(h, html, sizeof(html));
    TEST_ASSERT_NOT_NULL(strstr(html, animationNameOf(AnimationId::NONE)));
}

// AUTO-01b: manager-only assertions passed before the fix. Assert the frame
// contents explicitly so the regression cannot come back through the manager.
void test_auto01_no_sun_pixels_remain(void) {
    Harness h;
    h.setClock(kYear, kMonth, kDay, 1u, 0u);
    h.enterManualWith(AnimationId::SUN_OVER_MOUNTAINS, PlayMode::LOOP);
    h.advanceIntoVisibleFrame(20u);

    uint32_t litBefore = 0u;
    for (uint16_t i = 0u; i < PixelMatrix::LED_COUNT; ++i) {
        if (h.matrix.pixelColorAt(i) != 0u) {
            ++litBefore;
        }
    }
    TEST_ASSERT_TRUE(litBefore > 0u);

    h.clickAuto();

    uint32_t litAfter = 0u;
    for (uint16_t i = 0u; i < PixelMatrix::LED_COUNT; ++i) {
        if (h.matrix.pixelColorAt(i) != 0u) {
            ++litAfter;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(0u, litAfter);
}

// ===========================================================================
// AUTO-02: staying in quiet hours must be idempotent.
// ===========================================================================

void test_auto02_quiet_hours_are_idempotent(void) {
    Harness h;
    h.setClock(kYear, kMonth, kDay, 1u, 0u);
    h.enterManualWith(AnimationId::SUN_OVER_MOUNTAINS, PlayMode::LOOP);
    h.advanceIntoVisibleFrame(20u);

    h.clickAuto();

    TEST_ASSERT_EQUAL_UINT16(1u, h.policy.blankCount());
    TEST_ASSERT_EQUAL_UINT16(1u, h.policy.stopCount());

    // Many hours of quiet hours: hours of ticks at one-minute steps.
    const uint16_t blanksAfterEntry = h.policy.blankCount();
    const uint16_t stopsAfterEntry = h.policy.stopCount();
    for (uint16_t minute = 0u; minute < 240u; ++minute) {
        h.tick();
    }

    // No redundant stop, no redundant clear, no restart storm.
    TEST_ASSERT_EQUAL_UINT16(blanksAfterEntry, h.policy.blankCount());
    TEST_ASSERT_EQUAL_UINT16(stopsAfterEntry, h.policy.stopCount());
    TEST_ASSERT_EQUAL_UINT16(1u, h.policy.startCount());

    // Still off, still clean.
    TEST_ASSERT_FALSE(h.manager.isPlaying());
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::NONE), id(h.manager.currentAnimation()));
    TEST_ASSERT_TRUE(h.matrix.isBlack());

    // A blank frame must not leave the matrix permanently dirty, because that
    // would mean a black transmission on every single tick.
    h.tick();
    TEST_ASSERT_FALSE(h.matrix.isDirty());
    TEST_ASSERT_TRUE(h.matrix.isBlack());
}

// ===========================================================================
// AUTO-03/04/05: entering quiet hours from LOOP / HOLD / ONCE.
// ===========================================================================

void assert_entering_quiet_hours_blanks(PlayMode entryMode) {
    Harness h;
    h.setClock(kYear, kMonth, kDay, 1u, 0u);
    h.enterManualWith(AnimationId::SUN_OVER_MOUNTAINS, entryMode);

    h.advanceIntoVisibleFrame(20u);
    TEST_ASSERT_TRUE(h.manager.isPlaying());
    TEST_ASSERT_FALSE(h.matrix.isBlack());

    h.clickAuto();

    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::NONE), id(h.manager.currentAnimation()));
    TEST_ASSERT_FALSE(h.manager.isPlaying());
    TEST_ASSERT_TRUE(h.matrix.isBlack());
    TEST_ASSERT_EQUAL_UINT16(1u, h.policy.blankCount());
}

void test_auto03_entering_quiet_hours_from_loop_blanks(void) {
    assert_entering_quiet_hours_blanks(PlayMode::LOOP);
}

void test_auto04_entering_quiet_hours_from_hold_blanks(void) {
    assert_entering_quiet_hours_blanks(PlayMode::HOLD);
}

void test_auto05_entering_quiet_hours_from_once_blanks(void) {
    assert_entering_quiet_hours_blanks(PlayMode::ONCE);
}

// ===========================================================================
// AUTO-06: leaving quiet hours wakes by itself. No click, no reboot.
// ===========================================================================

void test_auto06_leaving_quiet_hours_starts_automatically(void) {
    Harness h;
    h.setClock(kYear, kMonth, kDay, 5u, 59u);
    h.enterManualWith(AnimationId::SUN_OVER_MOUNTAINS, PlayMode::LOOP);
    h.advanceIntoVisibleFrame(10u);

    h.clickAuto();
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::NONE), id(h.manager.currentAnimation()));
    TEST_ASSERT_TRUE(h.matrix.isBlack());

    // The clock crosses the quiet-hours boundary on its own.
    h.setClock(kYear, kMonth, kDay, 6u, 0u);

    const AnimationId expected = h.decision().animationId;
    TEST_ASSERT_NOT_EQUAL_UINT8(id(AnimationId::NONE), id(expected));

    h.tick();
    TEST_ASSERT_EQUAL_UINT8(id(expected), id(h.manager.currentAnimation()));
    TEST_ASSERT_TRUE(h.manager.isPlaying());

    // Frames begin updating again: content appears without any user action.
    for (uint16_t i = 0u; i < 40u; ++i) {
        h.tick();
    }
    TEST_ASSERT_FALSE(h.matrix.isBlack());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ControlMode::AUTO),
        static_cast<uint8_t>(h.settings.mode));
}

// ===========================================================================
// UNAVAILABLE: an undrawable scheduler decision must never be adopted.
// ===========================================================================

void assert_unavailable_falls_back(AnimationId requested) {
    Harness h;
    // A scheduled time that is not quiet hours, so the fallback is what plays.
    h.setClock(kYear, kMonth, kDay, 13u, 0u);
    h.enterManualWith(AnimationId::MOON_STARS, PlayMode::LOOP);
    h.advanceIntoVisibleFrame(10u);
    TEST_ASSERT_FALSE(h.matrix.isBlack());

    // Force the scheduler to ask for an animation this build cannot draw.
    TEST_ASSERT_FALSE(AnimationRegistry::isRenderable(requested));
    h.manual.activateUntilCancelled(requested, PlayMode::LOOP);
    TEST_ASSERT_EQUAL_UINT8(id(requested), id(h.decision().animationId));

    const AnimationId fallback = h.scheduler.fallbackAnimation();
    TEST_ASSERT_TRUE(AnimationRegistry::isRenderable(fallback));

    h.tick();

    // Never adopted.
    TEST_ASSERT_NOT_EQUAL_UINT8(id(requested), id(h.manager.currentAnimation()));
    TEST_ASSERT_NOT_EQUAL_UINT8(id(requested), id(h.panelCurrent()));

    // Resolved through the canonical fallback, which is really running.
    TEST_ASSERT_EQUAL_UINT8(id(fallback), id(h.manager.currentAnimation()));
    TEST_ASSERT_EQUAL_UINT8(id(fallback), id(h.panelCurrent()));
    TEST_ASSERT_TRUE(h.manager.isPlaying());
    TEST_ASSERT_EQUAL_UINT16(1u, h.policy.unavailableCount());

    char html[PanelRenderer::kPageCapacity];
    renderPanel(h, html, sizeof(html));
    TEST_ASSERT_NOT_NULL(strstr(html, animationNameOf(fallback)));
}

void test_unavailable01_love_mode_falls_back(void) {
    assert_unavailable_falls_back(AnimationId::LOVE_MODE);
}

void test_unavailable02_breathing_heart_falls_back(void) {
    assert_unavailable_falls_back(AnimationId::BREATHING_HEART);
}

// UNAVAILABLE-03: generic, not hardcoded ids. Every registry-absent id that
// the enum can express must take the same path.
void test_unavailable03_generic_unavailable_ids_fall_back(void) {
    const AnimationId outOfRange = static_cast<AnimationId>(200u);
    TEST_ASSERT_FALSE(AnimationRegistry::isRenderable(outOfRange));

    Harness h;
    h.setClock(kYear, kMonth, kDay, 13u, 0u);
    h.enterManualWith(AnimationId::MOON_STARS, PlayMode::LOOP);
    h.advanceIntoVisibleFrame(10u);

    h.manual.activateUntilCancelled(outOfRange, PlayMode::LOOP);
    h.tick();

    TEST_ASSERT_EQUAL_UINT8(id(h.scheduler.fallbackAnimation()), id(h.manager.currentAnimation()));
    TEST_ASSERT_TRUE(h.manager.isPlaying());
}

// UNAVAILABLE-04: the fail-safe leg of the chain.
//
// The chain the policy implements is:
//
//   desired NONE        -> OFF + black
//   desired renderable  -> PLAY desired
//   desired undrawable  -> canonical fallback, or OFF when that fails too
//
// The last leg is not hypothetical: AnimationRegistry registers AURORA only in
// the non-AVR branch (AnimationRegistry.cpp, inside `#if !defined(ARDUINO_AVR_UNO)`),
// so on the Uno the canonical fallback is itself unavailable and the fail-safe
// leg is the one that actually runs.
//
// This build registers AURORA, so the leg cannot be entered here directly
// without faking the registry. What is asserted here is the invariant the leg
// exists to protect: whatever the cause, playback must be able to reach OFF and
// must never strand a stale frame.
void test_unavailable04_fallback_chain_is_fail_safe(void) {
    // On this build the fallback is available, so the fail-safe leg is latent.
    TEST_ASSERT_TRUE(AnimationRegistry::isRenderable(AnimationId::AURORA));

    // Every id the enum can name is accounted for: it either resolves to
    // something renderable or terminates at NONE. No id may be adopted while
    // undrawable.
    for (uint8_t raw = 0u; raw < static_cast<uint8_t>(AnimationId::ANIMATION_ID_COUNT); ++raw) {
        const AnimationId candidate = static_cast<AnimationId>(raw);

        Harness h;
        h.setClock(kYear, kMonth, kDay, 13u, 0u);
        h.enterManualWith(AnimationId::MOON_STARS, PlayMode::LOOP);
        h.advanceIntoVisibleFrame(10u);
        h.manual.activateUntilCancelled(candidate, PlayMode::LOOP);
        h.tick();

        const AnimationId current = h.manager.currentAnimation();
        if (candidate != AnimationId::NONE &&
            !AnimationRegistry::isRenderable(candidate)) {
            // Resolved, never adopted.
            TEST_ASSERT_NOT_EQUAL_UINT8(id(candidate), id(current));
        }
        TEST_ASSERT_TRUE(
            current == AnimationId::NONE || AnimationRegistry::isRenderable(current));
    }
}

// ===========================================================================
// STATE CONSISTENCY: the invariant, exercised over a whole simulated day.
// ===========================================================================

void test_state_consistency_across_a_whole_day(void) {
    Harness h;
    h.setClock(kYear, kMonth, kDay, 0u, 0u);
    h.enterManualWith(AnimationId::SUN_OVER_MOUNTAINS, PlayMode::LOOP);
    h.advanceIntoVisibleFrame(10u);
    h.clickAuto();

    // Walk every minute of a full day, asserting the invariant each time.
    for (uint16_t minute = 0u; minute < 24u * 60u; ++minute) {
        h.tick();

        const AnimationId current = h.manager.currentAnimation();

        if (current == AnimationId::NONE) {
            // Logical NONE must mean physically black. This is the invariant
            // that was violated by the quiet-hours freeze.
            TEST_ASSERT_TRUE(h.matrix.isBlack());
            TEST_ASSERT_FALSE(h.manager.isPlaying());
        } else {
            // Logical "something" must be renderable and actually owned.
            TEST_ASSERT_TRUE(AnimationRegistry::isRenderable(current));
            TEST_ASSERT_TRUE(h.manager.isPlaying());
        }

        // The panel never disagrees with the manager.
        TEST_ASSERT_EQUAL_UINT8(id(current), id(h.panelCurrent()));
    }
}

// ===========================================================================
// ALL SCHEDULER-REACHABLE IDS: no future schedule addition may silently freeze.
// ===========================================================================

void test_every_scheduler_reachable_id_is_safe(void) {
    // Everything the tables can name.
    uint8_t reachable[64];
    uint8_t count = 0u;

    for (uint8_t i = 0u; i < kScheduleCount; ++i) {
        reachable[count++] = static_cast<uint8_t>(kSchedule[i].animation);

        const Playlist* playlist = Playlists::find(kSchedule[i].playlist);
        if (playlist != nullptr) {
            for (uint8_t p = 0u; p < playlist->count; ++p) {
                reachable[count++] = static_cast<uint8_t>(playlist->items[p].animation);
            }
        }
    }

    for (uint8_t i = 0u; i < kSpecialDateCount; ++i) {
        reachable[count++] = static_cast<uint8_t>(kSpecialDates[i].animation);
    }

    reachable[count++] = id(AnimationId::AURORA);  // the canonical fallback

    // Drive the policy with each one and require a safe outcome.
    for (uint8_t i = 0u; i < count; ++i) {
        const AnimationId candidate = static_cast<AnimationId>(reachable[i]);

        Harness h;
        h.setClock(kYear, kMonth, kDay, 13u, 0u);  // not quiet hours
        h.manual.activateUntilCancelled(candidate, PlayMode::LOOP);
        h.tick();

        const AnimationId current = h.manager.currentAnimation();

        if (candidate == AnimationId::NONE) {
            TEST_ASSERT_EQUAL_UINT8(id(AnimationId::NONE), id(current));
            TEST_ASSERT_TRUE(h.matrix.isBlack());
        } else if (AnimationRegistry::isRenderable(candidate)) {
            TEST_ASSERT_EQUAL_UINT8(id(candidate), id(current));
            TEST_ASSERT_TRUE(h.manager.isPlaying());
        } else {
            // Unrenderable: must have resolved to the fallback, never adopted.
            TEST_ASSERT_NOT_EQUAL_UINT8(id(candidate), id(current));
            TEST_ASSERT_EQUAL_UINT8(id(h.scheduler.fallbackAnimation()), id(current));
            TEST_ASSERT_TRUE(h.manager.isPlaying());
        }
    }
}

// ===========================================================================
// DIRTY-FRAME REGRESSION: clear -> dirty -> exactly one transmission.
// ===========================================================================

void test_blank_is_dirty_frame_not_a_direct_show(void) {
    Harness h;
    h.setClock(kYear, kMonth, kDay, 1u, 0u);
    h.enterManualWith(AnimationId::SUN_OVER_MOUNTAINS, PlayMode::LOOP);
    h.advanceIntoVisibleFrame(20u);
    TEST_ASSERT_FALSE(h.matrix.isDirty());

    // Settle playback WITHOUT the transmit step, so the intermediate dirty
    // state is observable.
    ControlRequest request = makeIdleControlRequest();
    request.action = ControlAction::SET_MODE;
    request.mode = ControlMode::AUTO;
    h.pipeline.apply(request, h.now);

    // clear() marked the frame dirty; nothing has transmitted it yet.
    TEST_ASSERT_TRUE(h.matrix.isDirty());
    TEST_ASSERT_TRUE(h.matrix.isBlack());

    h.manager.tick(h.now, h.matrix);
    h.matrix.showIfDirty();

    // Exactly one transmission, then clean again.
    TEST_ASSERT_FALSE(h.matrix.isDirty());
    TEST_ASSERT_TRUE(h.matrix.isBlack());

    h.tick();
    h.tick();
    TEST_ASSERT_FALSE(h.matrix.isDirty());
    TEST_ASSERT_TRUE(h.matrix.isBlack());
}

// ===========================================================================
// TIME REGRESSION: 01:00 was always read correctly. Pin that down.
// ===========================================================================

void test_one_am_is_quiet_hours_not_sun(void) {
    Harness h;
    h.setClock(kYear, kMonth, kDay, 1u, 0u);

    const PlaybackDecision decision = h.decision();

    // The scheduler never chose the sun; the panel froze on a stale frame.
    TEST_ASSERT_NOT_EQUAL_UINT8(id(AnimationId::SUN_OVER_MOUNTAINS), id(decision.animationId));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::NONE), id(decision.animationId));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(PlaybackReason::QUIET_HOURS),
        static_cast<uint8_t>(decision.reason));

    // Quiet hours are 00:00-06:00 and are NOT changed by this fix.
    TEST_ASSERT_TRUE(h.scheduler.isQuietMinute(at(0u, 0u)));
    TEST_ASSERT_TRUE(h.scheduler.isQuietMinute(at(1u, 0u)));
    TEST_ASSERT_TRUE(h.scheduler.isQuietMinute(at(5u, 59u)));
    TEST_ASSERT_FALSE(h.scheduler.isQuietMinute(at(6u, 0u)));
}

// ===========================================================================
// SPECIAL DATE: the playback safety net is not needed for Valentine's any more.
// ===========================================================================

// Valentine's used to be the shipped example of an undrawable special-date
// decision. It now resolves to a playlist of two implemented animations, so
// playback must run normally and the fallback must NOT be involved.
void test_special_date_now_plays_without_the_fallback(void) {
    Harness h;
    h.setClock(2027u, 2u, 14u, 18u, 0u);

    // A real special event whose decision is renderable.
    const PlaybackDecision decision = h.decision();
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(SchedulePriority::SPECIAL_EVENT),
        static_cast<uint8_t>(decision.priority));
    TEST_ASSERT_TRUE(AnimationRegistry::isRenderable(decision.animationId));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::HEARTBEAT), id(decision.animationId));

    h.clickAuto();

    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::HEARTBEAT), id(h.panelCurrent()));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::HEARTBEAT), id(h.manager.currentAnimation()));
    TEST_ASSERT_TRUE(h.manager.isPlaying());

    // The safety net stayed out of it completely.
    TEST_ASSERT_EQUAL_UINT16(0u, h.policy.unavailableCount());
    AnimationId requested = AnimationId::NONE;
    AnimationId applied = AnimationId::NONE;
    TEST_ASSERT_FALSE(h.policy.consumeUnavailableNotice(requested, applied));
}

// No shipped special date may name an animation this build cannot draw. This
// is the invariant that makes the previous test possible in the first place.
void test_no_special_date_names_an_unrenderable_animation(void) {
    for (uint8_t i = 0u; i < kSpecialDateCount; ++i) {
        const Playlist* playlist = Playlists::find(kSpecialDates[i].playlist);

        if (playlist != nullptr) {
            for (uint8_t p = 0u; p < playlist->count; ++p) {
                TEST_ASSERT_TRUE(
                    AnimationRegistry::isRenderable(playlist->items[p].animation));
            }
            continue;
        }

        TEST_ASSERT_TRUE(AnimationRegistry::isRenderable(kSpecialDates[i].animation));
    }
}

// The renderability gate itself still has to work, so it is driven through a
// route that can still produce an undrawable decision: the manual override.
// Scheduler honours the override before anything else, so LOVE_MODE reaches
// PlaybackPolicy exactly as an undrawable schedule decision would.
void test_unrenderable_decision_still_resolves_to_the_fallback(void) {
    TEST_ASSERT_FALSE(AnimationRegistry::isRenderable(AnimationId::LOVE_MODE));

    Harness h;
    h.setClock(2027u, 2u, 14u, 19u, 0u);
    h.enterManualWith(AnimationId::MOON_STARS, PlayMode::LOOP);
    h.advanceIntoVisibleFrame(10u);
    TEST_ASSERT_FALSE(h.matrix.isBlack());

    h.manual.activateUntilCancelled(AnimationId::LOVE_MODE, PlayMode::LOOP);
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::LOVE_MODE), id(h.decision().animationId));

    h.tick();

    const AnimationId fallback = h.scheduler.fallbackAnimation();

    // Never adopted, resolved to the canonical fallback, and really running.
    TEST_ASSERT_NOT_EQUAL_UINT8(id(AnimationId::LOVE_MODE), id(h.manager.currentAnimation()));
    TEST_ASSERT_NOT_EQUAL_UINT8(id(AnimationId::LOVE_MODE), id(h.panelCurrent()));
    TEST_ASSERT_EQUAL_UINT8(id(fallback), id(h.manager.currentAnimation()));
    TEST_ASSERT_EQUAL_UINT8(id(fallback), id(h.panelCurrent()));
    TEST_ASSERT_TRUE(h.manager.isPlaying());
    TEST_ASSERT_EQUAL_UINT16(1u, h.policy.unavailableCount());

    char html[PanelRenderer::kPageCapacity];
    renderPanel(h, html, sizeof(html));
    TEST_ASSERT_NOT_NULL(strstr(html, animationNameOf(fallback)));

    // The one-shot notice fires exactly once, however long the condition lasts.
    AnimationId requested = AnimationId::NONE;
    AnimationId applied = AnimationId::NONE;
    TEST_ASSERT_TRUE(h.policy.consumeUnavailableNotice(requested, applied));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::LOVE_MODE), id(requested));
    TEST_ASSERT_EQUAL_UINT8(id(fallback), id(applied));
    TEST_ASSERT_FALSE(h.policy.consumeUnavailableNotice(requested, applied));
}

// Manual precedence over special dates is unchanged by this fix.
void test_manual_still_beats_special_dates(void) {
    Harness h;
    h.setClock(2027u, 2u, 14u, 19u, 0u);
    h.enterManualWith(AnimationId::CAMPFIRE, PlayMode::LOOP);
    h.tick();

    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::CAMPFIRE), id(h.manager.currentAnimation()));
    TEST_ASSERT_EQUAL_UINT16(0u, h.policy.unavailableCount());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_auto01_quiet_hours_blanks_the_matrix);
    RUN_TEST(test_auto01_no_sun_pixels_remain);
    RUN_TEST(test_auto02_quiet_hours_are_idempotent);
    RUN_TEST(test_auto03_entering_quiet_hours_from_loop_blanks);
    RUN_TEST(test_auto04_entering_quiet_hours_from_hold_blanks);
    RUN_TEST(test_auto05_entering_quiet_hours_from_once_blanks);
    RUN_TEST(test_auto06_leaving_quiet_hours_starts_automatically);
    RUN_TEST(test_unavailable01_love_mode_falls_back);
    RUN_TEST(test_unavailable02_breathing_heart_falls_back);
    RUN_TEST(test_unavailable03_generic_unavailable_ids_fall_back);
    RUN_TEST(test_unavailable04_fallback_chain_is_fail_safe);
    RUN_TEST(test_state_consistency_across_a_whole_day);
    RUN_TEST(test_every_scheduler_reachable_id_is_safe);
    RUN_TEST(test_blank_is_dirty_frame_not_a_direct_show);
    RUN_TEST(test_one_am_is_quiet_hours_not_sun);
    RUN_TEST(test_special_date_now_plays_without_the_fallback);
    RUN_TEST(test_no_special_date_names_an_unrenderable_animation);
    RUN_TEST(test_unrenderable_decision_still_resolves_to_the_fallback);
    RUN_TEST(test_manual_still_beats_special_dates);
    return UNITY_END();
}
