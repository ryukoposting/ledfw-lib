#pragma once

#include "prelude.hh"

namespace meta {
    struct rdm_id_t {
        uint8_t bytes[6];
    };

    packed_struct build_id {
        uint32_t name_size;
        uint32_t desc_size;
        uint32_t type;
        uint8_t data[];
    };

    void init();

    size_t device_name_len();

    char const *device_name();

    size_t mfg_name_len();

    char const *mfg_name();

    rdm_id_t device_rdm_uid();

    char const *device_uid_str();
}
