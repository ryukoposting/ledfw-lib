#define NRF_LOG_MODULE_NAME meta
#include "prelude.hh"
#include "ble/meta.hh"
#include "meta.hh"
#include "nrf_log_ctrl.h"
#include "task.h"
#include "util.hh"

// #define M_DEVICE_UID_SIZE sizeof(meta::device_uid())
#define M_DEVICE_UID_SIZE 6 // conforms with RDM device UID size
#define MAX_MESSAGE_LEN 120

static meta::service::device_uid_char m_device_uid;
static meta::service::system_control_char m_sys_control;
static meta::service::log_char m_log;

static uint16_t m_service_handle = BLE_GATT_HANDLE_INVALID;
static uint8_t m_device_uid_buf[M_DEVICE_UID_SIZE] = {};

static void on_sys_control_write(ble_gatts_evt_write_t const &event);
BLE_GATT_WRITE_OBSERVER(m_sys_control_write, m_sys_control, on_sys_control_write);

CHARACTERISTIC_DEF(meta::service, device_uid_char,
    "Device UID",
    ble::char_uuid::meta_device_uid,
    ble_gatt_char_props_t { .read = true },
    m_device_uid_buf,
    M_DEVICE_UID_SIZE,
    M_DEVICE_UID_SIZE)
{
    auto uid = device_rdm_uid();
    memcpy(m_device_uid_buf, uid.bytes, M_DEVICE_UID_SIZE);
}

CHARACTERISTIC_DEF(meta::service, system_control_char,
    "System Control",
    ble::char_uuid::meta_sys_control,
    ble_gatt_char_props_t { .write = true },
    1)
{}

CHARACTERISTIC_DEF(meta::service, log_char,
    "Status Message",
    ble::char_uuid::meta_stat_msg,
    ble_gatt_char_props_t { .notify = true },
    MAX_MESSAGE_LEN)
{}

meta::service::service(): ble::service(ble::service_uuid::meta)
{}

ret_code_t meta::service::init()
{
    ret_code_t ret = NRF_SUCCESS;

    if (!is_initialized()) {
        ret = init_service();
        VERIFY_SUCCESS(ret);

        m_device_uid = meta::service::device_uid_char();
        ret = add_characteristic(m_device_uid);
        VERIFY_SUCCESS(ret);

        m_sys_control = meta::service::system_control_char();
        ret = add_characteristic(m_sys_control);
        VERIFY_SUCCESS(ret);

        m_log = meta::service::log_char();
        auto pf = ble_gatts_char_pf_t {
            .format = BLE_GATT_CPF_FORMAT_UTF8S,
            .name_space = BLE_GATT_CPF_NAMESPACE_BTSIG,
            .desc = BLE_GATT_CPF_NAMESPACE_DESCRIPTION_UNKNOWN,
        };
        m_log.set_presentation_format(&pf);
        ret = add_characteristic(m_log);
        VERIFY_SUCCESS(ret);
    }

    return ret;
}

uint16_t meta::service::service_handle()
{
    return m_service_handle;
}

uint16_t *meta::service::service_handle_ptr()
{
    return &m_service_handle;
}

ret_code_t meta::service::print(char const *msg)
{
    if (!msg) {
        return NRF_ERROR_NULL;
    }

    size_t len = strnlen(msg, MAX_MESSAGE_LEN);

    auto data = ble::notification {
        .conn_handle = ble::conn_handle(),
        .data = msg,
        .length = len,
        .offset = 0
    };

    return m_log.send(data);
}

/*
    01                                  -> reboot device
    02                                  -> (debug only) dump task data
    03                                  -> (debug only) dump heap data
*/
void on_sys_control_write(ble_gatts_evt_write_t const &event)
{
    if (event.len == 1 && event.data[0] == 0x01) {
        ret_code_t ret;

        vTaskSuspendAll();

        NRF_LOG_WARNING("User requested reset. Goodbye!");
        NRF_LOG_FLUSH();

        ble::disconnect(ble::conn_handle(), ble::disconnect_reason::local_host_terminated_connection);

        ret = sd_nvic_SystemReset();
        APP_ERROR_CHECK(ret);

#ifdef DEBUG
    } else if (event.len == 1 && event.data[0] == 0x02) {
        static TaskStatus_t task_status_list[16] = {};

        auto ntasks = uxTaskGetSystemState(task_status_list, 16, nullptr);
        NRF_LOG_INFO("ntasks = %u", ntasks);
        for (size_t i = 0; i < ntasks; ++i) {
            TaskStatus_t *p = &task_status_list[i];
            NRF_LOG_INFO("%s cur_prio=%d state=%d stack=%u", p->pcTaskName, p->uxCurrentPriority, p->eCurrentState, p->usStackHighWaterMark);
        }
    } else if (event.len == 1 && event.data[0] == 0x03) {
        auto free_amt = xPortGetFreeHeapSize();
        auto free_min = xPortGetMinimumEverFreeHeapSize();
        NRF_LOG_INFO("total heap size = %u", configTOTAL_HEAP_SIZE);
        NRF_LOG_INFO("currently free = %u", free_amt);
        NRF_LOG_INFO("minimum ever free = %u", free_min);
#endif
    }
}
