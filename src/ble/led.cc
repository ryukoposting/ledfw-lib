#define NRF_LOG_MODULE_NAME led
#include "prelude.hh"
#include "led.hh"
#include "ble/led.hh"
#include "ble/meta.hh"
#include "color.hh"
#include "userapp.hh"
#include "util.hh"
#include "task.hh"
#include "cfg.hh"
#include "fds.h"
#include "queue.h"

#if MAX_LED_CHANNELS > 4
#error MAX_LED_CHANNELS cannot be more than 4
#endif

#if MAX_LED_CHANNELS < 1
#error MAX_LED_CHANNELS cannot be less than 1
#endif

using namespace led;

enum class handle_write_type: uint8_t { color_mode, num_leds, refresh_rate };

struct service_context;

packed_struct handle_write_data {
    service_context *serv_context;
    ble::characteristic *characteristic;
    union {
        uint8_t color_mode;
        uint16_t num_leds;
        uint16_t refresh_rate;
    };
    handle_write_type type;
    bool save;
};

static TaskHandle_t m_handle_led_prop_write_thread;
static QueueHandle_t m_handle_led_prop_write_queue;

struct service_context {
    constexpr service_context(cfg::param<cfg::led_render_t> param):
        buf_num_leds { MAX_LEDS_PER_THREAD % (UINT8_MAX+1), MAX_LEDS_PER_THREAD / (UINT8_MAX+1) },
        buf_color_mode { (uint8_t)led::color_mode::rgb },
        buf_refresh_rate { DEFAULT_REFRESH_RATE_MSEC % (UINT8_MAX+1), DEFAULT_REFRESH_RATE_MSEC / (UINT8_MAX+1) },
        config_param(param),
        reset_strip(true),
        seqwrite_offset(0),
        user_buffer {}
    {}

    uint8_t buf_num_leds[sizeof(uint16_t)];
    uint8_t buf_color_mode[sizeof(uint8_t)];
    uint8_t buf_refresh_rate[sizeof(uint16_t)];
    cfg::param<cfg::led_render_t> config_param;
    bool reset_strip;
    size_t seqwrite_offset;
    uint8_t user_buffer[MAX_LEDS_PER_THREAD * 3];

    void reject_write_color_mode(ble::characteristic *characteristic)
    {
        auto data = handle_write_data {
            .serv_context = this,
            .characteristic = characteristic,
            .type = handle_write_type::color_mode,
            .save = false,
        };
        if (task::is_in_isr())
            xQueueSendFromISR(m_handle_led_prop_write_queue, &data, nullptr);
        else
            xQueueSend(m_handle_led_prop_write_queue, &data, 1);
    }

    void reject_write_refresh_rate(ble::characteristic *characteristic)
    {
        auto data = handle_write_data {
            .serv_context = this,
            .characteristic = characteristic,
            .type = handle_write_type::refresh_rate,
            .save = false,
        };
        if (task::is_in_isr())
            xQueueSendFromISR(m_handle_led_prop_write_queue, &data, nullptr);
        else
            xQueueSend(m_handle_led_prop_write_queue, &data, 1);
    }

    void reject_write_num_leds(ble::characteristic *characteristic)
    {
        auto data = handle_write_data {
            .serv_context = this,
            .characteristic = characteristic,
            .type = handle_write_type::num_leds,
            .save = false,
        };
        if (task::is_in_isr())
            xQueueSendFromISR(m_handle_led_prop_write_queue, &data, nullptr);
        else
            xQueueSend(m_handle_led_prop_write_queue, &data, 1);
    }


    void accept_write_color_mode(uint8_t value, ble::characteristic *characteristic)
    {
        auto data = handle_write_data {
            .serv_context = this,
            .characteristic = characteristic,
            .color_mode = value,
            .type = handle_write_type::color_mode,
            .save = true
        };
        if (task::is_in_isr())
            xQueueSendFromISR(m_handle_led_prop_write_queue, &data, nullptr);
        else
            xQueueSend(m_handle_led_prop_write_queue, &data, 1);
    }

    void accept_write_refresh_rate(uint16_t value, ble::characteristic *characteristic)
    {
        auto data = handle_write_data {
            .serv_context = this,
            .characteristic = characteristic,
            .refresh_rate = value,
            .type = handle_write_type::refresh_rate,
            .save = true
        };
        if (task::is_in_isr())
            xQueueSendFromISR(m_handle_led_prop_write_queue, &data, nullptr);
        else
            xQueueSend(m_handle_led_prop_write_queue, &data, 1);
    }

