#define NRF_LOG_MODULE_NAME dmx
#include "prelude.hh"
#include "dmx.hh"
#include "dmx/thread.hh"
#include "meta.hh"
#include "cfg.hh"
#include "message_buffer.h"

using namespace dmx;

static void dmx_packet_task(void*);

ret_code_t dmx::thread::init()
{
    auto status = xTaskCreate(
        dmx_packet_task,
        "DMX-PKT",
        256,
        this,
        3,
        &packet_task_handle
    );

    if (status != pdPASS) {
        return NRF_ERROR_NO_MEM;
    }

    slot_vals_subs_mutex = xSemaphoreCreateMutex();
    if (!slot_vals_subs_mutex)
        return NRF_ERROR_NO_MEM;

    ret_code_t ret = dmx_config.init();

    return ret;
}

void dmx::thread::set_frame_buf(MessageBufferHandle_t queue)
{
    packet_queue_handle = queue;
    vTaskResume(packet_task_handle);
}

ret_code_t dmx::thread::on_dmx_slot_vals(void *context, on_dmx_slot_vals_t callback, TickType_t max_delay)
{
    if (!callback)
        return NRF_ERROR_NULL;

    on_dmx_slot_vals_sub *p = (decltype(p))pvPortMalloc(sizeof(*p));
    if (xSemaphoreTake(slot_vals_subs_mutex, max_delay)) {
        p->callback = callback;
        p->context = context;
        p->next = slot_vals_subs;
        slot_vals_subs = p;
        xSemaphoreGive(slot_vals_subs_mutex);
        return NRF_SUCCESS;
    } else {
        return NRF_ERROR_BUSY;
    }
}

void dmx::thread::packet_task_func()
{
    static meta::rdm_id_t my_rdm_uid;
    static meta::rdm_id_t broadcast_rdm_uid = {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};

    my_rdm_uid = meta::device_rdm_uid();

    ret_code_t ret;
    auto config = cfg::dmx::config;

    config.subscribe(this, [](void *context, void const *data, size_t length) {
        auto self = (dmx::thread*)context;
        self->on_channel_cfg_update((cfg::dmx_config_t const*)data);
    });

    cfg::dmx_config_t dcfg;
    ret = config.get(&dcfg);
    if (ret == FDS_ERR_NOT_FOUND) {
        dcfg.channel = 0;
        dcfg.n_channels = MAX_USER_APP_SLOTS;
        dcfg.personality = 0;
        ret = config.set(&dcfg);
    }
    APP_ERROR_CHECK(ret);

    while (1) {
        if (!packet_queue_handle)
            vTaskSuspend(nullptr);

        auto nread = xMessageBufferReceive(packet_queue_handle, rx_buffer, sizeof(rx_buffer), portMAX_DELAY);

        {
            task::lock_guard<cfg::dmx_config_t> guard;
            if (dmx_config.take(guard, 0)) {
                memcpy(&dcfg, guard.as_ptr(), sizeof(cfg::dmx_config_t));
            }
        }

        /* DMX512 */
        if (dcfg.channel > 0 &&                             /* a subscription has been set, and... */
            nread > dcfg.channel &&                         /* the buffer contains data for the subscribed channels, and... */
            rx_buffer[0] == (uint8_t)start_code::dimmer)    /* this is a dimmer packet, and... */
        {
            auto chan_data = &rx_buffer[dcfg.channel];
            auto n_chan_datas = std::min(nread, (size_t)dcfg.n_channels);
            auto slot_vals = dmx_slot_vals { n_chan_datas, chan_data };

            xSemaphoreTake(slot_vals_subs_mutex, portMAX_DELAY);
            for (auto cb = slot_vals_subs; cb; cb = cb->next) {
                cb->callback(cb->context, &slot_vals, &dcfg);
            }
            xSemaphoreGive(slot_vals_subs_mutex);
        }

        /* RDM */
        if (nread >= 26 &&
            nread <= 257 &&
            rx_buffer[0] == (uint8_t)start_code::rdm &&
            rx_buffer[1] == (uint8_t)rdm_sub::message && 
            rx_buffer[2] == nread - 2 && (
                memcmp(&rx_buffer[3], my_rdm_uid.bytes, 6) == 0 ||
                memcmp(&rx_buffer[3], broadcast_rdm_uid.bytes, 6) == 0))
        {
            auto checksum_ofs = rx_buffer[2];
            auto const got_checksum = uint16_big_decode(&rx_buffer[checksum_ofs]);
            uint16_t expect_checksum = 0;
            for (size_t i = 0; i < checksum_ofs; ++i) {
                expect_checksum += rx_buffer[i];
            }

            if (got_checksum != expect_checksum) {
                NRF_LOG_WARNING("RDM checksum mismatch (expected=0x%04x got=0x%04x)", expect_checksum, got_checksum);
                continue;
            }

            if (rx_buffer[23] != nread - 26) {
                NRF_LOG_WARNING("RDM PDL (%u) does not match ML (%u)", rx_buffer[23], rx_buffer[2]);
                continue;
            }

            // meta::rdm_id_t source_rdm_uid;
            // memcpy(source_rdm_uid.bytes, &rx_buffer[9], 6);

            // auto const tn = rx_buffer[15];
            // auto const port_id = rx_buffer[16];
            // auto const msg_count = rx_buffer[17];
            // auto const sub_device = uint16_big_decode(&rx_buffer[18]);
            // auto const msg_len = rx_buffer[2] - 20;

            // auto const msg_body = (uint8_t const*)&rx_buffer[20];
        }
    }
}

ret_code_t thread::send(dmx_slot_vals const *slot_vals)
{
    dassert(!task::is_in_isr());
    xSemaphoreTake(slot_vals_subs_mutex, portMAX_DELAY);
    for (auto cb = slot_vals_subs; cb; cb = cb->next) {
        cb->callback(cb->context, slot_vals, nullptr);
    }
    xSemaphoreGive(slot_vals_subs_mutex);
    return NRF_SUCCESS;
}

void thread::slot_vals_task_func()
{
    vTaskSuspend(nullptr);
    unreachable();
}

void thread::on_channel_cfg_update(cfg::dmx_config_t const *config)
{
    {
        task::lock_guard<cfg::dmx_config_t> guard;
        if (dmx_config.take(guard, portMAX_DELAY)) {
            memcpy(guard.as_ptr(), config, sizeof(cfg::dmx_config_t));
        } else {
            unreachable();
        }
    }

    xSemaphoreTake(slot_vals_subs_mutex, portMAX_DELAY);
    for (auto cb = slot_vals_subs; cb; cb = cb->next) {
        cb->callback(cb->context, nullptr, config);
    }
    xSemaphoreGive(slot_vals_subs_mutex);
}

static void dmx_packet_task(void *arg)
{
    assert(arg != nullptr);
    ((dmx::thread*)arg)->packet_task_func();
    unreachable();
}
