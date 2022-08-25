#pragma once
#include "prelude.hh"
#include "led/transcode.hh"
#include "led/transport.hh"

namespace led {
    struct renderer {
        virtual ret_code_t render(led::transcode *transcoder, BaseType_t &refresh_msec) = 0;
        /* Load the renderer configuration from the `cfg` component. */
        virtual ret_code_t prepare_config() = 0;
        /* Send updated renderer configuration to the `cfg` component, if necessary. */
        virtual ret_code_t check_config() = 0;
    };
}
