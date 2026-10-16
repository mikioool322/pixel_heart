#include "time/NtpTimeProvider.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <Arduino.h>
#include <time.h>

#include "time/TimeConversion.h"

namespace {
// Central European Time with automatic daylight saving.
// Winter = CET (UTC+1), summer = CEST (UTC+2).
// The POSIX TZ string lets libc handle every DST transition; no season logic
// is implemented anywhere in this project.
constexpr const char* kTimezone = "CET-1CEST,M3.5.0,M10.5.0/3";

constexpr const char* kNtpServer1 = "pool.ntp.org";
constexpr const char* kNtpServer2 = "time.google.com";
constexpr const char* kNtpServer3 = "time.cloudflare.com";

// Pre-SNTP / untrustworthy clock. Shaped like a real date so any accidental
// use stays harmless, but flagged invalid so the Scheduler takes its fallback.
ClockSnapshot invalidClockSnapshot() {
    ClockSnapshot snapshot;
    snapshot.year = 0u;
    snapshot.month = 1u;
    snapshot.day = 1u;
    snapshot.hour = 0u;
    snapshot.minute = 0u;
    snapshot.second = 0u;
    snapshot.weekday = 4u;
    snapshot.valid = false;
    return snapshot;
}
}  // namespace

NtpTimeProvider::NtpTimeProvider()
    : started_(false),
      synchronized_(false) {
}

void NtpTimeProvider::begin() {
    if (started_) {
        return;
    }

    started_ = true;

    // configTzTime both sets the SNTP servers and installs the POSIX TZ rule,
    // so localtime() yields CET/CEST correctly across DST boundaries.
    configTzTime(kTimezone, kNtpServer1, kNtpServer2, kNtpServer3);
}

bool NtpTimeProvider::started() const {
    return started_;
}

bool NtpTimeProvider::synchronized() const {
    return synchronized_;
}

ClockSnapshot NtpTimeProvider::now() const {
    if (!started_) {
        return invalidClockSnapshot();
    }

    const time_t nowSecs = time(nullptr);
    if (nowSecs <= 0) {
        return invalidClockSnapshot();
    }

    struct tm local;
    if (localtime_r(&nowSecs, &local) == nullptr) {
        return invalidClockSnapshot();
    }

    ClockSnapshot snapshot = TimeConversion::fromBrokenDownTime(local, true);

    if (snapshot.valid) {
        synchronized_ = true;
    }

    return snapshot;
}

#endif  // defined(ARDUINO_ARCH_ESP32)
