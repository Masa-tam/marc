#ifndef MARC_DICTIONARY_LZSS_VALIDATOR_HPP
#define MARC_DICTIONARY_LZSS_VALIDATOR_HPP

#include "dictionary/lzss_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

enum class LzssValidationError : std::uint8_t {
    none,
    invalid_parameters,
    truncated_token,
    token_error,
    premature_end,
    trailing_tokens,
    limit_exceeded,
};

struct LzssValidationResult {
    std::size_t token_count{};
    std::size_t token_index{};
    std::size_t input_offset{};
    std::uint64_t output_size{};
    LzssFormatError format_error{LzssFormatError::none};
    LzssValidationError error{LzssValidationError::none};
};

[[nodiscard]] LzssValidationResult validate_lzss_token_stream(
    std::span<const std::byte> input, const LzssParameters& parameters,
    std::uint64_t declared_frame_size,
    const core::DecoderLimits& limits) noexcept;

} // namespace marc::dictionary::internal

#endif
