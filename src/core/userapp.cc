#define NRF_LOG_MODULE_NAME uapp
#include "prelude.hh"
#include "userapp.hh"
#include "task.hh"
#include "fds.h"
#include "fds_internal_defs.h"
#include "crc16.h"
#include "ble/userapp.hh"
NRF_LOG_MODULE_REGISTER();

#define MAX_RECORD_WORDS (FDS_VIRTUAL_PAGE_SIZE - 8u)
#define USERCODE_SIZE_WORDS (USERCODE_SIZE / sizeof(uint32_t))
#define FDS_FILE_ID 0x4110
#define FDS_RECORD_ID_CODE_BASE 0x9800
#define FDS_RECORD_ID_CODE_CRC  0xa000
#define LED_VECTBL_SIZE (4 * sizeof(uint32_t))

#define NOTIFY_UPDATE_DONE 0x1234
#define NOTIFY_WRITE_DONE 0x4567

using namespace userapp;

static void fds_callback(fds_evt_t const *event);
static bool m_fds_initialized = false;
static task::rw_lock m_lock;
static userapp::app_state m_app_state = app_state::uninitialized;
static userapp::storage_state m_storage_state = storage_state::uninitialized;

// m_temp_buf allows a user app to be loaded into RAM without overwriting any
// code that's currently running.
static char const m_default_app_name[] = "Default";
static char const m_default_provider_name[] = MANUFACTURER_NAME;
static uint32_t m_temp_buf[USERCODE_SIZE / sizeof(uint32_t)] align(sizeof(uint32_t)) = {
    USERCODE_MAGIC,
    0x00010000,
    (uint32_t)userapp::default_init,
    (uint32_t)userapp::default_refresh,
    (uint32_t)m_default_app_name,
    (uint32_t)m_default_provider_name,
    (uint32_t)nullptr,
    ((uint32_t)USERCODE_ARCH_CPU) | (((uint32_t)USERCODE_DEFAULT_APP_FLAGS) << 16)
};

using namespace userapp;

ret_code_t userapp::init()
{
    memset(USERCODE_BUFFER, 0, USERCODE_SIZE);

    ret_code_t ret;

    ret = fds_register(fds_callback);
    VERIFY_SUCCESS(ret);

    ret = fds_init();
    VERIFY_SUCCESS(ret);
    while (!m_fds_initialized) {}

    ret = m_lock.init();
    VERIFY_SUCCESS(ret);

    auto desc = fds_record_desc_t {};
    auto token = fds_find_token_t {};
    ret = fds_record_find_in_file(FDS_FILE_ID, &desc, &token);
    if (ret == FDS_ERR_NOT_FOUND) {
        ret = NRF_SUCCESS;
        m_storage_state = storage_state::empty;
    } else if (ret == NRF_SUCCESS) {
        m_storage_state = storage_state::user_app_stored;
    }
    VERIFY_SUCCESS(ret);

    return ret;
}

ret_code_t userapp::load_from_flash()
{
    auto result = m_lock.write([] () -> ret_code_t {
        // unused(context);
        if (m_storage_state != storage_state::user_app_stored) {
            return NRF_ERROR_INVALID_STATE;
        }
        auto old_app_state = m_app_state;
        m_app_state = app_state::loading_user_app;
        if (service().is_initialized())
            send_partial_state();

        memset(USERCODE_BUFFER, 0, sizeof(USERCODE_BUFFER));

        ret_code_t ret;
        auto desc = fds_record_desc_t {};
        uint16_t record_id = FDS_RECORD_ID_CODE_BASE;
        size_t read_offset = 0;
        while (1) {
            auto token = fds_find_token_t {};
            ret = fds_record_find(FDS_FILE_ID, record_id, &desc, &token);
            if (ret == FDS_ERR_NOT_FOUND) break;
            VERIFY_SUCCESS2(ret, {
                m_app_state = old_app_state;
            });

            auto record = fds_flash_record_t {};
            ret = fds_record_open(&desc, &record);
            VERIFY_SUCCESS2(ret, {
                m_app_state = old_app_state;
            });

            memcpy(&USERCODE_BUFFER[read_offset], record.p_data, record.p_header->length_words * sizeof(uint32_t));

            read_offset += record.p_header->length_words;
            record_id += 1;

            ret = fds_record_close(&desc);
            VERIFY_SUCCESS2(ret, {
                m_app_state = old_app_state;
            });
        }

        if (ret != FDS_ERR_NOT_FOUND) {
            assert(ret != NRF_SUCCESS);
            m_app_state = old_app_state;
            return ret;
        }

        /* retrieve and verify CRC */
        auto token = fds_find_token_t {};
        ret = fds_record_find(FDS_FILE_ID, FDS_RECORD_ID_CODE_CRC, &desc, &token);
        if (ret == FDS_ERR_NOT_FOUND) {
            m_app_state = old_app_state;
            return ERROR_USERCODE_MISSING_CRC;
        }

        auto record = fds_flash_record_t {};
        ret = fds_record_open(&desc, &record);
        VERIFY_SUCCESS2(ret, {
            m_app_state = old_app_state;
        });

        volatile auto expect_crc = *(uint32_t*)record.p_data;

        ret = fds_record_close(&desc);
        VERIFY_SUCCESS2(ret, {
            m_app_state = old_app_state;
        });

        auto actual_crc = crc16_compute((uint8_t const*)USERCODE_BUFFER, USERCODE_SIZE, nullptr);

        if (expect_crc != actual_crc) {
            m_app_state = old_app_state;
            return ERROR_USERCODE_INVALID_CRC;
        }

        m_app_state = app_state::user_app_loaded;
        led::reset_all();
        return ret;
    });

    userapp::queue_send_state();

    return result;
}

