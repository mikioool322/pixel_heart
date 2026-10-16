#pragma once

#include <stdint.h>

// Small POD civil-time value used by the scheduler layer.
//
// Conventions:
//   month   1..12
//   day     1..31
//   hour    0..23
//   minute  0..59
//   second  0..59
//   weekday 0 = Sunday .. 6 = Saturday
//
// No String, no dynamic allocation, trivially copyable.
struct ClockSnapshot {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t weekday;
    bool valid;
};

// Days since 1970-01-01 for a civil date (Howard Hinnant's algorithm).
// Provided so time providers can derive `weekday` without a date library.
inline int32_t civilToDays(int32_t year, uint8_t month, uint8_t day) {
    const int32_t y = year - (month <= 2u ? 1 : 0);
    const int32_t era = (y >= 0 ? y : y - 399) / 400;
    const uint32_t yoe = static_cast<uint32_t>(y - era * 400);          // 0..399
    const uint32_t m = static_cast<uint32_t>(month);
    const uint32_t doy = (153u * (m + (m > 2u ? -3u : 9u)) + 2u) / 5u + static_cast<uint32_t>(day) - 1u;
    const uint32_t doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;     // 0..146096

    return era * 146097 + static_cast<int32_t>(doe) - 719468;
}

inline uint8_t weekdayFromDays(int32_t days) {
    int32_t d = days % 7;
    if (d < 0) {
        d += 7;
    }

    // 1970-01-01 was a Thursday (4).
    return static_cast<uint8_t>((d + 4) % 7);
}

// Convenience builder for tests and development time sources.
inline ClockSnapshot makeClockSnapshot(
    uint16_t year,
    uint8_t month,
    uint8_t day,
    uint8_t hour,
    uint8_t minute,
    uint8_t second = 0u) {
    ClockSnapshot snapshot;
    snapshot.year = year;
    snapshot.month = month;
    snapshot.day = day;
    snapshot.hour = hour;
    snapshot.minute = minute;
    snapshot.second = second;
    snapshot.weekday = weekdayFromDays(civilToDays(year, month, day));
    snapshot.valid = true;
    return snapshot;
}

inline uint16_t clockMinuteOfDay(const ClockSnapshot& snapshot) {
    return static_cast<uint16_t>(snapshot.hour) * 60u + static_cast<uint16_t>(snapshot.minute);
}
