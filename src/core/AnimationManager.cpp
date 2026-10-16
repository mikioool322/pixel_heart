#include "core/AnimationManager.h"

#include "core/AnimationRegistry.h"

AnimationManager::AnimationManager()
    : currentAnimation_(nullptr),
      currentAnimationId_(AnimationId::NONE),
      currentMode_(PlayMode::ONCE),
      isPlaying_(false) {
}

void AnimationManager::play(AnimationId id, PlayMode mode, uint32_t now) {
    if (id == AnimationId::NONE) {
        stop();
        return;
    }

    Animation* nextAnimation = AnimationRegistry::getAnimation(id);
    if (nextAnimation == nullptr) {
        stop();
        return;
    }

    if (currentAnimation_ != nullptr && currentAnimation_ != nextAnimation) {
        currentAnimation_->stop();
    }

    currentAnimation_ = nextAnimation;
    currentAnimationId_ = id;
    currentMode_ = mode;
    isPlaying_ = true;
    currentAnimation_->start(now);
}

void AnimationManager::stop() {
    if (currentAnimation_ != nullptr) {
        currentAnimation_->stop();
    }

    currentAnimation_ = nullptr;
    currentAnimationId_ = AnimationId::NONE;
    currentMode_ = PlayMode::ONCE;
    isPlaying_ = false;
}

void AnimationManager::tick(uint32_t now, PixelMatrix& matrix) {
    if (!isPlaying_ || currentAnimation_ == nullptr) {
        return;
    }

    currentAnimation_->update(now, matrix);

    if (!currentAnimation_->isFinished()) {
        return;
    }

    switch (currentMode_) {
        case PlayMode::ONCE:
            stop();
            return;
        case PlayMode::LOOP:
            currentAnimation_->stop();
            currentAnimation_->start(now);
            return;
        case PlayMode::HOLD:
            currentAnimation_->stop();
            isPlaying_ = false;
            return;
        default:
            stop();
            return;
    }
}

AnimationId AnimationManager::currentAnimation() const {
    return currentAnimationId_;
}

bool AnimationManager::isPlaying() const {
    return isPlaying_;
}
