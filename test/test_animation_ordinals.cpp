#include <unity.h>

#include "control/AnimationCatalog.h"
#include "control/UserSettings.h"
#include "core/AnimationId.h"
#include "core/AnimationRegistry.h"

// Persisted-ordinal guard.
//
// SettingsStore writes the AnimationId's raw numeric value to NVS. Adding an
// animation therefore MUST NOT renumber anything that already shipped, or an
// old "set_anim" byte would silently resolve to a different animation after a
// firmware update. New ids are appended, and this test pins every value that
// has ever been persisted.

namespace {

uint8_t id(AnimationId value) {
    return static_cast<uint8_t>(value);
}

}  // namespace

void test_existing_animation_ordinals_are_unchanged(void) {
    // The complete historical record, as stored in NVS by shipped firmware.
    TEST_ASSERT_EQUAL_UINT8(0u, id(AnimationId::NONE));
    TEST_ASSERT_EQUAL_UINT8(1u, id(AnimationId::HEARTBEAT));
    TEST_ASSERT_EQUAL_UINT8(2u, id(AnimationId::BREATHING_HEART));
    TEST_ASSERT_EQUAL_UINT8(3u, id(AnimationId::ROSE_BLOOM));
    TEST_ASSERT_EQUAL_UINT8(4u, id(AnimationId::FIREWORKS));
    TEST_ASSERT_EQUAL_UINT8(5u, id(AnimationId::LOVE_MODE));
    TEST_ASSERT_EQUAL_UINT8(6u, id(AnimationId::CAMPFIRE));
    TEST_ASSERT_EQUAL_UINT8(7u, id(AnimationId::DANCING_DUCK));
    TEST_ASSERT_EQUAL_UINT8(8u, id(AnimationId::SLOT_MACHINE));
    TEST_ASSERT_EQUAL_UINT8(9u, id(AnimationId::SWIMMING_FISH));
    TEST_ASSERT_EQUAL_UINT8(10u, id(AnimationId::AURORA));
    TEST_ASSERT_EQUAL_UINT8(11u, id(AnimationId::AURORA_ARCTIC));
    TEST_ASSERT_EQUAL_UINT8(12u, id(AnimationId::AURORA_SUNSET));
    TEST_ASSERT_EQUAL_UINT8(13u, id(AnimationId::AURORA_NEON));
    TEST_ASSERT_EQUAL_UINT8(14u, id(AnimationId::CHRISTMAS_TREE));
    TEST_ASSERT_EQUAL_UINT8(15u, id(AnimationId::MOON_STARS));
    TEST_ASSERT_EQUAL_UINT8(16u, id(AnimationId::SUNSET_WATER));
    TEST_ASSERT_EQUAL_UINT8(17u, id(AnimationId::FALLING_STARS));
    TEST_ASSERT_EQUAL_UINT8(18u, id(AnimationId::SUN_OVER_MOUNTAINS));
}

void test_new_animations_are_appended(void) {
    TEST_ASSERT_EQUAL_UINT8(19u, id(AnimationId::HALLOWEEN_GHOST));
    TEST_ASSERT_EQUAL_UINT8(20u, id(AnimationId::FOX));
    TEST_ASSERT_EQUAL_UINT8(21u, id(AnimationId::ORANGE_CAT));
    TEST_ASSERT_EQUAL_UINT8(22u, id(AnimationId::PALM_BREEZE));
    TEST_ASSERT_EQUAL_UINT8(23u, id(AnimationId::JELLYFISH));
    TEST_ASSERT_EQUAL_UINT8(24u, id(AnimationId::LIGHTHOUSE));
    TEST_ASSERT_EQUAL_UINT8(25u, id(AnimationId::DISCO_BALL));
    TEST_ASSERT_EQUAL_UINT8(26u, id(AnimationId::MEADOW_FLOWER));
    TEST_ASSERT_EQUAL_UINT8(27u, id(AnimationId::BLUE_SKY_CLOUDS));

    // The sentinel grew by exactly the ids that were added, and nothing that
    // shipped before them moved.
    TEST_ASSERT_EQUAL_UINT8(28u, id(AnimationId::ANIMATION_ID_COUNT));
}

