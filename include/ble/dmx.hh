#pragma once

#include "prelude.hh"
#include "ble/service.hh"
#include "dmx/thread.hh"

namespace dmx {
    struct service: ble::service {
        service();

        CHARACTERISTIC(values_char);
        CHARACTERISTIC(config_char);

        ret_code_t init(dmx::thread *thread);

        uint16_t service_handle() override;

    protected:
        uint16_t *service_handle_ptr() override;
    };
};
