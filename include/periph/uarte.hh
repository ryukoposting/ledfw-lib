#pragma once

#include "dmx/transport.hh"
#include "sdk_config.h"

namespace uarte {
    using pin_t = uint32_t;

    enum id: uintptr_t {
#if NRFX_UARTE0_ENABLED
        UARTE0,
#endif
#if NRFX_UARTE1_ENABLED
        UARTE1,
#endif
#if NRFX_UARTE2_ENABLED
        UARTE2,
#endif
#if NRFX_UARTE3_ENABLED
        UARTE3,
#endif
        MAX_UARTE_INST
    };

    enum baud_rate: uint32_t {
        BAUD_250K,
    };

    struct uarte_init {
        baud_rate baud;
        pin_t rx;
        bool two_stop_bits;
    };

    struct transport: dmx::transport {
        constexpr transport(id inst):
            inst_id(inst)
        {}

        ret_code_t init(uarte_init const &init);

        MessageBufferHandle_t frame_buf() override;

        ret_code_t enable() override;

        ret_code_t disable() override;

    protected:
        id inst_id;
    };
};
