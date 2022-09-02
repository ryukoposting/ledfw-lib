#pragma once

#include "prelude.hh"
#include "led/renderer.hh"

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
    uint8_t const *dmx_vals;
    uint8_t id;
    uint8_t color_mode;
    uint16_t refresh_rate;
    uint16_t n_leds;
    uint16_t dmx_vals_len;
    uint8_t dmx_personality_idx;

    constexpr led_chan(led::renderer_props const &props):
        buffer(nullptr),
        dmx_vals(props.dmx_vals),
        id(0),
        color_mode(props.render_config.color_mode),
        refresh_rate(props.render_config.refresh_msec),
        n_leds(props.render_config.n_leds),
        dmx_vals_len(props.dmx_config.n_channels),
        dmx_personality_idx(props.dmx_config.personality)
    {}
};

// packed_struct slot_info {
//     uint32_t const name;
//     uint8_t const type;
//     uint16_t const id;
//     uint8_t value;
// };

enum slot_info_type: uint8_t {
    ST_PRIMARY = 0x00,
    ST_SEC_FINE = 0x01,
    ST_SEC_TIMING = 0x02,
    ST_SEC_SPEED = 0x03,
    ST_SEC_CONTROL = 0x04,
    ST_SEC_INDEX = 0x05,
    ST_SEC_ROTATION = 0x06,
    ST_SEC_INDEX_ROTATE = 0x07,
};

enum slot_info_id: uint16_t {
    SD_INTENSITY = 0x0001,
    SD_INTENSITY_MASTER = 0x0002,

    SD_PAN = 0x0101,
    SD_TILT = 0x0101,

    SD_COLOR_WHEEL = 0x0201,
    SD_COLOR_SUB_CYAN = 0x0202,
    SD_COLOR_SUB_YELLOW = 0x0203,
    SD_COLOR_SUB_MAGENTA = 0x0204,
    SD_COLOR_ADD_RED = 0x0205,
    SD_COLOR_ADD_GREEN = 0x0206,
    SD_COLOR_ADD_BLUE = 0x0207,
    SD_COLOR_CORRECTION = 0x0208,
    SD_COLOR_SCROLL = 0x0209,
    SD_COLOR_SEMAPHORE = 0x0210,
    SD_COLOR_ADD_AMBER = 0x0211,
    SD_COLOR_ADD_WHITE = 0x0212,
    SD_COLOR_ADD_WARM_WHITE = 0x0213,
    SD_COLOR_ADD_COOL_WHITE = 0x0214,
    SD_COLOR_SUB_UV = 0x0215,
    SD_COLOR_HUE = 0x0216,
    SD_COLOR_SATURATION = 0x0217,

    SD_STATIC_GOBO_WHEEL = 0x0301,
    SD_ROTO_GOBO_WHEEL = 0x0302,
    SD_PRISM_WHEEL = 0x0303,
    SD_EFFECTS_WHEEL = 0x0304,

    SD_BEAM_SIZE_IRIS = 0x0401,
    SD_EDGE = 0x0402,
    SD_FROST = 0x0403,
    SD_STROBE = 0x0404,
    SD_ZOOM = 0x0405,
    SD_FRAMING_SHUTTER = 0x0406,
    SD_SHUTTER_ROTATE = 0x0407,
    SD_DOUSER = 0x0408,
    SD_BARN_DOOR = 0x0409,

    SD_LAMP_CONTROL = 0x0501,
    SD_FIXTURE_CONTROL = 0x0502,
    SD_FIXTURE_SPEED = 0x0503,
    SD_MACRO = 0x0504,

    SD_UNDEFINED = 0xFFFF,
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
