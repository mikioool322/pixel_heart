#include "animations/SwimmingFishAnimation.h"

#include "core/Color.h"
#include "core/PixelMatrix.h"

namespace {

using namespace SwimmingFish;

constexpr Color kFishEye{235, 238, 240};
constexpr Color kFishPupil{15, 20, 30};
constexpr Color kBubble{100, 220, 255};

// One geometry for every fish. Facing right, the head (eye) is on the right
// and the tail column is on the left; facing left is this mask mirrored.
constexpr uint8_t kBodyMask[kFishHeight] = {
    0b001100u,
    0b011110u,
    0b111111u,
    0b011110u,
    0b001100u
};

struct FishDefinition {
    uint8_t paletteIndex;
    uint8_t lane;            // 0 = top, 1 = bottom
    uint8_t facesRight;      // 1 = LEFT -> RIGHT
    uint16_t startMoveStep;  // inclusive
    uint16_t endMoveStep;    // exclusive
    uint8_t bobPhase;
    uint8_t tailPhase;
};

// Deterministic choreography (move steps, 250 ms each):
//
//   A  [ 0, 23)  top,    left  -> right   blue / cyan
//   B  [ 8, 31)  bottom, right -> left    orange
//   C  [23, 46)  top,    left  -> right   purple / pink
//
// A and B share the screen, then B and C share the screen; A and C never do.
// A and C are both on the top lane and B is always on the bottom lane, so two
// simultaneous fish are always on DIFFERENT lanes and can never collide.
//
// Bob phases are all distinct (A=0, B=2, C=1) so simultaneous fish never bob
// in step.
constexpr FishDefinition kFish[kFishCount] = {
    { 0u, 0u, 1u,  0u, 23u, 0u, 0u },
    { 1u, 1u, 0u,  8u, 31u, 2u, 1u },
    { 2u, 0u, 1u, 23u, 46u, 1u, 2u }
};

constexpr Palette kPalettes[kFishCount] = {
    // A - blue / cyan
    { { 40u, 170u, 230u }, { 110u, 220u, 240u }, { 20u, 105u, 210u } },
    // B - orange / warm yellow
    { { 235u, 120u, 35u }, { 255u, 185u, 80u }, { 190u, 75u, 25u } },
    // C - purple / pink
    { { 180u, 75u, 205u }, { 235u, 135u, 225u }, { 120u, 45u, 165u } }
};

// Lanes are 7 rows apart so a 5-row sprite can bob by one row in either
// direction and still never reach the other lane.
constexpr int8_t kLaneBaseY[kLaneCount] = { 2, 9 };

// Slow vertical drift: 0 / -1 / 0 / +1, one row per 250 ms move step.
constexpr int8_t kBobTable[kBobTableSize] = { 0, -1, 0, 1 };

struct BubbleSchedule {
    uint8_t sourceFish;
    uint16_t startStep;   // render ticks
    uint16_t lifeSteps;
    int8_t drift;
};

// Every bubble is born at an already-running fish's mouth and is dead well
// before the loop boundary, so a bubble can never be alive across the seam.
// That is what makes the bubble layer loop as cleanly as the fish layer.
constexpr BubbleSchedule kBubbles[kBubbleCount] = {
    { 0u, 12u, 40u, 1 },
    { 1u, 28u, 44u, -1 },
    { 2u, 60u, 40u, 1 }
};

static_assert(
    kBubbles[0].startStep + kBubbles[0].lifeSteps <= kStepCount,
    "bubble 0 must die before the loop boundary");
static_assert(
    kBubbles[1].startStep + kBubbles[1].lifeSteps <= kStepCount,
    "bubble 1 must die before the loop boundary");
static_assert(
    kBubbles[2].startStep + kBubbles[2].lifeSteps <= kStepCount,
    "bubble 2 must die before the loop boundary");

static_assert(
    kStepCount % 3u == 0u,
    "the tail must complete whole cycles inside one loop");
static_assert(
    kLoopDurationMs % kMoveIntervalMs == 0u,
    "the loop must contain a whole number of horizontal steps");
static_assert(
    kFish[0].endMoveStep - kFish[0].startMoveStep == kCrossingMoveSteps &&
        kFish[1].endMoveStep - kFish[1].startMoveStep == kCrossingMoveSteps &&
        kFish[2].endMoveStep - kFish[2].startMoveStep == kCrossingMoveSteps,
    "every fish must cross the whole display");

bool inBounds(int16_t x, int16_t y) {
    return x >= 0 && y >= 0 &&
           x < static_cast<int16_t>(PixelMatrix::WIDTH) &&
           y < static_cast<int16_t>(PixelMatrix::HEIGHT);
}

}  // namespace

