#pragma once

#include "scheduling/TimeProvider.h"

// Controllable clock source for native tests, desktop simulation and
// temporary hardware development. Not the future NTP implementation.
class ManualTimeProvider final : public TimeProvider {
public:
    ManualTimeProvider();

    void set(const ClockSnapshot& value);
    ClockSnapshot now() const override;

private:
    ClockSnapshot value_;
};
