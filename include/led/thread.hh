#pragma once

#include "prelude.hh"
#include "led/transport.hh"
#include "led/transcode.hh"
#include "led/renderer.hh"

namespace led {
    struct thread {
        constexpr thread(transcode *transcoder0, transcode *transcoder1, transport *transport):
            handle(nullptr),
            tp(transport),
            tc {transcoder0, transcoder1},
            refresh_msec(DEFAULT_REFRESH_RATE_MSEC),
            render(nullptr)
        {};

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

        ret_code_t init(char const *name);

        void task_func();

    protected:
        void on_send_complete(BaseType_t *do_context_switch);

        TaskHandle_t handle;
        transport *tp;
        transcode *tc[2];
        BaseType_t refresh_msec;
        renderer *render;
    };

    /* every LED thread is suspended by default. Resume them by calling this function. */
    void resume_all();
}
