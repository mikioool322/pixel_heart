#pragma once

#include <stdint.h>
#include <time.h>

#include "scheduling/ClockSnapshot.h"

// Conversion helpers between a POSIX broken-down time and ClockSnapshot.
//
// Declared in a header free of ESP headers (struct tm comes from <time.h>, which
// exists on every target) so the mapping can be unit tested natively.
namespace TimeConversion {

// A system clock is only trusted once it is past this year. Guards against
// reporting the 1970 epoch as a valid civil time before SNTP has synced.
constexpr uint16_t kMinCredibleYear = 2024u;

inline ClockSnapshot fromBrokenDownTime(const struct tm& value, bool weekdayIsTrusted) {
    ClockSnapshot snapshot;

    const uint16_t year = static_cast<uint16_t>(value.tm_year + 1900);
    const uint8_t month = static_cast<uint8_t>(value.tm_mon + 1);
    const uint8_t day = static_cast<uint8_t>(value.tm_mday);

    snapshot.year = year;
    snapshot.month = month;
    snapshot.day = day;
    snapshot.hour = static_cast<uint8_t>(value.tm_hour);
    snapshot.minute = static_cast<uint8_t>(value.tm_min);
    snapshot.second = static_cast<uint8_t>(value.tm_sec);

    // tm_wday already uses 0 = Sunday, which matches the scheduler convention.
    snapshot.weekday = weekdayIsTrusted
        ? static_cast<uint8_t>(value.tm_wday)
        : weekdayFromDays(civilToDays(year, month, day));

    const bool plausible =
        month >= 1u && month <= 12u &&
        day >= 1u && day <= 31u &&
        snapshot.hour <= 23u &&
        snapshot.minute <= 59u &&
        snapshot.second <= 59u;

    snapshot.valid = plausible && year >= kMinCredibleYear;
    return snapshot;
}

}  // namespace TimeConversion
