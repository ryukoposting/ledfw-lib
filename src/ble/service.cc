#include "prelude.hh"
#include "ble/service.hh"

ble::service::service(service_uuid uuid): svc_uuid(uuid)
{}

ret_code_t ble::service::init_service()
{
    auto ble_uuid = uuid_for_service(this->uuid());

    return sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, service_handle_ptr());
}

ret_code_t ble::service::add_characteristic(characteristic &cha)
{
    if (!is_initialized()) {
        return NRF_ERROR_INVALID_STATE;
    }

    auto ble_uuid = uuid_for_char(cha.uuid());

    ble_gatts_char_md_t char_md = {};
    ble_gatts_attr_t attr = {};
    ble_gatts_attr_md_t attr_md = {};

    cha.set_gatts_attr_value(attr, attr_md, char_md);
    attr.p_uuid = &ble_uuid;
    attr.p_attr_md = &attr_md;

    char_md.char_props = cha.props();

    if (char_md.char_props.write) {
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
    }

    if (char_md.char_props.read) {
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    }

    return sd_ble_gatts_characteristic_add(
        service_handle(),
        &char_md,
        &attr,
        cha.char_handles_ptr()
    );
}

ble::characteristic::characteristic(char_uuid uuid, ble_gatt_char_props_t props, size_t buffer_sz):
    chr_uuid(uuid),
    chr_props(props),
    buffer(nullptr),
    buffer_max_len(buffer_sz),
    buffer_init_len(0),
    char_pres(nullptr),
    user_desc(nullptr)
{}

ble::characteristic::characteristic(char_uuid uuid, ble_gatt_char_props_t props, void *buffer, size_t buffer_sz):
    chr_uuid(uuid),
    chr_props(props),
    buffer((uint8_t*)buffer),
    buffer_max_len(buffer_sz),
    buffer_init_len(0),
    char_pres(nullptr),
    user_desc(nullptr)
{}

ble::characteristic::characteristic(char_uuid uuid, ble_gatt_char_props_t props, void *buffer, size_t init_sz, size_t buffer_sz):
    chr_uuid(uuid),
    chr_props(props),
    buffer((uint8_t*)buffer),
    buffer_max_len(buffer_sz),
    buffer_init_len(init_sz),
    char_pres(nullptr),
    user_desc(nullptr)
{}

ble::characteristic::characteristic(char const *user_desc, char_uuid uuid, ble_gatt_char_props_t props, size_t buffer_sz):
    chr_uuid(uuid),
    chr_props(props),
    buffer(nullptr),
    buffer_max_len(buffer_sz),
    buffer_init_len(0),
    char_pres(nullptr),
    user_desc(user_desc)
{}

ble::characteristic::characteristic(char const *user_desc, char_uuid uuid, ble_gatt_char_props_t props, void *buffer, size_t buffer_sz):
    chr_uuid(uuid),
    chr_props(props),
    buffer((uint8_t*)buffer),
    buffer_max_len(buffer_sz),
    buffer_init_len(0),
    char_pres(nullptr),
    user_desc(user_desc)
{}

ble::characteristic::characteristic(char const *user_desc, char_uuid uuid, ble_gatt_char_props_t props, void *buffer, size_t init_sz, size_t buffer_sz):
    chr_uuid(uuid),
    chr_props(props),
    buffer((uint8_t*)buffer),
    buffer_max_len(buffer_sz),
    buffer_init_len(init_sz),
    char_pres(nullptr),
    user_desc(user_desc)
{}

bool ble::characteristic::matches(ble_gatts_evt_write_t const &event)
{
    auto handles = char_handles_ptr();
    return handles && handles->value_handle != BLE_GATT_HANDLE_INVALID && handles->value_handle == event.handle;
}

ret_code_t ble::characteristic::send(notification &data, size_t *n_sent)
{
    if (data.length > UINT16_MAX || data.offset > UINT16_MAX) {
        return NRF_ERROR_INVALID_LENGTH;
    }

    auto handles = char_handles_ptr();
    if (!handles || handles->value_handle == BLE_GATT_HANDLE_INVALID) {
        return NRF_ERROR_INVALID_STATE;
    }

    uint16_t length = data.length;
    auto hvx = ble_gatts_hvx_params_t {
        .handle = handles->value_handle,
        .type = BLE_GATT_HVX_NOTIFICATION,
        .offset = (uint16_t)data.offset,
        .p_len = &length,
        .p_data = (uint8_t const*)data.data,
    };

    ret_code_t ret = sd_ble_gatts_hvx(data.conn_handle, &hvx);

    if (n_sent) {
        *n_sent = (ret == NRF_SUCCESS) ? *hvx.p_len : 0;
    }

    if (ret == NRF_ERROR_INVALID_STATE) {
        ret = NRF_SUCCESS; // ignore error where notification is not currently enabled
    }

    return ret;
}

ret_code_t ble::characteristic::set_value(void const *data, size_t length)
{
    if (length > buffer_max_len || length > UINT16_MAX) {
        return NRF_ERROR_INVALID_LENGTH;
    }

    auto handles = char_handles_ptr();
    if (!handles || handles->value_handle == BLE_GATT_HANDLE_INVALID) {
        return NRF_ERROR_INVALID_STATE;
    }

    auto value = ble_gatts_value_t {
        .len = (uint16_t)length,
        .offset = 0,
    };

    if (buffer) {
        if (data) {
            memcpy(buffer, data, length);
        }
    } else {
        value.p_value = (uint8_t*)data;
    }

    return sd_ble_gatts_value_set(BLE_CONN_HANDLE_INVALID, handles->value_handle, &value);
}
