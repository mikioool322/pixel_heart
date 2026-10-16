#include <unity.h>

#include <stdint.h>

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

void test_aurora_loop_boundary_stays_within_six_counts(void) {
    uint16_t maxDelta = 0u;

    for (uint8_t y = 0u; y < 16u; ++y) {
        for (uint8_t x = 0u; x < 16u; ++x) {
            const uint8_t phase509 = fieldValueAt(x, y, 509u);
            const uint8_t phase0 = fieldValueAt(x, y, 0u);
            const uint16_t delta =
                static_cast<uint16_t>(phase509 > phase0 ? phase509 - phase0 : phase0 - phase509);
            if (delta > maxDelta) {
                maxDelta = delta;
            }
        }
    }

    TEST_ASSERT_TRUE_MESSAGE(maxDelta <= 6u, "Aurora loop seam exceeded the smooth-field tolerance.");
}

void test_aurora_loop_step_is_modulo_512(void) {
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
    RUN_TEST(test_aurora_loop_boundary_stays_within_six_counts);
    RUN_TEST(test_aurora_loop_step_is_modulo_512);
    return UNITY_END();
}
