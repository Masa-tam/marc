#ifndef MARC_DICTIONARY_LZ78_VALIDATOR_HPP
#define MARC_DICTIONARY_LZ78_VALIDATOR_HPP

#include "dictionary/lz78_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

enum class Lz78ValidationError : std::uint8_t {
    none,
    invalid_parameters,
    truncated_token,
    token_error,
    premature_end,
    trailing_tokens,
    workspace_too_small,
    limit_exceeded,
};

struct Lz78ValidationResult {
    std::size_t token_count{};
    std::size_t token_index{};
    std::size_t input_offset{};
    std::uint32_t dictionary_entries{};
    std::uint64_t output_size{};
    Lz78FormatError format_error{Lz78FormatError::none};
    Lz78ValidationError error{Lz78ValidationError::none};
};

[[nodiscard]] std::size_t lz78_validation_workspace_entries(
    std::size_t serialized_size,
    const Lz78Parameters& parameters) noexcept;

[[nodiscard]] Lz78ValidationResult validate_lz78_token_stream(
    std::span<const std::byte> input, const Lz78Parameters& parameters,
    std::uint64_t declared_frame_size, const core::DecoderLimits& limits,
    std::span<Lz78PhraseEntry> phrase_workspace) noexcept;

} // namespace marc::dictionary::internal

#endif
