#include "scheduling/PlaybackPolicy.h"

#include "core/AnimationRegistry.h"

PlaybackPolicy::PlaybackPolicy(
    Scheduler& scheduler,
    AnimationManager& manager,
    PixelMatrix& matrix)
    : scheduler_(scheduler),
      manager_(manager),
      matrix_(matrix),
      applied_{AnimationId::NONE, PlayMode::LOOP, 0u,
               SchedulePriority::FALLBACK, PlaybackReason::FALLBACK},
      hasApplied_(false),
      startCount_(0u),
      stopCount_(0u),
      blankCount_(0u),
      unavailableCount_(0u),
      noticePending_(false),
      noticeRequested_(AnimationId::NONE),
      noticeApplied_(AnimationId::NONE) {
}

void PlaybackPolicy::reset() {
    hasApplied_ = false;
    startCount_ = 0u;
    stopCount_ = 0u;
    blankCount_ = 0u;
    unavailableCount_ = 0u;
    noticePending_ = false;
    noticeRequested_ = AnimationId::NONE;
    noticeApplied_ = AnimationId::NONE;
    applied_.animationId = AnimationId::NONE;
    applied_.playMode = PlayMode::LOOP;
    applied_.holdDurationMs = 0u;
    applied_.priority = SchedulePriority::FALLBACK;
    applied_.reason = PlaybackReason::FALLBACK;
}

bool PlaybackPolicy::isDefaultDecision(const PlaybackDecision& decision) {
    return decision.animationId == AnimationId::NONE;
}

bool PlaybackPolicy::sameDecision(const PlaybackDecision& a, const PlaybackDecision& b) {
    return a.animationId == b.animationId &&
           a.playMode == b.playMode &&
           a.priority == b.priority &&
           a.reason == b.reason;
}

void PlaybackPolicy::restart(uint32_t nowMs) {
    // Deliberately reuses the applied decision: PLAY restarts the current
    // animation, it does not choose a different one.
    if (!hasApplied_ || applied_.animationId == AnimationId::NONE) {
        return;
    }

    manager_.play(applied_.animationId, applied_.playMode, nowMs);
    ++startCount_;
}

void PlaybackPolicy::tick(uint32_t nowMs) {
    PlaybackDecision desired = scheduler_.evaluate(nowMs);

    // ---------------------------------------------------------------------
    // Renderability gate.
    //
    // The scheduler decides WHAT should play; it has no knowledge of which
    // animations this build can actually draw. LOVE_MODE and BREATHING_HEART
    // are declared, scheduler-reachable and unimplemented.
    //
    // Adopting such a decision and discovering the problem later is what left
    // the previous animation on the strip forever while the panel named
    // something else. So the decision is resolved HERE, before anything else
    // can observe it: an undrawable id becomes the scheduler's own canonical
    // fallback, and if even that cannot be drawn it becomes NONE, which is
    // the safe OFF state.
    //
    // Quiet hours are deliberately untouched: NONE is a legitimate decision
    // and still means "dark", not "play something else".
    // ---------------------------------------------------------------------
    if (!isDefaultDecision(desired) &&
        !AnimationRegistry::isRenderable(desired.animationId)) {
        const AnimationId requested = desired.animationId;
        const AnimationId fallback = scheduler_.fallbackAnimation();

        if (AnimationRegistry::isRenderable(fallback)) {
            desired.animationId = fallback;
            desired.priority = SchedulePriority::FALLBACK;
            desired.reason = PlaybackReason::FALLBACK;
        } else {
            // Fail safe: nothing can be drawn, so go dark rather than leave a
            // stale frame on the strip.
            desired.animationId = AnimationId::NONE;
            desired.priority = SchedulePriority::FALLBACK;
            desired.reason = PlaybackReason::FALLBACK;
        }

        // Count and report the transition only. A decision that persists for
        // hours must not produce a log line per tick.
        if (!hasApplied_ || !sameDecision(desired, applied_)) {
            ++unavailableCount_;
            noticePending_ = true;
            noticeRequested_ = requested;
            noticeApplied_ = desired.animationId;
        }
    }

    if (hasApplied_ && sameDecision(desired, applied_)) {
        // Identical decision. Do NOT restart the animation.
        // A LOOP/HOLD animation the manager has already released is picked
        // up again; a finished ONCE animation is left finished.
        if (!isDefaultDecision(desired) &&
            desired.playMode != PlayMode::ONCE &&
            !manager_.isPlaying() &&
            manager_.currentAnimation() != desired.animationId) {
            manager_.play(desired.animationId, desired.playMode, nowMs);
            ++startCount_;
        }
        return;
    }

    if (isDefaultDecision(desired)) {
        // Entering quiet hours / NONE.
        //
        // `wasActive` is what makes this idempotent. The FIRST transition into
        // the OFF state stops the animation and blanks the matrix once. Every
        // later NONE -> NONE tick finds nothing active, so it neither stops nor
        // clears again, and the frame stays clean instead of dirty forever.
        const bool wasActive =
            manager_.isPlaying() || manager_.currentAnimation() != AnimationId::NONE;

        if (wasActive) {
            manager_.stop();
            ++stopCount_;

            // The whole point: logical NONE must mean physically black, not
            // "whatever the last animation happened to leave on the strip".
            // clear() only marks the frame dirty; App's showIfDirty() is still
            // the one and only place that transmits it.
            matrix_.clear();
            ++blankCount_;
        }

        applied_ = desired;
        hasApplied_ = true;
        return;
    }

    if (manager_.isPlaying() || manager_.currentAnimation() != desired.animationId) {
        manager_.play(desired.animationId, desired.playMode, nowMs);
        ++startCount_;
    }

    applied_ = desired;
    hasApplied_ = true;
}

AnimationId PlaybackPolicy::appliedAnimation() const {
    return applied_.animationId;
}

PlayMode PlaybackPolicy::appliedMode() const {
    return applied_.playMode;
}

SchedulePriority PlaybackPolicy::appliedPriority() const {
    return applied_.priority;
}

PlaybackReason PlaybackPolicy::appliedReason() const {
    return applied_.reason;
}

uint16_t PlaybackPolicy::startCount() const {
    return startCount_;
}

uint16_t PlaybackPolicy::stopCount() const {
    return stopCount_;
}

uint16_t PlaybackPolicy::blankCount() const {
    return blankCount_;
}

uint16_t PlaybackPolicy::unavailableCount() const {
    return unavailableCount_;
}

bool PlaybackPolicy::consumeUnavailableNotice(AnimationId& requested, AnimationId& applied) {
    if (!noticePending_) {
        return false;
    }

    noticePending_ = false;
    requested = noticeRequested_;
    applied = noticeApplied_;
    return true;
}
