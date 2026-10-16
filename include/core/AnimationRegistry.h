#pragma once

#include "core/Animation.h"
#include "core/AnimationId.h"

class AnimationRegistry {
public:
    static Animation* getAnimation(AnimationId id);

    // THE canonical renderability check.
    //
    // Derived directly from getAnimation(), so it can never disagree with the
    // switch that actually instantiates animations: there is exactly one
    // inventory of what this build can render. Never maintain a second list.
    static bool isRenderable(AnimationId id);
};
