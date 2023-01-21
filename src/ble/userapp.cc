#define NRF_LOG_MODULE_NAME uapp
#include "prelude.hh"
#include "userapp.hh"
#include "ble/service.hh"
#include "ble/userapp.hh"
#include "util.hh"

#define APP_INFO_VERSION 1
#define DMX_INFO_VERSION 1
#define DMX_STRING_MAX 32u

using namespace userapp;

enum app_info_ind: size_t {
    I_INFO_VER = 0,
    I_INFO_APP_STATE = 1,
    I_INFO_STORAGE_STATE = 2,
    I_INFO_ARCH_CPU = 3,
    I_INFO_ARCH_FLAG_MASK = 4,
    I_INFO_ARCH_FLAG_REQ = 6,
    APP_INFO_LEN = 8
};

enum dmx_info_ind: size_t {
    I_DMX_VER = 0,
    I_DMX_N_PERSONALITIES = 1,
    I_DMX_CUR_PERSONALITY = 2,
    I_DMX_N_SLOTS = 3,
    DMX_INFO_LEN = 4
};

enum dmx_pers_data: size_t {
    I_PERS_MTYPE = 0,
    I_PERS_N = 1,
    I_PERS_N_SLOTS = 2,
    I_PERS_NAME = 3,
};

enum dmx_slot_data: size_t {
    I_SLOT_MTYPE = 0,
    I_SLOT_PERS_N = 1,
    I_SLOT_N = 2,
    I_SLOT_TYPE = 3,
    I_SLOT_ID = 4,
    I_SLOT_VALUE = 6,
    I_SLOT_NAME = 7
};

static userapp::service::program_char m_program;
static userapp::service::info_char m_info;
static userapp::service::app_name_char m_app_name;
static userapp::service::app_provider_char m_app_provider;
static userapp::service::dmx_info_char m_dmx_info;
static userapp::service::dmx_explorer_char m_dmx_explorer;

static uint16_t m_service_handle = BLE_GATT_HANDLE_INVALID;

static void on_program_write(ble_gatts_evt_write_t const &event);
BLE_GATT_WRITE_OBSERVER(m_program_write, m_program, on_program_write);

static void on_dmx_explorer_write(ble_gatts_evt_write_t const &event);
BLE_GATT_WRITE_OBSERVER(m_dmx_explorer_write, m_dmx_explorer, on_dmx_explorer_write);

CHARACTERISTIC_DEF(userapp::service, program_char,
    "Program",
    ble::char_uuid::ucode_program,
    ble_gatt_char_props_t { .write = true },
    WRITE_TEMPBUF_MAX_LEN + 1)
{}

CHARACTERISTIC_DEF(userapp::service, info_char,
    "App Info",
    ble::char_uuid::ucode_info,
    ble_gatt_char_props_t { .read = true, .notify = true },
    APP_INFO_LEN)
{}

CHARACTERISTIC_DEF(userapp::service, app_name_char,
    "App Name",
    ble::char_uuid::ucode_app_name,
    ble_gatt_char_props_t { .read = true, .notify = true },
    MAX_USER_APP_NAME_LEN)
{}

CHARACTERISTIC_DEF(userapp::service, app_provider_char,
    "App Provider",
    ble::char_uuid::ucode_app_provider,
    ble_gatt_char_props_t { .read = true, .notify = true },
    MAX_USER_APP_PROVIDER_LEN)
{}

CHARACTERISTIC_DEF(userapp::service, dmx_info_char,
    "DMX Info",
    ble::char_uuid::ucode_dmx_info,
    ble_gatt_char_props_t { .read = true, .notify = true },
    DMX_INFO_LEN)
{}

CHARACTERISTIC_DEF(userapp::service, dmx_explorer_char,
    "DMX Explorer",
    ble::char_uuid::ucode_dmx_explore,
    ble_gatt_char_props_t { .write = true, .notify = true},
    DMX_EXPLORER_LEN)
{}

userapp::service::service(): ble::service(ble::service_uuid::ucode)
{}

ret_code_t userapp::service::init()
{
    ret_code_t ret = NRF_SUCCESS;

    auto pf = ble_gatts_char_pf_t {
        .name_space = BLE_GATT_CPF_NAMESPACE_BTSIG,
        .desc = BLE_GATT_CPF_NAMESPACE_DESCRIPTION_UNKNOWN
    };

    if (!is_initialized()) {
        ret = init_service();
        VERIFY_SUCCESS(ret);

        m_program = service::program_char();
        ret = add_characteristic(m_program);
        VERIFY_SUCCESS(ret);

        m_info = service::info_char();
        ret = add_characteristic(m_info);
        VERIFY_SUCCESS(ret);

        m_app_name = service::app_name_char();
        pf.format = BLE_GATT_CPF_FORMAT_UTF8S;
        m_app_name.set_presentation_format(&pf);
        ret = add_characteristic(m_app_name);
        VERIFY_SUCCESS(ret);

        m_app_provider = service::app_provider_char();
        pf.format = BLE_GATT_CPF_FORMAT_UTF8S;
        m_app_provider.set_presentation_format(&pf);
        ret = add_characteristic(m_app_provider);
        VERIFY_SUCCESS(ret);

        m_dmx_info = service::dmx_info_char();
        ret = add_characteristic(m_dmx_info);
        VERIFY_SUCCESS(ret);

        m_dmx_explorer = service::dmx_explorer_char();
        ret = add_characteristic(m_dmx_explorer);
        VERIFY_SUCCESS(ret);
    }

    return ret;
}

