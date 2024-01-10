#pragma once

#include "cobs-decoder.hpp"
#include "types.hpp"
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
template <usize N>
class CobsBuffer {
private:
    std::array<u8, N> _inner;
    usize _write_idx;
    usize _read_idx;

public:
    CobsBuffer() = default;
    ~CobsBuffer() = default;

    usize available_to_read() {
        if (_read_idx == _write_idx) {
            return 0;
        } else if (_read_idx > _write_idx) {
            return N - _read_idx + _write_idx;
        } else {
            return _write_idx - _read_idx;
        }
    }

    usize available_to_write() {
        if (_read_idx == _write_idx) {
            return N;
        } else if (_read_idx > _write_idx) {
            return _read_idx - _write_idx;
        } else {
            return N - _write_idx + _read_idx; 
        }
    }

    /// Reads COBS-encoded bytes up to the lesser of the available amount or `output_len`.
    usize read_bytes(u8 * const output_buffer, usize output_len) {
        if (_read_idx == _write_idx) {
            return 0;
        } else if (_read_idx > _write_idx) {
            usize bytes_read = 0;
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
            usize bytes_read = 0;
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
    isize read_packet(u8 * const buf, usize buf_len) {
        isize ret_val = 0;
        CobsDecoder decoder{buf, buf_len};
        if (_read_idx == _write_idx) {
            ret_val = static_cast<isize>(DecodeError::NoBytes);
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
                            ret_val = static_cast<isize>(DecodeError::MsgIncomplete);
                        }
                    } else {
                        auto n_bytes = res2.error();
                        _read_idx += n_bytes;
                        if (_read_idx >= N) {
                            _read_idx -= N;
                        }
                        ret_val = static_cast<isize>(DecodeError::UnknownDecodeErr);
                    }
                }
            } else {
                auto n_bytes = res.error();
                _read_idx += n_bytes;
                if (_read_idx >= N) {
                    _read_idx -= N;
                }
                ret_val = static_cast<isize>(DecodeError::UnknownDecodeErr);
            }
        }

        return ret_val;
    }

    /// Writes COBS-encoded bytes into the ring buffer. The buffer will overwrite in
    /// the event that the data is larger than available space. Returns the number of
    /// bytes written.
    usize write_bytes(u8 const * const buf, usize len) {
        usize available = this->available_to_write();
        usize bytes_written = 0;
        if ((N - _write_idx) > len) {
            for (auto i = 0u; i < len; ++i) {
                _inner[_write_idx] = buf[i];
                _write_idx += 1;
                bytes_written += 1;
            }
        } else {
            for (auto i = 0u; i < (N - _write_idx); ++i) {
                _inner[_write_idx] = buf[i];
                _write_idx += 1;
                bytes_written += 1;
            }
            _write_idx = 0;
            for (auto i = bytes_written; i < len; ++i) {
                _inner[_write_idx] = buf[i];
                _write_idx += 1;
                bytes_written += 1;
            }
        }

        if (available < len) {
            _read_idx = _write_idx + 1;
            if (_read_idx >= N) {
                _read_idx = 0;
            }
        }

        return bytes_written;
    }
}