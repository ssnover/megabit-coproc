#include "cobs-decoder.hpp"

namespace {

enum class DecodeResultKind {
    NoData,
    DataComplete,
    DataContinue
};
using DecodeResult = std::tuple<DecodeResultKind, uint8_t>;

nonstd::expected<DecodeResult, uint8_t> _feed(DecoderState& state, uint8_t& state_data, uint8_t incoming_data) {
    if (state == DecoderState::Idle && incoming_data == 0x00) {
        state = DecoderState::Idle;
        state_data = 0x00;
        return {{DecodeResultKind::NoData, 0x00}};
    } else if (state == DecoderState::Idle && incoming_data == 0xFF) {
        state = DecoderState::GrabChain;
        state_data = 0xFE;
        return {{DecodeResultKind::NoData, 0x00}};
    } else if (state == DecoderState::Idle) {
        state = DecoderState::Grab;
        state_data = incoming_data - 1;
        return {{DecodeResultKind::NoData, 0x00}};
    } else if (state == DecoderState::Grab && state_data == 0x00 && incoming_data == 0x00) {
        state = DecoderState::Idle;
        state_data = 0x00;
        return {{DecodeResultKind::DataComplete, 0x00}};
    } else if (state == DecoderState::Grab && state_data == 0x00 && incoming_data == 0xFF) {
        state = DecoderState::GrabChain;
        state_data = 0xFE;
        return {{DecodeResultKind::DataContinue, 0x00}};
    } else if (state == DecoderState::Grab && state_data == 0x00) {
        state = DecoderState::Grab;
        state_data = incoming_data - 1;
        return {{DecodeResultKind::DataContinue, 0x00}};
    } else if (state == DecoderState::Grab && incoming_data == 0x00) {
        state = DecoderState::Idle;
        state_data = 0x00;
        return nonstd::make_unexpected(0x00);
    } else if (state == DecoderState::Grab) {
        state = DecoderState::Grab;
        state_data -= 1;
        return {{DecodeResultKind::DataContinue, incoming_data}};
    } else if (state == DecoderState::GrabChain && state_data == 0x00 && incoming_data == 0x00) {
        state = DecoderState::Idle;
        state_data = 0x00;
        return {{DecodeResultKind::DataComplete, 0x00}};
    } else if (state == DecoderState::GrabChain && state_data == 0x00 && incoming_data == 0xFF) {
        state = DecoderState::GrabChain;
        state_data = 0xFE;
        return {{DecodeResultKind::NoData, 0x00}};
    } else if (state == DecoderState::GrabChain && state_data == 0x00) {
        state = DecoderState::Grab;
        state_data = incoming_data - 1;
        return {{DecodeResultKind::NoData, 0x00}};
    } else if (state == DecoderState::GrabChain && incoming_data == 0x00) {
        state = DecoderState::Idle;
        state_data = 0x00;
        return nonstd::make_unexpected(0x00);
    } else if (state == DecoderState::GrabChain) {
        state = DecoderState::GrabChain;
        state_data -= 1;
        return {{DecodeResultKind::DataContinue, incoming_data}};
    }

    // Unreachable, but gotta make the compiler happy
    return nonstd::make_unexpected(0xFF);
}

} // anonymous

CobsDecoder::CobsDecoder(uint8_t * const dest, usize dest_len)
    : _dest(dest),
      _dest_len(dest_len),
      _dest_idx(0),
      _state(DecoderState::Idle),
      _state_data(0x00)
{
}

nonstd::expected<std::optional<std::tuple<usize, usize>>, usize>
    CobsDecoder::push(uint8_t const * const cobs_data, usize data_len) {
    for (auto i = 0u; i < data_len; ++i) {
        auto res = this->feed(cobs_data[i]);
        if (res.has_value()) {
            if (res.value().has_value()) {
                auto decoded_bytes_count = res.value().value();
                return {{{decoded_bytes_count, i + 1}}};
            }
        } else {
            return nonstd::make_unexpected(res.error());
        }
    }

    return {std::nullopt};
}

nonstd::expected<std::optional<usize>, usize> CobsDecoder::feed(uint8_t data) {
    auto res = _feed(this->_state, this->_state_data, data);
    if (res.has_value()) {
        auto decode_res = std::get<0>(res.value());
        if (decode_res == DecodeResultKind::NoData) {
            return {std::nullopt};
        } else if (decode_res == DecodeResultKind::DataContinue) {
            auto n = std::get<1>(res.value());
            if (_dest_idx >= _dest_len) {
                return nonstd::make_unexpected(_dest_idx);
            }
            _dest[_dest_idx] = n;
            _dest_idx += 1;
            return {std::nullopt};
        } else {
            return {{_dest_idx}};
        }
    }

    return nonstd::make_unexpected(res.error());
}