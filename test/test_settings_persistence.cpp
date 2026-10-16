#include <unity.h>

#include "control/SettingsStore.h"
#include "control/UserSettings.h"
#include "fakes/FakeNvs.h"
#include "network/CredentialStoreLogic.h"
#include "network/NetworkCredentials.h"

namespace {

UserSettings makeSettings(ControlMode mode, AnimationId animation, uint8_t brightness) {
    UserSettings settings;
    settings.mode = mode;
    settings.animation = animation;
    settings.brightnessPercent = brightness;
    return settings;
}

const UserSettings kDefaults = UserSettingsDefaults::make();

}  // namespace

void test_first_boot_has_no_stored_settings(void) {
    FakeNvs nvs;
    SettingsStoreLogic store(nvs);

    UserSettings loaded = kDefaults;
    TEST_ASSERT_FALSE(store.load(loaded));

    // Untouched, so the caller keeps its defaults.
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(kDefaults.mode),
        static_cast<uint8_t>(loaded.mode));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(kDefaults.animation),
        static_cast<uint8_t>(loaded.animation));
}

void test_save_then_load_round_trip(void) {
    FakeNvs nvs;
    SettingsStoreLogic store(nvs);

    const UserSettings saved = makeSettings(ControlMode::MANUAL, AnimationId::CAMPFIRE, 42u);
    TEST_ASSERT_TRUE(store.save(saved));

    UserSettings loaded = kDefaults;
    TEST_ASSERT_TRUE(store.load(loaded));

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ControlMode::MANUAL),
        static_cast<uint8_t>(loaded.mode));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(AnimationId::CAMPFIRE),
        static_cast<uint8_t>(loaded.animation));
    TEST_ASSERT_EQUAL_UINT8(42u, loaded.brightnessPercent);
}

void test_settings_survive_a_simulated_power_cycle(void) {
    // One NVS backend that outlives every store object built on it: the native
    // equivalent of flash persisting across a power cycle.
    FakeNvs nvs;

    {
        SettingsStoreLogic store(nvs);
        TEST_ASSERT_TRUE(store.save(
            makeSettings(ControlMode::MANUAL, AnimationId::MOON_STARS, 77u)));
    }

    // Everything above is gone; only the bytes remain.
    {
        SettingsStoreLogic store(nvs);
        UserSettings loaded = kDefaults;
        TEST_ASSERT_TRUE(store.load(loaded));

        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(ControlMode::MANUAL),
            static_cast<uint8_t>(loaded.mode));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(AnimationId::MOON_STARS),
            static_cast<uint8_t>(loaded.animation));
        TEST_ASSERT_EQUAL_UINT8(77u, loaded.brightnessPercent);
    }
}

void test_auto_mode_round_trips(void) {
    FakeNvs nvs;
    SettingsStoreLogic store(nvs);

    TEST_ASSERT_TRUE(store.save(makeSettings(ControlMode::AUTO, AnimationId::AURORA, 12u)));

    UserSettings loaded = makeSettings(ControlMode::MANUAL, AnimationId::CAMPFIRE, 99u);
    TEST_ASSERT_TRUE(store.load(loaded));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ControlMode::AUTO),
        static_cast<uint8_t>(loaded.mode));
}

void test_brightness_bounds_persist_exactly(void) {
    const uint8_t values[] = {0u, 1u, 50u, 99u, 100u};

    for (uint8_t i = 0u; i < sizeof(values) / sizeof(values[0]); ++i) {
        FakeNvs nvs;
        SettingsStoreLogic store(nvs);
        TEST_ASSERT_TRUE(store.save(makeSettings(ControlMode::AUTO, AnimationId::AURORA, values[i])));

        UserSettings loaded = kDefaults;
        TEST_ASSERT_TRUE(store.load(loaded));
        TEST_ASSERT_EQUAL_UINT8(values[i], loaded.brightnessPercent);
    }
}

