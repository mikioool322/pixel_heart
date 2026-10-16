#include <unity.h>

#include "control/AnimationCatalog.h"
#include "control/UserSettings.h"

// Phase 1 control logic. The LAN panel and App are ESP-only glue, so the
// testable surface is: the animation catalog arithmetic, the brightness
// mapping, and settings validation. Those are exactly the places a bug would
// turn into a wrong animation or a black panel.

void test_catalog_covers_every_animation_except_none(void) {
    // NONE means "nothing playing" and must never be selectable.
    TEST_ASSERT_FALSE(AnimationCatalog::isSelectable(AnimationId::NONE));
    TEST_ASSERT_EQUAL_UINT8(AnimationCatalog::kNoIndex, AnimationCatalog::indexOf(AnimationId::NONE));

    TEST_ASSERT_TRUE(AnimationCatalog::isSelectable(AnimationId::HEARTBEAT));
    TEST_ASSERT_TRUE(AnimationCatalog::isSelectable(AnimationId::SUN_OVER_MOUNTAINS));

    // Every id except NONE is selectable, and count agrees with the enum.
    uint8_t seen = 0u;
    for (uint8_t raw = 1u; raw < static_cast<uint8_t>(AnimationId::ANIMATION_ID_COUNT); ++raw) {
        TEST_ASSERT_TRUE(AnimationCatalog::isSelectable(static_cast<AnimationId>(raw)));
        ++seen;
    }
    TEST_ASSERT_EQUAL_UINT8(AnimationCatalog::kSelectableCount, seen);
}

void test_catalog_index_round_trips(void) {
    for (uint8_t index = 0u; index < AnimationCatalog::kSelectableCount; ++index) {
        const AnimationId id = AnimationCatalog::idAt(index);
        TEST_ASSERT_TRUE(AnimationCatalog::isSelectable(id));
        TEST_ASSERT_EQUAL_UINT8(index, AnimationCatalog::indexOf(id));
    }

    // Out-of-range index yields NONE rather than undefined behaviour.
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(AnimationId::NONE),
        static_cast<uint8_t>(AnimationCatalog::idAt(AnimationCatalog::kSelectableCount)));
}

void test_catalog_step_wraps_both_directions(void) {
    const AnimationId first = AnimationCatalog::idAt(0u);
    const AnimationId last = AnimationCatalog::idAt(
        static_cast<uint8_t>(AnimationCatalog::kSelectableCount - 1u));

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(AnimationCatalog::idAt(1u)),
        static_cast<uint8_t>(AnimationCatalog::step(first, 1)));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(last),
        static_cast<uint8_t>(AnimationCatalog::step(first, -1)));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(first),
        static_cast<uint8_t>(AnimationCatalog::step(last, 1)));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(AnimationCatalog::idAt(
            static_cast<uint8_t>(AnimationCatalog::kSelectableCount - 2u))),
        static_cast<uint8_t>(AnimationCatalog::step(last, -1)));

    // A full lap in either direction returns to the starting animation.
    AnimationId walk = first;
    for (uint8_t i = 0u; i < AnimationCatalog::kSelectableCount; ++i) {
        walk = AnimationCatalog::step(walk, 1);
    }
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(first), static_cast<uint8_t>(walk));

    walk = first;
    for (uint8_t i = 0u; i < AnimationCatalog::kSelectableCount; ++i) {
        walk = AnimationCatalog::step(walk, -1);
    }
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(first), static_cast<uint8_t>(walk));
}

void test_catalog_step_from_none_starts_at_first(void) {
    // NONE is not selectable, so stepping from it must land on a real
    // animation rather than on NONE again or out of range.
    const AnimationId forward = AnimationCatalog::step(AnimationId::NONE, 1);
    const AnimationId backward = AnimationCatalog::step(AnimationId::NONE, -1);
    TEST_ASSERT_TRUE(AnimationCatalog::isSelectable(forward));
    TEST_ASSERT_TRUE(AnimationCatalog::isSelectable(backward));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AnimationId::HEARTBEAT), static_cast<uint8_t>(forward));
}

void test_brightness_maps_between_percent_and_raw(void) {
    TEST_ASSERT_EQUAL_UINT8(0u, brightnessPercentToRaw(0u));
    TEST_ASSERT_EQUAL_UINT8(255u, brightnessPercentToRaw(100u));
    TEST_ASSERT_EQUAL_UINT8(127u, brightnessPercentToRaw(50u));
}

