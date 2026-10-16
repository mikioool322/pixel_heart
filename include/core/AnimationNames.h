#pragma once

#include "core/AnimationId.h"

// Short, stable human-readable name for an animation id.
//
// Used only for the read-only LAN status page. Kept out of the animation
// implementations themselves so no display code has to know about it.
const char* animationNameOf(AnimationId id);