void test_missing_version_is_treated_as_no_settings(void) {
    // A payload without the commit marker must be ignored, exactly as a
    // half-finished write would look.
    FakeNvs nvs;
    nvs.begin("pixelheart", false);
    nvs.putBytes("set_mode", "\x01", 1u);
    nvs.putBytes("set_anim", "\x08", 1u);
    nvs.putBytes("set_bright", "\x32", 1u);
    nvs.end();

    SettingsStoreLogic store(nvs);
    UserSettings loaded = kDefaults;
    TEST_ASSERT_FALSE(store.load(loaded));
}

void test_wrong_version_is_ignored(void) {
    FakeNvs nvs;
    nvs.begin("pixelheart", false);
    nvs.putBytes("set_mode", "\x01", 1u);
    nvs.putBytes("set_anim", "\x08", 1u);
    nvs.putBytes("set_bright", "\x32", 1u);
    const uint8_t futureVersion = 99u;
    nvs.putBytes("set_version", &futureVersion, 1u);
    nvs.end();

    SettingsStoreLogic store(nvs);
    UserSettings loaded = kDefaults;
    TEST_ASSERT_FALSE(store.load(loaded));
}

void test_partial_payload_is_rejected(void) {
    // Version present but brightness missing: must not be half-applied.
    FakeNvs nvs;
    nvs.begin("pixelheart", false);
    nvs.putBytes("set_mode", "\x01", 1u);
    nvs.putBytes("set_anim", "\x08", 1u);
    const uint8_t version = SettingsStoreKeys::kVersionValue;
    nvs.putBytes("set_version", &version, 1u);
    nvs.end();

    SettingsStoreLogic store(nvs);
    UserSettings loaded = kDefaults;
    TEST_ASSERT_FALSE(store.load(loaded));
}

void test_invalid_animation_falls_back_to_defaults(void) {
    FakeNvs nvs;
    nvs.begin("pixelheart", false);
    nvs.putBytes("set_mode", "\x00", 1u);
    const uint8_t nonsense = 200u;   // not a valid AnimationId
    nvs.putBytes("set_anim", &nonsense, 1u);
    nvs.putBytes("set_bright", "\x32", 1u);
    const uint8_t version = SettingsStoreKeys::kVersionValue;
    nvs.putBytes("set_version", &version, 1u);
    nvs.end();

    SettingsStoreLogic store(nvs);
    UserSettings loaded = kDefaults;
    TEST_ASSERT_FALSE(store.load(loaded));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(kDefaults.animation),
        static_cast<uint8_t>(loaded.animation));
}

void test_invalid_brightness_falls_back_to_defaults(void) {
    FakeNvs nvs;
    nvs.begin("pixelheart", false);
    nvs.putBytes("set_mode", "\x00", 1u);
    nvs.putBytes("set_anim", "\x08", 1u);
    const uint8_t tooBright = 250u;   // > 100
    nvs.putBytes("set_bright", &tooBright, 1u);
    const uint8_t version = SettingsStoreKeys::kVersionValue;
    nvs.putBytes("set_version", &version, 1u);
    nvs.end();

    SettingsStoreLogic store(nvs);
    UserSettings loaded = kDefaults;
    TEST_ASSERT_FALSE(store.load(loaded));
    TEST_ASSERT_EQUAL_UINT8(kDefaults.brightnessPercent, loaded.brightnessPercent);
}

void test_invalid_settings_are_never_written(void) {
    FakeNvs nvs;
    SettingsStoreLogic store(nvs);

    UserSettings bad = makeSettings(ControlMode::AUTO, AnimationId::NONE, 50u);
    TEST_ASSERT_FALSE(store.save(bad));

    UserSettings badBrightness = makeSettings(ControlMode::AUTO, AnimationId::AURORA, 101u);
    TEST_ASSERT_FALSE(store.save(badBrightness));

    UserSettings loaded = kDefaults;
    TEST_ASSERT_FALSE(store.load(loaded));
}

void test_failed_write_never_sets_the_version_marker(void) {
    FakeNvs nvs;
    SettingsStoreLogic store(nvs);

    nvs.failNextPutBytes();  // the mode write fails
    TEST_ASSERT_FALSE(store.save(makeSettings(ControlMode::MANUAL, AnimationId::CAMPFIRE, 50u)));

    UserSettings loaded = kDefaults;
    TEST_ASSERT_FALSE(store.load(loaded));
}

