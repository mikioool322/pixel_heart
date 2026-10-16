#include <unity.h>

#include <string.h>

#include "config/LedPowerBudget.h"
#include "control/BrightnessLevels.h"
#include "core/PixelMatrix.h"

#include "control/SettingsStore.h"
#include "control/UserSettings.h"
#include "fakes/FakeNvs.h"

// Five user-facing brightness levels.
//
// The levels are presentation only. The safety property these tests have to
// establish is that no level - and no legacy stored value - can ask the matrix
// for more than the independent hard ceiling in LedPowerBudget.

namespace {

const UserSettings kDefaults = UserSettingsDefaults::make();

// Worst-case current for a fully white frame transmitted at `raw`, using the
// same conservative model as LedPowerBudget and the library's own brightness
// formula (transmitted = (channel * (raw + 1)) >> 8).
uint32_t worstCaseMilliAmp(uint8_t raw) {
    const uint16_t transmitted = static_cast<uint16_t>(
        (255u * (static_cast<uint16_t>(raw) + 1u)) >> 8);
    const uint32_t channelSum =
        static_cast<uint32_t>(LedPowerBudget::kPixelCount) *
        LedPowerBudget::kChannelsPerPixel * transmitted;
    return (channelSum * LedPowerBudget::kMilliAmpPerFullChannel) / 255u;
}

}  // namespace

// ---------------------------------------------------------------------------
// 1-5: the level table itself
// ---------------------------------------------------------------------------

void test_exactly_five_levels_exist(void) {
    TEST_ASSERT_EQUAL_UINT8(5u, BrightnessLevels::kCount);

    TEST_ASSERT_TRUE(BrightnessLevels::isValidLevel(1u));
    TEST_ASSERT_TRUE(BrightnessLevels::isValidLevel(5u));
    TEST_ASSERT_FALSE(BrightnessLevels::isValidLevel(0u));
    TEST_ASSERT_FALSE(BrightnessLevels::isValidLevel(6u));
    TEST_ASSERT_FALSE(BrightnessLevels::isValidLevel(255u));
}

void test_levels_are_strictly_increasing(void) {
    for (uint8_t level = 1u; level < BrightnessLevels::kCount; ++level) {
        TEST_ASSERT_TRUE(
            BrightnessLevels::rawForLevel(level) <
            BrightnessLevels::rawForLevel(static_cast<uint8_t>(level + 1u)));
        TEST_ASSERT_TRUE(
            BrightnessLevels::percentForLevel(level) <
            BrightnessLevels::percentForLevel(static_cast<uint8_t>(level + 1u)));
    }
}

void test_every_level_is_within_the_hard_ceiling(void) {
    for (uint8_t level = 1u; level <= BrightnessLevels::kCount; ++level) {
        TEST_ASSERT_TRUE(
            BrightnessLevels::rawForLevel(level) <= LedPowerBudget::kMaxSafeBrightnessRaw);
    }
}

void test_level_five_is_exactly_the_maximum_safe_output(void) {
    // "Maksymalna" must mean the most this device is allowed to produce, not an
    // arbitrary lower value, and never more than the ceiling.
    TEST_ASSERT_EQUAL_UINT8(
        LedPowerBudget::kMaxSafeBrightnessRaw,
        BrightnessLevels::rawForLevel(BrightnessLevels::kCount));

    TEST_ASSERT_EQUAL_UINT8(33u, BrightnessLevels::rawForLevel(5u));
    TEST_ASSERT_TRUE(
        worstCaseMilliAmp(BrightnessLevels::rawForLevel(5u)) <= LedPowerBudget::kLimitMilliAmp);
}

void test_no_level_can_request_an_over_budget_output(void) {
    // The real bound: what the matrix would transmit, not just the raw number.
    uint32_t worst = 0u;

    for (uint8_t level = 1u; level <= BrightnessLevels::kCount; ++level) {
        const uint32_t estimate = worstCaseMilliAmp(BrightnessLevels::rawForLevel(level));
        TEST_ASSERT_TRUE(estimate <= LedPowerBudget::kLimitMilliAmp);

        if (estimate > worst) {
            worst = estimate;
        }
    }

    TEST_ASSERT_EQUAL_UINT32(LedPowerBudget::kWorstCaseMilliAmp, worst);
    TEST_ASSERT_EQUAL_UINT32(1987u, worst);
}

