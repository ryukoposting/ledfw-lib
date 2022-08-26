#define NRF_LOG_MODULE_NAME cfg
#include "prelude.hh"
#include "cfg.hh"
#include "fds.h"
#include "task.hh"

using namespace cfg;

#define FDS_FILE_ID 0x4111

struct param_cache {
    size_t length = 0;
    size_t alloc_length = 0;
    void *data = nullptr;
};

struct rcontext {
    id record_id;
    void *data;
    size_t length;
};

struct wcontext {
    id record_id;
    void const *data;
    size_t length;
};

static void fds_callback(fds_evt_t const *event);
static bool m_initialized = false;
static bool m_fds_initialized = false;

static void cfg_flash_thread(void *arg);
static TaskHandle_t m_cfg_flash_task;

static param_cache m_cache[cfg::N_PARAMS] = {};
static auto m_cache_lock = task::rw_lock();

static ret_code_t try_read(rcontext &context, bool &cache_hit);

static xSemaphoreHandle m_write_complete = nullptr;

ret_code_t cfg::init_flash_backend()
{
    if (m_initialized)
        return NRF_SUCCESS;

    ret_code_t ret;

    ret = fds_register(fds_callback);
    VERIFY_SUCCESS(ret);

    ret = fds_init();
    VERIFY_SUCCESS(ret);
    while (!m_fds_initialized) {}

    ret = m_cache_lock.init();
    VERIFY_SUCCESS(ret);

    m_write_complete = xSemaphoreCreateBinary();
    if (m_write_complete == nullptr)
        return NRF_ERROR_NO_MEM;

    auto status = xTaskCreate(
        cfg_flash_thread,
        "CFG-FLASH",
        256,
        nullptr,
        1,
        &m_cfg_flash_task
    );

    if (status != pdPASS) {
        return NRF_ERROR_NO_MEM;
    }

    if (ret == NRF_SUCCESS) {
        m_initialized = true;
    }

    return ret;
}

ret_code_t flash_backend::read(id record_id, void *data, size_t length)
{
    ret_code_t ret;
    auto context = rcontext { record_id, data, length };
    bool cache_hit = false;

    /* try reading the param with shared access */
    ret = m_cache_lock.read<rcontext,bool>(context, cache_hit, try_read);
    VERIFY_SUCCESS(ret);

    if (!cache_hit) {
        ret = m_cache_lock.write<rcontext,bool>(context, cache_hit, [](rcontext &context, bool &cache_hit) -> ret_code_t {
            /* try reading the param with exclusive access */
            ret_code_t ret = try_read(context, cache_hit);
            VERIFY_SUCCESS(ret);

            if (!cache_hit) {
                /* param has not been read, set it up */
                auto desc = fds_record_desc_t {};
                auto token = fds_find_token_t {};
                auto record = fds_flash_record_t {};

                auto const index = id_to_index(context.record_id);

                ret = fds_record_find(FDS_FILE_ID, (uint16_t)context.record_id, &desc, &token);
                VERIFY_SUCCESS(ret);

                ret = fds_record_open(&desc, &record);
                VERIFY_SUCCESS(ret);

                auto length = record.p_header->length_words * sizeof(uint32_t);
                auto data = pvPortMalloc(length);
                assert(data != nullptr);

                memcpy(data, record.p_data, length);

                ret = fds_record_close(&desc);
                VERIFY_SUCCESS(ret);

                m_cache[index] = param_cache { length, length, data };

                memcpy(context.data, data, std::min(length, context.length));
            }

            return ret;
        });
    }

    return ret;
}

