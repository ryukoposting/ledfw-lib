#pragma once

#include "prelude.hh"

namespace rdm {
    enum class cc: uint8_t {
        get            = 0x10,
        get_resp       = 0x11,
        set            = 0x20,
        set_resp       = 0x21,
        discovery      = 0x30,
        discovery_resp = 0x31,
    };

    enum class pid: uint16_t {
        disc_unique_branch          = 0x0001,
        disc_mute                   = 0x0002,
        disc_unmute                 = 0x0003,
        supported_parameters        = 0x0050,
        parameter_description       = 0x0051,
        device_info                 = 0x0060,
        software_version_label      = 0x00c0,
        dmx_personality             = 0x00e0,
        dmx_personality_description = 0x00e1,
        dmx_start_address           = 0x00f0,
        dmx_slot_info               = 0x0120,
        dmx_slot_description        = 0x0121,
        dmx_default_slot_value      = 0x0122,
        identify_device             = 0x1000,
    };

    enum class resp_type: uint8_t {
        ack          = 0x00,
        ack_overflow = 0x01,
        ack_timer    = 0x02,
        nack_reason  = 0x03,
    };

    enum class nack_reason: uint16_t {
        unknown_pid             = 0x0000,
        format_error            = 0x0001,
        hardware_fault          = 0x0002,
        proxy_reject            = 0x0003,
        write_protect           = 0x0004,
        unsupported_cc          = 0x0005,
        data_out_of_range       = 0x0006,
        buffer_full             = 0x0007,
        packet_size_unsupported = 0x0008,
        sub_device_out_of_range = 0x0009,
        proxy_buffer_full       = 0x000a,
    };

    /* contains an RDM packet with the following parts stripped:
     *      - start code
     *      - sub-start code
     *      - message length
     *      - checksum
     */
    struct packet {
        static constexpr size_t max_raw_length = 252;

        constexpr packet(uint8_t *raw):
            raw(raw)
        {}

        inline uint8_t const* dest_uid()   { return &raw[0]; }
        inline uint8_t const* source_uid() { return &raw[6]; }
        inline uint8_t tn()                { return raw[12]; }
        inline uint8_t port_id()           { return raw[13]; }
        inline uint8_t resp_type()         { return raw[13]; }
        inline uint8_t msg_count()         { return raw[14]; }
        inline uint16_t sub_device()       { return uint16_big_decode(&raw[15]); }
        inline cc cc()                     { return (rdm::cc)raw[17]; }
        inline pid pid()                   { return (rdm::pid)uint16_big_decode(&raw[18]); }
        inline uint8_t data_len()          { return raw[20]; }

        inline uint8_t *data()             { return &raw[21]; }

        inline void set_dest_uid(uint8_t const value[6])   { memcpy(&raw[0], value, 6); }
        inline void set_source_uid(uint8_t const value[6]) { memcpy(&raw[6], value, 6); }
        inline void set_tn(uint8_t value)                  { raw[12] = value; }
        inline void set_port_id(uint8_t value)             { raw[13] = value; }
        inline void set_resp_type(uint8_t value)           { raw[13] = value; }
        inline void set_msg_count(uint8_t value)           { raw[14] = value; }
        inline void set_sub_device(uint16_t value)         { uint16_big_encode(value, &raw[15]); }
        inline void set_cc(rdm::cc value)                  { raw[17] = (uint8_t)value; }
        inline void set_pid(rdm::pid value)                { uint16_big_encode((uint16_t)value, &raw[18]); }
        inline void set_data_len(uint8_t value)            { raw[20] = value; }

        uint8_t *raw;
    };
}
