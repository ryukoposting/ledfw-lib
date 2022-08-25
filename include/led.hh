#pragma once

#include "prelude.hh"
#include "led/transport.hh"
#include "led/transcode.hh"
#include "led/renderer.hh"
#include "led/thread.hh"

namespace led {
    enum class color_mode: uint8_t {
        rgb = 0,
        hsv = 1,
        hsl = 2,
    };

    void reset_all();
}