ret_code_t userapp::save_tempbuf_to_flash(uint16_t crc)
{
    auto result = m_lock.read<uint16_t>(crc, [] (uint16_t &crc) -> ret_code_t {
        static auto wrecord = fds_record_t {};
        static uint32_t crc_buf;

        crc_buf = crc;

        auto old_state = m_storage_state;
        m_storage_state = storage_state::storing_user_app;
        if (service().is_initialized())
            send_partial_state();

        ret_code_t ret;
        auto desc = fds_record_desc_t {};
        uint16_t record_id = FDS_RECORD_ID_CODE_BASE;
        size_t write_offset = 0;

        /* Update records that already exist in flash */
        while (write_offset < USERCODE_SIZE_WORDS) {
            auto token = fds_find_token_t {};
            ret = fds_record_find(FDS_FILE_ID, record_id, &desc, &token);
            if (ret == FDS_ERR_NOT_FOUND) break;
            VERIFY_SUCCESS2(ret, {
                m_storage_state = old_state;
            });

            auto record = fds_flash_record_t {};
            ret = fds_record_open(&desc, &record);
            VERIFY_SUCCESS2(ret, {
                m_storage_state = old_state;
            });

            auto length_words = record.p_header->length_words;

            ret = fds_record_close(&desc);
            VERIFY_SUCCESS2(ret, {
                m_storage_state = old_state;
            });

            wrecord.file_id = FDS_FILE_ID;
            wrecord.key = record_id;
            wrecord.data.p_data = &m_temp_buf[write_offset];
            wrecord.data.length_words = length_words;

            ret = fds_record_update(&desc, &wrecord);
            VERIFY_SUCCESS2(ret, {
                m_storage_state = old_state;
            });

            uint32_t notify_val = 0;
            xTaskNotifyWait(0, UINT32_MAX, &notify_val, portMAX_DELAY);
            assert(notify_val == NOTIFY_UPDATE_DONE);

            write_offset += length_words;
            record_id += 1;
        }

        /* Create new records to store the remainder of the program */
        while (write_offset < USERCODE_SIZE_WORDS) {
            auto length_words = std::min(MAX_RECORD_WORDS, USERCODE_SIZE_WORDS - write_offset);
            wrecord.file_id = FDS_FILE_ID;
            wrecord.key = record_id;
            wrecord.data.p_data = &m_temp_buf[write_offset];
            wrecord.data.length_words = length_words;

            ret = fds_record_write(&desc, &wrecord);
            VERIFY_SUCCESS2(ret, {
                m_storage_state = old_state;
            });

            uint32_t notify_val = 0;
            xTaskNotifyWait(0, UINT32_MAX, &notify_val, portMAX_DELAY);
            assert(notify_val == NOTIFY_WRITE_DONE);

            write_offset += length_words;
            record_id += 1;
        }

        wrecord.file_id = FDS_FILE_ID;
        wrecord.key = FDS_RECORD_ID_CODE_CRC;
        wrecord.data.p_data = &crc_buf;
        wrecord.data.length_words = 1;

        auto token = fds_find_token_t {};
        if (FDS_ERR_NOT_FOUND == fds_record_find(FDS_FILE_ID, FDS_RECORD_ID_CODE_CRC, &desc, &token)) {
            ret = fds_record_write(&desc, &wrecord);
            VERIFY_SUCCESS2(ret, {
                m_storage_state = old_state;
            });

            uint32_t notify_val = 0;
            xTaskNotifyWait(0, UINT32_MAX, &notify_val, portMAX_DELAY);
            assert(notify_val == NOTIFY_WRITE_DONE);
        } else {
            ret = fds_record_update(&desc, &wrecord);
                VERIFY_SUCCESS2(ret, {
                m_storage_state = old_state;
            });

            uint32_t notify_val = 0;
            xTaskNotifyWait(0, UINT32_MAX, &notify_val, portMAX_DELAY);
            assert(notify_val == NOTIFY_UPDATE_DONE);
        }


        m_storage_state = storage_state::user_app_stored;
        return NRF_SUCCESS;
    });

    userapp::queue_send_state();

    return result;
}

ret_code_t userapp::load_from_tempbuf()
{
    auto result = m_lock.write([] () -> ret_code_t {
        m_app_state = app_state::loading_user_app;
        if (service().is_initialized())
            send_partial_state();
        memcpy(USERCODE_BUFFER, m_temp_buf, USERCODE_SIZE);
        m_app_state = app_state::user_app_loaded;
        led::reset_all();
        return NRF_SUCCESS;
    });

    userapp::queue_send_state();

    return result;
}

