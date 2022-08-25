#pragma once

#include "prelude.hh"
#include "util.hh"
#include "color.hh"

namespace led {
    struct transcode {
        constexpr transcode(buffer &buf):
            output(buf)
        {}

        inline void clear()
        {
            output.reset();
        }

        inline uint8_t *ptr()
        {
            return output.ptr();
        }

        inline size_t len()
        {
            return output.len();
        }

        virtual ret_code_t write_bus_reset() = 0;

        virtual ret_code_t write(color::rgb &value) = 0;

    protected:
        buffer output;
    };
}
