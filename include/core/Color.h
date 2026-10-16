#pragma once

#include <stdint.h>

struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

inline Color scaleColor(Color color, uint8_t scale) {
    return {
        static_cast<uint8_t>((static_cast<uint16_t>(color.r) * scale) / 255u),
        static_cast<uint8_t>((static_cast<uint16_t>(color.g) * scale) / 255u),
        static_cast<uint8_t>((static_cast<uint16_t>(color.b) * scale) / 255u),
    };
}

namespace Colors {
constexpr Color Black{0, 0, 0};
constexpr Color Red{255, 0, 0};
constexpr Color Green{0, 255, 0};
constexpr Color Blue{0, 0, 255};
constexpr Color White{255, 255, 255};

constexpr Color HeartOutline{255, 35, 65};
constexpr Color HeartFill{230, 0, 35};
constexpr Color HeartCore{255, 75, 100};
constexpr Color HeartMergeLeft{255, 65, 135};
constexpr Color HeartMergeRight{235, 12, 55};
constexpr Color HeartMergeBlend{255, 105, 155};
constexpr Color HeartMergeFlash{255, 245, 255};
constexpr Color HeartSparkleWhite{255, 245, 245};
constexpr Color HeartSparklePink{255, 150, 180};
constexpr Color HeartSparkleSoft{245, 200, 210};

constexpr Color FireworkRed{255, 35, 55};
constexpr Color FireworkGold{255, 175, 35};
constexpr Color FireworkPink{255, 70, 145};
constexpr Color FireworkBlue{65, 120, 255};
constexpr Color FireworkWhite{255, 245, 225};
constexpr Color FireworkTrail{255, 130, 35};

constexpr Color FlameDark{70, 0, 0};
constexpr Color FlameRed{230, 12, 0};
constexpr Color FlameOrange{255, 75, 0};
constexpr Color FlameGold{255, 175, 20};
constexpr Color FlameWhite{255, 245, 180};

constexpr Color RoseShadow{95, 0, 22};
constexpr Color RoseRed{235, 15, 55};
constexpr Color RosePink{255, 70, 120};
constexpr Color RoseCore{70, 0, 20};
constexpr Color RoseBlush{255, 145, 175};

constexpr Color StemGreen{25, 115, 45};
constexpr Color LeafGreen{40, 160, 65};
}  // namespace Colors
