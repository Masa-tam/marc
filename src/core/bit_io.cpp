#include "core/bit_io.hpp"

namespace marc::core {

BitWriteResult BitWriter::write_bits(const std::uint64_t value,
                                     const std::uint8_t bit_count,
                                     const std::span<std::byte> output) noexcept {
    BitWriteResult result{};
    if (finished_ || bit_count > 64) {
        result.status = BitIoStatus::invalid_argument;
        return result;
    }

    while (result.bits_consumed < bit_count) {
        if (pending_bit_count_ == 8) {
            if (result.bytes_produced == output.size()) {
                result.status = BitIoStatus::need_output;
                return result;
            }
            output[result.bytes_produced++] = static_cast<std::byte>(pending_byte_);
            pending_byte_ = 0;
            pending_bit_count_ = 0;
        }

        const auto bit = static_cast<std::uint8_t>(
            (value >> result.bits_consumed) & UINT64_C(1));
        pending_byte_ |= static_cast<std::uint8_t>(bit << pending_bit_count_);
        ++pending_bit_count_;
        ++result.bits_consumed;
    }
    return result;
}

BitWriteResult BitWriter::finish(const std::span<std::byte> output) noexcept {
    BitWriteResult result{};
    if (finished_) {
        result.status = BitIoStatus::finished;
        return result;
    }
    if (pending_bit_count_ != 0) {
        if (output.empty()) {
            result.status = BitIoStatus::need_output;
            return result;
        }
        output[0] = static_cast<std::byte>(pending_byte_);
        result.bytes_produced = 1;
        pending_byte_ = 0;
        pending_bit_count_ = 0;
    }
    finished_ = true;
    result.status = BitIoStatus::finished;
    return result;
}

BitReadResult BitReader::read_bits(const std::span<const std::byte> input,
                                   const std::uint8_t bit_count) noexcept {
    BitReadResult result{};
    if (bit_count > 64) {
        result.status = BitIoStatus::invalid_argument;
        return result;
    }

    while (result.bits_produced < bit_count) {
        if (buffered_bit_count_ == 0) {
            if (result.bytes_consumed == input.size()) {
                result.status = BitIoStatus::need_input;
                return result;
            }
            buffered_byte_ = std::to_integer<std::uint8_t>(input[result.bytes_consumed++]);
            buffered_bit_count_ = 8;
        }
        const auto bit = static_cast<std::uint64_t>(buffered_byte_ & 1U);
        result.value |= bit << result.bits_produced;
        buffered_byte_ = static_cast<std::uint8_t>(buffered_byte_ >> 1U);
        --buffered_bit_count_;
        ++result.bits_produced;
    }
    return result;
}

BitIoStatus BitReader::align_to_byte(const bool require_zero_padding) noexcept {
    if (require_zero_padding && buffered_byte_ != 0) {
        return BitIoStatus::invalid_padding;
    }
    buffered_byte_ = 0;
    buffered_bit_count_ = 0;
    return BitIoStatus::complete;
}

} // namespace marc::core