    void accept_write_num_leds(uint16_t value, ble::characteristic *characteristic)
    {
        auto data = handle_write_data {
            .serv_context = this,
            .characteristic = characteristic,
            .num_leds = value,
            .type = handle_write_type::num_leds,
            .save = true
        };
        if (task::is_in_isr())
            xQueueSendFromISR(m_handle_led_prop_write_queue, &data, nullptr);
        else
            xQueueSend(m_handle_led_prop_write_queue, &data, 1);
    }

};

static void handle_led_prop_write(void *context)
{
    ret_code_t ret;
    while (1) {
        handle_write_data req;
        xQueueReceive(m_handle_led_prop_write_queue, &req, portMAX_DELAY);

        switch (req.type) {
        case handle_write_type::color_mode: {
            req.characteristic->send(req.serv_context->buf_color_mode, sizeof(req.serv_context->buf_color_mode));
        }   break;
        case handle_write_type::num_leds: {
            req.characteristic->send(req.serv_context->buf_num_leds, sizeof(req.serv_context->buf_num_leds));
        }   break;
        case handle_write_type::refresh_rate: {
            req.characteristic->send(req.serv_context->buf_refresh_rate, sizeof(req.serv_context->buf_refresh_rate));
        }   break;
        }

        if (req.save) {
            cfg::led_render_t config;
            ret = req.serv_context->config_param.get(&config);
            if (ret != NRF_SUCCESS) {
                NRF_LOG_WARNING("Reading config failed: %u", ret);
                continue;
            }

            switch (req.type) {
            case handle_write_type::color_mode: {
                config.color_mode = req.color_mode;
            }   break;
            case handle_write_type::num_leds: {
                config.n_leds = req.num_leds;
            }   break;
            case handle_write_type::refresh_rate: {
                config.refresh_msec = req.refresh_rate;
            }   break;
            }

            ret = req.serv_context->config_param.set(&config);
            if (ret != NRF_SUCCESS) {
                NRF_LOG_WARNING("Failed to update config: %u", ret);
                continue;
            }
        }
    }
}

static bool handle_led_prop_write_is_initialized()
{
    return m_handle_led_prop_write_thread != nullptr;
}

static ret_code_t init_handle_led_prop_write()
{
    m_handle_led_prop_write_queue = xQueueCreate(
        QUEUED_WRITES_PER_LED_CHAN * MAX_LED_CHANNELS,
        sizeof(handle_write_data)
    );

    if (m_handle_led_prop_write_queue == nullptr) {
        return NRF_ERROR_NO_MEM;
    }

    auto status = xTaskCreate(
        handle_led_prop_write,
        "LED-PROP-WRITE",
        128,
        nullptr,
        2,
        &m_handle_led_prop_write_thread
    );

    if (status != pdPASS) {
        return NRF_ERROR_NO_MEM;
    }

    return NRF_SUCCESS;
}

static uint16_t m_service_handles[MAX_LED_CHANNELS] = {
    BLE_GATT_HANDLE_INVALID,
#if MAX_LED_CHANNELS >= 2
    BLE_GATT_HANDLE_INVALID,
#endif
#if MAX_LED_CHANNELS >= 3
    BLE_GATT_HANDLE_INVALID,
#endif
#if MAX_LED_CHANNELS >= 4
    BLE_GATT_HANDLE_INVALID,
#endif
};

static service_context m_context[MAX_LED_CHANNELS] = {
    service_context(cfg::led0::render),
#if MAX_LED_CHANNELS >= 2
    service_context(cfg::led1::render),
#endif
#if MAX_LED_CHANNELS >= 3
    service_context(cfg::led2::render),
#endif
#if MAX_LED_CHANNELS >= 4
    service_context(cfg::led3::render),
#endif
};

#define CHN 0
#include "../src/ble/led_impl.cc"
#undef CHN

#if MAX_LED_CHANNELS >= 2
#define CHN 1
#include "../src/ble/led_impl.cc"
#undef CHN
#endif

#if MAX_LED_CHANNELS >= 3
#define CHN 2
#include "../src/ble/led_impl.cc"
#undef CHN
#endif

#if MAX_LED_CHANNELS >= 4
#define CHN 3
#include "../src/ble/led_impl.cc"
#undef CHN
#endif
