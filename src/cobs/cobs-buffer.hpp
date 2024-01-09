#pragma once

#include "cobs-decoder.hpp"
#include <algorithm>
#include <array>
#include <cstdint>

enum class DecodeError {
    UnknownDecodeErr = -1,
    MsgIncomplete = -2,
    NoBytes = -3,
}

/// Implements a ring buffer of size N for COBS-encoded data and supports reading
/// individually decoded packets.
template <std::size_t N>
class CobsBuffer {
private:
    std::array<uint8_t, N> _inner;
    usize_t _write_idx;
    usize_t _read_idx;

public:
    CobsBuffer() = default;
    ~CobsBuffer() = default;

    usize_t available_to_read() {
        if (_read_idx == _write_idx) {
            return 0;
        } else if (_read_idx > _write_idx) {
            return N - _read_idx + _write_idx;
        } else {
            return _write_idx - _read_idx;
        }
    }

    /// Reads COBS-encoded bytes up to the lesser of the available amount or `output_len`.
    usize_t read_bytes(uint8_t * const output_buffer, size_t output_len) {
        if (_read_idx == _write_idx) {
            return 0;
        } else if (_read_idx > _write_idx) {
            usize_t bytes_read = 0;
            for (auto i = _read_idx; i < std::min(N, _read_idx + output_len); ++i) {
                output_buffer[bytes_read] = _inner[i];
                bytes_read += 1;
                _read_idx += 1;
            }
            if _read_idx == N {
                _read_idx = 0;
            }
            if bytes_read < output_len {
                for (auto i = 0u; i < std::min(_write_idx, output_len - bytes_read); ++i) {
                    output_buffer[bytes_read] = _inner[i];
                    bytes_read += 1;
                    _read_idx += 1;
                }
            }
            return bytes_read;
        } else {
            usize_t bytes_read = 0;
            for (auto i = _read_idx; i < std::min(_write_idx, _read_idx + output_len); ++i) {
                output_buffer[bytes_read] = _inner[i];
                bytes_read += 1;
                _read_idx += 1;
            }
            return bytes_read;
        }
    }

    /// Decodes a single COBS-encoded section of the buffer and returns the resulting decoded
    /// packet as bytes in the output parameter `buf`. Returns the length of the decoded bytes.
    /// If an error occurs, the error is returned instead represented by `DecodeError`.
    size_t read_packet(uint8_t * const buf, usize_t buf_len) {
        size_t ret_val = 0;
        CobsDecoder decoder{buf, buf_len};
        if (_read_idx == _write_idx) {
            ret_val = static_cast<size_t>(DecodeError::NoBytes);
        } else if (_read_idx > _write_idx) {
            auto res = decoder.push(_inner.data() + _read_idx, N - _read_idx);
            if (res.has_value()) {
                auto o_value = res.value();
                if (o_value.has_value()) {
                    auto n = std::get<0>(o_value.value());
                    auto m = std::get<1>(o_value.value());
                    _read_idx += m;
                    ret_val = n;
                } else {
                    auto res2 = decoder.push(_inner.data(), _write_idx);
                    if (res2.has_value()) {
                        auto o_value2 = res2.value();
                        if (o_value2.has_value()) {
                            auto n = std::get<0>(o_value2.value());
                            auto m = std::get<1>(o_value2.value());
                            _read_idx = m;
                            ret_val = n;
                        } else {
                            ret_val = static_cast<size_t>(DecodeError::MsgIncomplete);
                        }
                    } else {
                        auto n_bytes = res2.error();
                        _read_idx += n_bytes;
                        if (_read_idx >= N) {
                            _read_idx -= N;
                        }
                        ret_val = static_cast<size_t>(DecodeError::UnknownDecodeErr);
                    }
                }
            } else {
                auto n_bytes = res.error();
                _read_idx += n_bytes;
                if (_read_idx >= N) {
                    _read_idx -= N;
                }
                ret_val = static_cast<size_t>(DecodeError::UnknownDecodeErr);
            }
        }

        return ret_val;
    }

    usize_t write_bytes(uint8_t const * const buf, usize_t len) {
        // TODO
        return 0;
    }
}