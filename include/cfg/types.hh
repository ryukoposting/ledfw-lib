#pragma once

#include "prelude.hh"

namespace cfg {
    template<typename T>
    packed_struct param_value {
        param_value(T const *t, uint16_t ver):
            version(ver),
            value(*t)
        {}

        constexpr param_value():
            version(0),
            value(),
            _padding_ {}
        {}

        uint16_t version;
        T value;
    private:
        /* pad the object so that its size is always a multiple of sizeof(size_t) */
        uint8_t _padding_[sizeof(size_t) - 1 - ((sizeof(T) + 1) % sizeof(size_t))];
    };

    packed_struct dmx_channel_t {
        uint16_t channel;
        uint16_t n_channels;
    };

    packed_struct led_render_t {
        uint16_t n_leds;
        uint16_t refresh_msec;
        uint8_t color_mode;
    };
}
