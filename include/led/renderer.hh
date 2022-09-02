#pragma once
#include "prelude.hh"
#include "led/transcode.hh"
#include "led/transport.hh"
#include "cfg.hh"

namespace led {
    struct renderer_props {
        cfg::led_render_t render_config;
        cfg::dmx_config_t dmx_config;
        uint8_t dmx_vals[MAX_USER_APP_SLOTS];
    };

    struct renderer {
        virtual ret_code_t render(led::transcode *transcoder, renderer_props const &props) = 0;
        virtual ret_code_t init_render(renderer_props const &props) = 0;
        // /* Load the renderer configuration from the `cfg` component. */
        // virtual ret_code_t prepare_config() = 0;
        // /* Send updated renderer configuration to the `cfg` component, if necessary. */
        // virtual ret_code_t check_config() = 0;
    };
}
