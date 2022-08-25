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

#if MAX_LED_CHANNELS > 4
#error MAX_LED_CHANNELS cannot be more than 4
#endif

#if MAX_LED_CHANNELS < 1
#error MAX_LED_CHANNELS cannot be less than 1
#endif

using namespace led;

struct service_context {
    constexpr service_context():
        buf_num_leds { MAX_LEDS_PER_THREAD % (UINT8_MAX+1), MAX_LEDS_PER_THREAD / (UINT8_MAX+1) },
        buf_color_mode { (uint8_t)led::color_mode::rgb },
        buf_refresh_rate { DEFAULT_REFRESH_RATE_MSEC % (UINT8_MAX+1), DEFAULT_REFRESH_RATE_MSEC / (UINT8_MAX+1) },
        reset_strip(true),
        config_changed(false),
        seqwrite_offset(0),
        dmx_data_len(0),
        config {},
        dmx_data {},
        user_buffer {}
    {}

    uint8_t buf_num_leds[2];
    uint8_t buf_color_mode[1];
    uint8_t buf_refresh_rate[2];
    bool reset_strip;
    bool config_changed;
    size_t seqwrite_offset;
    size_t dmx_data_len;
    cfg::led_render_t config;
    uint8_t dmx_data[MAX_SUBSCRIBED_DMX_CHANNELS];
    uint8_t user_buffer[MAX_LEDS_PER_THREAD * 3];

    inline void set_refresh_msec(uint16_t value)
    {
        config_changed = value != config.refresh_msec || config_changed;
        config.refresh_msec = value;
        uint16_encode(value, buf_refresh_rate);
    }

    inline void reset_refresh_msec()
    {
        set_refresh_msec(config.refresh_msec);
    }

    inline void set_color_mode(uint8_t value)
    {
        config_changed = value != config.color_mode || config_changed;
        config.color_mode = value;
        buf_color_mode[0] = value;
    }

    inline void reset_color_mode()
    {
        set_color_mode(config.color_mode);
    }

    inline void set_num_leds(uint16_t value)
    {
        config_changed = value != config.n_leds || config_changed;
        config.n_leds = value;
        uint16_encode(value, buf_num_leds);
    }

    inline void reset_num_leds()
    {
        set_num_leds(config.n_leds);
    }
};

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
    service_context(),
#if MAX_LED_CHANNELS >= 2
    service_context(),
#endif
#if MAX_LED_CHANNELS >= 3
    service_context(),
#endif
#if MAX_LED_CHANNELS >= 4
    service_context(),
#endif
};

static xSemaphoreHandle m_dmx_lock[MAX_LED_CHANNELS] = {};

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
