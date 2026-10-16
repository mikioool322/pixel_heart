#pragma once

#include <stdint.h>

#include "core/Animation.h"
#include "core/Color.h"
#include "core/PixelMatrix.h"

class HalloweenGhostAnimation : public Animation {
public:
    HalloweenGhostAnimation();

    void start(uint32_t now) override;

    void update(
        uint32_t now,
        PixelMatrix& matrix
    ) override;

    bool isFinished() const override;

    void stop() override;

private:
    // Gaze direction of the pupils.
    enum class Gaze : uint8_t {
        LEFT,
        CENTER,
        RIGHT
    };

    void renderFrame(
        PixelMatrix& matrix,
        uint8_t frame
    ) const;

    // Scene geometry. Single source of truth shared by rendering and tests, so
    // the timeline, the silhouette and the hem can never drift apart.
    static int8_t positionAt(uint8_t frame);
    static Gaze gazeAt(uint8_t frame);
    static uint8_t hemAt(uint8_t frame);
    static uint8_t frameCount();

    static uint8_t pupilX(bool rightEye, Gaze gaze);

    // Body silhouette rows 0..13.
    static bool bodyPixelAt(uint8_t x, uint8_t y);

    // Hem fringe: row is 14 or 15, hem is 0 or 1.
    static bool hemPixelAt(uint8_t hem, uint8_t row, uint8_t x);

    static Color shadeAt(uint8_t x, uint8_t y);

    uint32_t lastFrameAt_;

    uint8_t phase_;
    uint8_t renderedFrameCount_;

    bool finished_;
};
