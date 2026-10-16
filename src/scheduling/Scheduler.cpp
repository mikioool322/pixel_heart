#include "scheduling/Scheduler.h"

#include "scheduling/ClockSnapshot.h"

namespace {

// Every scheduled decision holds for the whole range, so the duration is
// informational only. PlaybackPolicy does not use it to time anything; the
// clock does.
constexpr uint32_t kScheduleHoldMs = 0u;

PlaybackDecision makeDecision(
    AnimationId animation,
    SchedulePriority priority,
    PlaybackReason reason) {
    PlaybackDecision decision;
    decision.animationId = animation;
    decision.playMode = PlayMode::LOOP;
    decision.holdDurationMs = kScheduleHoldMs;
    decision.priority = priority;
    decision.reason = reason;
    return decision;
}

// Seconds elapsed since the start of a time window, derived from civil time
// only. The window is identified by its start minute plus its end minute, so
// an overnight window such as 20:00 -> 06:00 is handled by the same code the
// schedule table already uses.
//
// No millis(), no stored timestamp, and correct across midnight because the
// wrap is computed here rather than inferred from a counter that could roll
// over or be reset by a reboot.
uint32_t secondsSinceWindowStart(
    uint16_t startMinute,
    uint16_t endMinute,
    const ClockSnapshot& snapshot,
    uint16_t minuteOfDay) {
    uint32_t minutesSince = 0u;

    if (endMinute <= startMinute) {
        // Overnight window such as 20:00 -> 06:00.
        minutesSince = (minuteOfDay >= startMinute)
            ? static_cast<uint32_t>(minuteOfDay - startMinute)
            : static_cast<uint32_t>(minuteOfDay) + 1440u - startMinute;
    } else if (minuteOfDay >= startMinute) {
        minutesSince = static_cast<uint32_t>(minuteOfDay - startMinute);
    }

    return minutesSince * 60u + static_cast<uint32_t>(snapshot.second);
}

// The ONE place a playlist is resolved to a concrete animation.
//
// Shared by the schedule table and the special-date table so both time a
// sequence from their own window's start and behave identically. A playlist row
// therefore restarts naturally when its block does.
AnimationId animationForWindow(
    PlaylistId playlist,
    AnimationId fallback,
    uint16_t startMinute,
    uint16_t endMinute,
    const ClockSnapshot& snapshot,
    uint16_t minuteOfDay) {
    if (playlist == PlaylistId::NONE) {
        return fallback;
    }

    const Playlist* found = Playlists::find(playlist);
    if (found == nullptr) {
        return fallback;
    }

    const PlaylistItem* item = Playlists::itemAt(
        *found,
        secondsSinceWindowStart(startMinute, endMinute, snapshot, minuteOfDay));

    if (item == nullptr) {
        // A non-looping playlist that has finished. Fall back to whatever the
        // row names, so the panel never goes dark by accident.
        return fallback;
    }

    return item->animation;
}

// Resolves a schedule row to a concrete animation.
AnimationId animationForRow(const ScheduleEntry& entry, const ClockSnapshot& snapshot, uint16_t minuteOfDay) {
    return animationForWindow(
        entry.playlist,
        entry.animation,
        entry.startMinute,
        entry.endMinute,
        snapshot,
        minuteOfDay);
}

}  // namespace

Scheduler::Scheduler(
    const TimeProvider& time,
    const ManualOverride& manual,
    const SchedulerConfig& config)
    : time_(time),
      manual_(manual),
      config_(config) {
}

void Scheduler::reset() {
    // No cached state exists: evaluate() derives everything from the clock.
}

const SchedulerConfig& Scheduler::config() const {
    return config_;
}

uint16_t Scheduler::minuteOfDayOf(const ClockSnapshot& snapshot) {
    return clockMinuteOfDay(snapshot);
}

bool Scheduler::isQuietMinute(uint16_t minuteOfDay) const {
    const uint16_t start = config_.quietStartMinute;
    const uint16_t end = config_.quietEndMinute;

    if (start == end) {
        return false;
    }

    if (start < end) {
        return minuteOfDay >= start && minuteOfDay < end;
    }

    // Wraps past midnight, e.g. 23:00 -> 07:00.
    return minuteOfDay >= start || minuteOfDay < end;
}

AnimationId Scheduler::fallbackAnimation() const {
    // AURORA is the project's agreed stand-in: it is what plays before NTP
    // produces a real date and time, and what plays when no schedule row matches.
    return AnimationId::AURORA;
}

PlaybackDecision Scheduler::fallbackDecision() const {
    return makeDecision(
        fallbackAnimation(),
        SchedulePriority::FALLBACK,
        PlaybackReason::FALLBACK);
}

PlaybackDecision Scheduler::evaluate(uint32_t nowMs) {
    // 1. Manual always wins, including over quiet hours.
    if (manual_.active(nowMs)) {
        return manual_.decision();
    }

    const ClockSnapshot snapshot = time_.now();

    // 2. No trustworthy clock: never guess a date.
    if (!snapshot.valid) {
        return fallbackDecision();
    }

    const uint16_t minuteOfDay = minuteOfDayOf(snapshot);
    const bool quiet = isQuietMinute(minuteOfDay);

    // 3. Special dates beat the normal schedule. A special date may name a
    //    playlist, resolved by the same code the schedule table uses.
    const SpecialDate* special = SpecialDates::resolve(snapshot, minuteOfDay, quiet);
    if (special != nullptr) {
        return makeDecision(
            animationForWindow(
                special->playlist,
                special->animation,
                special->startMinute,
                special->endMinute,
                snapshot,
                minuteOfDay),
            SchedulePriority::SPECIAL_EVENT,
            PlaybackReason::SPECIAL_EVENT);
    }

    // 4. Quiet hours: the panel is dark.
    if (quiet) {
        return makeDecision(
            AnimationId::NONE,
            SchedulePriority::TIME_OF_DAY,
            PlaybackReason::QUIET_HOURS);
    }

    // 5. The schedule table.
    const ScheduleEntry* row = ScheduleTable::resolveEntry(minuteOfDay);
    if (row != nullptr) {
        return makeDecision(
            animationForRow(*row, snapshot, minuteOfDay),
            SchedulePriority::TIME_OF_DAY,
            PlaybackReason::SCHEDULE);
    }

    // 6. A gap in the schedule: hold the fallback rather than going dark.
    return fallbackDecision();
}
