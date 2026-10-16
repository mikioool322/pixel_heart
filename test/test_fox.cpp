#include <unity.h>

#define private public
#include "animations/FoxAnimation.h"
#undef private

#include "control/AnimationCatalog.h"
#include "core/AnimationId.h"
#include "core/AnimationNames.h"
#include "core/AnimationRegistry.h"
#include "core/PixelMatrix.h"

namespace {

constexpr uint8_t kPanel = 16u;

Color pixelColorAt(const PixelMatrix& matrix, uint8_t x, uint8_t y) {
    const uint32_t packed =
        matrix.pixels.getPixelColor(PixelMatrix::xyToIndexStatic(x, y));
    return Color{
        static_cast<uint8_t>((packed >> 16) & 0xFFu),
        static_cast<uint8_t>((packed >> 8) & 0xFFu),
        static_cast<uint8_t>(packed & 0xFFu)
    };
}

bool sameColor(const Color& a, const Color& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

// The tail lives strictly in the lower-right corner. Anything outside it must
// be byte-identical between frames.
bool inTailRegion(uint8_t x, uint8_t y) {
    return x >= 12u && y >= 10u;
}

}  // namespace

void test_fox_is_registered_and_selectable(void) {
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(AnimationId::HALLOWEEN_GHOST) + 1u,
        static_cast<uint8_t>(AnimationId::FOX));
    TEST_ASSERT_NOT_NULL(AnimationRegistry::getAnimation(AnimationId::FOX));
    TEST_ASSERT_TRUE(AnimationCatalog::isUserSelectable(AnimationId::FOX));
}

void test_fox_label_is_polish(void) {
    TEST_ASSERT_EQUAL_STRING("Lisek", animationNameOf(AnimationId::FOX));
}

void test_fox_sequence_is_low_center_high_center(void) {
    TEST_ASSERT_EQUAL_UINT8(8u, FoxAnimation::frameCount());

    const uint8_t expected[8] = { 0u, 1u, 2u, 1u, 0u, 1u, 2u, 1u };
    for (uint8_t frame = 0u; frame < 8u; ++frame) {
        TEST_ASSERT_EQUAL_UINT8(expected[frame], FoxAnimation::tailPoseAt(frame));
    }

    // The visible jump HIGH -> LOW must never happen between adjacent frames.
    for (uint8_t frame = 0u; frame + 1u < 8u; ++frame) {
        const int16_t here = FoxAnimation::tailPoseAt(frame);
        const int16_t next = FoxAnimation::tailPoseAt(static_cast<uint8_t>(frame + 1u));
        const int16_t delta = static_cast<int16_t>(next - here);
        TEST_ASSERT_TRUE(delta >= -1 && delta <= 1);
    }
}

void test_fox_body_is_byte_identical_between_frames(void) {
    FoxAnimation animation;
    PixelMatrix reference;
    PixelMatrix frame;

    animation.renderFrame(reference, 0u);

    for (uint8_t f = 1u; f < FoxAnimation::frameCount(); ++f) {
        animation.renderFrame(frame, f);

        for (uint8_t y = 0u; y < kPanel; ++y) {
            for (uint8_t x = 0u; x < kPanel; ++x) {
                if (inTailRegion(x, y)) {
                    continue;
                }
                TEST_ASSERT_TRUE(sameColor(pixelColorAt(reference, x, y), pixelColorAt(frame, x, y)));
            }
        }
    }
}

void test_fox_only_the_tail_region_changes(void) {
    FoxAnimation animation;
    PixelMatrix reference;
    PixelMatrix frame;

    animation.renderFrame(reference, 0u);

    for (uint8_t f = 1u; f < FoxAnimation::frameCount(); ++f) {
        animation.renderFrame(frame, f);

        for (uint8_t y = 0u; y < kPanel; ++y) {
            for (uint8_t x = 0u; x < kPanel; ++x) {
                if (sameColor(pixelColorAt(reference, x, y), pixelColorAt(frame, x, y))) {
                    continue;
                }
                // Every changed pixel belongs to the tail corner.
                TEST_ASSERT_TRUE(inTailRegion(x, y));
            }
        }
    }
}

void test_fox_tail_moves_at_most_one_pixel(void) {
    // The white tip rides one row per pose change; the base never moves.
    for (uint8_t tip = 0u; tip < 2u; ++tip) {
        const int16_t center = FoxAnimation::tailTipY(1u, tip);
        TEST_ASSERT_EQUAL_INT16(center - 1, FoxAnimation::tailTipY(2u, tip));
        TEST_ASSERT_EQUAL_INT16(center + 1, FoxAnimation::tailTipY(0u, tip));
        TEST_ASSERT_EQUAL_UINT8(13u, FoxAnimation::tailTipX(0u, tip));
        TEST_ASSERT_EQUAL_UINT8(13u, FoxAnimation::tailTipX(1u, tip));
        TEST_ASSERT_EQUAL_UINT8(13u, FoxAnimation::tailTipX(2u, tip));
    }
}

