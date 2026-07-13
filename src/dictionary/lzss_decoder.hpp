#ifndef MARC_DICTIONARY_LZSS_DECODER_HPP
#define MARC_DICTIONARY_LZSS_DECODER_HPP

#include "dictionary/lzss_validator.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

enum class LzssDecodeError : std::uint8_t {
    none,
    invalid_token_stream,
    output_too_small,
    output_size_unsupported,
    internal_error,
};

struct LzssDecodeResult {
    std::size_t output_size{};
    std::size_t token_index{};
    std::size_t input_offset{};
    LzssValidationError validation_error{LzssValidationError::none};
    LzssFormatError format_error{LzssFormatError::none};
    LzssDecodeError error{LzssDecodeError::none};
};

[[nodiscard]] LzssDecodeResult decode_lzss_token_stream(
    std::span<const std::byte> input, const LzssParameters& parameters,
    std::uint64_t declared_frame_size, const core::DecoderLimits& limits,
    std::span<std::byte> output) noexcept;

} // namespace marc::dictionary::internal

#endif
