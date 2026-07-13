#ifndef MARC_DICTIONARY_LZ77_DECODER_HPP
#define MARC_DICTIONARY_LZ77_DECODER_HPP

#include "dictionary/lz77_validator.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

enum class Lz77DecodeError : std::uint8_t {
    none,
    invalid_token_stream,
    output_too_small,
    output_size_unsupported,
    internal_error,
};

struct Lz77DecodeResult {
    std::size_t output_size{};
    std::size_t token_index{};
    Lz77ValidationError validation_error{Lz77ValidationError::none};
    Lz77FormatError format_error{Lz77FormatError::none};
    Lz77DecodeError error{Lz77DecodeError::none};
};

[[nodiscard]] Lz77DecodeResult decode_lz77_token_stream(
    std::span<const std::byte> input, const Lz77Parameters& parameters,
    std::uint64_t declared_frame_size, const core::DecoderLimits& limits,
    std::span<std::byte> output) noexcept;

} // namespace marc::dictionary::internal

#endif