SwimmingFishAnimation::SwimmingFishAnimation()
    : startedAt_(0u),
      lastRenderedStep_(kNoStep),
      finished_(false) {
}

void SwimmingFishAnimation::start(uint32_t now) {
    startedAt_ = now;
    lastRenderedStep_ = kNoStep;
    finished_ = false;
}

void SwimmingFishAnimation::update(uint32_t now, PixelMatrix& matrix) {
    if (finished_) {
        return;
    }

    const uint32_t elapsed = static_cast<uint32_t>(now - startedAt_);
    if (elapsed >= SwimmingFish::kAnimationDurationMs) {
        finished_ = true;
        return;
    }

    const uint16_t step = stepAtElapsed(elapsed);

    // Dirty-frame preservation: the derived scene only changes on a tick
    // boundary, so an unchanged tick must not repaint (or retransmit) anything.
    if (step == lastRenderedStep_) {
        return;
    }

    renderFrame(matrix, step);
    lastRenderedStep_ = step;
}

bool SwimmingFishAnimation::isFinished() const {
    return finished_;
}

void SwimmingFishAnimation::stop() {
    startedAt_ = 0u;
    lastRenderedStep_ = kNoStep;
    finished_ = true;
}

uint16_t SwimmingFishAnimation::stepAtElapsed(uint32_t elapsedMs) {
    return static_cast<uint16_t>(
        (elapsedMs / SwimmingFish::kStepIntervalMs) % SwimmingFish::kStepCount);
}

// ---------------------------------------------------------------------------
// Derived scene queries
// ---------------------------------------------------------------------------

const SwimmingFish::Palette& SwimmingFishAnimation::paletteAt(uint8_t index) {
    return kPalettes[index % SwimmingFish::kFishCount];
}

uint8_t SwimmingFishAnimation::paletteCount() {
    return SwimmingFish::kFishCount;
}

uint8_t SwimmingFishAnimation::fishPaletteIndexAt(uint8_t index) {
    return kFish[index % SwimmingFish::kFishCount].paletteIndex;
}

uint8_t SwimmingFishAnimation::fishLaneAt(uint8_t index) {
    return kFish[index % SwimmingFish::kFishCount].lane;
}

bool SwimmingFishAnimation::fishFacesRightAt(uint8_t index) {
    return kFish[index % SwimmingFish::kFishCount].facesRight != 0u;
}

uint16_t SwimmingFishAnimation::fishStartMoveStepAt(uint8_t index) {
    return kFish[index % SwimmingFish::kFishCount].startMoveStep;
}

uint16_t SwimmingFishAnimation::fishEndMoveStepAt(uint8_t index) {
    return kFish[index % SwimmingFish::kFishCount].endMoveStep;
}

uint8_t SwimmingFishAnimation::fishBobPhaseAt(uint8_t index) {
    return kFish[index % SwimmingFish::kFishCount].bobPhase;
}

uint8_t SwimmingFishAnimation::fishTailPhaseAt(uint8_t index) {
    return kFish[index % SwimmingFish::kFishCount].tailPhase;
}

int8_t SwimmingFishAnimation::laneBaseY(uint8_t lane) {
    return kLaneBaseY[lane % SwimmingFish::kLaneCount];
}

int8_t SwimmingFishAnimation::bobOffsetAt(uint8_t index, uint16_t moveStep) {
    const uint16_t phase = static_cast<uint16_t>(fishBobPhaseAt(index));
    return kBobTable[(moveStep + phase) % SwimmingFish::kBobTableSize];
}

bool SwimmingFishAnimation::fishActiveAt(uint8_t index, uint16_t moveStep) {
    if (index >= SwimmingFish::kFishCount) {
        return false;
    }
    return moveStep >= kFish[index].startMoveStep && moveStep < kFish[index].endMoveStep;
}

