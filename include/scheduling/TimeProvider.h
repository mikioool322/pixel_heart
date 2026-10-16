#pragma once

#include "scheduling/ClockSnapshot.h"

// The only clock abstraction the scheduler is allowed to see.
//
// Implementations own whatever hardware / network / simulation source they
// need. Scheduler itself never calls millis(), WiFi, NTP or ESP APIs.
class TimeProvider {
public:
    virtual ~TimeProvider() = default;

    virtual ClockSnapshot now() const = 0;
};
