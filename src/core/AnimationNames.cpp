#include "core/AnimationNames.h"

// Human-readable Polish display names.
//
// This is the project's single animation-name mapper. The LAN panel and the
// serial control log both use it, so there is no second table to keep in sync.
// Internal identifiers (AnimationId values) are unchanged; only the strings a
// person reads are translated.
const char* animationNameOf(AnimationId id) {
    switch (id) {
        case AnimationId::NONE:
            return "Wyłączone";
        case AnimationId::HEARTBEAT:
            return "Bicie serca";
        case AnimationId::BREATHING_HEART:
            return "Oddychające serce";
        case AnimationId::ROSE_BLOOM:
            return "Kwitnąca róża";
        case AnimationId::FIREWORKS:
            return "Fajerwerki";
        case AnimationId::LOVE_MODE:
            return "Tryb miłosny";
        case AnimationId::CAMPFIRE:
            return "Ognisko";
        case AnimationId::DANCING_DUCK:
            return "Tańcząca kaczka";
        case AnimationId::SLOT_MACHINE:
            return "Jednoręki bandyta";
        case AnimationId::SWIMMING_FISH:
            return "Pływająca rybka";
        case AnimationId::AURORA:
            return "Zorza";
        case AnimationId::AURORA_ARCTIC:
            return "Zorza arktyczna";
        case AnimationId::AURORA_SUNSET:
            return "Zorza zachodu";
        case AnimationId::AURORA_NEON:
            return "Zorza neonowa";
        case AnimationId::CHRISTMAS_TREE:
            return "Choinka";
        case AnimationId::MOON_STARS:
            return "Księżyc i gwiazdy";
        case AnimationId::SUNSET_WATER:
            return "Zachód nad wodą";
        case AnimationId::FALLING_STARS:
            return "Spadające gwiazdy";
        case AnimationId::SUN_OVER_MOUNTAINS:
            return "Słońce nad górami";
        case AnimationId::HALLOWEEN_GHOST:
            return "Duszek";
        case AnimationId::FOX:
            return "Lisek";
        case AnimationId::ORANGE_CAT:
            return "Rudy kot";
        case AnimationId::PALM_BREEZE:
            return "Palma";
        case AnimationId::JELLYFISH:
            return "Meduza";
        case AnimationId::LIGHTHOUSE:
            return "Latarnia morska";
        case AnimationId::DISCO_BALL:
            return "Kula disco";
        case AnimationId::MEADOW_FLOWER:
            return "Kwiat na łące";
        case AnimationId::BLUE_SKY_CLOUDS:
            return "Błękitne niebo";
        default:
            return "Nieznana";
    }
}
