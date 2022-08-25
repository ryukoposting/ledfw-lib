#pragma once

#include "prelude.hh"
#include "led.hh"
#include "ble/service.hh"

#define LED_SERVICE_CLASS(id)\
    struct concat(service_,id): ble::service, led::renderer {\
        concat(service_,id) ();\
        ret_code_t init();\
        static constexpr size_t channel_id() { return id; };\
        CHARACTERISTIC(num_leds_char);\
        CHARACTERISTIC(color_mode_char);\
        CHARACTERISTIC(refresh_rate_char);\
        CHARACTERISTIC(control_char);\
        ret_code_t render(led::transcode *transcoder, BaseType_t &refresh_msec) override;\
        ret_code_t prepare_config() override;\
        ret_code_t check_config() override;\
        ret_code_t set_dmx(uint8_t const *vals, size_t length, TickType_t max_delay);\
        uint16_t service_handle() override;\
        void reset();\
        \
    protected:\
        uint16_t *service_handle_ptr() override;\
    }

namespace led {
    LED_SERVICE_CLASS(0);
#if MAX_LED_CHANNELS >= 2
    LED_SERVICE_CLASS(1);
#endif
#if MAX_LED_CHANNELS >= 3
    LED_SERVICE_CLASS(2);
#endif
#if MAX_LED_CHANNELS >= 4
    LED_SERVICE_CLASS(3);
#endif
}