ret_code_t userapp::service::update_app_info(desc const &desc, cfg::dmx_config_t const &config, app_state appst, storage_state storst)
{
    ret_code_t ret;
    desc_tbl tbl;
    uint8_t dmx_info[DMX_INFO_LEN] = {};

    ret = update_app_info(appst, storst);
    VERIFY_SUCCESS(ret);

    ret = desc.full_desc(tbl);
    VERIFY_SUCCESS(ret);

    ret = set_app_name(tbl.app_name);
    VERIFY_SUCCESS(ret);

    ret = set_provider_name(tbl.provider_name);
    VERIFY_SUCCESS(ret);

    size_t n_pers = desc.n_dmx_pers();
    size_t n_slots = 0;
    if (n_pers > config.personality) {
        auto pers = dmx_pers(desc.dmx_pers_tbl()[config.personality]);
        n_slots = pers.n_dmx_slots();
    }

    dmx_info[I_DMX_VER] = DMX_INFO_VERSION;
    dmx_info[I_DMX_N_PERSONALITIES] = desc.n_dmx_pers();
    dmx_info[I_DMX_CUR_PERSONALITY] = config.personality;
    dmx_info[I_DMX_N_SLOTS] = n_slots;

    ret = m_dmx_info.set_value(dmx_info, DMX_INFO_LEN);
    VERIFY_SUCCESS(ret);

    ret = m_dmx_info.send(dmx_info, DMX_INFO_LEN);
    VERIFY_SUCCESS(ret);

    return ret;
}

ret_code_t userapp::service::update_app_info(app_state appst, storage_state storst)
{
    ret_code_t ret;
    uint8_t value[APP_INFO_LEN] = {};

    value[I_INFO_VER] = APP_INFO_VERSION;
    value[I_INFO_APP_STATE] = (uint8_t)appst;
    value[I_INFO_STORAGE_STATE] = (uint8_t)storst;
    value[I_INFO_ARCH_CPU] = USERCODE_ARCH_CPU;
    uint16_encode(USERCODE_ARCH_FLAGS_MASK, &value[I_INFO_ARCH_FLAG_MASK]);
    uint16_encode(USERCODE_ARCH_FLAGS_VALUE, &value[I_INFO_ARCH_FLAG_REQ]);

    ret = m_info.set_value(value, APP_INFO_LEN);
    VERIFY_SUCCESS(ret);

    ret = m_info.send(value, APP_INFO_LEN);
    VERIFY_SUCCESS(ret);

    return ret;
}

ret_code_t userapp::service::set_app_name(char const *name)
{
    ret_code_t ret;

    void const *data = nullptr;
    size_t len = 0;

    if (name) {
        data = name;
        len = strnlen(name, MAX_USER_APP_NAME_LEN);
    }

    ret = m_app_name.set_value(data, len);
    VERIFY_SUCCESS(ret);

    ret = m_app_name.send(data, len);
    VERIFY_SUCCESS(ret);

    return ret;
}

ret_code_t userapp::service::set_provider_name(char const *name)
{
    ret_code_t ret;

    void const *data = nullptr;
    size_t len = 0;

    if (name) {
        data = name;
        len = strnlen(name, MAX_USER_APP_PROVIDER_LEN);
    }

    ret = m_app_provider.set_value(data, len);
    VERIFY_SUCCESS(ret);

    ret = m_app_provider.send(data, len);
    VERIFY_SUCCESS(ret);

    return ret;
}

ret_code_t userapp::service::send_personality_info(uint8_t index, uint8_t n_slots, dmx_pers const &pers)
{
    uint8_t buf[DMX_EXPLORER_LEN];
    buf[I_PERS_MTYPE] = (uint8_t)dmx_explorer_resp::personality_info;
    buf[I_PERS_N] = index;
    buf[I_PERS_N_SLOTS] = n_slots;

    size_t name_len = 0;
    if (pers.name()) {
        name_len = strnlen(pers.name(), std::min(DMX_EXPLORER_LEN - I_PERS_NAME, DMX_STRING_MAX));
        memcpy(&buf[I_PERS_NAME], pers.name(), name_len);
    }

    return m_dmx_explorer.send(buf, name_len + I_PERS_NAME);
}