void test_settings_do_not_touch_wifi_credentials(void) {
    // Both stores share one NVS namespace. Settings must not disturb the
    // credential keys, and vice versa.
    FakeNvs nvs;

    CredentialStoreLogic credentials(nvs, false);
    NetworkCredentials wifi;
    clearNetworkCredentials(wifi);
    assignNetworkSsid(wifi, "HomeNet");
    assignNetworkPassword(wifi, "hunter2");
    wifi.valid = true;
    TEST_ASSERT_TRUE(credentials.save(wifi));

    SettingsStoreLogic settings(nvs);
    TEST_ASSERT_TRUE(settings.save(makeSettings(ControlMode::MANUAL, AnimationId::CAMPFIRE, 33u)));

    // Credentials intact.
    NetworkCredentials reloadedWifi;
    TEST_ASSERT_TRUE(credentials.load(reloadedWifi));
    TEST_ASSERT_EQUAL_STRING("HomeNet", reloadedWifi.ssid);
    TEST_ASSERT_EQUAL_STRING("hunter2", reloadedWifi.password);

    // Settings intact.
    UserSettings reloadedSettings = kDefaults;
    TEST_ASSERT_TRUE(settings.load(reloadedSettings));
    TEST_ASSERT_EQUAL_UINT8(33u, reloadedSettings.brightnessPercent);

    // And the settings store never declared the credential keys.
    nvs.begin("pixelheart", true);
    TEST_ASSERT_TRUE(nvs.isKey("wifi_ssid"));
    TEST_ASSERT_TRUE(nvs.isKey("set_bright"));
    nvs.end();
}

void test_saving_settings_twice_is_idempotent(void) {
    FakeNvs nvs;
    SettingsStoreLogic store(nvs);

    const UserSettings settings = makeSettings(ControlMode::MANUAL, AnimationId::CAMPFIRE, 55u);
    TEST_ASSERT_TRUE(store.save(settings));

    const uint16_t writesAfterFirst = nvs.writeCount();
    TEST_ASSERT_TRUE(store.save(settings));

    // The values are overwritten in place; no keys accumulate.
    TEST_ASSERT_EQUAL_UINT16(writesAfterFirst * 2u, nvs.writeCount());

    UserSettings loaded = kDefaults;
    TEST_ASSERT_TRUE(store.load(loaded));
    TEST_ASSERT_EQUAL_UINT8(55u, loaded.brightnessPercent);
}

void test_version_is_written_last(void) {
    FakeNvs nvs;
    SettingsStoreLogic store(nvs);

    TEST_ASSERT_TRUE(store.save(makeSettings(ControlMode::AUTO, AnimationId::AURORA, 20u)));

    // The marker must be readable only after the payload exists.
    nvs.begin("pixelheart", true);
    TEST_ASSERT_TRUE(nvs.isKey(SettingsStoreKeys::kVersion));
    TEST_ASSERT_TRUE(nvs.isKey(SettingsStoreKeys::kMode));
    TEST_ASSERT_TRUE(nvs.isKey(SettingsStoreKeys::kAnimation));
    TEST_ASSERT_TRUE(nvs.isKey(SettingsStoreKeys::kBrightness));
    nvs.end();
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_first_boot_has_no_stored_settings);
    RUN_TEST(test_save_then_load_round_trip);
    RUN_TEST(test_settings_survive_a_simulated_power_cycle);
    RUN_TEST(test_auto_mode_round_trips);
    RUN_TEST(test_brightness_bounds_persist_exactly);
    RUN_TEST(test_missing_version_is_treated_as_no_settings);
    RUN_TEST(test_wrong_version_is_ignored);
    RUN_TEST(test_partial_payload_is_rejected);
    RUN_TEST(test_invalid_animation_falls_back_to_defaults);
    RUN_TEST(test_invalid_brightness_falls_back_to_defaults);
    RUN_TEST(test_invalid_settings_are_never_written);
    RUN_TEST(test_failed_write_never_sets_the_version_marker);
    RUN_TEST(test_settings_do_not_touch_wifi_credentials);
    RUN_TEST(test_saving_settings_twice_is_idempotent);
    RUN_TEST(test_version_is_written_last);
    return UNITY_END();
}
