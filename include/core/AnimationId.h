#pragma once

#include <stdint.h>

enum class AnimationId : uint8_t {
    NONE = 0,
    HEARTBEAT,
    BREATHING_HEART,
    ROSE_BLOOM,
    FIREWORKS,
    LOVE_MODE,
    CAMPFIRE,
    DANCING_DUCK,
    SLOT_MACHINE,
    SWIMMING_FISH,
    AURORA,
    AURORA_ARCTIC,
    AURORA_SUNSET,
    AURORA_NEON,
    CHRISTMAS_TREE,
    MOON_STARS,
    SUNSET_WATER,
    FALLING_STARS,
    SUN_OVER_MOUNTAINS,
    HALLOWEEN_GHOST,
    FOX,
    ORANGE_CAT,
    PALM_BREEZE,
    JELLYFISH,
    LIGHTHOUSE,
    DISCO_BALL,
    MEADOW_FLOWER,
    BLUE_SKY_CLOUDS,

    // Sentinel only. Not an animation: it exists so the animation list has a
    // single source of truth and no caller has to hardcode the last id.
    // Adding it changes no existing value.
    ANIMATION_ID_COUNT
};

// Persisted-ordinal guard: settings store the raw numeric value in NVS, so
// existing ids must never be renumbered. New ids are appended above, and this
// list pins every value that has ever shipped.
static_assert(static_cast<uint8_t>(AnimationId::NONE) == 0u, "ord NONE");
static_assert(static_cast<uint8_t>(AnimationId::HEARTBEAT) == 1u, "ord HEARTBEAT");
static_assert(static_cast<uint8_t>(AnimationId::BREATHING_HEART) == 2u, "ord BREATHING_HEART");
static_assert(static_cast<uint8_t>(AnimationId::ROSE_BLOOM) == 3u, "ord ROSE_BLOOM");
static_assert(static_cast<uint8_t>(AnimationId::FIREWORKS) == 4u, "ord FIREWORKS");
static_assert(static_cast<uint8_t>(AnimationId::LOVE_MODE) == 5u, "ord LOVE_MODE");
static_assert(static_cast<uint8_t>(AnimationId::CAMPFIRE) == 6u, "ord CAMPFIRE");
static_assert(static_cast<uint8_t>(AnimationId::DANCING_DUCK) == 7u, "ord DANCING_DUCK");
static_assert(static_cast<uint8_t>(AnimationId::SLOT_MACHINE) == 8u, "ord SLOT_MACHINE");
static_assert(static_cast<uint8_t>(AnimationId::SWIMMING_FISH) == 9u, "ord SWIMMING_FISH");
static_assert(static_cast<uint8_t>(AnimationId::AURORA) == 10u, "ord AURORA");
static_assert(static_cast<uint8_t>(AnimationId::AURORA_ARCTIC) == 11u, "ord AURORA_ARCTIC");
static_assert(static_cast<uint8_t>(AnimationId::AURORA_SUNSET) == 12u, "ord AURORA_SUNSET");
static_assert(static_cast<uint8_t>(AnimationId::AURORA_NEON) == 13u, "ord AURORA_NEON");
static_assert(static_cast<uint8_t>(AnimationId::CHRISTMAS_TREE) == 14u, "ord CHRISTMAS_TREE");
static_assert(static_cast<uint8_t>(AnimationId::MOON_STARS) == 15u, "ord MOON_STARS");
static_assert(static_cast<uint8_t>(AnimationId::SUNSET_WATER) == 16u, "ord SUNSET_WATER");
static_assert(static_cast<uint8_t>(AnimationId::FALLING_STARS) == 17u, "ord FALLING_STARS");
static_assert(static_cast<uint8_t>(AnimationId::SUN_OVER_MOUNTAINS) == 18u, "ord SUN_OVER_MOUNTAINS");
static_assert(static_cast<uint8_t>(AnimationId::HALLOWEEN_GHOST) == 19u, "ord HALLOWEEN_GHOST");
static_assert(static_cast<uint8_t>(AnimationId::FOX) == 20u, "ord FOX");
static_assert(static_cast<uint8_t>(AnimationId::ORANGE_CAT) == 21u, "ord ORANGE_CAT");
static_assert(static_cast<uint8_t>(AnimationId::PALM_BREEZE) == 22u, "ord PALM_BREEZE");
static_assert(static_cast<uint8_t>(AnimationId::JELLYFISH) == 23u, "ord JELLYFISH");
static_assert(static_cast<uint8_t>(AnimationId::LIGHTHOUSE) == 24u, "ord LIGHTHOUSE");
static_assert(static_cast<uint8_t>(AnimationId::DISCO_BALL) == 25u, "ord DISCO_BALL");
static_assert(static_cast<uint8_t>(AnimationId::MEADOW_FLOWER) == 26u, "ord MEADOW_FLOWER");
static_assert(static_cast<uint8_t>(AnimationId::BLUE_SKY_CLOUDS) == 27u, "ord BLUE_SKY_CLOUDS");
