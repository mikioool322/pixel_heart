#pragma once

#include <stdint.h>

// Shared scheduling configuration.
//
// Contains ONLY the quiet-hours window plus the minute-of-day helper used by
// the schedule and special-date tables. What plays *when* lives in
// ScheduleTable.h; which dates are special lives in SpecialDates.h. There is
// deliberately no second place to configure playback.
namespace SchedulerDefaults {

// Quiet hours: the panel is dark. Nothing else overrides this except a manual
// selection or a special date explicitly marked as allowed to.
constexpr uint16_t kQuietStartMinute = 0u;    // 00:00
constexpr uint16_t kQuietEndMinute = 360u;    // 06:00

}  // namespace SchedulerDefaults

// Minute-of-day from an hour/minute pair, so configuration tables read as
// clock times. hour may be 24 to mean "end of day" (1440).
constexpr uint16_t at(uint8_t hour, uint8_t minute) {
    return static_cast<uint16_t>(hour) * 60u + static_cast<uint16_t>(minute);
}

constexpr uint16_t kEndOfDay = at(24u, 0u);

struct SchedulerConfig {
    uint16_t quietStartMinute = SchedulerDefaults::kQuietStartMinute;
    uint16_t quietEndMinute = SchedulerDefaults::kQuietEndMinute;
};
