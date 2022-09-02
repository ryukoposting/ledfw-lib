#pragma once

#include "prelude.hh"
#include "dmx/transport.hh"
#include "message_buffer.h"
#include "util.hh"
#include "cfg.hh"
#include "task/lock.hh"

namespace dmx {
    struct dmx_slot_vals {
        size_t n_vals;
        uint8_t const *vals;
    };

    using on_dmx_slot_vals_t = void (*)(void *, dmx_slot_vals const *, cfg::dmx_config_t const *);

    struct thread {
        constexpr thread():
            rx_buffer {},
            packet_task_handle(nullptr),
            slot_vals_subs(nullptr),
            packet_queue_handle(nullptr),
            dmx_config(),
            slot_vals_subs_mutex(nullptr)
        {}

        struct on_dmx_slot_vals_sub {
            constexpr on_dmx_slot_vals_sub(on_dmx_slot_vals_t callback, void *context):
                callback(callback),
                context(context),
                next(nullptr)
            {}
            on_dmx_slot_vals_t callback;
            void *context;
            on_dmx_slot_vals_sub *next;
        };

        ret_code_t init();

        /* sets the buffer that the thread will use to receive DMX/RDM packets */
        void set_frame_buf(MessageBufferHandle_t queue);

        inline bool is_initialized()
        {
            return packet_task_handle != nullptr;
        }

        ret_code_t on_dmx_slot_vals(void *context, on_dmx_slot_vals_t callback, TickType_t max_delay);

        inline ret_code_t on_dmx_slot_vals(void *context, on_dmx_slot_vals_t callback)
        {
            return on_dmx_slot_vals(context, callback, portMAX_DELAY);
        }

        ret_code_t send(dmx_slot_vals const *);

        void packet_task_func();
        void slot_vals_task_func();
    protected:
        void on_channel_cfg_update(cfg::dmx_config_t const *config);
        uint8_t rx_buffer[DMX_MAX_FRAME_SIZE];
        TaskHandle_t packet_task_handle;
        on_dmx_slot_vals_sub *slot_vals_subs;
        MessageBufferHandle_t packet_queue_handle;
        // double_buf<cfg::dmx_config_t> dmx_config;
        task::lock<cfg::dmx_config_t> dmx_config;
        xSemaphoreHandle slot_vals_subs_mutex;
    };
}
