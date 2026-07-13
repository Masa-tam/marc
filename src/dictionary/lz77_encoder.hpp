#ifndef MARC_DICTIONARY_LZ77_ENCODER_HPP
#define MARC_DICTIONARY_LZ77_ENCODER_HPP

#include "dictionary/lz77_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

enum class Lz77EncodeError : std::uint8_t {
    none,
    invalid_parameters,
    input_limit_exceeded,
    serialized_limit_exceeded,
    output_too_small,
    arithmetic_overflow,
    internal_error,
};

struct Lz77EncodeResult {
    std::size_t input_size{};
    std::size_t output_size{};
    std::size_t token_count{};
    Lz77FormatError format_error{Lz77FormatError::none};
    Lz77EncodeError error{Lz77EncodeError::none};
};

[[nodiscard]] Lz77EncodeResult plan_lz77_token_stream(
    std::span<const std::byte> input, const Lz77Parameters& parameters,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] Lz77EncodeResult encode_lz77_token_stream(
    std::span<const std::byte> input, const Lz77Parameters& parameters,
    const core::DecoderLimits& limits, std::span<std::byte> output) noexcept;

} // namespace marc::dictionary::internal

#endif
