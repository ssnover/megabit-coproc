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
    u8 * const _dest;
    usize _dest_len;
    usize _dest_idx;
    DecoderState _state;
    u8 _state_data;

public:
    CobsDecoder(u8 * const dest, usize dest_len);
    ~CobsDecoder() = default;

    nonstd::expected<std::optional<std::tuple<usize, usize>>, usize> 
        push(
            u8 const * const cobs_data, 
            usize data_len
        );

    nonstd::expected<std::optional<usize>, usize> feed(u8 data);
};