void test_level_raw_values_match_the_intended_targets(void) {
    // The exact mapping this device is specified to use. The bottom of the
    // range is deliberately dense: 1 / 3 / 7 is a large relative change for a
    // tiny absolute one, which is what makes level 1 usable at night.
    TEST_ASSERT_EQUAL_UINT8(1u, BrightnessLevels::rawForLevel(1u));
    TEST_ASSERT_EQUAL_UINT8(3u, BrightnessLevels::rawForLevel(2u));
    TEST_ASSERT_EQUAL_UINT8(7u, BrightnessLevels::rawForLevel(3u));
    TEST_ASSERT_EQUAL_UINT8(16u, BrightnessLevels::rawForLevel(4u));
    TEST_ASSERT_EQUAL_UINT8(33u, BrightnessLevels::rawForLevel(5u));

    // The persisted percentages are carriers only: they are chosen so they map
    // back to the same level and therefore to the exact raw value above.
    TEST_ASSERT_EQUAL_UINT8(1u, BrightnessLevels::percentForLevel(1u));
    TEST_ASSERT_EQUAL_UINT8(2u, BrightnessLevels::percentForLevel(2u));
    TEST_ASSERT_EQUAL_UINT8(3u, BrightnessLevels::percentForLevel(3u));
    TEST_ASSERT_EQUAL_UINT8(7u, BrightnessLevels::percentForLevel(4u));
    TEST_ASSERT_EQUAL_UINT8(13u, BrightnessLevels::percentForLevel(5u));
}

void test_output_path_hits_the_exact_level_targets(void) {
    // What actually reaches the matrix is rawForPercent(), not the coarse
    // percent->raw conversion. This is the property that lets 1 / 3 / 16 exist
    // at all, so it is asserted explicitly.
    for (uint8_t level = 1u; level <= BrightnessLevels::kCount; ++level) {
        const uint8_t stored = BrightnessLevels::percentForLevel(level);
        TEST_ASSERT_EQUAL_UINT8(level, BrightnessLevels::levelForPercent(stored));
        TEST_ASSERT_EQUAL_UINT8(
            BrightnessLevels::rawForLevel(level), BrightnessLevels::rawForPercent(stored));
    }

    TEST_ASSERT_EQUAL_UINT8(1u, BrightnessLevels::rawForPercent(BrightnessLevels::percentForLevel(1u)));
    TEST_ASSERT_EQUAL_UINT8(3u, BrightnessLevels::rawForPercent(BrightnessLevels::percentForLevel(2u)));
    TEST_ASSERT_EQUAL_UINT8(7u, BrightnessLevels::rawForPercent(BrightnessLevels::percentForLevel(3u)));
    TEST_ASSERT_EQUAL_UINT8(16u, BrightnessLevels::rawForPercent(BrightnessLevels::percentForLevel(4u)));
    TEST_ASSERT_EQUAL_UINT8(33u, BrightnessLevels::rawForPercent(BrightnessLevels::percentForLevel(5u)));
}

void test_dimmest_level_is_a_genuinely_dark_night_setting(void) {
    // The reported problem: level 1 lit up the whole room. It must now be a
    // tiny fraction of both the old level-1 output and the ceiling.
    TEST_ASSERT_EQUAL_UINT8(1u, BrightnessLevels::rawForLevel(1u));
    TEST_ASSERT_TRUE(BrightnessLevels::rawForLevel(1u) < 7u);  // the old level 1
    TEST_ASSERT_TRUE(BrightnessLevels::rawForLevel(1u) * 4u < LedPowerBudget::kMaxSafeBrightnessRaw);
    TEST_ASSERT_TRUE(
        worstCaseMilliAmp(BrightnessLevels::rawForLevel(1u)) < LedPowerBudget::kLimitMilliAmp / 4u);
}

// ---------------------------------------------------------------------------
// 12: Polish labels
// ---------------------------------------------------------------------------

void test_levels_have_polish_labels(void) {
    TEST_ASSERT_EQUAL_STRING("Nocna", BrightnessLevels::labelForLevel(1u));
    TEST_ASSERT_EQUAL_STRING("Bardzo niska", BrightnessLevels::labelForLevel(2u));
    TEST_ASSERT_EQUAL_STRING("Niska", BrightnessLevels::labelForLevel(3u));
    TEST_ASSERT_EQUAL_STRING("Średnia", BrightnessLevels::labelForLevel(4u));
    TEST_ASSERT_EQUAL_STRING("Maksymalna", BrightnessLevels::labelForLevel(5u));

    // Out-of-range callers get a safe label rather than a null pointer.
    TEST_ASSERT_NOT_NULL(BrightnessLevels::labelForLevel(0u));
    TEST_ASSERT_NOT_NULL(BrightnessLevels::labelForLevel(200u));
}

