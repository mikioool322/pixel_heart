#pragma once

#include <stdint.h>
#include <string.h>

#define NEO_GRB 1
#define NEO_KHZ800 2

extern int g_neo_show_count;

class Adafruit_NeoPixel {
public:
    Adafruit_NeoPixel(uint16_t n, uint8_t p, uint8_t t)
        : count_(n), pin_(p), type_(t), brightness_(0) {
        memset(pixels_, 0, sizeof(pixels_));
    }

    void begin() {}

    void clear() {
        memset(pixels_, 0, sizeof(pixels_));
    }

    void setPixelColor(uint16_t index, uint8_t r, uint8_t g, uint8_t b) {
        if (index >= count_) {
            return;
        }

        pixels_[index * 3u] = r;
        pixels_[index * 3u + 1u] = g;
        pixels_[index * 3u + 2u] = b;
    }

    void setPixelColor(uint16_t index, uint32_t color) {
        if (index >= count_) {
            return;
        }

        uint8_t r = static_cast<uint8_t>((color >> 16) & 0xFFu);
        uint8_t g = static_cast<uint8_t>((color >> 8) & 0xFFu);
        uint8_t b = static_cast<uint8_t>(color & 0xFFu);
        pixels_[index * 3u] = r;
        pixels_[index * 3u + 1u] = g;
        pixels_[index * 3u + 2u] = b;
    }

    uint32_t getPixelColor(uint16_t index) const {
        if (index >= count_) {
            return 0u;
        }

        return (static_cast<uint32_t>(pixels_[index * 3u]) << 16) |
               (static_cast<uint32_t>(pixels_[index * 3u + 1u]) << 8) |
               static_cast<uint32_t>(pixels_[index * 3u + 2u]);
    }

    void setBrightness(uint8_t brightness) {
        brightness_ = brightness;
    }

    void show() {
        ++g_neo_show_count;
    }

private:
    uint16_t count_;
    uint8_t pin_;
    uint8_t type_;
    uint8_t brightness_;
    uint8_t pixels_[256u * 3u];
};
