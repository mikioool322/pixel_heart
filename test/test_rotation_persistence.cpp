#include <unity.h>

#include "control/SettingsStore.h"
#include "control/UserSettings.h"
#include "core/DisplayRotation.h"
#include "fakes/FakeNvs.h"

namespace {

UserSettings makeSettings(ControlMode mode, AnimationId animation, uint8_t brightness, DisplayRotation rotation) {
    UserSettings settings;
    settings.mode = mode;
    settings.animation = animation;
    settings.brightnessPercent = brightness;
    settings.rotation = rotation;
    return settings;
}

const UserSettings kDefaults = UserSettingsDefaults::make();

// Writes a settings record directly, so a legacy or corrupt record can be
// simulated exactly.
void putByte(FakeNvs& nvs, const char* key, uint8_t value) {
    nvs.begin("pixelheart", false);
    nvs.putBytes(key, &value, 1u);
    nvs.end();
}

}  // namespace

// ---------------------------------------------------------------------------
// Rotation value helpers
// ---------------------------------------------------------------------------

void test_rotation_ordinal_round_trips(void) {
    const DisplayRotation all[] = {
        DisplayRotation::DEG_0,
        DisplayRotation::DEG_90,
        DisplayRotation::DEG_180,
        DisplayRotation::DEG_270,
    };

    for (uint8_t i = 0u; i < 4u; ++i) {
        const uint8_t ordinal = displayRotationToOrdinal(all[i]);
        TEST_ASSERT_EQUAL_UINT8(i, ordinal);
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(all[i]),
            static_cast<uint8_t>(displayRotationFromOrdinal(ordinal)));
        TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(i) * 90u, displayRotationDegrees(all[i]));
    }
}

void test_rotation_degrees_mapping(void) {
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(DisplayRotation::DEG_0),
        static_cast<uint8_t>(displayRotationFromDegrees(0u)));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(DisplayRotation::DEG_90),
        static_cast<uint8_t>(displayRotationFromDegrees(90u)));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(DisplayRotation::DEG_180),
        static_cast<uint8_t>(displayRotationFromDegrees(180u)));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(DisplayRotation::DEG_270),
        static_cast<uint8_t>(displayRotationFromDegrees(270u)));
}

void test_nonsense_rotation_input_falls_back_to_deg0(void) {
    // A hand-crafted POST must not be able to leave the panel undefined.
    const uint16_t nonsense[] = {1u, 45u, 89u, 91u, 360u, 65535u};
    for (uint8_t i = 0u; i < 6u; ++i) {
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(DisplayRotation::DEG_0),
            static_cast<uint8_t>(displayRotationFromDegrees(nonsense[i])));
    }

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(DisplayRotation::DEG_0),
        static_cast<uint8_t>(displayRotationFromOrdinal(4u)));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(DisplayRotation::DEG_0),
        static_cast<uint8_t>(displayRotationFromOrdinal(255u)));
}

void test_rotation_validity(void) {
    TEST_ASSERT_TRUE(isDisplayRotationValid(DisplayRotation::DEG_0));
    TEST_ASSERT_TRUE(isDisplayRotationValid(DisplayRotation::DEG_90));
    TEST_ASSERT_TRUE(isDisplayRotationValid(DisplayRotation::DEG_180));
    TEST_ASSERT_TRUE(isDisplayRotationValid(DisplayRotation::DEG_270));
    TEST_ASSERT_FALSE(isDisplayRotationValid(static_cast<DisplayRotation>(4u)));
    TEST_ASSERT_FALSE(isDisplayRotationValid(static_cast<DisplayRotation>(200u)));
}

void test_default_rotation_is_deg0(void) {
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(DisplayRotation::DEG_0),
        static_cast<uint8_t>(kDefaults.rotation));
    TEST_ASSERT_TRUE(isUserSettingsValid(kDefaults));
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

void test_rotation_survives_a_power_cycle_for_every_value(void) {
    const DisplayRotation all[] = {
        DisplayRotation::DEG_0,
        DisplayRotation::DEG_90,
        DisplayRotation::DEG_180,
        DisplayRotation::DEG_270,
    };

    for (uint8_t i = 0u; i < 4u; ++i) {
        FakeNvs nvs;

        {
            SettingsStoreLogic store(nvs);
            TEST_ASSERT_TRUE(store.save(
                makeSettings(ControlMode::AUTO, AnimationId::AURORA, 30u, all[i])));
        }

        // "Power cycle": a brand new store over the same bytes.
        {
            SettingsStoreLogic store(nvs);
            UserSettings loaded = kDefaults;
            TEST_ASSERT_TRUE(store.load(loaded));
            TEST_ASSERT_EQUAL_UINT8(
                static_cast<uint8_t>(all[i]),
                static_cast<uint8_t>(loaded.rotation));
        }
    }
}

