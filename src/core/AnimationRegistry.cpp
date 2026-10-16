#include "core/AnimationRegistry.h"

#include "animations/AuroraAnimation.h"
#include "animations/BlueSkyCloudsAnimation.h"
#include "animations/ChristmasTreeAnimation.h"
#include "animations/DancingDuckAnimation.h"
#include "animations/DiscoBallAnimation.h"
#include "animations/FallingStarsAnimation.h"
#include "animations/FoxAnimation.h"
#include "animations/HalloweenGhostAnimation.h"
#include "animations/HeartbeatAnimation.h"
#include "animations/JellyfishAnimation.h"
#include "animations/LighthouseAnimation.h"
#include "animations/MeadowFlowerAnimation.h"
#include "animations/MoonStarsAnimation.h"
#include "animations/OrangeCatAnimation.h"
#include "animations/PalmBreezeAnimation.h"
#include "animations/SlotMachineAnimation.h"
#include "animations/SunOverMountainsAnimation.h"
#include "animations/SunsetWaterAnimation.h"
#include "animations/SwimmingFishAnimation.h"

#if !defined(ARDUINO_AVR_UNO)
#include "animations/CampfireAnimation.h"
#include "animations/FireworksAnimation.h"
#include "animations/RoseBloomAnimation.h"
#endif

namespace {
#if defined(ARDUINO_AVR_UNO)
ChristmasTreeAnimation christmasTreeAnimation;
MoonStarsAnimation moonStarsAnimation;
SunsetWaterAnimation sunsetWaterAnimation;
FallingStarsAnimation fallingStarsAnimation;
SunOverMountainsAnimation sunOverMountainsAnimation;
PalmBreezeAnimation palmBreezeAnimation;
MeadowFlowerAnimation meadowFlowerAnimation;
BlueSkyCloudsAnimation blueSkyCloudsAnimation;
#else
HeartbeatAnimation heartbeatAnimation;
RoseBloomAnimation roseBloomAnimation;
FireworksAnimation fireworksAnimation;
CampfireAnimation campfireAnimation;
DancingDuckAnimation dancingDuckAnimation;
SlotMachineAnimation slotMachineAnimation;
SwimmingFishAnimation swimmingFishAnimation;
HalloweenGhostAnimation halloweenGhostAnimation;
FoxAnimation foxAnimation;
OrangeCatAnimation orangeCatAnimation;
PalmBreezeAnimation palmBreezeAnimation;
// ESP-only, like the three above: these carry enough lookup data that AVR-GCC
// would copy it into RAM, and the Uno has no headroom for that.
JellyfishAnimation jellyfishAnimation;
LighthouseAnimation lighthouseAnimation;
DiscoBallAnimation discoBallAnimation;
MeadowFlowerAnimation meadowFlowerAnimation;
BlueSkyCloudsAnimation blueSkyCloudsAnimation;
AuroraAnimation auroraArctic(AuroraPaletteId::ARCTIC);
AuroraAnimation auroraSunset(AuroraPaletteId::SUNSET);
AuroraAnimation auroraNeon(AuroraPaletteId::NEON);
ChristmasTreeAnimation christmasTreeAnimation;
MoonStarsAnimation moonStarsAnimation;
SunsetWaterAnimation sunsetWaterAnimation;
FallingStarsAnimation fallingStarsAnimation;
SunOverMountainsAnimation sunOverMountainsAnimation;
#endif
}

bool AnimationRegistry::isRenderable(AnimationId id) {
    // Deliberately implemented in terms of getAnimation() rather than as an
    // independent list: the registry entry IS the definition of renderable.
    return getAnimation(id) != nullptr;
}

Animation* AnimationRegistry::getAnimation(AnimationId id) {
#if defined(ARDUINO_AVR_UNO)
    switch (id) {
        case AnimationId::CHRISTMAS_TREE:
            return &christmasTreeAnimation;
        case AnimationId::MOON_STARS:
            return &moonStarsAnimation;
        case AnimationId::SUNSET_WATER:
            return &sunsetWaterAnimation;
        case AnimationId::FALLING_STARS:
            return &fallingStarsAnimation;
        case AnimationId::SUN_OVER_MOUNTAINS:
            return &sunOverMountainsAnimation;
        case AnimationId::PALM_BREEZE:
            return &palmBreezeAnimation;
        case AnimationId::MEADOW_FLOWER:
            return &meadowFlowerAnimation;
        case AnimationId::BLUE_SKY_CLOUDS:
            return &blueSkyCloudsAnimation;
        default:
            return nullptr;
    }
#else
    switch (id) {
        case AnimationId::HEARTBEAT:
            return &heartbeatAnimation;
        case AnimationId::ROSE_BLOOM:
            return &roseBloomAnimation;
        case AnimationId::FIREWORKS:
            return &fireworksAnimation;
        case AnimationId::CAMPFIRE:
            return &campfireAnimation;
        case AnimationId::DANCING_DUCK:
            return &dancingDuckAnimation;
        case AnimationId::SLOT_MACHINE:
            return &slotMachineAnimation;
        case AnimationId::SWIMMING_FISH:
            return &swimmingFishAnimation;
        case AnimationId::AURORA:
        case AnimationId::AURORA_ARCTIC:
            return &auroraArctic;
        case AnimationId::AURORA_SUNSET:
            return &auroraSunset;
        case AnimationId::AURORA_NEON:
            return &auroraNeon;
        case AnimationId::CHRISTMAS_TREE:
            return &christmasTreeAnimation;
        case AnimationId::MOON_STARS:
            return &moonStarsAnimation;
        case AnimationId::SUNSET_WATER:
            return &sunsetWaterAnimation;
        case AnimationId::FALLING_STARS:
            return &fallingStarsAnimation;
        case AnimationId::SUN_OVER_MOUNTAINS:
            return &sunOverMountainsAnimation;
        case AnimationId::HALLOWEEN_GHOST:
            return &halloweenGhostAnimation;
        case AnimationId::FOX:
            return &foxAnimation;
        case AnimationId::ORANGE_CAT:
            return &orangeCatAnimation;
        case AnimationId::PALM_BREEZE:
            return &palmBreezeAnimation;
        case AnimationId::JELLYFISH:
            return &jellyfishAnimation;
        case AnimationId::LIGHTHOUSE:
            return &lighthouseAnimation;
        case AnimationId::DISCO_BALL:
            return &discoBallAnimation;
        case AnimationId::MEADOW_FLOWER:
            return &meadowFlowerAnimation;
        case AnimationId::BLUE_SKY_CLOUDS:
            return &blueSkyCloudsAnimation;
        default:
            return nullptr;
    }
#endif
}
