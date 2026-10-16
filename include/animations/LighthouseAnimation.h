#pragma once

#include <stdint.h>

#include "core/Animation.h"
#include "core/PixelMatrix.h"

// A dark night scene with a lighthouse sweeping a warm beam across the panel.
//
// The scene itself never moves: only the beam, and three small reflections of it
// on the water. The beam is a real wedge - one to two pixels tall at the beacon,
// about seven at the far edge - drawn as three brightness bands so it reads as
// light rather than as a line.
class LighthouseAnimation : public Animation {
public:
    LighthouseAnimation();

    void start(uint32_t now) override;
    void update(uint32_t now, PixelMatrix& matrix) override;
    bool isFinished() const override;
    void stop() override;

    // One beam step is 300 ms; a full loop is 12 steps = 3600 ms.
    static constexpr uint16_t kStepIntervalMs = 300u;
    static constexpr uint8_t kStepCount = 12u;
    static constexpr uint32_t kLoopDurationMs =
        static_cast<uint32_t>(kStepCount) * kStepIntervalMs;

    // Seven distinct beam elevations, swept 0 -> 6 -> 1 and back.
    static constexpr uint8_t kBeamElevationCount = 7u;

    static constexpr uint8_t kBeamOriginX = 5u;
    static constexpr uint8_t kBeamOriginY = 6u;
    static constexpr uint8_t kBeamFarX = 15u;

    // Scene geometry, exposed so the tests can assert invariants directly.
    static uint8_t elevationAtStep(uint8_t step);
    static int8_t elevationOffset(uint8_t elevation);

    static uint8_t beamCentreY(uint8_t elevation, uint8_t x);
    static uint8_t beamHalfWidth(uint8_t x);

    static bool towerContains(uint8_t x, uint8_t y);
    static bool beaconContains(uint8_t x, uint8_t y);
    static bool baseContains(uint8_t x, uint8_t y);

    static uint8_t starCount();
    static uint8_t starX(uint8_t index);
    static uint8_t starY(uint8_t index);

    static uint8_t reflectionCount();
    static uint8_t reflectionX(uint8_t index);
    static uint8_t reflectionLevel(uint8_t elevation, uint8_t index);

private:
    void renderFrame(PixelMatrix& matrix, uint8_t step) const;
    void drawSky(PixelMatrix& matrix) const;
    void drawSea(PixelMatrix& matrix) const;
    void drawStars(PixelMatrix& matrix) const;
    void drawLighthouse(PixelMatrix& matrix) const;
    void drawBeam(PixelMatrix& matrix, uint8_t elevation) const;
    void drawReflections(PixelMatrix& matrix, uint8_t elevation) const;

    uint32_t startedAt_;
    // 0xFF is not a valid step, so the first update paints immediately.
    uint8_t lastStep_;
    bool finished_;
};
