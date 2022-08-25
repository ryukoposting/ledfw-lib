#pragma once

#include "prelude.hh"
#include "app_util.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline uint8_t uint64_encode(uint64_t value, uint8_t *output)
{
    assert(output != nullptr);

    output[0] = (value >> 0) & 0xff;
    output[1] = (value >> 8) & 0xff;
    output[2] = (value >> 16) & 0xff;
    output[3] = (value >> 24) & 0xff;
    output[4] = (value >> 32) & 0xff;
    output[5] = (value >> 40) & 0xff;
    output[6] = (value >> 48) & 0xff;
    output[7] = (value >> 56) & 0xff;

    return 8;
}

#ifndef __STDC_LIB_EXT1__
static inline size_t strnlen(char const *p, size_t maxlen)
{
    size_t len = 0;
    while (*p && len < maxlen) {
        ++p;
        ++len;
    }
    return len;
}
#endif

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
struct buffer {
    constexpr buffer(void *p, size_t length):
        basep((uint8_t*)p),
        pos(0),
        length(length)
    {};

    constexpr buffer(void *p, size_t pos, size_t length):
        basep((uint8_t*)p),
        pos(pos),
        length(length)
    {};

    inline size_t len()
    {
        return pos;
    }

    inline size_t max_len()
    {
        return length;
    }

    inline uint8_t *ptr()
    {
        return basep;
    }

    inline void reset()
    {
        pos = 0;
    }

    ret_code_t write(void const *bytes, size_t nbytes);

    ret_code_t fill(uint8_t value, size_t n);

protected:
    uint8_t *basep;
    size_t pos;
    size_t length;
};

template<size_t N, typename T = uint8_t>
struct circbuf {
    constexpr circbuf():
        start(0),
        length(0)
    {}

    constexpr size_t capacity() const { return N; }
    inline size_t len() const { return length; }
    inline bool is_empty() const { return length == 0; }
    inline bool is_full() const { return length == N; }
    inline size_t remaining() const { return capacity() - len(); }

    ret_code_t push(T const *data, size_t n)
    {
        if (n == 0)
            return NRF_SUCCESS;
        if (n > remaining())
            return NRF_ERROR_NO_MEM;
        if (!data)
            return NRF_ERROR_NULL;

        auto const end = (start + len()) % capacity();
        auto const copy = std::min(n, capacity() - end);

        write(data, copy, end);

        if (copy < n) {
            write(&data[copy], n - copy, 0);
        }

        length += n;

        return NRF_SUCCESS;
    }

    inline ret_code_t push(T const *data)
    {
        return push(data, 1);
    }

    ret_code_t pop(T *data, size_t n)
    {
        if (n == 0)
            return NRF_SUCCESS;
        if (n > len())
            return NRF_ERROR_INVALID_LENGTH;
        if (!data)
            return NRF_ERROR_NULL;

        auto const end = (start + len()) % capacity();
        auto const copy = std::min(n, capacity() - start);

        read(data, copy, start);

        if (copy < n) {
            read(&data[copy], n - copy, 0);
        }

        length -= n;
        start = (start + n) % capacity();

        return NRF_SUCCESS;
    }

    inline ret_code_t pop(T *data)
    {
        return pop(data, 1);
    }

    inline ret_code_t pop_some(T *data, size_t n, size_t *npop)
    {
        ret_code_t ret;

        auto const n2 = std::min(n, len());

        ret = pop(data, n2);

        if (npop && ret == NRF_SUCCESS)
            *npop = n2;

        return ret;
    }

    ret_code_t peek(T *data) const
    {
        if (!data)
            return NRF_ERROR_NULL;
        if (len() == 0)
            return NRF_ERROR_INVALID_LENGTH;

        *data = raw[start];

        return NRF_SUCCESS;
    }

    inline ret_code_t discard(size_t n)
    {
        if (n == 0)
            return NRF_SUCCESS;
        if (n > len())
            return NRF_ERROR_INVALID_LENGTH;

        length -= n;
        start = (start + n) % capacity();

        return NRF_SUCCESS;
    }

protected:
    size_t start;
    size_t length;
    T raw[N];

private:
    inline void write(T const *data, size_t n, size_t offset)
    {
        assert(offset + n < capacity());
        for (size_t i = 0; i < n; ++i, ++offset) {
            raw[offset] = data[i];
        }
    }

    inline void read(T *data, size_t n, size_t offset)
    {
        assert(offset + n < capacity());
        for (size_t i = 0; i < n; ++i, ++offset) {
            data[i] = raw[offset];
        }
    }
};


template<size_t NBytes, size_t NFrames>
struct framebuf {
    constexpr framebuf():
        b_buf(),
        f_buf()
    {}

    inline size_t byte_capacity() const { return NBytes; }
    inline size_t frame_capacity() const { return NFrames; }
    inline size_t len() const { return f_buf.len(); }
    inline bool is_empty() const { return len() == 0; }
    inline bool is_full() const { return f_buf.len() == NFrames || b_buf.len() == NBytes; }
    inline size_t bytes_remaining() const { return b_buf.remaining(); }
    inline size_t frames_remaining() const { return f_buf.remaining(); }

    ret_code_t push(uint8_t const *data, size_t nbytes)
    {
        if (nbytes > bytes_remaining() || 1 > frames_remaining())
            return NRF_ERROR_NO_MEM;

        ret_code_t ret;

        ret = b_buf.push(data, nbytes);
        if (ret == NRF_SUCCESS)
            ret = f_buf.push(&nbytes);

        return ret;
    }


    ret_code_t peek(size_t *frame_len) const
    {
        return f_buf.peek(frame_len);
    }

    ret_code_t pop(uint8_t *data, size_t n, size_t *data_len, size_t *frame_len)
    {
        if (is_empty())
            return NRF_ERROR_NO_MEM;
        if (n > 0 && !data)
            return NRF_ERROR_NULL;

        ret_code_t ret;
        size_t nframe = 0;

        ret = f_buf.pop(&nframe);
        if (ret != NRF_SUCCESS)
            return NRF_ERROR_INTERNAL;

        if (frame_len)
            *frame_len = nframe;

        auto const ndata = std::min(n, nframe);

        if (data_len)
            *data_len = ndata;

        ret = b_buf.pop(data, ndata);
        if (ret != NRF_SUCCESS)
            return NRF_ERROR_INTERNAL;

        ret = b_buf.discard(nframe - ndata);
        if (ret != NRF_SUCCESS)
            return NRF_ERROR_INTERNAL;

        return ret;
    }

protected:
    circbuf<NBytes, uint8_t> b_buf;
    circbuf<NFrames, size_t> f_buf;
};

#endif