int8_t SwimmingFishAnimation::fishXAt(uint8_t index, uint16_t moveStep) {
    const FishDefinition& def = kFish[index % SwimmingFish::kFishCount];

    if (moveStep < def.startMoveStep || moveStep >= def.endMoveStep) {
        // Not on stage: park it fully outside the edge it would come from, so
        // a caller that renders regardless still draws nothing.
        return def.facesRight != 0u ? SwimmingFish::kEntryLeftX : SwimmingFish::kEntryRightX;
    }

    const int16_t travelled = static_cast<int16_t>(moveStep - def.startMoveStep);
    return def.facesRight != 0u
        ? static_cast<int8_t>(SwimmingFish::kEntryLeftX + travelled)
        : static_cast<int8_t>(SwimmingFish::kEntryRightX - travelled);
}

int8_t SwimmingFishAnimation::fishYAt(uint8_t index, uint16_t moveStep) {
    return static_cast<int8_t>(laneBaseY(fishLaneAt(index)) + bobOffsetAt(index, moveStep));
}

uint8_t SwimmingFishAnimation::tailFrameAt(uint8_t index, uint16_t step) {
    const uint16_t phase = static_cast<uint16_t>(fishTailPhaseAt(index));
    return static_cast<uint8_t>((step + phase) % 3u);
}

uint8_t SwimmingFishAnimation::bubbleSourceFish(uint8_t slot) {
    return kBubbles[slot % SwimmingFish::kBubbleCount].sourceFish;
}

uint16_t SwimmingFishAnimation::bubbleStartStep(uint8_t slot) {
    return kBubbles[slot % SwimmingFish::kBubbleCount].startStep;
}

uint16_t SwimmingFishAnimation::bubbleLifeSteps(uint8_t slot) {
    return kBubbles[slot % SwimmingFish::kBubbleCount].lifeSteps;
}

int8_t SwimmingFishAnimation::bubbleDrift(uint8_t slot) {
    return kBubbles[slot % SwimmingFish::kBubbleCount].drift;
}

bool SwimmingFishAnimation::bubbleActiveAt(uint8_t slot, uint16_t step) {
    const uint16_t start = bubbleStartStep(slot);
    return step >= start && step < static_cast<uint16_t>(start + bubbleLifeSteps(slot));
}

int8_t SwimmingFishAnimation::bubbleXAt(uint8_t slot, uint16_t step) {
    const uint16_t age = static_cast<uint16_t>(step - bubbleStartStep(slot));

    // Spawn just outside the source fish's mouth, never inside the sprite.
    const uint8_t fishIndex = bubbleSourceFish(slot);
    const uint16_t spawnMoveStep = static_cast<uint16_t>(
        bubbleStartStep(slot) / SwimmingFish::kStepsPerMove);
    const int8_t mouthX = fishXAt(fishIndex, spawnMoveStep);
    const int16_t spawnX = fishFacesRightAt(fishIndex)
        ? static_cast<int16_t>(mouthX) + static_cast<int16_t>(SwimmingFish::kFishWidth)
        : static_cast<int16_t>(mouthX) - 1;

    const int16_t drift = static_cast<int16_t>(
        age / SwimmingFish::kBubbleDriftStepsPerPixel) *
        static_cast<int16_t>(bubbleDrift(slot));

    return static_cast<int8_t>(spawnX + drift);
}

