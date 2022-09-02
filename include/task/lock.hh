#pragma once

#include "prelude.hh"
#include "semphr.h"
#include "task.hh"

namespace task {

template<typename T>
struct lock;

template<typename T>
struct lock_guard {
    constexpr lock_guard():
        mutex(nullptr),
        data(nullptr)
    {}

    ~lock_guard()
    {
        if (mutex)
            xSemaphoreGive(mutex);
    }

    T *as_ptr() const
    {
        assert(mutex);
        return data;
    }

    T &as_ref() const
    {
        assert(mutex);
        return *data;
    }
protected:
    void set(xSemaphoreHandle m, T *d)
    {
        mutex = m;
        data = d;
    }

    xSemaphoreHandle mutex;
    T *data;

    friend class lock<T>;
};

template<typename T>
struct lock_guard_isr {
    constexpr lock_guard_isr():
        mutex(nullptr),
        data(nullptr),
        do_yield(nullptr)
    {}

    ~lock_guard_isr()
    {
        if (mutex)
            xSemaphoreGiveFromISR(mutex, do_yield);
    }

    T *as_ptr() const
    {
        assert(mutex);
        return data;
    }

    T &as_ref() const
    {
        assert(mutex);
        return *data;
    }
protected:
    void set(xSemaphoreHandle m, T *d, BaseType_t *y)
    {
        mutex = m;
        data = d;
        do_yield = y;
    }

    xSemaphoreHandle mutex;
    T *data;
    BaseType_t *do_yield;

    friend class lock<T>;
};


template<typename T>
struct lock {
    constexpr lock(T &initial):
        mutex(nullptr),
        data(initial)
    {}

    constexpr lock():
        mutex(nullptr),
        data()
    {}

    ret_code_t init()
    {
        if (mutex)
            return NRF_SUCCESS;

        mutex = xSemaphoreCreateMutex();

        if (!mutex)
            return NRF_ERROR_NO_MEM;
        else
            return NRF_SUCCESS;
    }

    bool take(lock_guard<T> &guard, TickType_t max_wait)
    {
        dassert(!task::is_in_isr());
        auto taken = xSemaphoreTake(mutex, max_wait);
        if (!taken)
            return false;

        guard.set(mutex, &data);
        return true;
    }

    bool take_from_isr(lock_guard_isr<T> &guard, BaseType_t *do_yield)
    {
        dassert(task::is_in_isr());
        auto taken = xSemaphoreTakeFromISR(mutex, do_yield);
        if (!taken)
            return false;

        guard.set(mutex, &data, do_yield);
        return true;
    }

protected:
    xSemaphoreHandle mutex;
    T data;
};

}
