#ifndef MARC_DICTIONARY_LZ77_VALIDATOR_HPP
#define MARC_DICTIONARY_LZ77_VALIDATOR_HPP

#include "dictionary/lz77_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

enum class Lz77ValidationError : std::uint8_t {
    none,
    invalid_parameters,
    truncated_token,
    token_error,
    premature_end,
    trailing_tokens,
    limit_exceeded,
    arithmetic_overflow,
};

struct Lz77ValidationResult {
    std::size_t token_count{};
    std::size_t token_index{};
    std::uint64_t output_size{};
    Lz77FormatError format_error{Lz77FormatError::none};
    Lz77ValidationError error{Lz77ValidationError::none};
};

[[nodiscard]] Lz77ValidationResult validate_lz77_token_stream(
    std::span<const std::byte> input, const Lz77Parameters& parameters,
    std::uint64_t declared_frame_size,
    const core::DecoderLimits& limits) noexcept;

} // namespace marc::dictionary::internal

#endif
