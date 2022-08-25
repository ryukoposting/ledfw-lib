#pragma once

#include "prelude.hh"
#include "led/transport.hh"
#include "led/transcode.hh"
#include "sdk_config.h"

namespace spi {
    using pin = uint8_t;

    enum id: uintptr_t {
#if NRFX_SPIM0_ENABLED
        SPI0,
#endif
#if NRFX_SPIM1_ENABLED
        SPI1,
#endif
#if NRFX_SPIM2_ENABLED
        SPI2,
#endif
#if NRFX_SPIM3_ENABLED
        SPI3,
#endif
        MAX_SPIM_INST
    };

    enum spi_frequency: uint32_t {
        // FREQ_4M,
        FREQ_8M,
        // FREQ_16M,
        // FREQ_32M,
    };

    struct spi_init {
        spi_frequency frequency;
        pin mosi;
        pin sck;
    };

    struct transport: led::transport {
        transport(id id);

        constexpr transport(): inst_id((id)0xfffff) {}

        ret_code_t init(spi_init const &init);

        ret_code_t set_buffer(uint8_t *buffer, size_t length) override;

        ret_code_t send() override;

        ret_code_t on_send_complete(void *context, send_complete_t callback) override;

        bool is_ready() override;

        inline id instance_id()
        {
            return inst_id;
        }

    protected:

        id inst_id;
    };

    /**
     * @brief Transcode LED data for transmission to WS2812 over 8MHZ SPI.
     */
    struct transcode_8mhz: led::transcode {
        constexpr transcode_8mhz(buffer &buf):
            led::transcode(buf)
        {};

        constexpr static size_t bytes_per_led = 24;

        constexpr static size_t bytes_per_reset = 300;

        ret_code_t write_bus_reset() override;

        ret_code_t write(color::rgb &value) override;
    };

    /**
     * @brief Alternate transcoder for WS2812 over 8MHZ SPI.
     * 
     * Compared to `transcode_8mhz`, this transcoder is slower and less
     * memory-efficient. However, this transcoder is also more accurate to the
     * timings specified in the WS2812 datasheet.
     * 
     * If we are having problems with data corruption on long LED strips, try
     * switching to this transcoder.
     */
    struct transcode_8mhz_alt: led::transcode {
        constexpr transcode_8mhz_alt(buffer &buf):
            led::transcode(buf)
        {};

        constexpr static size_t bytes_per_led = 30;

        constexpr static size_t bytes_per_reset = 380;

        ret_code_t write_bus_reset() override;

        ret_code_t write(color::rgb &value) override;
    };
}
