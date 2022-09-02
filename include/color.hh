#pragma once

#include "prelude.hh"

namespace color {
    enum simple {
        BLACK,
        RED,
        GREEN,
        BLUE,
    };

    enum class curve {
        none,
        ws2812,
    };

    packed_struct rgb {
        rgb(uint8_t r, uint8_t g, uint8_t b);

        constexpr rgb(simple color):
            red(  color == RED ? 255
                  : 0),
            green(color == GREEN ? 255
                  : 0),
            blue( color == BLUE ? 255
                  : 0)
        {}

        constexpr rgb(): red(0), green(0), blue(0) {}

        uint8_t red;
        uint8_t green;
        uint8_t blue;
    };

    packed_struct hsv {
        hsv(uint8_t h, uint8_t s, uint8_t v);

        constexpr hsv(): hue(0), saturation(0), value(0) {}

        uint8_t hue;
        uint8_t saturation;
        uint8_t value;

        void to_rgb(rgb& result, curve curve);
    };

    packed_struct hsl {
        hsl(uint8_t h, uint8_t s, uint8_t l);

        constexpr hsl(): hue(0), saturation(0), luminance(0) {}

        uint8_t hue;
        uint8_t saturation;
        uint8_t luminance;

        void to_rgb(rgb& result, curve curve);
    };
}
