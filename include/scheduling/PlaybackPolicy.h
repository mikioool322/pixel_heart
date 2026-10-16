#pragma once

#include <stdint.h>

#include "core/AnimationManager.h"
#include "core/AnimationId.h"
#include "core/PixelMatrix.h"
#include "core/PlayMode.h"
#include "scheduling/PlaybackDecision.h"
#include "scheduling/SchedulePriority.h"
#include "scheduling/Scheduler.h"

// Bridges Scheduler decisions to AnimationManager.
//
// The policy calls the manager only when the desired decision actually
// differs from what is already applied. A scheduler decision that stays the
// same for hundreds of ticks results in exactly one start() call.
//
// ONE INVARIANT, enforced here and nowhere else:
//
//     after tick() returns, what the panel reports and what the strip shows
//     are the same thing.
//
//   * currentAnimation() != NONE  ->  that id is renderable and IS running
//   * currentAnimation() == NONE  ->  the matrix is black
//
// Reaching it needs two protections, both about the OFF state and about
// content this build cannot draw:
//
//   1. A NONE decision (quiet hours, or no renderable fallback) blanks the
//      matrix. Without that the strip keeps the last frame the previous
//      animation transmitted, which reads as a frozen panel.
//
//   2. A decision the registry cannot instantiate is never adopted as the
//      current animation. It is substituted with the scheduler's canonical
//      fallback before anything else looks at it, so no intermediate state
//      ever claims an undrawable animation is on screen.
//
// Matrix ownership is unchanged: the policy only calls clear(), which marks
// the frame dirty. App's showIfDirty() remains the single transmit point, so
// there is no framebuffer, no extra allocation and no direct show().
class PlaybackPolicy {
public:
    PlaybackPolicy(Scheduler& scheduler, AnimationManager& manager, PixelMatrix& matrix);

    void tick(uint32_t nowMs);

    // Re-runs the currently applied animation from its first frame. Used by the
    // panel's PLAY button; does not change WHAT is playing.
    void restart(uint32_t nowMs);

    // Test / diagnostic accessors.
    AnimationId appliedAnimation() const;
    PlayMode appliedMode() const;
    SchedulePriority appliedPriority() const;
    PlaybackReason appliedReason() const;
    uint16_t startCount() const;
    uint16_t stopCount() const;

    // Transitions into the OFF state, i.e. X -> NONE. Each one clears the
    // matrix exactly once. Steady-state NONE ticks do not increment this.
    uint16_t blankCount() const;

    // Decisions requested by the scheduler that this build cannot actually
    // render, counted once per distinct decision rather than once per tick.
    uint16_t unavailableCount() const;

    // A once-per-transition notice for the serial log. Reading it clears it,
    // so a condition that persists for hours is reported once, not every tick.
    // `requested` is what the scheduler asked for, `applied` is what is
    // actually playing instead.
    bool consumeUnavailableNotice(AnimationId& requested, AnimationId& applied);

    void reset();

private:
    static bool isDefaultDecision(const PlaybackDecision& decision);
    static bool sameDecision(const PlaybackDecision& a, const PlaybackDecision& b);

    Scheduler& scheduler_;
    AnimationManager& manager_;
    PixelMatrix& matrix_;

    PlaybackDecision applied_;
    bool hasApplied_;
    uint16_t startCount_;
    uint16_t stopCount_;
    uint16_t blankCount_;
    uint16_t unavailableCount_;

    bool noticePending_;
    AnimationId noticeRequested_;
    AnimationId noticeApplied_;
};
