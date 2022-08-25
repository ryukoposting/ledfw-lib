#pragma once

#include "prelude.hh"
#include "dmx/transport.hh"
#include "dmx/thread.hh"
#include "cfg.hh"

namespace dmx {
    enum class start_code: uint8_t {
        dimmer = 0,
        ascii_text = 0x17,
        manuf_specific = 0x91,
        rdm = 0xcc,
    };

    enum class rdm_sub: uint8_t {
        message = 0x01,
    };
}