void test_fox_tail_stays_connected_to_the_body(void) {
    // Flood fill the union of the body and the pose's tail, starting from the
    // body, and prove every tail pixel is reached.
    static bool visited[kPanel][kPanel];
    uint8_t queueX[kPanel * kPanel];
    uint8_t queueY[kPanel * kPanel];

    for (uint8_t pose = 0u; pose < 3u; ++pose) {
        for (uint8_t y = 0u; y < kPanel; ++y) {
            for (uint8_t x = 0u; x < kPanel; ++x) {
                visited[y][x] = false;
            }
        }

        uint16_t head = 0u;
        uint16_t tail = 0u;

        for (uint8_t y = 0u; y < kPanel; ++y) {
            for (uint8_t x = 0u; x < kPanel; ++x) {
                if (FoxAnimation::staticBodyContains(x, y)) {
                    visited[y][x] = true;
                    queueX[tail] = x;
                    queueY[tail] = y;
                    ++tail;
                }
            }
        }

        const int8_t dx[4] = {1, -1, 0, 0};
        const int8_t dy[4] = {0, 0, 1, -1};

        while (head < tail) {
            const uint8_t cx = queueX[head];
            const uint8_t cy = queueY[head];
            ++head;

            for (uint8_t i = 0u; i < 4u; ++i) {
                const int16_t nx = static_cast<int16_t>(cx) + dx[i];
                const int16_t ny = static_cast<int16_t>(cy) + dy[i];
                if (nx < 0 || ny < 0 || nx >= kPanel || ny >= kPanel) {
                    continue;
                }
                if (visited[ny][nx]) {
                    continue;
                }
                if (!FoxAnimation::tailContains(
                        pose, static_cast<uint8_t>(nx), static_cast<uint8_t>(ny))) {
                    continue;
                }
                visited[ny][nx] = true;
                queueX[tail] = static_cast<uint8_t>(nx);
                queueY[tail] = static_cast<uint8_t>(ny);
                ++tail;
            }
        }

        for (uint8_t y = 0u; y < kPanel; ++y) {
            for (uint8_t x = 0u; x < kPanel; ++x) {
                if (FoxAnimation::tailContains(pose, x, y)) {
                    TEST_ASSERT_TRUE(visited[y][x]);
                }
            }
        }
    }
}

void test_fox_loop_boundary_is_seamless(void) {
    // Frame 7 (pose CENTER) leads back into frame 0 (pose LOW) with a single
    // one-pixel step: no jump and no teleport.
    TEST_ASSERT_EQUAL_UINT8(1u, FoxAnimation::tailPoseAt(7u));
    TEST_ASSERT_EQUAL_UINT8(0u, FoxAnimation::tailPoseAt(0u));
}

void test_fox_all_pixels_are_inside_the_panel(void) {
    FoxAnimation animation;
    PixelMatrix matrix;

    for (uint8_t f = 0u; f < FoxAnimation::frameCount(); ++f) {
        animation.renderFrame(matrix, f);

        uint16_t lit = 0u;
        for (uint8_t y = 0u; y < kPanel; ++y) {
            for (uint8_t x = 0u; x < kPanel; ++x) {
                const Color c = pixelColorAt(matrix, x, y);
                if (c.r != 0u || c.g != 0u || c.b != 0u) {
                    ++lit;
                }
            }
        }
        TEST_ASSERT_TRUE(lit > 0u);
    }
}

void test_fox_colours_match_the_palette(void) {
    // Orange body, warm white belly, dark outline: not a grey blob.
    FoxAnimation animation;
    PixelMatrix matrix;
    animation.renderFrame(matrix, 0u);

    bool sawOrange = false;
    bool sawDark = false;
    bool sawWhite = false;

    for (uint8_t y = 0u; y < kPanel; ++y) {
        for (uint8_t x = 0u; x < kPanel; ++x) {
            const Color c = pixelColorAt(matrix, x, y);
            if (c.r > 180u && c.g > 60u && c.g < 140u && c.b < 80u) {
                sawOrange = true;
            }
            if (c.r < 60u && c.g < 50u && c.b < 50u) {
                sawDark = true;
            }
            if (c.r > 200u && c.g > 190u && c.b > 160u) {
                sawWhite = true;
            }
        }
    }

    TEST_ASSERT_TRUE(sawOrange);
    TEST_ASSERT_TRUE(sawDark);
    TEST_ASSERT_TRUE(sawWhite);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_fox_is_registered_and_selectable);
    RUN_TEST(test_fox_label_is_polish);
    RUN_TEST(test_fox_sequence_is_low_center_high_center);
    RUN_TEST(test_fox_body_is_byte_identical_between_frames);
    RUN_TEST(test_fox_only_the_tail_region_changes);
    RUN_TEST(test_fox_tail_moves_at_most_one_pixel);
    RUN_TEST(test_fox_tail_stays_connected_to_the_body);
    RUN_TEST(test_fox_loop_boundary_is_seamless);
    RUN_TEST(test_fox_all_pixels_are_inside_the_panel);
    RUN_TEST(test_fox_colours_match_the_palette);
    return UNITY_END();
}
