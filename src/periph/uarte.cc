#define NRF_LOG_MODULE_NAME uarte
#include "prelude.hh"
#include "periph/uarte.hh"
#include "sdk_config.h"
#include "nrfx_uarte.h"
#include "nrf_uarte.h"
#include "stream_buffer.h"
NRF_LOG_MODULE_REGISTER();

using namespace uarte;

struct uarte_context {
    uarte::id inst_id;
    MessageBufferHandle_t frame_buf;
    bool is_enabled;
    uint8_t buffer0[DMX_MAX_FRAME_SIZE] __attribute__((aligned(sizeof(uint32_t))));

    inline transport get_transport() { return transport(inst_id); }
};


static uarte_context m_uarte_context[MAX_UARTE_INST] __attribute__((aligned(sizeof(uint32_t)))) = {};
static nrfx_uarte_t m_uarte_inst[] = {
#if NRFX_UARTE0_ENABLED
    NRFX_UARTE_INSTANCE(0)
#endif
#if NRFX_UARTE1_ENABLED
    NRFX_UARTE_INSTANCE(1)
#endif
#if NRFX_UARTE2_ENABLED
    NRFX_UARTE_INSTANCE(2)
#endif
#if NRFX_UARTE3_ENABLED
    NRFX_UARTE_INSTANCE(3)
#endif
};

static void uarte_evt_handler(nrfx_uarte_event_t const *event, void *context);

ret_code_t uarte::transport::init(uarte_init const &init)
{
    ret_code_t ret;

    m_uarte_context[inst_id].is_enabled = false;

    m_uarte_context[inst_id].frame_buf = xMessageBufferCreate(DMX_MAX_QUEUED_FRAMES * DMX_MAX_FRAME_SIZE);
    if (m_uarte_context[inst_id].frame_buf == nullptr)
        return NRF_ERROR_NO_MEM;

    auto inst = &m_uarte_inst[inst_id];
    nrfx_uarte_config_t config = NRFX_UARTE_DEFAULT_CONFIG;

    switch (config.baudrate) {
    case BAUD_250K: config.baudrate = NRF_UARTE_BAUDRATE_250000; break;
    }

    config.pselrxd = init.rx;
    config.p_context = &m_uarte_context[inst_id];
    m_uarte_context[inst_id].inst_id = inst_id;
    if (init.two_stop_bits) {
        config.stop = NRF_UARTE_STOPBITS_2;
    }

    ret = nrfx_uarte_init(inst, &config, uarte_evt_handler);
    VERIFY_SUCCESS(ret);

    return ret;
}

MessageBufferHandle_t uarte::transport::frame_buf()
{
    return m_uarte_context[inst_id].frame_buf;
}

ret_code_t uarte::transport::enable()
{
    ret_code_t ret = nrfx_uarte_rx(&m_uarte_inst[inst_id], m_uarte_context[inst_id].buffer0, sizeof(m_uarte_context[inst_id].buffer0));
    if (ret == NRF_SUCCESS) {
        m_uarte_context[inst_id].is_enabled = true;
    }
    return ret;
}

ret_code_t uarte::transport::disable()
{
    m_uarte_context[inst_id].is_enabled = false;
    nrfx_uarte_rx_abort(&m_uarte_inst[inst_id]);
    return NRF_SUCCESS;
}

static void uarte_evt_handler(nrfx_uarte_event_t const *event, void *context)
{
    if (!context) {
        NRF_LOG_WARNING("Null context");
        return;
    }

    if (!event) {
        return;
    }

    auto ctxt = ((uarte_context*)context);
    auto tx = ctxt->get_transport();
    BaseType_t do_yield = pdFALSE;

    if (event->type == NRFX_UARTE_EVT_RX_DONE) {
        auto ptr = event->data.rxtx.p_data;
        auto length = event->data.rxtx.bytes;
        if (ctxt->is_enabled) {
            xMessageBufferSendFromISR(tx.frame_buf(), ptr, length, &do_yield);
            nrfx_uarte_rx(&m_uarte_inst[ctxt->inst_id], ctxt->buffer0, sizeof(ctxt->buffer0));
        }
        
    } else if (event->type == NRFX_UARTE_EVT_ERROR && (event->data.error.error_mask & NRF_UARTE_ERROR_BREAK_MASK)) {
        if (ctxt->is_enabled) {
            nrfx_uarte_rx(&m_uarte_inst[ctxt->inst_id], ctxt->buffer0, sizeof(ctxt->buffer0));
        }
    }

    portYIELD_FROM_ISR(do_yield);
}
