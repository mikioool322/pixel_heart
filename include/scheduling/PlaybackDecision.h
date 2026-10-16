#pragma once

#include <stdint.h>

#include "core/AnimationId.h"
#include "core/PlayMode.h"
#include "scheduling/SchedulePriority.h"

enum class PlaybackReason : uint8_t {
    FALLBACK,
    SCHEDULE,
    QUIET_HOURS,
    SEASONAL,
    SPECIAL_EVENT,
    MANUAL_OVERRIDE
};

// The scheduler's answer to "what should be playing right now".
struct PlaybackDecision {
    AnimationId animationId;
    PlayMode playMode;
    uint32_t holdDurationMs;
    SchedulePriority priority;
    PlaybackReason reason;
};
