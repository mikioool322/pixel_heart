#pragma once

#include <stdint.h>

class PixelMatrix;

class Animation {
public:
    virtual ~Animation() = default;

    virtual void start(uint32_t now) = 0;
    virtual void update(uint32_t now, PixelMatrix& matrix) = 0;
    virtual bool isFinished() const = 0;
    virtual void stop() = 0;
};
