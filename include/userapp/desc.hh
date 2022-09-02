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

    /** @brief A DMX slot descriptor. */
    struct dmx_slot {
        constexpr dmx_slot(uint32_t addr):
            buffer((uint32_t*)addr)
        {};

        inline uint8_t type() const  { return buffer[1] & 0xff; }
        inline uint16_t id() const   { return (buffer[1] >> 8) & 0xffff; }
        inline uint8_t value() const { return (buffer[1] >> 24) & 0xff; }
        inline uint32_t addr() const { return (uint32_t)buffer; }

        char const *name() const
        {
            if (buffer[0] >= USERCODE_START_ADDR && buffer[0] < USERCODE_START_ADDR + USERCODE_SIZE) {
                return (char const*)buffer[0];
            } else {
                return nullptr;
            }
        }

    protected:
        uint32_t const *buffer;
    };

    /** @brief A DMX personality descriptor. */
    struct dmx_pers {
        constexpr dmx_pers(uint32_t addr):
            buffer((uint32_t*)addr)
        {};

        inline uint32_t addr() const { return (uint32_t)buffer; }

        inline char const *name() const
        {
            if (buffer[0] >= USERCODE_START_ADDR && buffer[0] < USERCODE_START_ADDR + USERCODE_SIZE) {
                return (char const*)buffer[0];
            } else {
                return nullptr;
            }
        }

        uint32_t const *dmx_slots_tbl() const
        {
            return &buffer[1];
        }

        size_t n_dmx_slots() const
        {
            size_t result;
            for (result = 0; result <= MAX_USER_APP_SLOTS && buffer[result + 1]; ++result);
            if (result > MAX_USER_APP_SLOTS) return 0;
            return result;
        }
    protected:
        uint32_t const *buffer;
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

        uint32_t const *dmx_pers_tbl() const
        {
            return &buffer[8];
        }

        size_t n_dmx_pers() const
        {
            size_t result;
            for (result = 0; result <= MAX_USER_APP_PERSONALITIES && buffer[result + 8]; ++result);
            if (result > MAX_USER_APP_PERSONALITIES) return 0;
            return result;
        }

        // template<typename T>
        // using each_personality_t = ret_code_t (*)(T&, dmx_pers const&);

        // template<typename T>
        // inline ret_code_t each_personality(T &context, each_personality_t<T> func) const
        // {
        //     ret_code_t ret = NRF_SUCCESS;
        //     for (size_t i = 8; buffer[i] && (ret == NRF_SUCCESS); ++i) {
        //         auto pers = dmx_pers(buffer[i]);
        //         ret = func(context, pers);
        //         if (ret == BREAK)
        //             return NRF_SUCCESS;
        //     }
        //     return ret;
        // }
    protected:
        uint32_t const *buffer;
    };
}
