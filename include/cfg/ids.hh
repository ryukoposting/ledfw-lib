#pragma once

#include "prelude.hh"



namespace cfg {
    enum class id: uint16_t {
#define CFG(name_,id_) name_ = id_,
#include "def/cfg.def"
#undef CFG
    };

    enum class __notif_helper: uint32_t {
#define CFG(name_,id_) name_,
#include "def/cfg.def"
#undef CFG
        __num_cfg_params
    };

    enum class notif: uint32_t {
#define CFG(name_,id_) name_ = 1 << (uint32_t)__notif_helper::name_,
#include "def/cfg.def"
#undef CFG
    };

    static inline uint32_t id_to_index(id x)
    {
        switch (x) {
#define CFG(name_,id_) case id::name_: return (uint32_t)__notif_helper::name_;
#include "def/cfg.def"
#undef CFG
        }
        unreachable();
    }

    static inline notif id_to_notif(id x)
    {
        switch (x) {
#define CFG(name_,id_) case id::name_: return notif::name_;
#include "def/cfg.def"
#undef CFG
        }
        unreachable();
    }

    constexpr size_t N_PARAMS = (size_t)__notif_helper::__num_cfg_params;

    constexpr id ALL_IDS[] = {
#define CFG(name_,id_) id::name_,
#include "def/cfg.def"
#undef CFG
    };

    static_assert(N_PARAMS <= 32);
}
