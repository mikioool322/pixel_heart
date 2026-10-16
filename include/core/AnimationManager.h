#pragma once

#include <stdint.h>

#include "core/Animation.h"
#include "core/AnimationId.h"
#include "core/PlayMode.h"
#include "core/PixelMatrix.h"

class AnimationManager {
public:
    AnimationManager();

    void play(AnimationId id, PlayMode mode, uint32_t now);
    void stop();
    void tick(uint32_t now, PixelMatrix& matrix);
    AnimationId currentAnimation() const;
    bool isPlaying() const;

private:
    Animation* currentAnimation_;
    AnimationId currentAnimationId_;
    PlayMode currentMode_;
    bool isPlaying_;
};
