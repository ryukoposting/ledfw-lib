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
        CHARACTERISTIC(dmx_info_char);
        CHARACTERISTIC(dmx_explorer_char);
    
        uint16_t service_handle() override;

        ret_code_t update_app_info(desc const &desc, cfg::dmx_config_t const &config, app_state appst, storage_state storst);

        ret_code_t update_app_info(app_state appst, storage_state storst);

        ret_code_t set_app_name(char const *name);

        ret_code_t set_provider_name(char const *provider);

        ret_code_t send_personality_info(uint8_t index, uint8_t n_slots, dmx_pers const &pers);

        ret_code_t send_slot_info(uint8_t pers, uint8_t index, dmx_slot const &slot);

    protected:
        uint16_t *service_handle_ptr() override;
    };

    ret_code_t send_state();

    ret_code_t send_partial_state();
}
