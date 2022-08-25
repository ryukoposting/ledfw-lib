#pragma once

#include "prelude.hh"

#if defined(NRF51)
#define PHYSICAL_PAGE_SIZE_BYTES   (1024)
#else
#define PHYSICAL_PAGE_SIZE_BYTES   (4096)
#endif

#define USERCODE_START_ADDR 0x2003F000
#define USERCODE_SIZE 0x1000
#define USERCODE_BUFFER ((uint32_t*)(USERCODE_START_ADDR))
#define USERCODE_MAGIC   0x00041198
#define USERCODE_VERSION_MIN 0x00010000
#define USERCODE_VERSION_MAX 0x0001ffff

#define USERCODE_ARCH_CPU           4
#define USERCODE_ARCH_FLAGS_MASK    0xfffe
#define USERCODE_ARCH_FLAGS_VALUE   0x0000
#define USERCODE_DEFAULT_APP_FLAGS  0x0001

#define WRITE_TEMPBUF_MAX_LEN 128

#ifdef __cplusplus
extern "C" {
#endif

packed_struct led_chan {
    uint8_t *buffer;
    uint8_t *dmx_vals;
    uint8_t id;
    uint8_t color_mode;
    uint16_t refresh_rate;
    uint16_t n_leds;
    uint16_t dmx_vals_len;
};

#ifdef __cplusplus
}
#endif

namespace userapp {
    using init_func_t = void (*)(led_chan*);
    using refresh_func_t = void (*)(led_chan*);
    using with_cb_t = ret_code_t (*)(init_func_t, refresh_func_t, void*);

    constexpr uint32_t data_section_size = USERCODE_SIZE / 2;
    constexpr uint32_t code_section_size = USERCODE_SIZE / 2;

    constexpr uint32_t data_ram_addr = USERCODE_START_ADDR;
    constexpr uint32_t code_ram_addr = data_ram_addr + data_section_size;

    constexpr uint32_t npages = (data_section_size + code_section_size) / PHYSICAL_PAGE_SIZE_BYTES;

    static_assert((data_section_size + code_section_size) % PHYSICAL_PAGE_SIZE_BYTES == 0);

    enum class app_state: uint8_t {
        uninitialized = 0,
        loading_user_app = 1,
        user_app_loaded = 2,
    };

    enum class storage_state: uint8_t {
        uninitialized = 0,
        empty = 1,
        storing_user_app = 2,
        user_app_stored = 3,
    };
}