void test_rotation_persists_alongside_the_other_settings(void) {
    FakeNvs nvs;

    {
        SettingsStoreLogic store(nvs);
        TEST_ASSERT_TRUE(store.save(
            makeSettings(ControlMode::MANUAL, AnimationId::CAMPFIRE, 66u, DisplayRotation::DEG_270)));
    }

    {
        SettingsStoreLogic store(nvs);
        UserSettings loaded = kDefaults;
        TEST_ASSERT_TRUE(store.load(loaded));

        // Nothing else was disturbed by adding rotation.
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(ControlMode::MANUAL),
            static_cast<uint8_t>(loaded.mode));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(AnimationId::CAMPFIRE),
            static_cast<uint8_t>(loaded.animation));
        TEST_ASSERT_EQUAL_UINT8(66u, loaded.brightnessPercent);
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(DisplayRotation::DEG_270),
            static_cast<uint8_t>(loaded.rotation));
    }
}

void test_legacy_record_without_rotation_still_loads(void) {
    // A record written before rotation existed: version + mode + animation +
    // brightness, and NO set_rot key. It must load, with rotation at DEG_0.
    FakeNvs nvs;
    putByte(nvs, "set_version", SettingsStoreKeys::kVersionValue);
    putByte(nvs, "set_mode", static_cast<uint8_t>(ControlMode::MANUAL));
    putByte(nvs, "set_anim", static_cast<uint8_t>(AnimationId::MOON_STARS));
    putByte(nvs, "set_bright", 44u);

    SettingsStoreLogic store(nvs);
    UserSettings loaded = kDefaults;
    TEST_ASSERT_TRUE(store.load(loaded));

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(DisplayRotation::DEG_0),
        static_cast<uint8_t>(loaded.rotation));
    // And the legacy values are honoured, not discarded.
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ControlMode::MANUAL),
        static_cast<uint8_t>(loaded.mode));
    TEST_ASSERT_EQUAL_UINT8(44u, loaded.brightnessPercent);
}

void test_corrupt_rotation_falls_back_to_deg0_without_losing_settings(void) {
    // A corrupt rotation must not throw away otherwise-good settings, unlike a
    // corrupt animation id which does invalidate the record.
    FakeNvs nvs;
    putByte(nvs, "set_version", SettingsStoreKeys::kVersionValue);
    putByte(nvs, "set_mode", static_cast<uint8_t>(ControlMode::MANUAL));
    putByte(nvs, "set_anim", static_cast<uint8_t>(AnimationId::CAMPFIRE));
    putByte(nvs, "set_bright", 55u);
    putByte(nvs, "set_rot", 200u);   // not a rotation

    SettingsStoreLogic store(nvs);
    UserSettings loaded = kDefaults;
    TEST_ASSERT_TRUE(store.load(loaded));

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(DisplayRotation::DEG_0),
        static_cast<uint8_t>(loaded.rotation));
    TEST_ASSERT_EQUAL_UINT8(55u, loaded.brightnessPercent);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(AnimationId::CAMPFIRE),
        static_cast<uint8_t>(loaded.animation));
}

void test_missing_settings_still_yield_deg0(void) {
    FakeNvs nvs;
    SettingsStoreLogic store(nvs);

    UserSettings loaded = kDefaults;
    TEST_ASSERT_FALSE(store.load(loaded));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(DisplayRotation::DEG_0),
        static_cast<uint8_t>(loaded.rotation));
}

void test_invalid_rotation_is_never_written(void) {
    FakeNvs nvs;
    SettingsStoreLogic store(nvs);

    UserSettings bad = makeSettings(
        ControlMode::AUTO, AnimationId::AURORA, 50u, static_cast<DisplayRotation>(9u));
    TEST_ASSERT_FALSE(store.save(bad));

    UserSettings loaded = kDefaults;
    TEST_ASSERT_FALSE(store.load(loaded));
}

void test_rotation_write_does_not_duplicate_keys(void) {
    // Saving twice must overwrite in place, exactly like the other settings.
    FakeNvs nvs;
    SettingsStoreLogic store(nvs);

    TEST_ASSERT_TRUE(store.save(
        makeSettings(ControlMode::AUTO, AnimationId::AURORA, 20u, DisplayRotation::DEG_90)));
    TEST_ASSERT_TRUE(store.save(
        makeSettings(ControlMode::AUTO, AnimationId::AURORA, 20u, DisplayRotation::DEG_270)));

    UserSettings loaded = kDefaults;
    TEST_ASSERT_TRUE(store.load(loaded));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(DisplayRotation::DEG_270),
        static_cast<uint8_t>(loaded.rotation));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_rotation_ordinal_round_trips);
    RUN_TEST(test_rotation_degrees_mapping);
    RUN_TEST(test_nonsense_rotation_input_falls_back_to_deg0);
    RUN_TEST(test_rotation_validity);
    RUN_TEST(test_default_rotation_is_deg0);
    RUN_TEST(test_rotation_survives_a_power_cycle_for_every_value);
    RUN_TEST(test_rotation_persists_alongside_the_other_settings);
    RUN_TEST(test_legacy_record_without_rotation_still_loads);
    RUN_TEST(test_corrupt_rotation_falls_back_to_deg0_without_losing_settings);
    RUN_TEST(test_missing_settings_still_yield_deg0);
    RUN_TEST(test_invalid_rotation_is_never_written);
    RUN_TEST(test_rotation_write_does_not_duplicate_keys);
    return UNITY_END();
}
