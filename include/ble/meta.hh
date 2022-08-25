#pragma once

#include "prelude.hh"
#include "ble/service.hh"

namespace meta {
    struct service: ble::service {
        service();

        ret_code_t init();

        CHARACTERISTIC(device_uid_char);
        CHARACTERISTIC(system_control_char);
        CHARACTERISTIC(log_char);

        uint16_t service_handle() override;

        ret_code_t print(char const *msg);

    protected:
        uint16_t *service_handle_ptr() override;
    };
}