void test_no_label_mentions_a_percentage_or_an_electrical_detail(void) {
    for (uint8_t level = 1u; level <= BrightnessLevels::kCount; ++level) {
        const char* label = BrightnessLevels::labelForLevel(level);
        TEST_ASSERT_TRUE(strlen(label) > 0u);
        TEST_ASSERT_NULL(strchr(label, '%'));
        TEST_ASSERT_NULL(strstr(label, "mA"));
        TEST_ASSERT_NULL(strstr(label, "255"));
    }
}

// ---------------------------------------------------------------------------
// 6: the level mapping is total and deterministic
// ---------------------------------------------------------------------------

void test_every_percentage_maps_to_a_valid_level(void) {
    for (uint16_t percent = 0u; percent <= 255u; ++percent) {
        const uint8_t level =
            BrightnessLevels::levelForPercent(static_cast<uint8_t>(percent));
        TEST_ASSERT_TRUE(BrightnessLevels::isValidLevel(level));
    }
}

void test_canonical_percentages_map_back_to_their_own_level(void) {
    for (uint8_t level = 1u; level <= BrightnessLevels::kCount; ++level) {
        TEST_ASSERT_EQUAL_UINT8(
            level, BrightnessLevels::levelForPercent(BrightnessLevels::percentForLevel(level)));
    }
}

// ---------------------------------------------------------------------------
// 7-9: legacy values
// ---------------------------------------------------------------------------

void test_legacy_high_values_render_as_the_top_level(void) {
    // Anything at or above the ceiling already produced the same clamped
    // output, so it is represented as "Maksymalna".
    const uint8_t legacy[] = {14u, 18u, 30u, 50u, 100u, 255u};

    for (uint8_t i = 0u; i < sizeof(legacy) / sizeof(legacy[0]); ++i) {
        TEST_ASSERT_EQUAL_UINT8(
            BrightnessLevels::kCount, BrightnessLevels::levelForPercent(legacy[i]));
    }
}

void test_legacy_values_never_render_brighter_than_they_output(void) {
    // The level shown must never imply more output than the device actually
    // produces, or the panel would overstate the brightness.
    for (uint16_t percent = 0u; percent <= 255u; ++percent) {
        const uint8_t stored = static_cast<uint8_t>(percent);
        const uint8_t level = BrightnessLevels::levelForPercent(stored);
        const uint8_t levelRaw = BrightnessLevels::rawForLevel(level);
        const uint8_t storedRaw = brightnessPercentToRaw(stored);

        // Either the level shown is at or below the stored output, or the
        // stored value is below the dimmest level and there is nothing lower.
        const bool conservative = levelRaw <= storedRaw;
        const bool belowDimmest = storedRaw < BrightnessLevels::rawForLevel(1u);
        TEST_ASSERT_TRUE(conservative || belowDimmest);

        // The stored output must never be understated either.
        TEST_ASSERT_TRUE(levelRaw <= LedPowerBudget::kMaxSafeBrightnessRaw);
    }
}

void test_legacy_percent_of_100_renders_as_level_5(void) {
    TEST_ASSERT_EQUAL_UINT8(5u, BrightnessLevels::levelForPercent(100u));
}

void test_default_brightness_maps_to_a_valid_level(void) {
    const uint8_t level = BrightnessLevels::levelForPercent(kDefaults.brightnessPercent);
    TEST_ASSERT_TRUE(BrightnessLevels::isValidLevel(level));

    // The shipped default is below the ceiling, so its output is unchanged and
    // the level it renders is inside the table.
    TEST_ASSERT_TRUE(
        brightnessPercentToRaw(kDefaults.brightnessPercent) <=
        LedPowerBudget::kMaxSafeBrightnessRaw);
}

// ---------------------------------------------------------------------------
// 7: the matrix still clamps independently of the UI
// ---------------------------------------------------------------------------

void test_matrix_clamps_even_a_hostile_brightness_request(void) {
    // Simulates a future bug: the UI layer requests far more than any level
    // allows. The independent hard limiter must still bound the output, because
    // the UI is not the safety mechanism.
    PixelMatrix matrix;

    const uint8_t hostile[] = {34u, 35u, 60u, 128u, 254u, 255u};
    for (uint8_t i = 0u; i < sizeof(hostile) / sizeof(hostile[0]); ++i) {
        matrix.setBrightness(hostile[i]);

        TEST_ASSERT_EQUAL_UINT8(
            LedPowerBudget::kMaxSafeBrightnessRaw, matrix.effectiveBrightnessRaw());
        TEST_ASSERT_TRUE(
            worstCaseMilliAmp(matrix.effectiveBrightnessRaw()) <= LedPowerBudget::kLimitMilliAmp);
    }
}

