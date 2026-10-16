#pragma once

#include <stdint.h>

#include "core/AnimationId.h"
#include "core/PlayMode.h"
#include "scheduling/PlaybackDecision.h"
#include "scheduling/SchedulePriority.h"

enum class ManualOverrideMode : uint8_t {
    INACTIVE,
    TIMED,
    UNTIL_CANCELLED
};

// User-driven "play this now" state. Lives above the Scheduler in the
// priority order but is owned by whoever drives playback.
class ManualOverride {
public:
    ManualOverride();

    void clear();

    void activateUntilCancelled(AnimationId animationId, PlayMode playMode);

    void activateTimed(
        AnimationId animationId,
        PlayMode playMode,
        uint32_t durationMs,
        uint32_t nowMs);

    // Convenience helpers for the supported timed presets.
    void activateFor5Minutes(AnimationId animationId, PlayMode playMode, uint32_t nowMs);
    void activateFor15Minutes(AnimationId animationId, PlayMode playMode, uint32_t nowMs);
    void activateFor60Minutes(AnimationId animationId, PlayMode playMode, uint32_t nowMs);

    bool active(uint32_t nowMs) const;
    ManualOverrideMode mode() const;
    uint32_t remainingMs(uint32_t nowMs) const;

    PlaybackDecision decision() const;

private:
    ManualOverrideMode mode_;
    AnimationId animationId_;
    PlayMode playMode_;
    uint32_t startedAtMs_;
    uint32_t durationMs_;
};
