#define NRF_LOG_MODULE_NAME dmx
#include "prelude.hh"
#include "ble/service.hh"
#include "ble/dmx.hh"
#include "dmx.hh"
#include "cfg.hh"
#include "userapp.hh"
#include "task/lock.hh"

using namespace dmx;

#define CONFIG_CHAR_LEN (sizeof(cfg::dmx_config_t) + 1)

struct char_write {
    enum { CONFIG, VALUE } kind;
    size_t n_values;
    union {
        cfg::dmx_config_t config;
        uint8_t value[MAX_USER_APP_SLOTS];
    };
};

static uint16_t m_service_handle = BLE_GATT_HANDLE_INVALID;

static dmx::service::values_char m_values;
static dmx::service::config_char m_config;

static void on_values_write(ble_gatts_evt_write_t const &event);
BLE_GATT_WRITE_OBSERVER(m_values_write, m_values, on_values_write);

static void on_config_write(ble_gatts_evt_write_t const &event);
BLE_GATT_WRITE_OBSERVER(m_config_write, m_config, on_config_write);

static void dmx_char_write_handler(void *arg);
static TaskHandle_t m_dmx_char_write_task = nullptr;
static xQueueHandle m_dmx_char_write_queue = nullptr;

static dmx::thread *m_dmx_thread = nullptr;

CHARACTERISTIC_DEF(dmx::service, values_char,
    "DMX Values",
    ble::char_uuid::dmx_values,
    ble_gatt_char_props_t { .read = true, .write = true, .notify = true },
    MAX_USER_APP_SLOTS)
{}

CHARACTERISTIC_DEF(dmx::service, config_char,
    "DMX Config",
    ble::char_uuid::dmx_config,
    ble_gatt_char_props_t { .read = true, .write = true, .notify = true },
    CONFIG_CHAR_LEN)
{}

dmx::service::service(): ble::service(ble::service_uuid::dmx)
{}

ret_code_t dmx::service::init(dmx::thread *thread)
{
    ret_code_t ret = NRF_SUCCESS;

    if (!is_initialized()) {
        ret = init_service();
        VERIFY_SUCCESS(ret);

        m_values = service::values_char();
        ret = add_characteristic(m_values);
        VERIFY_SUCCESS(ret);

        m_config = service::config_char();
        ret = add_characteristic(m_config);
        VERIFY_SUCCESS(ret);

        ret = thread->on_dmx_slot_vals(nullptr, [](void *_, dmx_slot_vals const *vals, cfg::dmx_config_t const *config) {
            if (config) {
                uint8_t config_buf[CONFIG_CHAR_LEN] = {1, };
                memcpy(&config_buf[1], config, sizeof(cfg::dmx_config_t));

                m_config.set_value(config_buf, CONFIG_CHAR_LEN);
                m_config.send(config_buf, CONFIG_CHAR_LEN);
            }

            if (vals) {
                m_values.set_value(vals->vals, vals->n_vals);
                m_values.send(vals->vals, vals->n_vals);
            }
        });
        VERIFY_SUCCESS(ret);

        auto status = xTaskCreate(
            dmx_char_write_handler,
            "DMX-BLE",
            256,
            nullptr,
            2,
            &m_dmx_char_write_task
        );
        if (status != pdPASS)
            return NRF_ERROR_NO_MEM;

        m_dmx_char_write_queue = xQueueCreate(8, sizeof(char_write));
        if (!m_dmx_char_write_queue) {
            return NRF_ERROR_NO_MEM;
        }

        m_dmx_thread = thread;
    }

    return ret;
}

uint16_t dmx::service::service_handle()
{
    return m_service_handle;
}

uint16_t *dmx::service::service_handle_ptr()
{
    return &m_service_handle;
}

static void on_values_write(ble_gatts_evt_write_t const &event)
{
    char_write data { char_write::VALUE };
    if (event.data && event.len > 0 && event.len <= MAX_USER_APP_SLOTS) {
        memcpy(data.value, event.data, event.len);
        data.n_values = event.len;
        xQueueSend(m_dmx_char_write_queue, &data, pdMS_TO_TICKS(2));
    }
}

static void on_config_write(ble_gatts_evt_write_t const &event)
{
    char_write data { char_write::CONFIG };
    if (event.data && event.len == CONFIG_CHAR_LEN && event.data[0] == 1) {
        memcpy(&data.config, &event.data[1], sizeof(cfg::dmx_config_t));
        xQueueSend(m_dmx_char_write_queue, &data, pdMS_TO_TICKS(2));
    }
}

static void dmx_char_write_handler(void *arg)
{
    unused(arg);

    while (!m_dmx_thread) vTaskDelay(10);

    auto config = cfg::dmx::config;

    config.subscribe(nullptr, [](void *context, void const *data, size_t length) {
        unused(context);
        unused(data);
        unused(length);
        userapp::queue_send_state();
    });

    while (1) {
        char_write evt = {};

        xQueueReceive(m_dmx_char_write_queue, &evt, portMAX_DELAY);

        if (evt.kind == char_write::VALUE) {
            auto vals = dmx_slot_vals { evt.n_values, evt.value };
            m_dmx_thread->send(&vals);
        }

        if (evt.kind == char_write::CONFIG) {
            config.set(&evt.config);
            // m_dmx_thread->send(&evt.config);
        }
    }
}
