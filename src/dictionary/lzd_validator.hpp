#ifndef MARC_DICTIONARY_LZD_VALIDATOR_HPP
#define MARC_DICTIONARY_LZD_VALIDATOR_HPP

#include "dictionary/lzd_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

enum class LzdValidationError : std::uint8_t {
    none,
    invalid_parameters,
    truncated_token,
    token_error,
    premature_end,
    trailing_tokens,
    workspace_too_small,
    limit_exceeded,
};

struct LzdValidationResult {
    std::size_t token_count{};
    std::size_t token_index{};
    std::size_t input_offset{};
    std::uint32_t dictionary_entries{};
    std::uint64_t output_size{};
    LzdFormatError format_error{LzdFormatError::none};
    LzdValidationError error{LzdValidationError::none};
};

[[nodiscard]] std::size_t lzd_validation_workspace_entries(
    std::size_t serialized_size,
    const LzdParameters& parameters) noexcept;

[[nodiscard]] std::size_t lzd_validation_workspace_entries(
    std::size_t serialized_size, std::uint64_t declared_frame_size,
    const LzdParameters& parameters) noexcept;

[[nodiscard]] LzdValidationResult validate_lzd_token_stream(
    std::span<const std::byte> input, const LzdParameters& parameters,
    std::uint64_t declared_frame_size, const core::DecoderLimits& limits,
    std::span<LzdPhraseEntry> phrase_workspace) noexcept;

} // namespace marc::dictionary::internal

#endif
