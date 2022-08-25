#ifdef VSCODE /* make vscode autocomplete shut up */
#include "prelude.hh"
#include "ble/led.hh"
#include "ble/meta.hh"
#include "userapp.hh"
#include "util.hh"
#include "led.hh"
#include "cfg.hh"
#include "fds.h"
#define CHN 0
struct service_context {
    uint8_t buf_num_leds[2];
    uint8_t buf_color_mode[1];
    uint8_t buf_refresh_rate[2];
    uint8_t buf_control[64];
    bool reset_strip;
    bool config_changed;
    size_t seqwrite_offset;
    size_t dmx_data_len;
    cfg::led_render_t config;
    uint8_t dmx_data[MAX_SUBSCRIBED_DMX_CHANNELS];
    uint8_t user_buffer[MAX_LEDS_PER_THREAD * 3];

    void set_refresh_msec(uint16_t value);
    void reset_refresh_msec();
    void set_color_mode(uint8_t value);
    void reset_color_mode();
    void set_num_leds(uint16_t value);
    void reset_num_leds();
};
extern uint16_t m_service_handles[MAX_LED_CHANNELS] = {};
extern service_context m_context[MAX_LED_CHANNELS] = {};
extern xSemaphoreHandle m_dmx_lock[MAX_LED_CHANNELS] = {};
using namespace led;
#endif /* VSCODE */

#define SHIFT(p) *p; ++p

#define svc concat(service_,CHN)
#define cfgx cfg::concat(led,CHN)
#define m_num_leds concat(m_num_leds_,CHN)
#define m_color_mode concat(m_color_mode_,CHN)
#define m_refresh_rate concat(m_refresh_rate_,CHN)
#define m_control concat(m_control_,CHN)
#define context (m_context[CHN])
#define m_dmx_mutex (m_dmx_lock[CHN])
#define SCHN stringify(CHN)

static svc::num_leds_char m_num_leds;
static svc::color_mode_char m_color_mode;
static svc::refresh_rate_char m_refresh_rate;
static svc::control_char m_control;

#define on_num_leds_write concat(on_num_leds_write_,CHN)
static void on_num_leds_write(ble_gatts_evt_write_t const &event);
BLE_GATT_WRITE_OBSERVER(concat(m_num_leds,_write), m_num_leds, on_num_leds_write);

#define on_color_mode_write concat(on_color_mode_write_,CHN)
static void on_color_mode_write(ble_gatts_evt_write_t const &event);
BLE_GATT_WRITE_OBSERVER(concat(m_color_mode,_write), m_color_mode, on_color_mode_write);

#define on_refresh_rate_write concat(on_refresh_rate_write_,CHN)
static void on_refresh_rate_write(ble_gatts_evt_write_t const &event);
BLE_GATT_WRITE_OBSERVER(concat(m_refresh_rate,_write), m_refresh_rate, on_refresh_rate_write);

#define on_control_write concat(on_control_write_,CHN)
static void on_control_write(ble_gatts_evt_write_t const &event);
BLE_GATT_WRITE_OBSERVER(concat(m_control,_write), m_control, on_control_write);

#define set_led_chan concat(set_led_chan_,CHN)
static void set_led_chan(led_chan &chan);

// #define init_leds_cb concat(init_leds_cb_,CHN)
// static ret_code_t init_leds_cb(userapp::init_func_t init, userapp::refresh_func_t refresh, void *ctxt);

// #define refresh_leds_cb concat(refresh_leds_cb_,CHN)
// static ret_code_t refresh_leds_cb(userapp::init_func_t init, userapp::refresh_func_t refresh, void *ctxt);

CHARACTERISTIC_DEF(led::svc, num_leds_char,
    stringify(CHN) ": Number of LEDs",
    ble::char_uuid::concat3(led,CHN,_length),
    ble_gatt_char_props_t { .read = true, .write = true, .notify = true },
    context.buf_num_leds,
    sizeof(context.buf_num_leds),
    sizeof(context.buf_num_leds))
{}

CHARACTERISTIC_DEF(led::svc, color_mode_char,
    stringify(CHN) ": Color Mode",
    ble::char_uuid::concat3(led,CHN,_color_mode),
    ble_gatt_char_props_t { .read = true, .write = true, .notify = true },
    context.buf_color_mode,
    sizeof(context.buf_color_mode),
    sizeof(context.buf_color_mode))
{}