ret_code_t flash_backend::write(id record_id, void const *data, size_t length)
{
    ret_code_t ret;
    auto context = wcontext { record_id, data, length };

    ret = m_cache_lock.write<wcontext>(context, [](wcontext &context) -> ret_code_t {
        auto const index = id_to_index(context.record_id);
        auto const notify = id_to_notif(context.record_id);
        assert(index < N_PARAMS);

        if (m_cache[index].data) {
            if (m_cache[index].alloc_length >= context.length) {
                if (m_cache[index].alloc_length == context.length &&
                    memcmp(m_cache[index].data, context.data, context.length) == 0)
                {
                    return NRF_SUCCESS; /* no change from current data, return success */
                }
                memcpy(m_cache[index].data, context.data, context.length);
                m_cache[index].length = context.length;
            } else {
                vPortFree(m_cache[index].data);
                m_cache[index].data = pvPortMalloc(context.length);
                assert(m_cache[index].data != nullptr);
                m_cache[index].length = context.length;
                memcpy(m_cache[index].data, context.data, context.length);
            }

            xTaskNotify(m_cfg_flash_task, (uint32_t)notify, eSetBits);

            return NRF_SUCCESS;
        }

        m_cache[index].data = pvPortMalloc(context.length);
        assert(m_cache[index].data != nullptr);
        m_cache[index].length = context.length;
        memcpy(m_cache[index].data, context.data, context.length);

        xTaskNotify(m_cfg_flash_task, (uint32_t)notify, eSetBits);

        return NRF_SUCCESS;
    });

    return ret;
}


static void cfg_flash_thread(void *arg)
{
    unused(arg);

    while (1) {
        ret_code_t ret;
        uint32_t flags;
        static auto wrecord = fds_record_t {};

        vTaskDelay(pdMS_TO_TICKS(1000));

        xTaskNotifyWait(0, UINT32_MAX, &flags, portMAX_DELAY);

        for (auto record_id : ALL_IDS) {
            auto mask = id_to_notif(record_id);
            // auto record_id = ALL_IDS[i];

            if (flags & (uint16_t)mask) {
                do {
                    auto desc = fds_record_desc_t {};
                    auto token = fds_find_token_t {};
                    wrecord.file_id = FDS_FILE_ID;
                    wrecord.key = (uint16_t)record_id;
                    ret = fds_record_find(FDS_FILE_ID, (uint16_t)record_id, &desc, &token);
                    if (ret == FDS_ERR_NOT_FOUND) {
                        /* create new record, store it to flash */
                        ret = m_cache_lock.read<fds_record_t, fds_record_desc_t>(wrecord, desc, [](fds_record_t &wrecord, fds_record_desc_t &desc) -> ret_code_t {
                            auto index = id_to_index((id)wrecord.key);
                            wrecord.data.p_data = m_cache[index].data;
                            wrecord.data.length_words = m_cache[index].length / sizeof(uint32_t);
                            return fds_record_write(&desc, &wrecord);
                        });
                    } else if (ret == NRF_SUCCESS) {
                        /* update a record that already exists in flash */
                        ret = m_cache_lock.read<fds_record_t, fds_record_desc_t>(wrecord, desc, [](fds_record_t &wrecord, fds_record_desc_t &desc) -> ret_code_t {
                            auto index = id_to_index((id)wrecord.key);
                            wrecord.data.p_data = m_cache[index].data;
                            wrecord.data.length_words = m_cache[index].length / sizeof(uint32_t);
                            return fds_record_update(&desc, &wrecord);
                        });
                    }

                    if (ret == FDS_ERR_NO_SPACE_IN_FLASH) {
                        task::schedule_fds_gc();
                        vTaskDelay(pdMS_TO_TICKS(2000));
                    } else {
                        APP_ERROR_CHECK(ret);
                        break;
                    }
                } while (1);
            }
        }
    }
}

static ret_code_t try_read(rcontext &context, bool &cache_hit) {
    auto const index = id_to_index(context.record_id);
    assert(index < N_PARAMS);

    if (m_cache[index].data) {
        cache_hit = true;
        memcpy(context.data, m_cache[index].data, std::min(context.length, m_cache[index].length));
    }

    return NRF_SUCCESS;
};


static void fds_callback(fds_evt_t const *event)
{
    BaseType_t do_yield = pdFALSE;

    if (event && event->id == FDS_EVT_INIT) {
        m_fds_initialized = true;
    } else if (event && event->id == FDS_EVT_UPDATE && event->write.file_id == FDS_FILE_ID) {
        if (task::is_in_isr()) {
            xSemaphoreGiveFromISR(m_write_complete, &do_yield);
        } else {
            xSemaphoreGive(m_write_complete);
        }
    } else if (event && event->id == FDS_EVT_WRITE && event->write.file_id == FDS_FILE_ID) {
        if (task::is_in_isr()) {
            xSemaphoreGiveFromISR(m_write_complete, &do_yield);
        } else {
            xSemaphoreGive(m_write_complete);
        }
    }

    portYIELD_FROM_ISR(do_yield);
}