ret_code_t userapp::erase()
{
    return fds_file_delete(FDS_FILE_ID);
}

ret_code_t userapp::with(void *context, with_cb_t func)
{
    return m_lock.read<void*, with_cb_t>(context, func, [] (void *&context, with_cb_t &func) -> ret_code_t {
        if (m_app_state != app_state::user_app_loaded) {
            return ERROR_USERCODE_NOT_AVAILABLE;
        }

        ret_code_t ret;
        // auto context = context_buf[0];
        // auto func = (with_cb_t)context_buf[1];

        auto d = desc(USERCODE_START_ADDR);
        auto tbl = desc_tbl {};

        ret = d.full_desc(tbl);
        VERIFY_SUCCESS(ret);

        return func(tbl.init, tbl.refresh, context);
    });
}

ret_code_t userapp::with_desc(void *context, with_desc_t func)
{
    return m_lock.read<void*, with_desc_t>(context, func, [] (void *&context, with_desc_t &func) -> ret_code_t {
        if (m_app_state != app_state::user_app_loaded) {
            return ERROR_USERCODE_NOT_AVAILABLE;
        }

        ret_code_t ret;
        // auto context = context_buf[0];
        // auto func = (with_cb_t)context_buf[1];

        auto d = desc(USERCODE_START_ADDR);
        auto tbl = desc_tbl {};

        ret = d.full_desc(tbl);
        VERIFY_SUCCESS(ret);

        return func(context, d);
    });
}

app_state userapp::get_app_state()
{
    return m_app_state;
}

storage_state userapp::get_storage_state()
{
    return m_storage_state;
}

ret_code_t userapp::clear_tempbuf()
{
    memset((void*)m_temp_buf, 0, sizeof(m_temp_buf));
    return NRF_SUCCESS;
}

ret_code_t userapp::write_tempbuf(size_t offset, uint8_t const *buffer, size_t length)
{
    if (offset + length > sizeof(m_temp_buf)) {
        return NRF_ERROR_INVALID_LENGTH;
    }

    memcpy((void*) (((uint8_t*)m_temp_buf) + offset), buffer, length);

    return NRF_SUCCESS;
}

uint16_t userapp::crc_tempbuf()
{
    return crc16_compute((uint8_t const*)m_temp_buf, sizeof(m_temp_buf), nullptr);
}

void userapp::default_init(led_chan *chan)
{
    if (chan->dmx_vals_len >= 4 && chan->dmx_vals[3] < 3) {
        chan->color_mode = chan->dmx_vals[3];
    } else {
        chan->color_mode = (uint8_t)led::color_mode::rgb;
    }

    uint32_t i = 0;
    for (uint16_t n = 0; n < chan->n_leds; n++) {
        chan->buffer[i++] = 0; /* set red to 0 */
        chan->buffer[i++] = 0; /* set green to 0 */
        chan->buffer[i++] = 0; /* set blue to 0 */
    }
}

void userapp::default_refresh(led_chan *chan)
{
    if (chan->dmx_vals_len >= 4 && chan->dmx_vals[3] < 3) {
        chan->color_mode = chan->dmx_vals[3];
    } else {
        chan->color_mode = (uint8_t)led::color_mode::rgb;
    }

    if (chan->dmx_vals_len >= 3) {
        uint32_t i = 0;
        for (uint16_t n = 0; n < chan->n_leds; n++) {
            chan->buffer[i++] = chan->dmx_vals[0];
            chan->buffer[i++] = chan->dmx_vals[1];
            chan->buffer[i++] = chan->dmx_vals[2];
        }
    }
}

static void fds_callback(fds_evt_t const *event)
{
    BaseType_t do_yield = pdFALSE;

    if (event && event->id == FDS_EVT_INIT) {
        m_fds_initialized = true;
    } else if (event && event->id == FDS_EVT_DEL_FILE && event->del.file_id == FDS_FILE_ID) {
        if (event->result == NRF_SUCCESS) {
            NRF_LOG_INFO("Erase complete, sending status.")
            m_storage_state = storage_state::empty;
            userapp::queue_send_state();
        }
    } else if (event && event->id == FDS_EVT_UPDATE && event->write.file_id == FDS_FILE_ID) {
        if (task::is_in_isr()) {
            xTaskNotifyFromISR(userapp::thread(), NOTIFY_UPDATE_DONE, eSetValueWithOverwrite, &do_yield);
        } else {
            xTaskNotify(userapp::thread(), NOTIFY_UPDATE_DONE, eSetValueWithOverwrite);
        }
    } else if (event && event->id == FDS_EVT_WRITE && event->write.file_id == FDS_FILE_ID) {
        if (task::is_in_isr()) {
            xTaskNotifyFromISR(userapp::thread(), NOTIFY_WRITE_DONE, eSetValueWithOverwrite, &do_yield);
        } else {
            xTaskNotify(userapp::thread(), NOTIFY_WRITE_DONE, eSetValueWithOverwrite);
        }
    }

    portYIELD_FROM_ISR(do_yield);
}
