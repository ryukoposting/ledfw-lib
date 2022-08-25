#pragma once

#include "prelude.hh"
#include "cfg/ids.hh"

namespace cfg {
    struct ibackend {
        virtual ret_code_t read(id record_id, void *data, size_t length) = 0;
        virtual ret_code_t write(id record_id, void const *data, size_t length) = 0;
    };

    struct flash_backend: ibackend {
        enum file {
            normal,
            recovery
        };
        ret_code_t read(id record_id, void *data, size_t length) override;
        ret_code_t write(id record_id, void const *data, size_t length) override;
    };
}
