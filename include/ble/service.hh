#pragma once

#include "prelude.hh"
#include "ble.hh"
#include "nrf_sdh_ble.h"

#define SVC_BLE_OBSERVER_PRIO 4

#define CHARACTERISTIC(name_)\
    struct name_: ble::characteristic {\
        name_();\
        ble_gatts_char_handles_t *char_handles_ptr() override;\
    protected:\
        static ble_gatts_char_handles_t m_char_handles;\
    }

#define CHARACTERISTIC_DEF(ns_, name_, ...)\
    ble_gatts_char_handles_t ns_::name_::m_char_handles = { .value_handle = BLE_GATT_HANDLE_INVALID };\
    ble_gatts_char_handles_t *ns_::name_::char_handles_ptr() { return &m_char_handles; }\
    ns_::name_::name_(): ble::characteristic(__VA_ARGS__)

#define BLE_CONNECTED_OBSERVER(name_, handler_) NRF_SDH_BLE_OBSERVER(name_, SVC_BLE_OBSERVER_PRIO, [] (ble_evt_t const *ble_evt, void *p_context) {\
        unused(p_context);\
        if (!ble_evt) return;\
        if (ble_evt->header.evt_id == BLE_GAP_EVT_CONNECTED) { static_cast<ble::on_connect_t>(handler_)(ble_evt); }\
    }, NULL);

#define BLE_DISCONNECTED_OBSERVER(name_, handler_) NRF_SDH_BLE_OBSERVER(name_, SVC_BLE_OBSERVER_PRIO, [] (ble_evt_t const *ble_evt, void *p_context) {\
        unused(p_context);\
        if (!ble_evt) return;\
        if (ble_evt->header.evt_id == BLE_GAP_EVT_DISCONNECTED) { static_cast<ble::on_disconnect_t>(handler_)(ble_evt); }\
    }, NULL);

#define BLE_GATT_WRITE_OBSERVER(name_, characteristic_, handler_) NRF_SDH_BLE_OBSERVER(name_, SVC_BLE_OBSERVER_PRIO, [] (ble_evt_t const *ble_evt, void *p_context) {\
        unused(p_context);\
        if (!ble_evt) return;\
        if ((ble_evt->header.evt_id == BLE_GATTS_EVT_WRITE) && (characteristic_.matches(ble_evt->evt.gatts_evt.params.write))) {\
            static_cast<ble::on_gatt_write_t>(handler_)(ble_evt->evt.gatts_evt.params.write);\
        }\
    }, NULL);

namespace ble {
    using on_connect_t = void (*)(void);
    using on_disconnect_t = void (*)(void);
    using on_gatt_write_t = void (*)(ble_gatts_evt_write_t const &event);

    struct characteristic {
        characteristic() = delete;

        /**
         * @brief Construct a new characteristic object.
         * 
         * @param uuid The characteristic's UUID.
         * @param props read/write/notify flags.
         * @param buffer_sz The maximum number of bytes that can be read/written from this characteristic.
         */
        characteristic(char_uuid uuid, ble_gatt_char_props_t props, size_t buffer_sz);

        /**
         * @brief Construct a new characteristic object
         * 
         * @param uuid The characteristic's UUID.
         * @param props read/write/notify flags.
         * @param buffer A buffer that will be used to hold the characteristic's value. This must be statically allocated!
         * @param buffer_sz The maximum number of bytes that can be read/written from this characteristic.
         */
        characteristic(char_uuid uuid, ble_gatt_char_props_t props, void *buffer, size_t buffer_sz);

        /**
         * @brief Construct a new characteristic object
         * 
         * @param uuid The characteristic's UUID.
         * @param props read/write/notify flags.
         * @param buffer A buffer that will be used to hold the characteristic's value. This must be statically allocated!
         * @param init_sz The length of the value currently in the buffer.
         * @param buffer_sz The maximum number of bytes that can be read/written from this characteristic.
         */
        characteristic(char_uuid uuid, ble_gatt_char_props_t props, void *buffer, size_t init_sz, size_t buffer_sz);

        /**
         * @brief Construct a new characteristic object.
         * 
         * @param user_desc Constant string with a human-friendly description of the characteristic.
         * @param uuid The characteristic's UUID.
         * @param props read/write/notify flags.
         * @param buffer_sz The maximum number of bytes that can be read/written from this characteristic.
         */
        characteristic(char const *user_desc, char_uuid uuid, ble_gatt_char_props_t props, size_t buffer_sz);

