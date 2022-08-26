#define NRF_LOG_MODULE_NAME task
#include "prelude.hh"
#include "task.hh"
#include "log.hh"
#include "fds.h"
#include "time.hh"
#include "nrf_ble_lesc.h"
NRF_LOG_MODULE_REGISTER();

#define NOTIFY_FDS_GC 0x0001

static TaskHandle_t m_gc_task = nullptr;
static void gc_thread(void *arg);

void vApplicationIdleHook()
{
    static int idle_count = 0;

    ret_code_t ret;
#if defined(USE_LESC) && USE_LESC
    ret = nrf_ble_lesc_request_handler();
#endif

    idle_count += 1;
#if NRF_LOG_ENABLED
    vTaskResume(log::thread());
#endif
}

ret_code_t task::init()
{
    auto status = xTaskCreate(
        gc_thread,
        "GC",
        128,
        nullptr,
        1,
        &m_gc_task
    );

    if (status != pdPASS) {
        return NRF_ERROR_NO_MEM;
    }

    return NRF_SUCCESS;
}

void task::schedule_gc()
{
    xTaskNotify(m_gc_task, NOTIFY_FDS_GC, eSetBits);
}

void task::schedule_gc_from_isr(BaseType_t *do_yield)
{
    xTaskNotifyFromISR(m_gc_task, NOTIFY_FDS_GC, eSetBits, do_yield);
}

bool task::is_in_isr()
{
    return 0 != (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk);
}

static void gc_thread(void *arg)
{
    unused(arg);

    while (1) {
        ret_code_t ret;
        uint32_t flags = 0;
        xTaskNotifyWait(0, UINT32_MAX, &flags, portMAX_DELAY);

        if (flags & NOTIFY_FDS_GC) {
            ret = fds_gc();
            if (ret != NRF_SUCCESS) {
                NRF_LOG_WARNING("fds_gc: 0x%x", ret);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

ret_code_t task::rw_lock::init()
{
    n_readers = 0;
    n_writers = 0;

    read_lock = xSemaphoreCreateMutex();
    if (!read_lock) {
        return NRF_ERROR_RESOURCES;
    }

    write_lock = xSemaphoreCreateMutex();
    if (!read_lock) {
        return NRF_ERROR_RESOURCES;
    }

    resource_lock = xSemaphoreCreateBinary();
    if (!read_lock) {
        return NRF_ERROR_RESOURCES;
    }

    block_readers = xSemaphoreCreateBinary();
    if (!read_lock) {
        return NRF_ERROR_RESOURCES;
    }

    xSemaphoreGive(resource_lock);
    xSemaphoreGive(block_readers);

    return NRF_SUCCESS;
}

/* configSUPPORT_STATIC_ALLOCATION is set to 1, so the application must provide an
implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
used by the Idle task. */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize )
{
/* If the buffers to be provided to the Idle task are declared inside this
function then they must be declared static - otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xIdleTaskTCB;
static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
    state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task's stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
/*-----------------------------------------------------------*/

/* configSUPPORT_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer,
                                     StackType_t **ppxTimerTaskStackBuffer,
                                     uint32_t *pulTimerTaskStackSize )
{
/* If the buffers to be provided to the Timer task are declared inside this
function then they must be declared static - otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xTimerTaskTCB;
static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

    /* Pass out a pointer to the StaticTask_t structure in which the Timer
    task's state will be stored. */
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

    /* Pass out the array that will be used as the Timer task's stack. */
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;

    /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configTIMER_TASK_STACK_DEPTH is specified in words, not bytes. */
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
