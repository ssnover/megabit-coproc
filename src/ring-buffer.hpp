#pragma once

#include <zephyr/sys/ring_buffer.h>
#include <array>
#include <cstdint>

template <typename T, std::size_t N>
class RingBuffer {
private:
    std::array<T, N> inner_buffer;
    struct ring_buf inner;

public:

    explicit RingBuffer() 
        : inner_buffer(),
          inner()
    {
        ring_buf_init(&inner, inner_buffer.size(), inner_buffer.data());
    }
    ~RingBuffer() = default;

    std::size_t read(T * const output_buffer, std::size_t buffer_len) {
        return ring_buf_get(&inner, output_buffer, buffer_len);
    }

    std::size_t write(T * const input_buffer, std::size_t bytes_to_write) {
        return ring_buf_put(&inner, input_buffer, bytes_to_write);
    }

    std::size_t space_in_buffer() {
        return ring_buf_space_get(&inner);
    }

    bool empty() {
        return ring_buf_space_get(&inner) == N;
    }
};