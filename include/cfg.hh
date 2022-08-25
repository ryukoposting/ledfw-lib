#pragma once

#include "prelude.hh"
#include "cfg/ids.hh"
#include "cfg/backend.hh"
#include "cfg/types.hh"

namespace cfg {
    template<typename T, id ID, uint16_t VER, class B = flash_backend>
    struct param {
        static constexpr auto id = ID;
        static constexpr auto version = VER;

        static inline ret_code_t get(T *value)
        {
            if (!value)
                return NRF_ERROR_NULL;

            ret_code_t ret;
            param_value<T> data;

            ret = B().read(id, (void*)&data, sizeof(data));
            VERIFY_SUCCESS(ret);

            if (data.version != version)
                return ERROR_WRONG_VERSION;

            *value = data.value;

            return ret;
        }

        static inline ret_code_t set(T const *value)
        {
            if (!value)
                return NRF_ERROR_NULL;

            ret_code_t ret;
            auto data = param_value<T>(value, version);

            ret = B().write(id, (void const*)&data, sizeof(data));
            VERIFY_SUCCESS(ret);

            return ret;
        }
    };

    ret_code_t init();

    ret_code_t init_flash_backend();

    /* DMX configuration parameters */
    namespace dmx {
        using channel = param<dmx_channel_t, id::dmx_channel, 1>;
    }

    /* LED driver configuration parameters */
    namespace led0 {
        using render = param<led_render_t, id::led0_render, 1>;
    }

    namespace led1 {
        using render = param<led_render_t, id::led1_render, 1>;
    }

    namespace led2 {
        using render = param<led_render_t, id::led2_render, 1>;
    }

    namespace led3 {
        using render = param<led_render_t, id::led3_render, 1>;
    }
}
