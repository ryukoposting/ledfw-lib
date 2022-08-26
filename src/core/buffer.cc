#include "util.hh"

ret_code_t buffer::write(void const *bytes, size_t nbytes)
{
    if (nbytes > length - pos) {
        return NRF_ERROR_INVALID_LENGTH;
    }

    n_leading_zeros = std::min(n_leading_zeros, pos);

    memcpy(&basep[pos], bytes, nbytes);
    pos += nbytes;

    return NRF_SUCCESS;
}

ret_code_t buffer::fill(uint8_t value, size_t n)
{
    if (n > length - pos) {
        return NRF_ERROR_INVALID_LENGTH;
    }

    if (value == 0 && pos + n <= n_leading_zeros) {
        return NRF_SUCCESS; /* these bytes are already zero-filled */
    }

    auto const do_set_leading_zero = (pos == 0 && value == 0);

    for (; n > 0; --n) {
        basep[pos++] = value;
    }

    if (do_set_leading_zero) {
        n_leading_zeros = n;
    }

    return NRF_SUCCESS;
}
