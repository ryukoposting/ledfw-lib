#pragma once

#include "prelude.hh"
#include "userapp/types.hh"

namespace userapp {
    struct desc_tbl {
        uint32_t magic;
        uint32_t version;
        init_func_t init;
        refresh_func_t refresh;
        char const *provider_name;
        char const *app_name;
        char const *app_id;
    };

    struct desc {
        constexpr desc(): desc(USERCODE_START_ADDR) {}
        constexpr desc(uint32_t addr):
            buffer((uint32_t*)addr)
        {};

        inline ret_code_t magic_number(uint32_t &magic) const
        {
            magic = buffer[0];
            if (magic != USERCODE_MAGIC) {
                return ERROR_USERCODE_INVALID_MAGIC;
            } else {
                return NRF_SUCCESS;
            }
        }

        inline ret_code_t version(uint32_t &ver) const
        {
            ver = buffer[1];
            if (ver < USERCODE_VERSION_MIN || ver > USERCODE_VERSION_MAX) {
                return ERROR_USERCODE_INVALID_VERSION;
            } else {
                return NRF_SUCCESS;
            }
        }

        inline ret_code_t architecture(uint32_t &arch) const
        {
            arch = buffer[7];
            uint16_t cpu = arch & 0xffff;
            uint16_t flags = (arch >> 16) & 0xffff;

            if (cpu != USERCODE_ARCH_CPU) {
                return ERROR_USERCODE_INVALID_ARCH;
            }

            if ((flags & USERCODE_ARCH_FLAGS_MASK) != USERCODE_ARCH_FLAGS_VALUE) {
                return ERROR_USERCODE_INVALID_ARCH;
            }

            return NRF_SUCCESS;
        }

        ret_code_t full_desc(desc_tbl &tbl) const;

    protected:
        uint32_t const *buffer;
    };
}
