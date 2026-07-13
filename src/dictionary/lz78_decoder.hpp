#ifndef MARC_DICTIONARY_LZ78_DECODER_HPP
#define MARC_DICTIONARY_LZ78_DECODER_HPP

#include "dictionary/lz78_validator.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

enum class Lz78DecodeError : std::uint8_t {
    none,
    invalid_token_stream,
    output_too_small,
    output_size_unsupported,
    internal_error,
};

struct Lz78DecodeResult {
    std::size_t output_size{};
    std::size_t token_index{};
    std::size_t input_offset{};
    Lz78ValidationError validation_error{Lz78ValidationError::none};
    Lz78FormatError format_error{Lz78FormatError::none};
    Lz78DecodeError error{Lz78DecodeError::none};
};

[[nodiscard]] Lz78DecodeResult decode_lz78_token_stream(
    std::span<const std::byte> input, const Lz78Parameters& parameters,
    std::uint64_t declared_frame_size, const core::DecoderLimits& limits,
    std::span<Lz78PhraseEntry> phrase_workspace,
    std::span<std::byte> output) noexcept;

} // namespace marc::dictionary::internal

#endif
