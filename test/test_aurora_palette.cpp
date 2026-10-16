#include <unity.h>

#define private public
#include "animations/AuroraAnimation.h"
#undef private

#include "core/AnimationId.h"
#include "core/AnimationRegistry.h"

namespace {
constexpr uint16_t kPhasePeriod = 512u;
constexpr uint16_t kPhaseStep = 3u;

uint8_t triangleWave(uint8_t value) {
    if (value < 128u) {
        return static_cast<uint8_t>(value * 2u);
    }
    return static_cast<uint8_t>((255u - value) * 2u);
}

uint8_t fieldValueAt(uint8_t x, uint8_t y, uint16_t phase) {
    const uint16_t base =
        phase + static_cast<uint16_t>(x) * 17u + static_cast<uint16_t>(y) * 11u;
    const uint16_t secondary =
        static_cast<uint16_t>(phase / 2u + x * 7u + y * 19u);

    const uint8_t a = static_cast<uint8_t>(base & 0xFFu);
    const uint8_t b = static_cast<uint8_t>(secondary & 0xFFu);

    const uint8_t waveA = triangleWave(a);
    const uint8_t waveB = triangleWave(b);

    uint16_t value =
        static_cast<uint16_t>(waveA) * 3u +
        static_cast<uint16_t>(waveB) * 2u;
    value /= 5u;

    return static_cast<uint8_t>(value);
}
}  // namespace

void test_aurora_palette_registry_aliases_and_variants(void) {
    Animation* arctic = AnimationRegistry::getAnimation(AnimationId::AURORA);
    Animation* arcticExplicit = AnimationRegistry::getAnimation(AnimationId::AURORA_ARCTIC);
    Animation* sunset = AnimationRegistry::getAnimation(AnimationId::AURORA_SUNSET);
    Animation* neon = AnimationRegistry::getAnimation(AnimationId::AURORA_NEON);

    TEST_ASSERT_NOT_NULL(arctic);
    TEST_ASSERT_NOT_NULL(arcticExplicit);
    TEST_ASSERT_NOT_NULL(sunset);
    TEST_ASSERT_NOT_NULL(neon);
    TEST_ASSERT_EQUAL_PTR(arctic, arcticExplicit);
}

void test_aurora_variants_share_identical_motion_scalar_field(void) {
    for (uint8_t y = 0u; y < 16u; ++y) {
        for (uint8_t x = 0u; x < 16u; ++x) {
            const uint8_t phase0 = fieldValueAt(x, y, 0u);
            const uint8_t phase509 = fieldValueAt(x, y, 509u);
            const uint8_t phase3 = fieldValueAt(x, y, 3u);

            TEST_ASSERT_EQUAL_UINT8(phase0, fieldValueAt(x, y, 512u));
            TEST_ASSERT_TRUE(phase509 <= 255u);
            TEST_ASSERT_TRUE(phase3 <= 255u);
        }
    }
}

void test_aurora_loop_remains_exact_512_step_cycle(void) {
    uint16_t phase = 0u;
    for (uint16_t i = 0u; i < kPhasePeriod; ++i) {
        phase = static_cast<uint16_t>((phase + kPhaseStep) % kPhasePeriod);
    }
    TEST_ASSERT_EQUAL_UINT16(0u, phase);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_aurora_palette_registry_aliases_and_variants);
    RUN_TEST(test_aurora_variants_share_identical_motion_scalar_field);
    RUN_TEST(test_aurora_loop_remains_exact_512_step_cycle);
    return UNITY_END();
}