CHARACTERISTIC_DEF(led::svc, refresh_rate_char,
    stringify(CHN) ": Refresh Rate",
    ble::char_uuid::concat3(led,CHN,_refresh_rate),
    ble_gatt_char_props_t { .read = true, .write = true, .notify = true },
    context.buf_refresh_rate,
    sizeof(context.buf_refresh_rate),
    sizeof(context.buf_refresh_rate))
{}

CHARACTERISTIC_DEF(led::svc, control_char,
    stringify(CHN) ": Control",
    ble::char_uuid::concat3(led,CHN,_control),
    ble_gatt_char_props_t { .write = true },
    64)
{}

svc::svc(): ble::service(ble::service_uuid::led0), led::renderer()
{}

ret_code_t svc::init()
{
    ret_code_t ret = NRF_SUCCESS;
    
    if (m_dmx_mutex == nullptr) {
        m_dmx_mutex = xSemaphoreCreateMutex();
        if (m_dmx_mutex == nullptr)
            return NRF_ERROR_NO_MEM;
    }

    auto pf = ble_gatts_char_pf_t {
        .name_space = BLE_GATT_CPF_NAMESPACE_BTSIG,
        .desc = BLE_GATT_CPF_NAMESPACE_DESCRIPTION_UNKNOWN,
    };

    if (!is_initialized()) {
        ret = init_service();
        VERIFY_SUCCESS(ret);

        m_num_leds = svc::num_leds_char();
        pf.format = BLE_GATT_CPF_FORMAT_UINT16;
        m_num_leds.set_presentation_format(&pf);
        ret = add_characteristic(m_num_leds);
        VERIFY_SUCCESS(ret);

        m_color_mode = svc::color_mode_char();
        pf.format = BLE_GATT_CPF_FORMAT_UINT8;
        m_color_mode.set_presentation_format(&pf);
        ret = add_characteristic(m_color_mode);
        VERIFY_SUCCESS(ret);

        m_refresh_rate = svc::refresh_rate_char();
        pf.format = BLE_GATT_CPF_FORMAT_UINT16;
        m_refresh_rate.set_presentation_format(&pf);
        ret = add_characteristic(m_refresh_rate);
        VERIFY_SUCCESS(ret);

        m_control = svc::control_char();
        ret = add_characteristic(m_control);
        VERIFY_SUCCESS(ret);
    }

    return ret;
}

uint16_t svc::service_handle()
{
    return m_service_handles[CHN];
}

uint16_t *svc::service_handle_ptr()
{
    return &m_service_handles[CHN];
}

ret_code_t svc::prepare_config()
{
    ret_code_t ret;

    ret = cfgx::render::get(&context.config);
    if (ret == FDS_ERR_NOT_FOUND) {
        context.config = cfg::led_render_t {
            .n_leds = MAX_LEDS_PER_THREAD,
            .refresh_msec = DEFAULT_REFRESH_RATE_MSEC,
            .color_mode = (uint8_t)led::color_mode::rgb
        };
        ret = cfgx::render::set(&context.config);
    }

    return ret;
}

ret_code_t svc::check_config()
{
    ret_code_t ret = NRF_SUCCESS;

    if (context.config_changed)
        ret = cfgx::render::set(&context.config);

    return ret;
}

