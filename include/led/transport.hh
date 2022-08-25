#pragma once

#include "prelude.hh"

namespace led {
    struct transport {
        /**
         * @brief Callback that fires when a transport has finished sending a unit of data.
         * 
         * @return `false` - the transport will keep ownership of the buffer.
         *         `true` - the transport will release ownership of the buffer.
         */
        using send_complete_t = bool (*)(uint8_t *buffer, size_t length, BaseType_t *do_context_switch, void *context);

        /**
         * @brief Set the next buffer to be sent by the transport.
         * 
         * THIS DOES NOT ACTUALLY TRANSMIT DATA! Calling `send` will transmit
         * the buffer set by this function.
         */
        virtual ret_code_t set_buffer(uint8_t *buffer, size_t length) = 0;
        virtual ret_code_t send() = 0;

        virtual ret_code_t on_send_complete(void *context, send_complete_t callback) = 0;
        virtual bool is_ready() = 0;
    };
}
