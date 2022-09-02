#define NRF_LOG_MODULE_NAME uapp
#include "prelude.hh"
#include "userapp.hh"
#include "task.hh"
#include "cfg.hh"
#include "ble/meta.hh"
#include "ble/userapp.hh"

#define CHECK_RET_MSG(ret, err_msg)\
    if ((ret) != NRF_SUCCESS) {\
        NRF_LOG_WARNING(err_msg " (%d)", ret);\
        meta::service().print(err_msg);\
        continue;\
    }

#define ACTN_ASSERT(expr, err_msg)\
    if (!(expr)) {\
        NRF_LOG_WARNING(err_msg " (" stringify(expr) ")");\
        meta::service().print(err_msg);\
        continue;\
    }

#define ACTN_FAILED(err_msg)\
    NRF_LOG_WARNING(err_msg);\
    meta::service().print(err_msg);\
    continue;

using namespace userapp;

static TaskHandle_t m_userapp_thread;
static QueueHandle_t m_action_queue;

static void userapp_thread(void *arg)
{
    unused(arg);

    ret_code_t ret;

    auto channel_config_param = cfg::dmx::config;

    cfg::dmx_config_t config;
    ret = channel_config_param.get(&config);
    if (ret == FDS_ERR_NOT_FOUND) {
        config.channel = 0;
        config.n_channels = 0;
        config.personality = 0;
        ret = channel_config_param.set(&config);
    }
    APP_ERROR_CHECK(ret);

    NRF_LOG_DEBUG("Loading application");
    ret = load_from_flash();
    if (ret != NRF_SUCCESS) {
        NRF_LOG_DEBUG("Loading application from flash: 0x%x", ret);
        ret = load_from_tempbuf();
    }
    APP_ERROR_CHECK(ret);

    /* begin all LED render threads */
    led::resume_all();

    send_state();

    while (1) {
        queued_action actn = {};
        auto received = xQueueReceive(m_action_queue, &actn, portMAX_DELAY);
        if (received == pdFALSE) {
            unreachable();
        }

        ret = NRF_SUCCESS;

        switch (actn.type) {
        case action::other:
            ACTN_ASSERT(actn.other.func != nullptr, "action::OTHER did not define func");
            ret = actn.other.func(actn.other.context);
            break;

        case action::erase:
            ret = erase();
            CHECK_RET_MSG(ret, "Erase failed");
            break;

        case action::begin_write:
            ret = clear_tempbuf();
            CHECK_RET_MSG(ret, "Failed to begin write");
            ret = ble::set_conn_rate(ble::conn_rate::high_speed);
            CHECK_RET_MSG(ret, "Failed to change conn rate");
            break;

        case action::write:
            ret = write_tempbuf(actn.write.offset, actn.write.buffer, actn.write.length);
            vPortFree(actn.write.buffer);
            CHECK_RET_MSG(ret, "Write failed");
            NRF_LOG_DEBUG("Write %u bytes to 0x%04x", actn.write.length, actn.write.offset);
            break;

        case action::commit: {
            uint16_t crc = crc_tempbuf();
            NRF_LOG_DEBUG("got crc: 0x%04x actual crc: 0x%04x", actn.commit.crc, crc);
            ACTN_ASSERT(crc == actn.commit.crc, "Failed while receiving application (CRC)");

            ret = load_from_tempbuf();
            CHECK_RET_MSG(ret, "Failed to load application");

            ret = save_tempbuf_to_flash(crc);
            CHECK_RET_MSG(ret, "Failed to save application");

            task::schedule_fds_gc();

            // ret = ble::set_conn_rate(ble::conn_rate::normal);
            // CHECK_RET_MSG(ret, "Failed to change conn rate");
        }   break;

        case action::run: {
            uint16_t crc = crc_tempbuf();
            NRF_LOG_DEBUG("got crc: 0x%04x actual crc: 0x%04x", actn.run.crc, crc);
            ACTN_ASSERT(crc == actn.run.crc, "Failed while receiving application (CRC)");

            ret = load_from_tempbuf();
            CHECK_RET_MSG(ret, "Failed to load application");

            // ret = ble::set_conn_rate(ble::conn_rate::normal);
            // CHECK_RET_MSG(ret, "Failed to change conn rate");
        }   break;

        case action::dmx_config: {
            ret = channel_config_param.set(&actn.dmx_config.config);
            CHECK_RET_MSG(ret, "DMX config failed");
        }   break;

        case action::send_state:
            send_state();
            break;

        case action::dmx_explorer: {
            if (get_app_state() != app_state::user_app_loaded || ble::conn_handle() == BLE_CONN_HANDLE_INVALID)
                continue;

            auto d = desc();

            ACTN_ASSERT(actn.dmx_explorer.personality < d.n_dmx_pers(), "Personality does not exist");
            auto pers = dmx_pers(d.dmx_pers_tbl()[actn.dmx_explorer.personality]);

            /* send personality info */
            if (actn.dmx_explorer.command == dmx_explorer_cmd::get_personality_info ||
                actn.dmx_explorer.command == dmx_explorer_cmd::get_personality_and_slot_info)
            {
                service().send_personality_info(
                    actn.dmx_explorer.personality,
                    pers.n_dmx_slots(),
                    pers);
            }

            /* send slot info */
            if (actn.dmx_explorer.command == dmx_explorer_cmd::get_slot_info) {
                ACTN_ASSERT(actn.dmx_explorer.slot < pers.n_dmx_slots(), "Slot does not exist");
                auto slot = dmx_slot(pers.dmx_slots_tbl()[actn.dmx_explorer.slot]);

                service().send_slot_info(
                    actn.dmx_explorer.personality,
                    actn.dmx_explorer.slot,
                    slot);
            }

            /* send all slot info */
            if (actn.dmx_explorer.command == dmx_explorer_cmd::get_personality_and_slot_info) {
                auto const n_slots = pers.n_dmx_slots();
                for (size_t i = 0; i < n_slots; ++i) {
                    auto slot = dmx_slot(pers.dmx_slots_tbl()[i]);

                    service().send_slot_info(
                        actn.dmx_explorer.personality,
                        i,
                        slot);
                }
            }
        }
        }
    }
}

ret_code_t userapp::init_thread()
{
    auto status = xTaskCreate(userapp_thread, "USERAPP", 256, NULL, 3, &m_userapp_thread);
    if (pdPASS != status) {
        return NRF_ERROR_NO_MEM;
    }

    m_action_queue = xQueueCreate(24, sizeof(userapp::queued_action));
    if (m_action_queue == nullptr) {
        return NRF_ERROR_NO_MEM;
    }

    return NRF_SUCCESS;
}

TaskHandle_t userapp::thread()
{
    return m_userapp_thread;
}

ret_code_t userapp::queue_action(queued_action const *actn, BaseType_t *do_yield)
{
    BaseType_t success;

    if (task::is_in_isr()) {
        success = xQueueSendFromISR(m_action_queue, actn, do_yield);
    } else {
        success = xQueueSend(m_action_queue, actn, portMAX_DELAY);
    }

    if (success == pdTRUE) {
        return NRF_SUCCESS;
    } else {
        return NRF_ERROR_NO_MEM;
    }
}