int8_t SwimmingFishAnimation::bubbleYAt(uint8_t slot, uint16_t step) {
    const uint16_t age = static_cast<uint16_t>(step - bubbleStartStep(slot));
    const uint16_t spawnMoveStep = static_cast<uint16_t>(
        bubbleStartStep(slot) / SwimmingFish::kStepsPerMove);
    const int16_t spawnY = static_cast<int16_t>(
        fishYAt(bubbleSourceFish(slot), spawnMoveStep)) + 2;

    return static_cast<int8_t>(
        spawnY - static_cast<int16_t>(age / SwimmingFish::kBubbleRiseStepsPerPixel));
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void SwimmingFishAnimation::renderFrame(PixelMatrix& matrix, uint16_t step) const {
    matrix.clear();

    // Bubbles first, fish on top: a freshly released bubble is drawn behind the
    // mouth it came from rather than over it.
    renderBubbles(matrix, step);

    const uint16_t moveStep = static_cast<uint16_t>(step / SwimmingFish::kStepsPerMove);

    for (uint8_t i = 0u; i < SwimmingFish::kFishCount; ++i) {
        if (!fishActiveAt(i, moveStep)) {
            continue;
        }

        drawFish(
            matrix,
            fishXAt(i, moveStep),
            fishYAt(i, moveStep),
            fishFacesRightAt(i),
            tailFrameAt(i, step),
            paletteAt(i));
    }
}

void SwimmingFishAnimation::renderBubbles(PixelMatrix& matrix, uint16_t step) const {
    for (uint8_t slot = 0u; slot < SwimmingFish::kBubbleCount; ++slot) {
        if (!bubbleActiveAt(slot, step)) {
            continue;
        }

        const int16_t bx = bubbleXAt(slot, step);
        const int16_t by = bubbleYAt(slot, step);

        if (!inBounds(bx, by)) {
            continue;
        }

        const Color color = (by <= 4)
            ? scaleColor(kBubble, 170u)
            : kBubble;

        matrix.setPixel(static_cast<uint8_t>(bx), static_cast<uint8_t>(by), color);
    }
}

void SwimmingFishAnimation::drawFish(
    PixelMatrix& matrix,
    int16_t originX,
    int16_t originY,
    bool facingRight,
    uint8_t tailFrame,
    const SwimmingFish::Palette& palette
) const {
    constexpr uint8_t width = SwimmingFish::kFishWidth;
    constexpr uint8_t height = SwimmingFish::kFishHeight;

    const auto mapLocalX = [&](uint8_t localX) -> int16_t {
        return facingRight
            ? static_cast<int16_t>(localX)
            : static_cast<int16_t>(width - 1u - localX);
    };

    for (uint8_t localY = 0u; localY < height; ++localY) {
        const uint8_t bits = kBodyMask[localY];

        for (uint8_t localX = 0u; localX < width; ++localX) {
            const uint8_t bit = static_cast<uint8_t>(1u << (width - 1u - localX));
            if ((bits & bit) == 0u) {
                continue;
            }

            const int16_t px = originX + mapLocalX(localX);
            const int16_t py = originY + static_cast<int16_t>(localY);
            if (!inBounds(px, py)) {
                continue;
            }

            const Color color =
                (localX == 3u && (localY == 1u || localY == 2u))
                    ? palette.highlight
                    : palette.body;

            matrix.setPixel(static_cast<uint8_t>(px), static_cast<uint8_t>(py), color);
        }
    }

    // Tail: always the column opposite the head, wobbling between three poses.
    const int16_t tailPx = originX + mapLocalX(0u);

    switch (tailFrame) {
        case 0u:
            for (uint8_t localY = 1u; localY <= 3u; localY += 2u) {
                const int16_t py = originY + static_cast<int16_t>(localY);
                if (inBounds(tailPx, py)) {
                    matrix.setPixel(
                        static_cast<uint8_t>(tailPx), static_cast<uint8_t>(py), palette.tail);
                }
            }
            break;

        case 1u:
            for (uint8_t localY = 0u; localY <= 4u; localY += 4u) {
                const int16_t py = originY + static_cast<int16_t>(localY);
                if (inBounds(tailPx, py)) {
                    matrix.setPixel(
                        static_cast<uint8_t>(tailPx), static_cast<uint8_t>(py), palette.tail);
                }
            }
            break;

        case 2u:
        default: {
            const int16_t py = originY + 2;
            if (inBounds(tailPx, py)) {
                matrix.setPixel(
                    static_cast<uint8_t>(tailPx), static_cast<uint8_t>(py), palette.tail);
            }
            break;
        }
    }

    // Eye and pupil share the head column, so the gaze follows the direction of
    // travel without any per-fish logic.
    const int16_t eyePx = originX + mapLocalX(4u);

    const int16_t eyePy = originY + 1;
    if (inBounds(eyePx, eyePy)) {
        matrix.setPixel(static_cast<uint8_t>(eyePx), static_cast<uint8_t>(eyePy), kFishEye);
    }

    const int16_t pupilPy = originY + 2;
    if (inBounds(eyePx, pupilPy)) {
        matrix.setPixel(static_cast<uint8_t>(eyePx), static_cast<uint8_t>(pupilPy), kFishPupil);
    }
}
