/// This implementation is based on https://crates.io/crates/cobs

#pragma once

#include "expected-lite/expected.hpp"
#include "types.hpp"
#include <cstdint>
#include <optional>
#include <tuple>

enum class DecoderState {
    Idle, // State machine has not received any non-zero bytes
    Grab, // 1-254 bytes, can be header or 00
    GrabChain, // 255 bytes, will be a header next
};

class CobsDecoder {
private:
    uint8_t * const _dest;
    usize _dest_len;
    usize _dest_idx;
    DecoderState _state;
    uint8_t _state_data;

public:
    CobsDecoder(uint8_t * const dest, usize dest_len);
    ~CobsDecoder() = default;

    nonstd::expected<std::optional<std::tuple<usize, usize>>, usize> 
        push(
            uint8_t const * const cobs_data, 
            usize data_len
        );

    nonstd::expected<std::optional<usize>, usize> feed(uint8_t data);
};