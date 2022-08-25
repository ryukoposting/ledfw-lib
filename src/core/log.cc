#include "prelude.hh"
#include "log.hh"
#include "task.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"

static TaskHandle_t m_logger_thread;

static void logger_thread(void *arg)
{
    unused(arg);
    while (1) {
        NRF_LOG_FLUSH();
        vTaskSuspend(NULL);
    }
}

ret_code_t log::init_thread()
{
    if (pdPASS != xTaskCreate(logger_thread, "LOGGER", 256, NULL, 1, &m_logger_thread)) {
        return NRF_ERROR_NO_MEM;
    } else {
        return NRF_SUCCESS;
    }
}

TaskHandle_t log::thread()
{
    return m_logger_thread;
}
