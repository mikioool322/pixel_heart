#pragma once

#include <stdint.h>

#include "core/AnimationId.h"
#include "core/PlayMode.h"
#include "scheduling/ManualOverride.h"
#include "scheduling/PlaybackDecision.h"
#include "scheduling/ScheduleTable.h"
#include "scheduling/SchedulerConfig.h"
#include "scheduling/SpecialDates.h"
#include "scheduling/TimeProvider.h"

// Decides WHAT should play, WHEN, and WHY.
//
// Scheduler knows only about a TimeProvider. It never calls millis(), WiFi,
// NTP or any ESP / Arduino time API. It never touches AnimationManager.
//
// evaluate() is a pure function of the clock and the manual override: there is
// no cache and no hidden state. Calling it a thousand times inside the same
// minute returns the same decision every time, and "do not restart the same
// animation" is PlaybackPolicy's job, not the Scheduler's.
//
// Resolution order:
//   MANUAL       > SPECIAL DATE > SCHEDULE > FALLBACK
//   with quiet hours producing NONE unless a rule or the user overrides them.
class Scheduler {
public:
    Scheduler(
        const TimeProvider& time,
        const ManualOverride& manual,
        const SchedulerConfig& config = SchedulerConfig());

    // Evaluate the current schedule. Safe to call at any frequency.
    PlaybackDecision evaluate(uint32_t nowMs);

    // Kept for API stability; there is no cached state to drop.
    void reset();

    const SchedulerConfig& config() const;

    static uint16_t minuteOfDayOf(const ClockSnapshot& snapshot);

    bool isQuietMinute(uint16_t minuteOfDay) const;

    // The one canonical "nothing specific is scheduled" animation.
    //
    // Public because PlaybackPolicy needs the SAME value when the scheduler asks
    // for something this build cannot render. Exposing it here keeps a single
    // definition: there is deliberately no second fallback constant anywhere.
    AnimationId fallbackAnimation() const;

private:
    PlaybackDecision fallbackDecision() const;

    const TimeProvider& time_;
    const ManualOverride& manual_;
    SchedulerConfig config_;
};
