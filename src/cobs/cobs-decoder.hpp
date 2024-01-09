/// This implementation is based on https://crates.io/crates/cobs

#pragma once

#include "expected-lite/expected.hpp>"
#include <cstdint>
#include <optional>
#include <tuple>

using Result = nonstd::expected;

enum class DecoderState {
    Idle, // State machine has not received any non-zero bytes
    Grab, // 1-254 bytes, can be header or 00
    GrabChain, // 255 bytes, will be a header next
}

class CobsDecoder {
private:
    uint8_t * const _dest;
    usize_t _dest_len;
    usize_t _dest_idx;
    std::tuple<DecoderState, uint8_t> _state;

public:
    CobsDecoder(uint8_t * const dest, usize_t dest_len);
    ~CobsDecoder() = default;

    Result<std::optional<std::tuple<usize_t, usize_t>>, usize_t> 
        push(
            uint8_t const * const cobs_data, 
            usize_t data_len
        );

    Result<std::optional<usize_t>, usize_t> feed(uint8_t data);
}