ret_code_t userapp::service::send_slot_info(uint8_t pers, uint8_t index, dmx_slot const &slot)
{
    uint8_t buf[DMX_EXPLORER_LEN];
    buf[I_SLOT_MTYPE] = (uint8_t)dmx_explorer_resp::slot_info;
    buf[I_SLOT_PERS_N] = pers;
    buf[I_SLOT_N] = index;
    buf[I_SLOT_TYPE] = slot.type();
    uint16_encode(slot.id(), &buf[I_SLOT_ID]);
    buf[I_SLOT_VALUE] = slot.value();

    size_t name_len = 0;
    if (slot.name()) {
        name_len = strnlen(slot.name(), std::min(DMX_EXPLORER_LEN - I_SLOT_NAME, DMX_STRING_MAX));
        memcpy(&buf[I_SLOT_NAME], slot.name(), name_len);
    }

    return m_dmx_explorer.send(buf, name_len + I_SLOT_NAME);
}

uint16_t userapp::service::service_handle()
{
    return m_service_handle;
}

uint16_t *userapp::service::service_handle_ptr()
{
    return &m_service_handle;
}

ret_code_t userapp::send_state()
{
    ret_code_t ret;
    auto appst = get_app_state();
    auto storst = get_storage_state();
    auto d = desc();

    auto dmx_config_param = cfg::dmx::config;
    cfg::dmx_config_t config;
    ret = dmx_config_param.get(&config);
    VERIFY_SUCCESS(ret);

    ret = service().update_app_info(d, config, appst, storst);
    VERIFY_SUCCESS(ret);

    return ret;
}

ret_code_t userapp::send_partial_state()
{
    ret_code_t ret;
    auto appst = get_app_state();
    auto storst = get_storage_state();

    ret = service().update_app_info(appst, storst);
    VERIFY_SUCCESS(ret);

    return ret;
}

/*
    00                  -> update status
    01                  -> erase flash
    02                  -> begin write
    03 (aaaa) xx xx ..  -> write (aaaa = offset) ...
    04 xxxx             -> commit (crc)
    05 xxxx             -> run without commit (crc)
 */
static void on_program_write(ble_gatts_evt_write_t const &event)
{
    ret_code_t ret;
    if (event.len == 1 && event.data && event.data[0] == 0) {
        ret = queue_send_state();
        if (ret != NRF_SUCCESS) {
            NRF_LOG_ERROR("queue_send_state: %u", ret);
        }

    } else if (event.len == 1 && event.data && event.data[0] == 1) {
        ret = queue_erase();
        if (ret != NRF_SUCCESS) {
            NRF_LOG_ERROR("queue_erase: %u", ret);
        }

    } else if (event.len == 1 && event.data && event.data[0] == 2) {
        ret = queue_begin_write();
        if (ret != NRF_SUCCESS) {
            NRF_LOG_ERROR("queue_begin_write: %u", ret);
        }

    } else if (event.len > 3 && event.data && event.data[0] == 3) {
        auto offset = uint16_decode(&event.data[1]);
        ret = queue_write(offset, &event.data[3], event.len - 3);
        if (ret != NRF_SUCCESS) {
            NRF_LOG_ERROR("queue_write: %u", ret);
        }

    } else if (event.len == 3 && event.data && event.data[0] == 4) {
        auto crc = uint16_decode(&event.data[1]);
        ret = queue_commit(crc);
        if (ret != NRF_SUCCESS) {
            NRF_LOG_ERROR("queue_commit: %u", ret);
        }

    } else if (event.len == 3 && event.data && event.data[0] == 5) {
        auto crc = uint16_decode(&event.data[1]);
        ret = queue_run(crc);
        if (ret != NRF_SUCCESS) {
            NRF_LOG_ERROR("queue_run: %u", ret);
        }
    }
}

/*
    01 xx           get info for personality xx
    02 xx yy        get info for slot yy of personality xx
    03 xx           get info for personality xx and all its slots
*/
static void on_dmx_explorer_write(ble_gatts_evt_write_t const &event)
{
    if (!event.data) return;

    if (event.len == 2 && event.data[0] == (uint8_t)dmx_explorer_cmd::get_personality_info) {
        queue_dmx_explore(dmx_explorer_cmd::get_personality_info, event.data[1], 0);
    } else if (event.len == 3 && event.data[0] == (uint8_t)dmx_explorer_cmd::get_slot_info) {
        queue_dmx_explore(dmx_explorer_cmd::get_slot_info, event.data[1], event.data[2]);
    } else if (event.len == 2 && event.data[0] == (uint8_t)dmx_explorer_cmd::get_personality_and_slot_info) {
        queue_dmx_explore(dmx_explorer_cmd::get_personality_and_slot_info, event.data[1], 0);
    }
}
