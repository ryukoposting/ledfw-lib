#pragma once

#include "prelude.hh"
#include "ble_advertising.h"

namespace ble {
    enum class disconnect_reason: uint8_t {
        remote_user_terminated_connection = BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION,
        local_host_terminated_connection = BLE_HCI_LOCAL_HOST_TERMINATED_CONNECTION,
    };

    enum class service_uuid: uint16_t {
        meta = 0x3000,
        ucode = 0x3100,
        led0 = 0x4000,
#if MAX_LED_CHANNELS >= 2
        led1 = 0x4100,
#endif
#if MAX_LED_CHANNELS >= 3
        led2 = 0x4200,
#endif
#if MAX_LED_CHANNELS >= 4
        led3 = 0x4300,
#endif
        dmx = 0x5000,
    };

    enum class char_uuid: uint16_t {
        meta_device_uid    = 0x01 + (uint16_t)service_uuid::meta,
        meta_sys_control   = 0x02 + (uint16_t)service_uuid::meta,
        meta_stat_msg      = 0x40 + (uint16_t)service_uuid::meta,

        ucode_program      = 0x01 + (uint16_t)service_uuid::ucode,
        ucode_info         = 0x02 + (uint16_t)service_uuid::ucode,
        ucode_app_name     = 0x03 + (uint16_t)service_uuid::ucode,
        ucode_app_provider = 0x04 + (uint16_t)service_uuid::ucode,
        ucode_dmx_info     = 0x10 + (uint16_t)service_uuid::ucode,
        ucode_dmx_explore  = 0x11 + (uint16_t)service_uuid::ucode,

        led0_length        = 0x01 + (uint16_t)service_uuid::led0, // read/write led strip length
        led0_color_mode    = 0x02 + (uint16_t)service_uuid::led0, // hsv/hsl/rgb
        led0_refresh_rate  = 0x03 + (uint16_t)service_uuid::led0, // read/write refresh rate
        led0_control       = 0x80 + (uint16_t)service_uuid::led0, // reset, simple programming functionality
#if MAX_LED_CHANNELS >= 2
        led1_length        = 0x01 + (uint16_t)service_uuid::led1, // read/write led strip length
        led1_color_mode    = 0x02 + (uint16_t)service_uuid::led1, // hsv/hsl/rgb
        led1_refresh_rate  = 0x03 + (uint16_t)service_uuid::led1, // read/write refresh rate
        led1_control       = 0x80 + (uint16_t)service_uuid::led1, // reset, simple programming functionality
#endif
#if MAX_LED_CHANNELS >= 3
        led2_length        = 0x01 + (uint16_t)service_uuid::led2, // read/write led strip length
        led2_color_mode    = 0x02 + (uint16_t)service_uuid::led2, // hsv/hsl/rgb
        led2_refresh_rate  = 0x03 + (uint16_t)service_uuid::led2, // read/write refresh rate
        led2_control       = 0x80 + (uint16_t)service_uuid::led2, // reset, simple programming functionality
#endif
#if MAX_LED_CHANNELS >= 4
        led3_length        = 0x01 + (uint16_t)service_uuid::led3, // read/write led strip length
        led3_color_mode    = 0x02 + (uint16_t)service_uuid::led3, // hsv/hsl/rgb
        led3_refresh_rate  = 0x03 + (uint16_t)service_uuid::led3, // read/write refresh rate
        led3_control       = 0x80 + (uint16_t)service_uuid::led3, // reset, simple programming functionality
#endif

        dmx_values         = 0x01 + (uint16_t)service_uuid::dmx,
        dmx_config         = 0x10 + (uint16_t)service_uuid::dmx
    };

    enum class conn_rate {
        normal,
        high_speed
    };

    struct notification {
        uint16_t conn_handle;
        void const *data;
        size_t length;
        size_t offset;
    };

    ret_code_t init();

    ret_code_t init_thread(bool erase_bonds);

    void erase_bonds();

    uint16_t conn_handle();

    void get_advertising_config(ble_adv_modes_config_t &config);

    void set_advertising_config(ble_adv_modes_config_t &config);

    ret_code_t set_advertising_data(void const *data, size_t len);

    void disconnect(uint16_t conn_handle, ble::disconnect_reason reason);

    uint8_t uuid_type();

    size_t max_att_data_len();

    ret_code_t set_conn_rate(conn_rate rate);

    static inline ble_uuid_t uuid_for_service(service_uuid uuid)
    {
        ble_uuid_t result = {
            .uuid = static_cast<uint16_t>(uuid),
            .type = uuid_type()
        };

        return result;
    }

    static inline ble_uuid_t uuid_for_char(char_uuid uuid)
    {
        ble_uuid_t result = {
            .uuid = static_cast<uint16_t>(uuid),
            .type = uuid_type()
        };

        return result;
    }
}