ret_code_t svc::render(led::transcode *transcoder, BaseType_t &refresh_msec)
{
    ret_code_t ret;
    
    ret = transcoder->write_bus_reset();
    VERIFY_SUCCESS(ret);

    refresh_msec = context.config.refresh_msec;
    bool do_fill_zeros = false;

    if (context.reset_strip) {
        context.reset_strip = false;
        do_fill_zeros = true;
        auto op = [] (userapp::init_func_t init, userapp::refresh_func_t refresh, void *ctxt) -> ret_code_t {
            unused(ctxt);
            unused(refresh);
            led_chan chan;
            set_led_chan(chan);
            init(&chan);
            if (chan.color_mode != (uint8_t)context.config.color_mode) {
                NRF_LOG_DEBUG("App set color mode to %u", chan.color_mode);
                context.set_color_mode(chan.color_mode);
            }
            return NRF_SUCCESS;
        };
        ret = userapp::with(nullptr, op);
        if (ret == ERROR_USERCODE_NOT_AVAILABLE)
            ret = NRF_SUCCESS; // ignore 'user code unavailable' error
        VERIFY_SUCCESS(ret);

    } else if (context.config.n_leds > 0) {
        auto op = [] (userapp::init_func_t init, userapp::refresh_func_t refresh, void *ctxt) -> ret_code_t {
            unused(ctxt);
            unused(init);
            led_chan chan;
            set_led_chan(chan);
            refresh(&chan);
            if (chan.color_mode != (uint8_t)context.config.color_mode) {
                NRF_LOG_DEBUG("App set color mode to %u", chan.color_mode);
                context.set_color_mode(chan.color_mode);
            }
            return NRF_SUCCESS;
        };
        ret = userapp::with(nullptr, op);
        if (ret == ERROR_USERCODE_NOT_AVAILABLE)
            ret = NRF_SUCCESS; // ignore 'user code unavailable' error
        VERIFY_SUCCESS(ret);
    }

    uint8_t *ptr = context.user_buffer;

    for (size_t i = 0; i < context.config.n_leds && i < MAX_LEDS_PER_THREAD; ++i) {
        color::rgb wval;

        switch ((color_mode)context.config.color_mode) {
        case color_mode::rgb:
            wval.red = SHIFT(ptr);
            wval.green = SHIFT(ptr);
            wval.blue = SHIFT(ptr);
            break;
        case color_mode::hsv: {
            color::hsv hsv;
            hsv.hue = SHIFT(ptr);
            hsv.saturation = SHIFT(ptr);
            hsv.value = SHIFT(ptr);
            hsv.to_rgb(wval);
        }   break;
        case color_mode::hsl: {
            color::hsl hsl;
            hsl.hue = SHIFT(ptr);
            hsl.saturation = SHIFT(ptr);
            hsl.luminance = SHIFT(ptr);
            hsl.to_rgb(wval);
        }   break;
        }

        ret = transcoder->write(wval);
        VERIFY_SUCCESS(ret);
    }

    if (do_fill_zeros) {
        auto off = color::rgb(color::BLACK);
        for (size_t i = context.config.n_leds; i < MAX_LEDS_PER_THREAD; ++i) {
            ret = transcoder->write(off);
            VERIFY_SUCCESS(ret);
        }
    }

    ret = transcoder->write_bus_reset();
    VERIFY_SUCCESS(ret);

    return NRF_SUCCESS;
}

ret_code_t svc::set_dmx(uint8_t const *vals, size_t length, TickType_t max_delay)
{
    if (length > 0 && !vals)
        return NRF_ERROR_NULL;

    auto taken = xSemaphoreTake(m_dmx_mutex, max_delay);
    if (!taken)
        return NRF_ERROR_BUSY;

    context.dmx_data_len = length;
    if (vals)
        memcpy(context.dmx_data, vals, length);

    xSemaphoreGive(m_dmx_mutex);
    return NRF_SUCCESS;
}

void svc::reset()
{
    context.reset_strip = true;
}

static void on_num_leds_write(ble_gatts_evt_write_t const &event)
{
    auto notif = ble::notification {
        .conn_handle = ble::conn_handle(),
        .data = context.buf_num_leds,
        .length = sizeof(context.buf_num_leds),
        .offset = 0
    };

    if (event.len != 2 || !event.data) {
        meta::service()
            .print("Malformed write to Channel " SCHN " 'Number of LEDs' characteristic (invalid length)");
        context.reset_num_leds();
        m_num_leds.send(notif);
        return;
    }

    auto num_leds = uint16_decode(event.data);

    if (num_leds > MAX_LEDS_PER_THREAD) {
        meta::service()
            .print("Invalid number of LEDs (maximum: " stringify(MAX_LEDS_PER_THREAD) ")");
        context.reset_num_leds();
        m_num_leds.send(notif);
    } else {
        context.reset_strip = true;
        context.set_num_leds(num_leds);
    }
}

static void on_color_mode_write(ble_gatts_evt_write_t const &event)
{
    auto notif = ble::notification {
        .conn_handle = ble::conn_handle(),
        .data = context.buf_color_mode,
        .length = sizeof(context.buf_color_mode),
        .offset = 0
    };

    if (event.len != 1 || !event.data) {
        meta::service()
            .print("Malformed write to Channel " SCHN " 'Color Mode' characteristic (invalid length)");
        context.reset_color_mode();
        m_color_mode.send(notif);
    } else if (event.data[0] > 2) {
        meta::service()
            .print("Invalid color mode (must be 0, 1, or 2)");
        context.reset_color_mode();
        m_color_mode.send(notif);
    } else {
        context.reset_strip = true;
        context.set_color_mode(event.data[0]);
    }
}