void test_brightness_percent_round_trips_for_every_step(void) {
    // The UI stores percent, the matrix wants raw. Every percent value must
    // survive the round trip, or the panel would drift each time it is shown.
    for (uint8_t percent = 0u; percent <= 100u; ++percent) {
        const uint8_t raw = brightnessPercentToRaw(percent);
        // Re-derive percent the way the default constant does.
        const uint8_t back = static_cast<uint8_t>(
            (static_cast<uint16_t>(raw) * 100u + 127u) / 255u);
        TEST_ASSERT_EQUAL_UINT8(percent, back);
    }
}

void test_default_brightness_matches_the_board_default(void) {
    // One source of truth: the percent default is derived from the board's raw
    // default, so the startup brightness is unchanged by this phase.
    const uint8_t expected = static_cast<uint8_t>(
        (static_cast<uint16_t>(HardwareConfig::DEFAULT_BRIGHTNESS) * 100u + 127u) / 255u);
    TEST_ASSERT_EQUAL_UINT8(expected, UserSettingsDefaults::kBrightnessPercent);
    TEST_ASSERT_EQUAL_UINT8(
        HardwareConfig::DEFAULT_BRIGHTNESS,
        brightnessPercentToRaw(UserSettingsDefaults::kBrightnessPercent));
}

void test_brightness_is_clamped_to_the_valid_range(void) {
    TEST_ASSERT_EQUAL_UINT8(0u, clampBrightnessPercent(-50));
    TEST_ASSERT_EQUAL_UINT8(0u, clampBrightnessPercent(-1));
    TEST_ASSERT_EQUAL_UINT8(0u, clampBrightnessPercent(0));
    TEST_ASSERT_EQUAL_UINT8(37u, clampBrightnessPercent(37));
    TEST_ASSERT_EQUAL_UINT8(100u, clampBrightnessPercent(100));
    TEST_ASSERT_EQUAL_UINT8(100u, clampBrightnessPercent(101));
    TEST_ASSERT_EQUAL_UINT8(100u, clampBrightnessPercent(1000));

    // Repeated +1 from the top must not wrap around to a dark panel.
    uint8_t percent = 100u;
    percent = clampBrightnessPercent(static_cast<int>(percent) + 1);
    TEST_ASSERT_EQUAL_UINT8(100u, percent);

    percent = 0u;
    percent = clampBrightnessPercent(static_cast<int>(percent) - 1);
    TEST_ASSERT_EQUAL_UINT8(0u, percent);
}

void test_defaults_are_valid_and_auto(void) {
    const UserSettings settings = UserSettingsDefaults::make();
    TEST_ASSERT_TRUE(isUserSettingsValid(settings));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ControlMode::AUTO),
        static_cast<uint8_t>(settings.mode));
    TEST_ASSERT_TRUE(AnimationCatalog::isSelectable(settings.animation));
    TEST_ASSERT_TRUE(settings.brightnessPercent <= 100u);
}

void test_settings_validation_rejects_impossible_values(void) {
    UserSettings settings = UserSettingsDefaults::make();
    TEST_ASSERT_TRUE(isUserSettingsValid(settings));

    // NONE is not a playable selection.
    settings.animation = AnimationId::NONE;
    TEST_ASSERT_FALSE(isUserSettingsValid(settings));

    // Out-of-range animation id.
    settings.animation = static_cast<AnimationId>(200u);
    TEST_ASSERT_FALSE(isUserSettingsValid(settings));

    // Brightness above 100.
    settings = UserSettingsDefaults::make();
    settings.brightnessPercent = 101u;
    TEST_ASSERT_FALSE(isUserSettingsValid(settings));

    // Unknown mode value.
    settings = UserSettingsDefaults::make();
    settings.mode = static_cast<ControlMode>(7u);
    TEST_ASSERT_FALSE(isUserSettingsValid(settings));
}

void test_idle_control_request_does_nothing(void) {
    const ControlRequest request = makeIdleControlRequest();
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ControlAction::NONE),
        static_cast<uint8_t>(request.action));
    TEST_ASSERT_EQUAL_UINT8(0u, request.brightnessPercent);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_catalog_covers_every_animation_except_none);
    RUN_TEST(test_catalog_index_round_trips);
    RUN_TEST(test_catalog_step_wraps_both_directions);
    RUN_TEST(test_catalog_step_from_none_starts_at_first);
    RUN_TEST(test_brightness_maps_between_percent_and_raw);
    RUN_TEST(test_brightness_percent_round_trips_for_every_step);
    RUN_TEST(test_default_brightness_matches_the_board_default);
    RUN_TEST(test_brightness_is_clamped_to_the_valid_range);
    RUN_TEST(test_defaults_are_valid_and_auto);
    RUN_TEST(test_settings_validation_rejects_impossible_values);
    RUN_TEST(test_idle_control_request_does_nothing);
    return UNITY_END();
}
