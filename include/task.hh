#pragma once

#include "prelude.hh"
#include "util.hh"
#include "semphr.h"
#include "nrf_atomic.h"

#ifdef __cplusplus
extern "C" {
#endif

void vApplicationIdleHook(void);
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize);
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize);

#ifdef __cplusplus
}
#endif

namespace task {
    ret_code_t init();

    void schedule_fds_gc();
    void schedule_fds_gc_from_isr(BaseType_t *do_yield);

    void schedule_pm_gc();
    void schedule_pm_gc_from_isr(BaseType_t *do_yield);

    bool is_in_isr();

    struct atomic {
        constexpr atomic(uint32_t initial): value(initial) {}

        inline uint32_t fetch_store(uint32_t x)
        {
            return nrf_atomic_u32_fetch_store(&value, x);
        }

        inline uint32_t store(uint32_t x)
        {
            return nrf_atomic_u32_fetch_store(&value, x);
        }

        inline uint32_t operator++()
        {
            return nrf_atomic_u32_add(&value, 1);
        }
    
        inline uint32_t operator++(int)
        {
            return nrf_atomic_u32_fetch_add(&value, 1);
        }

        inline uint32_t operator--()
        {
            return nrf_atomic_u32_sub_hs(&value, 1);
        }

        inline uint32_t operator--(int)
        {
            return nrf_atomic_u32_fetch_sub_hs(&value, 1);
        }
    protected:
        uint32_t value;
    };

    struct rw_lock {
        ret_code_t init();

        template<typename T>
        inline ret_code_t read(T &context, ret_code_t (*func)(T&))
        {
            assert(!task::is_in_isr());

            xSemaphoreTake(block_readers, portMAX_DELAY);
            xSemaphoreTake(read_lock, portMAX_DELAY);
            if (++n_readers == 1) {
                xSemaphoreTake(resource_lock, portMAX_DELAY);
            }
            xSemaphoreGive(read_lock);
            xSemaphoreGive(block_readers);

            auto result = func(context);

            xSemaphoreTake(read_lock, portMAX_DELAY);
            if (--n_readers == 0) {
                xSemaphoreGive(resource_lock);
            }
            xSemaphoreGive(read_lock);

            return result;
        }

        template<typename T, typename U>
        inline ret_code_t read(T &c1, U &c2, ret_code_t (*func)(T&, U&))
        {
            assert(!task::is_in_isr());

            xSemaphoreTake(block_readers, portMAX_DELAY);
            xSemaphoreTake(read_lock, portMAX_DELAY);
            if (++n_readers == 1) {
                xSemaphoreTake(resource_lock, portMAX_DELAY);
            }
            xSemaphoreGive(read_lock);
            xSemaphoreGive(block_readers);

            auto result = func(c1, c2);

            xSemaphoreTake(read_lock, portMAX_DELAY);
            if (--n_readers == 0) {
                xSemaphoreGive(resource_lock);
            }
            xSemaphoreGive(read_lock);

            return result;
        }

        inline ret_code_t read(ret_code_t (*func)(void))
        {
            assert(!task::is_in_isr());

            xSemaphoreTake(block_readers, portMAX_DELAY);
            xSemaphoreTake(read_lock, portMAX_DELAY);
            if (++n_readers == 1) {
                xSemaphoreTake(resource_lock, portMAX_DELAY);
            }
            xSemaphoreGive(read_lock);
            xSemaphoreGive(block_readers);

            auto result = func();

            xSemaphoreTake(read_lock, portMAX_DELAY);
            if (--n_readers == 0) {
                xSemaphoreGive(resource_lock);
            }
            xSemaphoreGive(read_lock);

            return result;
        }

        template<typename T>
        inline ret_code_t write(T &context, ret_code_t (*func)(T&))
        {
            assert(!task::is_in_isr());

            xSemaphoreTake(write_lock, portMAX_DELAY);
            if (++n_writers == 1) {
                xSemaphoreTake(block_readers, portMAX_DELAY);
            }
            xSemaphoreGive(write_lock);
            xSemaphoreTake(resource_lock, portMAX_DELAY);

            auto result = func(context);

            xSemaphoreGive(resource_lock);
            xSemaphoreTake(write_lock, portMAX_DELAY);
            if (--n_writers == 0) {
                xSemaphoreGive(block_readers);
            }
            xSemaphoreGive(write_lock);

            return result;
        }

        template<typename T, typename U>
        inline ret_code_t write(T &c1, U &c2, ret_code_t (*func)(T&, U&))
        {
            assert(!task::is_in_isr());

            xSemaphoreTake(write_lock, portMAX_DELAY);
            if (++n_writers == 1) {
                xSemaphoreTake(block_readers, portMAX_DELAY);
            }
            xSemaphoreGive(write_lock);
            xSemaphoreTake(resource_lock, portMAX_DELAY);

            auto result = func(c1, c2);

            xSemaphoreGive(resource_lock);
            xSemaphoreTake(write_lock, portMAX_DELAY);
            if (--n_writers == 0) {
                xSemaphoreGive(block_readers);
            }
            xSemaphoreGive(write_lock);

            return result;
        }

        inline ret_code_t write(ret_code_t (*func)(void))
        {
            assert(!task::is_in_isr());

            xSemaphoreTake(write_lock, portMAX_DELAY);
            if (++n_writers == 1) {
                xSemaphoreTake(block_readers, portMAX_DELAY);
            }
            xSemaphoreGive(write_lock);
            xSemaphoreTake(resource_lock, portMAX_DELAY);

            auto result = func();

            xSemaphoreGive(resource_lock);
            xSemaphoreTake(write_lock, portMAX_DELAY);
            if (--n_writers == 0) {
                xSemaphoreGive(block_readers);
            }
            xSemaphoreGive(write_lock);

            return result;
        }

    protected:
        xSemaphoreHandle block_readers;
        xSemaphoreHandle write_lock;
        xSemaphoreHandle read_lock;
        xSemaphoreHandle resource_lock;
        size_t n_readers;
        size_t n_writers;
    };
}
