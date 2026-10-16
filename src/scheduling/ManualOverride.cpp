#include "scheduling/ManualOverride.h"

namespace {
constexpr uint32_t kFiveMinutesMs = 5UL * 60UL * 1000UL;
constexpr uint32_t kFifteenMinutesMs = 15UL * 60UL * 1000UL;
constexpr uint32_t kSixtyMinutesMs = 60UL * 60UL * 1000UL;

uint32_t elapsedSince(uint32_t nowMs, uint32_t startMs) {
    // Unsigned arithmetic is wrap-safe for millis() rollover.
    return static_cast<uint32_t>(nowMs - startMs);
}
}  // namespace

ManualOverride::ManualOverride()
    : mode_(ManualOverrideMode::INACTIVE),
      animationId_(AnimationId::NONE),
      playMode_(PlayMode::LOOP),
      startedAtMs_(0u),
      durationMs_(0u) {
}

void ManualOverride::clear() {
    mode_ = ManualOverrideMode::INACTIVE;
    animationId_ = AnimationId::NONE;
    playMode_ = PlayMode::LOOP;
    startedAtMs_ = 0u;
    durationMs_ = 0u;
}

void ManualOverride::activateUntilCancelled(AnimationId animationId, PlayMode playMode) {
    mode_ = ManualOverrideMode::UNTIL_CANCELLED;
    animationId_ = animationId;
    playMode_ = playMode;
    startedAtMs_ = 0u;
    durationMs_ = 0u;
}

void ManualOverride::activateTimed(
    AnimationId animationId,
    PlayMode playMode,
    uint32_t durationMs,
    uint32_t nowMs) {
    mode_ = ManualOverrideMode::TIMED;
    animationId_ = animationId;
    playMode_ = playMode;
    startedAtMs_ = nowMs;
    durationMs_ = durationMs;
}

void ManualOverride::activateFor5Minutes(
    AnimationId animationId,
    PlayMode playMode,
    uint32_t nowMs) {
    activateTimed(animationId, playMode, kFiveMinutesMs, nowMs);
}

void ManualOverride::activateFor15Minutes(
    AnimationId animationId,
    PlayMode playMode,
    uint32_t nowMs) {
    activateTimed(animationId, playMode, kFifteenMinutesMs, nowMs);
}

void ManualOverride::activateFor60Minutes(
    AnimationId animationId,
    PlayMode playMode,
    uint32_t nowMs) {
    activateTimed(animationId, playMode, kSixtyMinutesMs, nowMs);
}

bool ManualOverride::active(uint32_t nowMs) const {
    switch (mode_) {
        case ManualOverrideMode::INACTIVE:
            return false;
        case ManualOverrideMode::TIMED:
            return elapsedSince(nowMs, startedAtMs_) < durationMs_;
        case ManualOverrideMode::UNTIL_CANCELLED:
            return true;
        default:
            return false;
    }
}

ManualOverrideMode ManualOverride::mode() const {
    return mode_;
}

uint32_t ManualOverride::remainingMs(uint32_t nowMs) const {
    switch (mode_) {
        case ManualOverrideMode::INACTIVE:
            return 0u;
        case ManualOverrideMode::UNTIL_CANCELLED:
            return 0u;
        case ManualOverrideMode::TIMED: {
            const uint32_t elapsed = elapsedSince(nowMs, startedAtMs_);
            return elapsed >= durationMs_ ? 0u : static_cast<uint32_t>(durationMs_ - elapsed);
        }
        default:
            return 0u;
    }
}

PlaybackDecision ManualOverride::decision() const {
    PlaybackDecision decision;
    decision.animationId = animationId_;
    decision.playMode = playMode_;
    decision.priority = SchedulePriority::MANUAL;
    decision.reason = PlaybackReason::MANUAL_OVERRIDE;

    switch (mode_) {
        case ManualOverrideMode::TIMED:
            decision.holdDurationMs = durationMs_;
            break;
        case ManualOverrideMode::UNTIL_CANCELLED:
            decision.holdDurationMs = 0u;
            break;
        case ManualOverrideMode::INACTIVE:
        default:
            decision.holdDurationMs = 0u;
            decision.animationId = AnimationId::NONE;
            decision.priority = SchedulePriority::FALLBACK;
            decision.reason = PlaybackReason::FALLBACK;
            break;
    }

    return decision;
}
