#include "prelude.hh"
#include "time.hh"
#include "task.hh"

TickType_t time::ticks()
{
    if (task::is_in_isr())
        return xTaskGetTickCountFromISR();
    else
        return xTaskGetTickCount();
}

TickType_t time::msecs()
{
    auto ticks = task::is_in_isr() ? xTaskGetTickCountFromISR() : xTaskGetTickCount();

    return (TickType_t) ((1000ull * (uint64_t)ticks) / (uint64_t)configTICK_RATE_HZ);
}
