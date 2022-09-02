#pragma once

#include "prelude.hh"
#include "cfg/ids.hh"
#include "fds.h"

namespace cfg {
    using subscription_t = void (*)(void *callback, void const *data, size_t length);

    struct ibackend {
        virtual ret_code_t read(id record_id, void *data, size_t length) = 0;
        virtual ret_code_t write(id record_id, void const *data, size_t length) = 0;
        virtual ret_code_t subscribe(id record_id, void *context, subscription_t callback) = 0;
    };

    struct flash_backend: ibackend {
        ret_code_t read(id record_id, void *data, size_t length) override;
        ret_code_t write(id record_id, void const *data, size_t length) override;
        ret_code_t subscribe(id record_id, void *context, subscription_t callback) override;
    };
}
