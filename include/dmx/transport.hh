#pragma once

#include "prelude.hh"
#include "message_buffer.h"
#include "task.hh"

#define DMX_MAX_FRAME_SIZE 520
#define DMX_MAX_QUEUED_FRAMES 8

namespace dmx {
    struct transport {
        virtual MessageBufferHandle_t frame_buf() = 0;
        virtual ret_code_t enable() = 0;
        virtual ret_code_t disable() = 0;
    };
}
