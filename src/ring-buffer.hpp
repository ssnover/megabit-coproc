#pragma once

#include "types.hpp"
#include <zephyr/sys/ring_buffer.h>
#include <array>
#include <cstdint>

template <usize N>
class RingBuffer {
private:
    std::array<uint8_t, N> inner_buffer;
    struct ring_buf inner;

public:

    explicit RingBuffer() 
        : inner_buffer(),
          inner()
    {
        ring_buf_init(&inner, inner_buffer.size(), inner_buffer.data());
    }
    ~RingBuffer() = default;

    usize read(uint8_t * const output_buffer, usize buffer_len) {
        return ring_buf_get(&inner, output_buffer, buffer_len);
    }

    usize write(uint8_t * const input_buffer, usize bytes_to_write) {
        return ring_buf_put(&inner, input_buffer, bytes_to_write);
    }

    usize space_in_buffer() {
        return ring_buf_space_get(&inner);
    }

    bool empty() {
        return ring_buf_space_get(&inner) == N;
    }
};