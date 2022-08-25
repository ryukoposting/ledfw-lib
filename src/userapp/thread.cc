#define NRF_LOG_MODULE_NAME uapp
#include "prelude.hh"
#include "userapp.hh"
#include "task.hh"
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

            task::schedule_gc();
        }   break;

        case action::run: {
            uint16_t crc = crc_tempbuf();
            NRF_LOG_DEBUG("got crc: 0x%04x actual crc: 0x%04x", actn.run.crc, crc);
            ACTN_ASSERT(crc == actn.run.crc, "Failed while receiving application (CRC)");

            ret = load_from_tempbuf();
            CHECK_RET_MSG(ret, "Failed to load application");
        }   break;

        case action::send_state:
            send_state();
            break;
        }
    }
}

ret_code_t userapp::init_thread()
{
    auto status = xTaskCreate(userapp_thread, "USERAPP", 256, NULL, 3, &m_userapp_thread);
    if (pdPASS != status) {
        return NRF_ERROR_NO_MEM;
    }

    m_action_queue = xQueueCreate(16, sizeof(userapp::queued_action));
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
