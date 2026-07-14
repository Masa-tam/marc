#ifndef MARC_DICTIONARY_LZW_DECODER_HPP
#define MARC_DICTIONARY_LZW_DECODER_HPP

#include "dictionary/lzw_validator.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

enum class LzwDecodeError : std::uint8_t {
    none,
    invalid_code_stream,
    output_too_small,
    output_size_unsupported,
    internal_error,
};

struct LzwDecodeResult {
    std::size_t output_size{};
    std::size_t code_index{};
    std::size_t input_offset{};
    std::uint64_t input_bit_offset{};
    LzwValidationError validation_error{LzwValidationError::none};
    LzwFormatError format_error{LzwFormatError::none};
    LzwDecodeError error{LzwDecodeError::none};
};

[[nodiscard]] LzwDecodeResult decode_lzw_code_stream(
    std::span<const std::byte> input, const LzwParameters& parameters,
    std::uint64_t declared_frame_size, const core::DecoderLimits& limits,
    std::span<LzwPhraseEntry> phrase_workspace,
    std::span<std::byte> output) noexcept;

} // namespace marc::dictionary::internal

#endif
