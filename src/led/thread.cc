#define NRF_LOG_MODULE_NAME led
#include "prelude.hh"
#include "led.hh"
#include "time.hh"
#include "cfg.hh"

using namespace led;

#define CFG_UPDATE_INTERVAL_MSEC 3000
#define SEND_COMPLETE 0x12345678
#define DO_RENDER     0xcdef0123

static size_t m_n_led_threads = 0;
static TaskHandle_t m_led_threads[MAX_LED_CHANNELS];

static void led_task_func(void *context);

ret_code_t thread::init(char const *name)
{
    if (++m_n_led_threads > MAX_LED_CHANNELS) {
        return NRF_ERROR_INVALID_STATE;
    }

    auto result = xTaskCreate(
        led_task_func,
        name,
        256,
        this,
        4,
        &handle
    );

    m_led_threads[m_n_led_threads-1] = handle;

    if (result != pdPASS) {
        return NRF_ERROR_NO_MEM;
    } else {
        return NRF_SUCCESS;
    }
}


void thread::task_func()
{
    /* suspend until the userapp thread calls led::resume_all() */
    vTaskSuspend(handle);

    assert(render_config_param != nullptr);
    assert(tc[0] != nullptr);
    assert(tc[1] != nullptr);
    assert(tp != nullptr);

    ret_code_t ret;
    uint8_t tc_idx = 0;

    /* retrieve the renderer config, or set up default if it is unavailable */
    ret = render_config_param->get(&render_config);
    if (ret == FDS_ERR_NOT_FOUND) {
        render_config.color_mode = (uint8_t)led::color_mode::rgb;
        render_config.n_leds = MAX_LEDS_PER_THREAD;
        render_config.refresh_msec = DEFAULT_REFRESH_RATE_MSEC;
        ret = render_config_param->set(&render_config);
    }
    APP_ERROR_CHECK(ret);

    /* subscribe to the renderer config */
    ret = render_config_param->subscribe(this, [](void *context, void const *data, size_t size) {
        auto self = (thread*)context;
        assert(size >= sizeof(cfg::led_render_t));
        self->on_render_config_change((cfg::led_render_t*)data);
    });
    APP_ERROR_CHECK(ret);

    /* set the transport completion callback */
    tp->on_send_complete(this, [](uint8_t *buffer, size_t length, BaseType_t *do_context_switch, void *context) {
        unused(buffer);
        unused(length);
        auto self = (thread*)context;

        self->on_send_complete(do_context_switch);

        return true;
    });

    /* set initial values in the renderer */
    auto props = renderer_props {
        .render_config = render_config
    };
    render->init_render(props);

    auto cur_tc = tc[tc_idx];

    cur_tc->clear();
    cur_tc->write_bus_reset();
    auto off = color::rgb(color::BLACK);
    for (size_t i = 0; i < MAX_LEDS_PER_THREAD; ++i) {
        cur_tc->write(off);
    }
    cur_tc->write_bus_reset();

    ret = tp->set_buffer(cur_tc->ptr(), cur_tc->len());
    APP_ERROR_CHECK(ret);

    while (1) {
        auto now = time::ticks();

        /* send the buffer */
        ret = tp->send();
        APP_ERROR_CHECK(ret);

        /* call the renderer on the other buffer */
        tc_idx = (tc_idx + 1) & 1;
        cur_tc = tc[tc_idx];

        props.render_config = render_config;
        props.dmx_config = dmx_config;
        memcpy(props.dmx_vals, dmx_vals, std::min(sizeof(props.dmx_vals), sizeof(dmx_vals)));

        if (render) {
            cur_tc->clear();
            ret = render->render(cur_tc, props);
            APP_ERROR_CHECK(ret);
        }

        /* wait until `send()` is finished */
        uint32_t nv = 0;
        auto notify = xTaskNotifyWait(0, UINT32_MAX, &nv, pdMS_TO_TICKS(refresh_msec));
        if (notify != pdPASS) {
            NRF_LOG_WARNING("Missed TX notification")
        }

        /* switch to other buffer */
        ret = tp->set_buffer(cur_tc->ptr(), cur_tc->len());
        APP_ERROR_CHECK(ret);

        vTaskDelayUntil(&now, pdMS_TO_TICKS(refresh_msec));
    }
}

void thread::set_renderer(renderer *r)
{
    render = r;
}

void thread::on_send_complete(BaseType_t *do_context_switch)
{
    auto status = xTaskNotifyFromISR(handle, SEND_COMPLETE, eSetValueWithoutOverwrite, do_context_switch);
    assert(status == pdPASS);
}

void thread::on_render_config_change(cfg::led_render_t *config)
{
    NRF_LOG_DEBUG("config change: mode=%u, nleds=%u, refresh=%u", config->color_mode, config->n_leds, config->refresh_msec);
    render_config = *config;
}

void led::resume_all()
{
    for (size_t i = 0; i < m_n_led_threads; ++i) {
        vTaskResume(m_led_threads[i]);
    }
}

static void led_task_func(void *context)
{
    thread *task = (thread*)context;
    assert(task != nullptr);
    task->task_func();    
}
