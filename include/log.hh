#pragma once

#include "prelude.hh"

namespace log {
    ret_code_t init_thread(void);

    TaskHandle_t thread();
}
