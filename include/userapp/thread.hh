#pragma once

#include "prelude.hh"
#include "userapp/types.hh"
#include "cfg.hh"

#define DMX_EXPLORER_LEN 48

namespace userapp {
    enum class action: uint32_t {
        other,
        erase,
        write,
        begin_write,
        commit,
        run,
        send_state,
        dmx_config,
        dmx_explorer,
    };

    enum class dmx_explorer_cmd: uint8_t {
        get_personality_info = 1,
        get_slot_info = 2,
        get_personality_and_slot_info = 3,
    };

    enum class dmx_explorer_resp: uint8_t {
        personality_info = 1,
        slot_info = 2,
    };

    packed_struct queued_action_other {
        void *context;
        ret_code_t (*func)(void*);
    };

    packed_struct queued_action_write {
        size_t length;
        uint8_t *buffer;
        uint16_t offset;
    };

    packed_struct queued_action_commit {
        uint16_t crc;
    };

    packed_struct queued_action_run {
        uint16_t crc;
    };

    packed_struct queued_action_dmx_config {
        cfg::dmx_config_t config;
    };

    packed_struct queued_action_dmx_explorer {
        dmx_explorer_cmd command;
        uint8_t personality;
        uint8_t slot;
    };

    packed_struct queued_action {
        action type;
        union {
            queued_action_other other;
            queued_action_write write;
            queued_action_commit commit;
            queued_action_run run;
            queued_action_dmx_config dmx_config;
            queued_action_dmx_explorer dmx_explorer;
        };
    };

    ret_code_t init_thread();

    TaskHandle_t thread();

    ret_code_t queue_action(queued_action const *actn, BaseType_t *do_yield);

    inline ret_code_t queue_callback(ret_code_t (*func)(void*), void *context)
    {
        BaseType_t do_yield;
        auto actn = queued_action {
            .type = action::other,
            .other = queued_action_other {
                .context = context,
                .func = func
            }
        };
        return queue_action(&actn, &do_yield);
    }

    inline ret_code_t queue_erase()
    {
        BaseType_t do_yield;
        auto actn = queued_action {
            .type = action::erase
        };
        return queue_action(&actn, &do_yield);
    }

    inline ret_code_t queue_write(uint16_t offset, uint8_t const *buffer, size_t length)
    {
        if (length > WRITE_TEMPBUF_MAX_LEN) {
            return NRF_ERROR_INVALID_LENGTH;
        }

        if (!buffer) {
            return NRF_ERROR_NULL;
        }

        auto mbuffer = pvPortMalloc(length);
        if (mbuffer == nullptr) {
            return NRF_ERROR_NO_MEM;
        }

        BaseType_t do_yield;
        auto actn = queued_action {
            .type = action::write,
            .write = queued_action_write {
                .length = length,
                .buffer = (uint8_t*)mbuffer,
                .offset = offset
            }
        };
        memcpy(mbuffer, buffer, length);
        return queue_action(&actn, &do_yield);
    }

    inline ret_code_t queue_begin_write()
    {
        BaseType_t do_yield;
        auto actn = queued_action {
            .type = action::begin_write
        };
        return queue_action(&actn, &do_yield);
    }

    inline ret_code_t queue_commit(uint16_t crc)
    {
        BaseType_t do_yield;
        auto actn = queued_action {
            .type = action::commit,
            .commit = queued_action_commit {
                .crc = crc
            }
        };
        return queue_action(&actn, &do_yield);
    }

    inline ret_code_t queue_run(uint16_t crc)
    {
        BaseType_t do_yield;
        auto actn = queued_action {
            .type = action::run,
            .run = queued_action_run {
                .crc = crc
            }
        };
        return queue_action(&actn, &do_yield);
    }

    inline ret_code_t queue_send_state()
    {
        BaseType_t do_yield;
        auto actn = queued_action {
            .type = action::send_state
        };
        return queue_action(&actn, &do_yield);
    }

    inline ret_code_t queue_dmx_config(cfg::dmx_config_t config)
    {
        BaseType_t do_yield;
        auto actn = queued_action {
            .type = action::dmx_config,
            .dmx_config = queued_action_dmx_config { config }
        };
        return queue_action(&actn, &do_yield);
    }

    inline ret_code_t queue_dmx_explore(dmx_explorer_cmd command, uint8_t personality, uint8_t slot)
    {
        BaseType_t do_yield;
        auto actn = queued_action {
            .type = action::dmx_explorer,
            .dmx_explorer = queued_action_dmx_explorer { command, personality, slot }
        };
        return queue_action(&actn, &do_yield);
    }
}
