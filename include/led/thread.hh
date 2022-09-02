#pragma once

#include "prelude.hh"
#include "led/transport.hh"
#include "led/transcode.hh"
#include "led/renderer.hh"
#include "cfg.hh"
#include "dmx.hh"

namespace led {
    struct thread {
        constexpr thread(transcode *transcoder0, transcode *transcoder1, transport *transport, cfg::param<cfg::led_render_t> *render_config_param):
            handle(nullptr),
            tp(transport),
            tc {transcoder0, transcoder1},
            refresh_msec(DEFAULT_REFRESH_RATE_MSEC),
            render(nullptr),
            render_config_param(render_config_param),
            render_config {},
            dmx_config {},
            dmx_vals {}
        {}

        void set_renderer(renderer *render);

        inline void enable()
        {
            vTaskResume(handle);
        }

        inline bool is_enabled()
        {
            TaskStatus_t status = {};
            vTaskGetInfo(handle, &status, pdFALSE, eInvalid);
            return status.eCurrentState != eSuspended;
        }

        inline void update(dmx::dmx_slot_vals const *vals, cfg::dmx_config_t const *config)
        {
            if (config) {
                dmx_config = *config;
            }

            if (vals) {
                memcpy(dmx_vals, vals->vals, vals->n_vals);
                if (vals->n_vals < sizeof(dmx_vals)) {
                    memset(&dmx_vals[vals->n_vals], 0, sizeof(dmx_vals) - vals->n_vals);
                }
            }
        }

        ret_code_t init(char const *name);

        void task_func();

    protected:
        void on_send_complete(BaseType_t *do_context_switch);
        void on_render_config_change(cfg::led_render_t *config);

        TaskHandle_t handle;
        transport *tp;
        transcode *tc[2];
        BaseType_t refresh_msec;
        renderer *render;
        cfg::param<cfg::led_render_t> *render_config_param;
        cfg::led_render_t render_config;
        cfg::dmx_config_t dmx_config;
        uint8_t dmx_vals[MAX_USER_APP_SLOTS];
    };

    /* every LED thread is suspended by default. Resume them by calling this function. */
    void resume_all();
}
