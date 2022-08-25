#include "prelude.hh"
#include "periph/spi.hh"

using namespace spi;

#define ZERO 0b11100000     /* T0H=375ns   T1L=605ns */
#define ONE  0b11111000     /* T1H=625ns   T1L=375ns */

ret_code_t transcode_8mhz::write_bus_reset()
{
    return output.fill(0, transcode_8mhz::bytes_per_reset);
}

ret_code_t transcode_8mhz::write(color::rgb &value)
{
    uint8_t buf[24] = {};

    buf[0] = (value.green & (1 << 7)) ? ONE : ZERO;
    buf[1] = (value.green & (1 << 6)) ? ONE : ZERO;
    buf[2] = (value.green & (1 << 5)) ? ONE : ZERO;
    buf[3] = (value.green & (1 << 4)) ? ONE : ZERO;
    buf[4] = (value.green & (1 << 3)) ? ONE : ZERO;
    buf[5] = (value.green & (1 << 2)) ? ONE : ZERO;
    buf[6] = (value.green & (1 << 1)) ? ONE : ZERO;
    buf[7] = (value.green & (1 << 0)) ? ONE : ZERO;

    buf[8] =  (value.red & (1 << 7)) ? ONE : ZERO;
    buf[9] =  (value.red & (1 << 6)) ? ONE : ZERO;
    buf[10] = (value.red & (1 << 5)) ? ONE : ZERO;
    buf[11] = (value.red & (1 << 4)) ? ONE : ZERO;
    buf[12] = (value.red & (1 << 3)) ? ONE : ZERO;
    buf[13] = (value.red & (1 << 2)) ? ONE : ZERO;
    buf[14] = (value.red & (1 << 1)) ? ONE : ZERO;
    buf[15] = (value.red & (1 << 0)) ? ONE : ZERO;

    buf[16] = (value.blue & (1 << 7)) ? ONE : ZERO;
    buf[17] = (value.blue & (1 << 6)) ? ONE : ZERO;
    buf[18] = (value.blue & (1 << 5)) ? ONE : ZERO;
    buf[19] = (value.blue & (1 << 4)) ? ONE : ZERO;
    buf[20] = (value.blue & (1 << 3)) ? ONE : ZERO;
    buf[21] = (value.blue & (1 << 2)) ? ONE : ZERO;
    buf[22] = (value.blue & (1 << 1)) ? ONE : ZERO;
    buf[23] = (value.blue & (1 << 0)) ? ONE : ZERO;

    return output.write(buf, sizeof(buf));
}
