#include <unity.h>

#include <string.h>
#include <time.h>

#include "time/TimeConversion.h"

namespace {
struct tm makeTm(int year, int mon0, int mday, int hour, int min, int sec, int wday) {
    struct tm value;
    memset(&value, 0, sizeof(value));
    value.tm_year = year - 1900;
    value.tm_mon = mon0;
    value.tm_mday = mday;
    value.tm_hour = hour;
    value.tm_min = min;
    value.tm_sec = sec;
    value.tm_wday = wday;
    return value;
}
}  // namespace

void test_month_is_offset_from_zero_based_tm_mon(void) {
    // tm_mon is 0-based; ClockSnapshot.month is 1-based.
    const struct tm january = makeTm(2026, 0, 15, 8u, 30u, 0u, 4u);
    const ClockSnapshot s1 = TimeConversion::fromBrokenDownTime(january, true);
    TEST_ASSERT_EQUAL_UINT8(1u, s1.month);

    const struct tm december = makeTm(2026, 11, 24, 8u, 30u, 0u, 4u);
    const ClockSnapshot s2 = TimeConversion::fromBrokenDownTime(december, true);
    TEST_ASSERT_EQUAL_UINT8(12u, s2.month);
}

void test_year_day_and_time_fields_map_directly(void) {
    const struct tm value = makeTm(2026, 8u, 30u, 20u, 14u, 32u, 3u);
    const ClockSnapshot snapshot = TimeConversion::fromBrokenDownTime(value, true);

    TEST_ASSERT_EQUAL_UINT16(2026u, snapshot.year);
    TEST_ASSERT_EQUAL_UINT8(9u, snapshot.month);
    TEST_ASSERT_EQUAL_UINT8(30u, snapshot.day);
    TEST_ASSERT_EQUAL_UINT8(20u, snapshot.hour);
    TEST_ASSERT_EQUAL_UINT8(14u, snapshot.minute);
    TEST_ASSERT_EQUAL_UINT8(32u, snapshot.second);
    TEST_ASSERT_TRUE(snapshot.valid);
}

void test_tm_wday_matches_scheduler_weekday_convention(void) {
    // tm_wday already uses 0 = Sunday, which is what the scheduler expects.
    const char* expected[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

    for (int wday = 0; wday < 7; ++wday) {
        const struct tm value = makeTm(2026, 8u, 30u, 12u, 0u, 0u, wday);
        const ClockSnapshot snapshot = TimeConversion::fromBrokenDownTime(value, true);
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(wday), snapshot.weekday);
        (void)expected[wday];
    }
}

void test_weekday_can_be_derived_when_untrusted(void) {
    // When the caller cannot vouch for tm_wday, the date itself is used.
    // 2026-09-30 is a Wednesday, i.e. tm_wday 3.
    const struct tm value = makeTm(2026, 8u, 30u, 12u, 0u, 0u, 99);
    const ClockSnapshot untrusted = TimeConversion::fromBrokenDownTime(value, false);
    TEST_ASSERT_EQUAL_UINT8(3u, untrusted.weekday);
    TEST_ASSERT_TRUE(untrusted.valid);

    const ClockSnapshot trusted = TimeConversion::fromBrokenDownTime(value, true);
    TEST_ASSERT_EQUAL_UINT8(99u, trusted.weekday);
}

void test_epoch_and_pre_2024_are_invalid(void) {
    // The 1970 epoch must never be presented as a valid civil time.
    const struct tm epoch = makeTm(1970, 0, 1u, 0u, 0u, 0u, 4u);
    const ClockSnapshot snapshot = TimeConversion::fromBrokenDownTime(epoch, true);
    TEST_ASSERT_FALSE(snapshot.valid);

    const struct tm justBefore = makeTm(2023, 11u, 31u, 23u, 59u, 59u, 0u);
    TEST_ASSERT_FALSE(TimeConversion::fromBrokenDownTime(justBefore, true).valid);

    const struct tm threshold = makeTm(2024, 0, 1u, 0u, 0u, 0u, 1u);
    TEST_ASSERT_TRUE(TimeConversion::fromBrokenDownTime(threshold, true).valid);
}

void test_implausible_field_ranges_are_invalid(void) {
    const struct tm badMonth = makeTm(2026, 13u, 1u, 0u, 0u, 0u, 4u);
    TEST_ASSERT_FALSE(TimeConversion::fromBrokenDownTime(badMonth, true).valid);

    const struct tm badHour = makeTm(2026, 0, 1u, 25u, 0u, 0u, 4u);
    TEST_ASSERT_FALSE(TimeConversion::fromBrokenDownTime(badHour, true).valid);

    const struct tm badMinute = makeTm(2026, 0, 1u, 0u, 60u, 0u, 4u);
    TEST_ASSERT_FALSE(TimeConversion::fromBrokenDownTime(badMinute, true).valid);

    const struct tm badDay = makeTm(2026, 0, 0u, 0u, 0u, 0u, 4u);
    TEST_ASSERT_FALSE(TimeConversion::fromBrokenDownTime(badDay, true).valid);
}

void test_round_trip_matches_libc_for_a_polish_summer_date(void) {
    // CEST: UTC+2 in summer. 2026-07-15 20:00 local == 18:00 UTC == 1784138400.
    //
    // POSIX localtime_r is not part of the Windows C runtime, so the libc
    // round-trip is only asserted where it exists. The field mapping itself is
    // covered by the host-independent tests above.
#if defined(_WIN32)
    TEST_IGNORE_MESSAGE("localtime_r unavailable on the Windows CRT");
#else
    const time_t utc = 1784138400;
    struct tm local;
    TEST_ASSERT_NOT_NULL(localtime_r(&utc, &local));

    const ClockSnapshot snapshot = TimeConversion::fromBrokenDownTime(local, true);
    TEST_ASSERT_EQUAL_UINT16(2026u, snapshot.year);
    TEST_ASSERT_EQUAL_UINT8(7u, snapshot.month);
    TEST_ASSERT_EQUAL_UINT8(15u, snapshot.day);
    TEST_ASSERT_TRUE(snapshot.valid);
#endif
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_month_is_offset_from_zero_based_tm_mon);
    RUN_TEST(test_year_day_and_time_fields_map_directly);
    RUN_TEST(test_tm_wday_matches_scheduler_weekday_convention);
    RUN_TEST(test_weekday_can_be_derived_when_untrusted);
    RUN_TEST(test_epoch_and_pre_2024_are_invalid);
    RUN_TEST(test_implausible_field_ranges_are_invalid);
    RUN_TEST(test_round_trip_matches_libc_for_a_polish_summer_date);
    return UNITY_END();
}
