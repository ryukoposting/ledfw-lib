#include "prelude.hh"
#include "periph/spi.hh"
#include "nrfx_spim.h"
#include "nrf_spim.h"
#include "nrfx_ppi.h"

using namespace spi;
using send_complete_t = led::transport::send_complete_t;

struct tx_buffer { uint8_t *buffer; size_t length; };

static void spim_evt_handler(nrfx_spim_evt_t const *event, void *context);
static nrfx_spim_t *spim_inst(id id);
static send_complete_t callback(id id, send_complete_t cb);
static tx_buffer *queued_buffer(id id, tx_buffer *buffer);
static bool ready(id id, bool *v);
static void *cb_context[MAX_SPIM_INST] = {};

transport::transport(id id): led::transport(),
    inst_id(id)
{
    assert(id < MAX_SPIM_INST);
}

ret_code_t transport::init(spi_init const &init)
{
    nrfx_spim_config_t config = NRFX_SPIM_DEFAULT_CONFIG;

    config.sck_pin = init.sck;
    config.mosi_pin = init.mosi;
    config.bit_order = NRF_SPIM_BIT_ORDER_MSB_FIRST;
    config.mode = NRF_SPIM_MODE_0;

    switch (init.frequency) {
    // case FREQ_4M: config.frequency = NRF_SPIM_FREQ_4M; break;
    case FREQ_8M: config.frequency = NRF_SPIM_FREQ_8M; break;
    // case FREQ_16M: config.frequency = NRF_SPIM_FREQ_16M; break;
    // case FREQ_32M: config.frequency = NRF_SPIM_FREQ_32M; break;
    }

    ret_code_t ret = nrfx_spim_init(spim_inst(inst_id), &config, spim_evt_handler, (void*)static_cast<uintptr_t>(inst_id));
    VERIFY_SUCCESS(ret);

    bool set_ready = true;
    ready(inst_id, &set_ready);

    return ret;
}

ret_code_t transport::on_send_complete(void *context, send_complete_t cb)
{
    callback(inst_id, cb);
    cb_context[inst_id] = context;
    return NRF_SUCCESS;
}

ret_code_t transport::set_buffer(uint8_t *buf, size_t length)
{
    if (!ready(inst_id, nullptr)) {
        return NRF_ERROR_BUSY;
    }

    ret_code_t ret;

    auto spim = spim_inst(inst_id);
    nrfx_spim_xfer_desc_t xfer = NRFX_SPIM_XFER_TX(buf, length);

    ret = nrfx_spim_xfer(spim, &xfer, NRFX_SPIM_FLAG_HOLD_XFER | NRFX_SPIM_FLAG_REPEATED_XFER);
    // ret = nrfx_spim_xfer(spim, &xfer, NRFX_SPIM_FLAG_HOLD_XFER);
    VERIFY_SUCCESS(ret);

    CRITICAL_REGION_ENTER();
        auto bufp = queued_buffer(inst_id, nullptr);
        bufp->buffer = buf;
        bufp->length = length;
    CRITICAL_REGION_EXIT();

    return NRF_SUCCESS;
}

ret_code_t transport::send()
{
    if (!ready(inst_id, nullptr)) {
        return NRF_ERROR_BUSY;
    }

    ret_code_t ret = NRF_SUCCESS;

    auto spim = spim_inst(inst_id);

    CRITICAL_REGION_ENTER();
        auto bufp = queued_buffer(inst_id, nullptr);
        if (!bufp->buffer)
            ret = ERROR_NO_BUFFER;
        bool set_ready = false;
        ready(inst_id, &set_ready);
    CRITICAL_REGION_EXIT();
    VERIFY_SUCCESS(ret);

    nrf_spim_task_trigger(spim->p_reg, NRF_SPIM_TASK_START);

    return NRF_SUCCESS;
}

bool transport::is_ready()
{
    return ready(inst_id, nullptr);
}

static void spim_evt_handler(nrfx_spim_evt_t const *event, void *context)
{
    if (event && event->type == NRFX_SPIM_EVENT_DONE) {
        auto spi = transport((id)reinterpret_cast<uintptr_t>(context));
        auto inst_id = spi.instance_id();
        bool do_release_buffer = false;
        BaseType_t do_context_switch = pdFALSE;

        CRITICAL_REGION_ENTER();
            auto completion = callback(inst_id, nullptr);
            auto buffer = queued_buffer(inst_id, nullptr);
            do_release_buffer = buffer && completion && completion(buffer->buffer, buffer->length, &do_context_switch, cb_context[inst_id]);
        CRITICAL_REGION_EXIT();

        if (do_release_buffer) {
            tx_buffer txbuf = { .buffer = nullptr, .length = 0 };
            queued_buffer(inst_id, &txbuf);
        }

        bool set_ready = true;
        ready(inst_id, &set_ready);
        portYIELD_FROM_ISR(do_context_switch);
    }
}

static nrfx_spim_t *spim_inst(id id)
{
    static nrfx_spim_t spim_inst[] = {
    #if NRFX_CHECK(NRFX_SPIM0_ENABLED)
        NRFX_SPIM_INSTANCE(0),
    #endif
    #if NRFX_CHECK(NRFX_SPIM1_ENABLED)
        NRFX_SPIM_INSTANCE(1),
    #endif
    #if NRFX_CHECK(NRFX_SPIM2_ENABLED)
        NRFX_SPIM_INSTANCE(2),
    #endif
    #if NRFX_CHECK(NRFX_SPIM3_ENABLED)
        NRFX_SPIM_INSTANCE(3),
    #endif
    };

    assert(id < MAX_SPIM_INST);

    return &spim_inst[id];
}

static send_complete_t cb_inst[MAX_SPIM_INST] = {};
static send_complete_t callback(id id, send_complete_t cb)
{
    assert(id < MAX_SPIM_INST);

    if (cb) {
        cb_inst[id] = cb;
    }

    return cb_inst[id];
}

static tx_buffer *queued_buffer(id id, tx_buffer *buffer)
{
    static tx_buffer tx_buffer_inst[MAX_SPIM_INST] = {};

    assert(id < MAX_SPIM_INST);

    if (buffer) {
        tx_buffer_inst[id].buffer = buffer->buffer;
        tx_buffer_inst[id].length = buffer->length;
    }

    return &tx_buffer_inst[id];
}

static bool ready(id id, bool *x)
{
    static bool ready_inst[MAX_SPIM_INST] = {};

    if (x) {
        ready_inst[id] = *x;
    }

    return ready_inst[id];
}