        /**
         * @brief Construct a new characteristic object
         * 
         * @param user_desc Constant string with a human-friendly description of the characteristic.
         * @param uuid The characteristic's UUID.
         * @param props read/write/notify flags.
         * @param buffer A buffer that will be used to hold the characteristic's value. This must be statically allocated!
         * @param buffer_sz The maximum number of bytes that can be read/written from this characteristic.
         */
        characteristic(char const *user_desc, char_uuid uuid, ble_gatt_char_props_t props, void *buffer, size_t buffer_sz);

        /**
         * @brief Construct a new characteristic object
         * 
         * @param user_desc Constant string with a human-friendly description of the characteristic.
         * @param uuid The characteristic's UUID.
         * @param props read/write/notify flags.
         * @param buffer A buffer that will be used to hold the characteristic's value. This must be statically allocated!
         * @param init_sz The length of the value currently in the buffer.
         * @param buffer_sz The maximum number of bytes that can be read/written from this characteristic.
         */
        characteristic(char const *user_desc, char_uuid uuid, ble_gatt_char_props_t props, void *buffer, size_t init_sz, size_t buffer_sz);

        inline char_uuid uuid() const
        {
            return chr_uuid;
        }

        inline ble_gatt_char_props_t props() const
        {
            return chr_props;
        }

        inline void set_presentation_format(ble_gatts_char_pf_t const *char_pf)
        {
            char_pres = char_pf;
        }

        inline void set_gatts_attr_value(ble_gatts_attr_t &attr, ble_gatts_attr_md_t &attr_md, ble_gatts_char_md_t &char_md)
        {
            attr.p_value = buffer;
            attr.init_len = buffer_init_len;
            attr.init_offs = 0;
            attr.max_len = buffer ? buffer_max_len : ble::max_att_data_len();
            attr_md.vloc = buffer ? BLE_GATTS_VLOC_USER : BLE_GATTS_VLOC_STACK;
            attr_md.vlen = 1;
            char_md.p_char_user_desc = (uint8_t*)user_desc;
            char_md.char_user_desc_size = user_desc ? strlen(user_desc) : 0;
            char_md.char_user_desc_max_size = char_md.char_user_desc_size;
            char_md.p_char_pf = char_pres;
        }

        inline bool is_initialized()
        {
            auto handles = char_handles_ptr();
            return handles && handles->value_handle != BLE_GATT_HANDLE_INVALID;
        }

        virtual ble_gatts_char_handles_t *char_handles_ptr() = 0;

        /**
         * @brief Check to see if a GATTS event is associated with this characteristic.
         */
        bool matches(ble_gatts_evt_write_t const&);

        /**
         * @brief Send a notification.
         * 
         * @param data notification data.
         * @param n_sent number of bytes sent in the notification.
         */
        ret_code_t send(notification &data, size_t *n_sent);

        /**
         * @brief Send a notification.
         * 
         * @param data notification data.
         */
        inline ret_code_t send(notification &data)
        {
            return send(data, nullptr);
        }

        inline ret_code_t send(void const *data, size_t length)
        {
            auto n = notification {
                .conn_handle = ble::conn_handle(),
                .data = data,
                .length = length,
                .offset = 0
            };
            return send(n);
        }

        /**
         * @brief Set the characteristic's value.
         */
        ret_code_t set_value(void const *data, size_t length);

    private:
        char_uuid chr_uuid;
        ble_gatt_char_props_t chr_props;
        uint8_t *buffer;
        size_t buffer_max_len;
        size_t buffer_init_len;
        ble_gatts_char_pf_t const *char_pres;
        char const *user_desc;
    };

    struct service {
        service() = delete;
        
        service(service_uuid uuid);

        inline bool is_initialized()
        {
            return service_handle() != BLE_GATT_HANDLE_INVALID;
        }

        inline service_uuid uuid() const
        {
            return svc_uuid;
        }

        virtual uint16_t service_handle() = 0;

    protected:
        virtual uint16_t *service_handle_ptr() = 0;
        ret_code_t init_service();
        ret_code_t add_characteristic(characteristic &cha);

    private:
        service_uuid svc_uuid;
    };
}
