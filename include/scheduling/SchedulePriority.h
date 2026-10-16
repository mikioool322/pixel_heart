#pragma once

#include <stdint.h>

// Higher numeric value wins.
enum class SchedulePriority : uint8_t {
    FALLBACK = 10,
    TIME_OF_DAY = 30,
    SEASONAL = 50,
    SPECIAL_EVENT = 70,
    MANUAL = 100
};