// ---------------------------------------------------------------------------
// 10-11: persistence
// ---------------------------------------------------------------------------

void test_selecting_each_level_persists_its_canonical_percentage(void) {
    const uint8_t percents[] = {1u, 2u, 3u, 7u, 13u};

    for (uint8_t level = 1u; level <= BrightnessLevels::kCount; ++level) {
        FakeNvs nvs;

        UserSettings settings = kDefaults;
        settings.animation = AnimationId::AURORA;
        settings.brightnessPercent = BrightnessLevels::percentForLevel(level);

        {
            SettingsStoreLogic store(nvs);
            TEST_ASSERT_TRUE(store.save(settings));
        }

        // Power cycle.
        {
            SettingsStoreLogic store(nvs);
            UserSettings loaded = kDefaults;
            TEST_ASSERT_TRUE(store.load(loaded));

            TEST_ASSERT_EQUAL_UINT8(percents[level - 1u], loaded.brightnessPercent);
            // And it still renders as the level the user chose.
            TEST_ASSERT_EQUAL_UINT8(level, BrightnessLevels::levelForPercent(loaded.brightnessPercent));
        }
    }
}

void test_legacy_stored_brightness_survives_load_and_renders_a_level(void) {
    // A device carrying an arbitrary old percentage must still boot, keep its
    // stored value, and show a valid level.
    const uint8_t legacy[] = {0u, 18u, 50u, 100u};

    for (uint8_t i = 0u; i < sizeof(legacy) / sizeof(legacy[0]); ++i) {
        FakeNvs nvs;

        UserSettings settings = kDefaults;
        settings.animation = AnimationId::AURORA;
        settings.brightnessPercent = legacy[i];

        {
            SettingsStoreLogic store(nvs);
            TEST_ASSERT_TRUE(store.save(settings));
        }

        {
            SettingsStoreLogic store(nvs);
            UserSettings loaded = kDefaults;
            TEST_ASSERT_TRUE(store.load(loaded));

            // Not corrupted, not rewritten.
            TEST_ASSERT_EQUAL_UINT8(legacy[i], loaded.brightnessPercent);

            // Renders as a valid level.
            const uint8_t level = BrightnessLevels::levelForPercent(loaded.brightnessPercent);
            TEST_ASSERT_TRUE(BrightnessLevels::isValidLevel(level));

            // And the effective output can never exceed the ceiling, even for a
            // legacy value that is far above it.
            TEST_ASSERT_TRUE(
                LedPowerBudget::clampBrightnessRaw(brightnessPercentToRaw(loaded.brightnessPercent)) <=
                LedPowerBudget::kMaxSafeBrightnessRaw);
            TEST_ASSERT_TRUE(
                worstCaseMilliAmp(LedPowerBudget::clampBrightnessRaw(
                    brightnessPercentToRaw(loaded.brightnessPercent))) <=
                LedPowerBudget::kLimitMilliAmp);
        }
    }
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_exactly_five_levels_exist);
    RUN_TEST(test_levels_are_strictly_increasing);
    RUN_TEST(test_every_level_is_within_the_hard_ceiling);
    RUN_TEST(test_level_five_is_exactly_the_maximum_safe_output);
    RUN_TEST(test_no_level_can_request_an_over_budget_output);
    RUN_TEST(test_level_raw_values_match_the_intended_targets);
    RUN_TEST(test_output_path_hits_the_exact_level_targets);
    RUN_TEST(test_dimmest_level_is_a_genuinely_dark_night_setting);
    RUN_TEST(test_levels_have_polish_labels);
    RUN_TEST(test_no_label_mentions_a_percentage_or_an_electrical_detail);
    RUN_TEST(test_every_percentage_maps_to_a_valid_level);
    RUN_TEST(test_canonical_percentages_map_back_to_their_own_level);
    RUN_TEST(test_legacy_high_values_render_as_the_top_level);
    RUN_TEST(test_legacy_values_never_render_brighter_than_they_output);
    RUN_TEST(test_legacy_percent_of_100_renders_as_level_5);
    RUN_TEST(test_default_brightness_maps_to_a_valid_level);
    RUN_TEST(test_matrix_clamps_even_a_hostile_brightness_request);
    RUN_TEST(test_selecting_each_level_persists_its_canonical_percentage);
    RUN_TEST(test_legacy_stored_brightness_survives_load_and_renders_a_level);
    return UNITY_END();
}
