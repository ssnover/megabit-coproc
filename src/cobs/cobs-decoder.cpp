#include "cobs-decoder.hpp"

CobsDecoder::CobsDecoder(uint8_t * const dest, usize_t dest_len)
    : _dest(dest),
      _dest_len(dest_len),
      _dest_idx(0),
      _state(DecoderState::Idle, 0x00)
{
}

Result<std::optional<std::tuple<usize_t, usize_t>>, usize_t>
    CobsDecoder::push(uint8_t const * const cobs_data, usize_t data_len) {
    for (auto i = 0u; i < data_len; ++i) {
        auto res = this->feed(cobs_data[i]);
        // todo
    }

    return nonstd::make_expected(std::nullopt);
}

Result<std::optional<usize_t>, usize_t> CobsDecoder::feed(uint8_t data) {
    
}