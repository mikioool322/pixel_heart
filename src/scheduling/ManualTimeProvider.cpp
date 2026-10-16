#include "scheduling/ManualTimeProvider.h"

ManualTimeProvider::ManualTimeProvider() {
    value_.year = 0u;
    value_.month = 1u;
    value_.day = 1u;
    value_.hour = 0u;
    value_.minute = 0u;
    value_.second = 0u;
    value_.weekday = 4u;
    value_.valid = false;
}

void ManualTimeProvider::set(const ClockSnapshot& value) {
    value_ = value;
}

ClockSnapshot ManualTimeProvider::now() const {
    return value_;
}
