#ifndef MARC_DICTIONARY_LZMW_VALIDATOR_HPP
#define MARC_DICTIONARY_LZMW_VALIDATOR_HPP

#include "dictionary/lzmw_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

enum class LzmwValidationError : std::uint8_t {
    none,
    invalid_parameters,
    truncated_token,
    token_error,
    premature_end,
    trailing_tokens,
    workspace_too_small,
    limit_exceeded,
};

struct LzmwValidationResult {
    std::size_t token_count{};
    std::size_t token_index{};
    std::size_t input_offset{};
    std::uint32_t dictionary_entries{};
    std::uint64_t output_size{};
    LzmwFormatError format_error{LzmwFormatError::none};
    LzmwValidationError error{LzmwValidationError::none};
};

[[nodiscard]] std::size_t lzmw_validation_workspace_entries(
    std::size_t serialized_size,
    const LzmwParameters& parameters) noexcept;

[[nodiscard]] LzmwValidationResult validate_lzmw_token_stream(
    std::span<const std::byte> input, const LzmwParameters& parameters,
    std::uint64_t declared_frame_size, const core::DecoderLimits& limits,
    std::span<LzmwPhraseEntry> phrase_workspace) noexcept;

} // namespace marc::dictionary::internal

#endif
