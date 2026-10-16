#pragma once

#include <stdint.h>

#include "scheduling/TimeProvider.h"
#include "time/TimeConversion.h"

#if defined(ARDUINO_ARCH_ESP32)

// NTP / system-clock backed TimeProvider.
//
// Responsibilities:
//   - configure the timezone once
//   - hand the NTP server list to the ESP32 system-time machinery
//   - read local civil time
//   - report valid == false until a credible clock exists
//
// It deliberately does NOT know about Wi-Fi credentials or the captive portal.
// The App starts it once the network reports a connection.
class NtpTimeProvider final : public TimeProvider {
public:
    NtpTimeProvider();

    // Configures timezone + NTP servers. Idempotent: only the first call has
    // an effect, so App may call it defensively every tick.
    void begin();

    // True once begin() has configured the system time machinery.
    bool started() const;

    // True once a credible civil time has been observed. Latched from now(),
    // hence the mutable member: observing the clock is a read, but it also
    // records that a credible clock has been seen at least once this boot.
    bool synchronized() const;

    ClockSnapshot now() const override;

private:
    bool started_;
    mutable bool synchronized_;
};

#endif  // defined(ARDUINO_ARCH_ESP32)