void test_old_stored_bytes_still_resolve_to_the_same_animation(void) {
    // A value written by an older firmware is read back as a raw byte and cast
    // straight back to AnimationId. For every pre-existing id that cast must
    // still name the same animation and still be user-selectable.
    struct Legacy {
        uint8_t stored;
        AnimationId expected;
    };

    const Legacy legacy[] = {
        { 1u, AnimationId::HEARTBEAT },
        { 6u, AnimationId::CAMPFIRE },
        { 10u, AnimationId::AURORA },
        { 14u, AnimationId::CHRISTMAS_TREE },
        { 15u, AnimationId::MOON_STARS },
        { 18u, AnimationId::SUN_OVER_MOUNTAINS },
    };

    for (uint8_t i = 0u; i < (sizeof(legacy) / sizeof(legacy[0])); ++i) {
        const AnimationId restored = static_cast<AnimationId>(legacy[i].stored);
        TEST_ASSERT_EQUAL_UINT8(id(legacy[i].expected), id(restored));
        TEST_ASSERT_TRUE(AnimationCatalog::isSelectable(restored));
    }
}

void test_legacy_settings_record_still_validates(void) {
    // A record as an old firmware would have stored it: MANUAL on CAMPFIRE.
    UserSettings settings = UserSettingsDefaults::make();
    settings.mode = ControlMode::MANUAL;
    settings.animation = static_cast<AnimationId>(6u);
    settings.brightnessPercent = 42u;

    TEST_ASSERT_TRUE(isUserSettingsValid(settings));
    TEST_ASSERT_EQUAL_UINT8(id(AnimationId::CAMPFIRE), id(settings.animation));
}

void test_new_ids_are_registered_and_selectable(void) {
    TEST_ASSERT_TRUE(AnimationCatalog::isUserSelectable(AnimationId::HALLOWEEN_GHOST));
    TEST_ASSERT_TRUE(AnimationCatalog::isUserSelectable(AnimationId::FOX));
    TEST_ASSERT_TRUE(AnimationCatalog::isUserSelectable(AnimationId::ORANGE_CAT));
    TEST_ASSERT_NOT_NULL(AnimationRegistry::getAnimation(AnimationId::HALLOWEEN_GHOST));
    TEST_ASSERT_NOT_NULL(AnimationRegistry::getAnimation(AnimationId::FOX));
    TEST_ASSERT_NOT_NULL(AnimationRegistry::getAnimation(AnimationId::ORANGE_CAT));

    // The newest ones, appended after PALM_BREEZE.
    TEST_ASSERT_TRUE(AnimationCatalog::isUserSelectable(AnimationId::JELLYFISH));
    TEST_ASSERT_TRUE(AnimationCatalog::isUserSelectable(AnimationId::LIGHTHOUSE));
    TEST_ASSERT_TRUE(AnimationCatalog::isUserSelectable(AnimationId::DISCO_BALL));
    TEST_ASSERT_NOT_NULL(AnimationRegistry::getAnimation(AnimationId::JELLYFISH));
    TEST_ASSERT_NOT_NULL(AnimationRegistry::getAnimation(AnimationId::LIGHTHOUSE));
    TEST_ASSERT_NOT_NULL(AnimationRegistry::getAnimation(AnimationId::DISCO_BALL));

    TEST_ASSERT_TRUE(AnimationCatalog::isUserSelectable(AnimationId::MEADOW_FLOWER));
    TEST_ASSERT_TRUE(AnimationCatalog::isUserSelectable(AnimationId::BLUE_SKY_CLOUDS));
    TEST_ASSERT_NOT_NULL(AnimationRegistry::getAnimation(AnimationId::MEADOW_FLOWER));
    TEST_ASSERT_NOT_NULL(AnimationRegistry::getAnimation(AnimationId::BLUE_SKY_CLOUDS));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_existing_animation_ordinals_are_unchanged);
    RUN_TEST(test_new_animations_are_appended);
    RUN_TEST(test_old_stored_bytes_still_resolve_to_the_same_animation);
    RUN_TEST(test_legacy_settings_record_still_validates);
    RUN_TEST(test_new_ids_are_registered_and_selectable);
    return UNITY_END();
}