static void on_refresh_rate_write(ble_gatts_evt_write_t const &event)
{
    auto notif = ble::notification {
        .conn_handle = ble::conn_handle(),
        .data = context.buf_refresh_rate,
        .length = sizeof(context.buf_refresh_rate),
        .offset = 0
    };

    if (event.len != 2 || !event.data) {
        meta::service()
            .print("Malformed write to Channel " SCHN " 'Refresh Rate' characteristic (invalid length)");
        context.reset_refresh_msec();
        m_refresh_rate.send(notif);
        return;
    }

    auto refresh_msec = uint16_decode(event.data);

    if (refresh_msec < MINIMUM_REFRESH_RATE_MSEC) {
        meta::service()
            .print("Invalid refresh interval (minimum: " stringify(MINIMUM_REFRESH_RATE_MSEC) ")");
        context.reset_refresh_msec();
        m_refresh_rate.send(notif);
    } else if (refresh_msec > MAXIMUM_REFRESH_RATE_MSEC) {
        meta::service()
            .print("Invalid refresh interval (maximum: " stringify(MAXIMUM_REFRESH_RATE_MSEC) ")");
        context.reset_refresh_msec();
        m_refresh_rate.send(notif);
    } else {
        context.set_refresh_msec(refresh_msec);
    }
}

/*
    00                          -> turn off whole strip
    01 xxxx yyyy aa bb cc       -> set yyyy leds starting from xxxx to (aa, bb, cc)

    10 xxxx                     -> set offset for "sequential write" cmd
    11 aa bb cc ...             -> sequential write. set LEDS starting from current "sequential write" ptr
*/
static void on_control_write(ble_gatts_evt_write_t const &event)
{
    if (event.len == 1 && event.data && event.data[0] == 0) {
        memset(context.user_buffer, 0, sizeof(context.user_buffer));
        context.reset_strip = true;

    } else if (event.len == 8 && event.data && event.data[0] == 1) {
        auto offset = 3 * (size_t)uint16_decode(&event.data[1]);
        auto length = uint16_decode(&event.data[3]);

        for (; (offset+2 < sizeof(context.user_buffer)) && (length > 0); --length) {
            context.user_buffer[offset++] = event.data[5];
            context.user_buffer[offset++] = event.data[6];
            context.user_buffer[offset++] = event.data[7];
        }

    } else if (event.len == 3 && event.data && event.data[0] == 0x10) {
        context.seqwrite_offset = uint16_decode(&event.data[1]);

    } else if (event.len > 1 && event.data && event.data[0] == 0x11) {
        size_t i = 1;
        for (; (i + 2 < event.len) && (context.seqwrite_offset < MAX_LEDS_PER_THREAD); ++context.seqwrite_offset) {
            auto pos = 3 * context.seqwrite_offset;
            context.user_buffer[pos] = event.data[i++];
            context.user_buffer[pos+1] = event.data[i++];
            context.user_buffer[pos+2] = event.data[i++];
        }
    }
}

static void set_led_chan(led_chan &chan)
{
    constexpr auto read_wait_msec = MINIMUM_REFRESH_RATE_MSEC - 4;
    static_assert(read_wait_msec > 0);

    static uint8_t dmx_data[MAX_SUBSCRIBED_DMX_CHANNELS] = {};
    static size_t dmx_data_len = 0;

    chan.id = CHN;
    chan.buffer = context.user_buffer;
    chan.color_mode = (uint8_t)context.config.color_mode;
    chan.refresh_rate = context.config.refresh_msec;
    chan.n_leds = context.config.n_leds;

    auto taken = xSemaphoreTake(m_dmx_mutex, pdMS_TO_TICKS(read_wait_msec));
    if (taken == pdTRUE) {
        memcpy(dmx_data, context.dmx_data, std::min(sizeof(dmx_data), sizeof(context.dmx_data)));
        dmx_data_len = context.dmx_data_len;
        xSemaphoreGive(m_dmx_mutex);
    }

    chan.dmx_vals = dmx_data;
    chan.dmx_vals_len = dmx_data_len;
}
