#include "util.hh"

ret_code_t buffer::write(void const *bytes, size_t nbytes)
{
    if (nbytes > length - pos) {
        return NRF_ERROR_INVALID_LENGTH;
    }

    memcpy(&basep[pos], bytes, nbytes);
    pos += nbytes;

    return NRF_SUCCESS;
}

ret_code_t buffer::fill(uint8_t value, size_t n)
{
    if (n > length - pos) {
        return NRF_ERROR_INVALID_LENGTH;
    }

    for (; n > 0; --n) {
        basep[pos++] = value;
    }

    return NRF_SUCCESS;
}
