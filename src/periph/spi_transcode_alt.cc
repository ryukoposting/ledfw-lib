#include "prelude.hh"
#include "periph/spi.hh"

using namespace spi;

#define ONE_A   0b11111100U
#define ZERO_A  0b11100000U

#define ONE_B   0b00111111U
#define ZERO_B  0b00111000U

#define ONE_C0  0b00001111U
#define ZERO_C0 0b00001110U

#define ONE_C   0b11000011U
#define ZERO_C  0b00000011U

#define ONE_D   0b11110000U
#define ZERO_D  0b10000000U

ret_code_t transcode_8mhz_alt::write_bus_reset()
{
    return output.fill(0, transcode_8mhz_alt::bytes_per_reset);
}

ret_code_t transcode_8mhz_alt::write(color::rgb &value)
{
    uint8_t buf[30];

    buf[0]  = (value.green & (1 << 7)) ? ONE_A : ZERO_A;
    buf[1]  = (value.green & (1 << 6)) ? ONE_B : ZERO_B;
    buf[2]  = (value.green & (1 << 5)) ? ONE_C0 : ZERO_C0;
    buf[3]  = (value.green & (1 << 5)) ? ONE_C : ZERO_C;
    buf[4]  = (value.green & (1 << 4)) ? ONE_D : ZERO_D;

    buf[5]  = (value.green & (1 << 3)) ? ONE_A : ZERO_A;
    buf[6]  = (value.green & (1 << 2)) ? ONE_B : ZERO_B;
    buf[7]  = (value.green & (1 << 1)) ? ONE_C0 : ZERO_C0;
    buf[8]  = (value.green & (1 << 1)) ? ONE_C : ZERO_C;
    buf[9]  = (value.green & (1 << 0)) ? ONE_D : ZERO_D;

    buf[10] = (value.red & (1 << 7)) ? ONE_A : ZERO_A;
    buf[11] = (value.red & (1 << 6)) ? ONE_B : ZERO_B;
    buf[12] = (value.red & (1 << 5)) ? ONE_C0 : ZERO_C0;
    buf[13] = (value.red & (1 << 5)) ? ONE_C : ZERO_C;
    buf[14] = (value.red & (1 << 4)) ? ONE_D : ZERO_D;

    buf[15] = (value.red & (1 << 3)) ? ONE_A : ZERO_A;
    buf[16] = (value.red & (1 << 2)) ? ONE_B : ZERO_B;
    buf[17] = (value.red & (1 << 1)) ? ONE_C0 : ZERO_C0;
    buf[18] = (value.red & (1 << 1)) ? ONE_C : ZERO_C;
    buf[19] = (value.red & (1 << 0)) ? ONE_D : ZERO_D;

    buf[20] = (value.blue & (1 << 7)) ? ONE_A : ZERO_A;
    buf[21] = (value.blue & (1 << 6)) ? ONE_B : ZERO_B;
    buf[22] = (value.blue & (1 << 5)) ? ONE_C0 : ZERO_C0;
    buf[23] = (value.blue & (1 << 5)) ? ONE_C : ZERO_C;
    buf[24] = (value.blue & (1 << 4)) ? ONE_D : ZERO_D;

    buf[25] = (value.blue & (1 << 3)) ? ONE_A : ZERO_A;
    buf[26] = (value.blue & (1 << 2)) ? ONE_B : ZERO_B;
    buf[27] = (value.blue & (1 << 1)) ? ONE_C0 : ZERO_C0;
    buf[28] = (value.blue & (1 << 1)) ? ONE_C : ZERO_C;
    buf[29] = (value.blue & (1 << 0)) ? ONE_D : ZERO_D;

    return output.write(buf, sizeof(buf));
}
