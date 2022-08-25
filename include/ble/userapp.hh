#pragma once

#include "prelude.hh"
#include "userapp.hh"
#include "ble/service.hh"

namespace userapp {
    struct service: ble::service {
        service();

        ret_code_t init();

        CHARACTERISTIC(program_char);
        CHARACTERISTIC(info_char);
        CHARACTERISTIC(app_name_char);
        CHARACTERISTIC(app_provider_char);
    
        uint16_t service_handle() override;

        ret_code_t update_app_info(desc_tbl const &desc, app_state appst, storage_state storst);

        ret_code_t update_app_info(app_state appst, storage_state storst);

        ret_code_t set_app_name(char const *name);

        ret_code_t set_provider_name(char const *provider);

    protected:
        uint16_t *service_handle_ptr() override;
    };

    ret_code_t send_state();

    ret_code_t send_partial_state();
}
