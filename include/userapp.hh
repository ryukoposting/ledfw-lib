#pragma once

#include "prelude.hh"
#include "task.hh"
#include "led.hh"
#include "userapp/types.hh"
#include "userapp/desc.hh"
#include "userapp/thread.hh"
#include <tuple>

namespace userapp {
    using with_desc_t = ret_code_t (*)(void *context, desc &desc);

    ret_code_t init();

    /* this must only be called at the beginning of the userapp thread.
     * subsequent loads should be performed using `load_from_tempbuf` */
    ret_code_t load_from_flash();

    ret_code_t load_from_tempbuf();

    ret_code_t with(void *context, with_cb_t func);

    app_state get_app_state();

    storage_state get_storage_state();

    ret_code_t erase();

    ret_code_t clear_tempbuf();

    ret_code_t write_tempbuf(size_t offset, uint8_t const *buffer, size_t length);

    uint16_t crc_tempbuf();

    ret_code_t with_desc(void *context, with_desc_t func);

    template<typename T>
    using with_desc_2_t = ret_code_t (*)(desc&, T&);

    template<typename T>
    inline ret_code_t with_desc_2(T &context, with_desc_2_t<T> func)
    {
        std::tuple<T&, with_desc_2_t<T>> wcontext { context, func };
        return with_desc(&wcontext, [](void *pcontext, desc &desc) -> ret_code_t {
            auto wcontext = static_cast<std::tuple<T&, with_desc_2_t<T>>*>(pcontext);
            auto [context, func] = *wcontext;
            return func(desc, context);
        });
    }

    /* this must only be called from the userapp thread. */
    ret_code_t save_tempbuf_to_flash(uint16_t crc);

    void default_init(led_chan *chan);
    void default_refresh(led_chan *chan);
};
