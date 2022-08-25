#define NRF_LOG_MODULE_NAME dmx
#include "prelude.hh"
#include "dmx.hh"
#include "dmx/thread.hh"
#include "message_buffer.h"

using namespace dmx;

static void dmx_task(void*);

ret_code_t dmx::thread::init()
{
    auto status = xTaskCreate(
        dmx_task,
        "DMX",
        256,
        this,
        3,
        &task_handle
    );

    if (status != pdPASS) {
        return NRF_ERROR_NO_MEM;
    }

    return NRF_SUCCESS;
}

TaskHandle_t dmx::thread::handle()
{
    return task_handle;
}

void dmx::thread::set_frame_buf(MessageBufferHandle_t queue)
{
    queue_handle = queue;
}

void dmx::thread::task_func()
{
    while (1) {
        if (!queue_handle) continue;

        auto nread = xMessageBufferReceive(queue_handle, rx_buffer, sizeof(rx_buffer), portMAX_DELAY);

        volatile auto new_chan_vals_cb = on_new_chan_values;
        volatile auto contexts = on_new_chan_values_context;
        volatile auto sub_starts = sub_start;
        volatile auto n_subs = n_sub;

        if (sub_starts > 0 &&                              /* a subscription has been set, and... */
            rx_buffer[0] == (uint8_t)start_code::dimmer && /* this is a dimmer packet, and... */
            nread > sub_starts &&                          /* the buffer contains data for the subscribed channels, and... */
            new_chan_vals_cb)                              /* a dimmer value callback has been set */
        {
            auto chan_data = &rx_buffer[sub_starts];
            auto n_chan_datas = std::min(nread, (size_t)n_subs);

            new_chan_vals_cb(chan_data, sub_starts, n_chan_datas, contexts);
        }
    }
}

static void dmx_task(void *arg)
{
    assert(arg != nullptr);
    ((dmx::thread*)arg)->task_func();
}
