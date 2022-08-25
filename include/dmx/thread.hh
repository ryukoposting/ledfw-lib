#pragma once

#include "prelude.hh"
#include "dmx/transport.hh"
#include "message_buffer.h"

namespace dmx {
    using on_new_chan_values_t = void (*)(uint8_t const *vals, size_t chan, size_t len, void *context);
    struct thread {
        constexpr thread():
            sub_start(0),
            n_sub(0),
            rx_buffer {},
            task_handle(nullptr),
            queue_handle(nullptr),
            on_new_chan_values(nullptr),
            on_new_chan_values_context(nullptr)
        {}

        ret_code_t init();

        TaskHandle_t handle();

        void set_frame_buf(MessageBufferHandle_t queue);

        inline bool is_initialized()
        {
            return handle() != nullptr;
        }

        inline ret_code_t set_subscription(size_t start, size_t len)
        {
            if (start + len > 512 || (start == 0 && len > 0))
                return NRF_ERROR_INVALID_PARAM;
            if (len > MAX_SUBSCRIBED_DMX_CHANNELS)
                return NRF_ERROR_INVALID_PARAM;

            sub_start = start;
            n_sub = len;
            return NRF_SUCCESS;
        }

        inline void set_on_new_chan_values(void *context, on_new_chan_values_t callback)
        {
            on_new_chan_values = callback;
            on_new_chan_values_context = context;
        }

        void task_func();

    protected:
        size_t sub_start;
        size_t n_sub;
        uint8_t rx_buffer[DMX_MAX_FRAME_SIZE];
        TaskHandle_t task_handle;
        MessageBufferHandle_t queue_handle;
        on_new_chan_values_t on_new_chan_values;
        void *on_new_chan_values_context;
    };
}
