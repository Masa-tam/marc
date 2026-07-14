#ifndef MARC_DICTIONARY_LZW_VALIDATOR_HPP
#define MARC_DICTIONARY_LZW_VALIDATOR_HPP

#include "dictionary/lzw_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

enum class LzwValidationError : std::uint8_t {
    none,
    invalid_parameters,
    premature_code,
    code_error,
    trailing_data,
    workspace_too_small,
    limit_exceeded,
};

struct LzwValidationResult {
    std::size_t code_count{};
    std::size_t code_index{};
    std::size_t input_offset{};
    std::uint64_t input_bit_offset{};
    std::uint32_t dictionary_entries{};
    std::uint64_t output_size{};
    LzwFormatError format_error{LzwFormatError::none};
    LzwValidationError error{LzwValidationError::none};
};

[[nodiscard]] std::size_t lzw_validation_workspace_entries(
    std::size_t serialized_size,
    const LzwParameters& parameters) noexcept;

[[nodiscard]] LzwValidationResult validate_lzw_code_stream(
    std::span<const std::byte> input, const LzwParameters& parameters,
    std::uint64_t declared_frame_size, const core::DecoderLimits& limits,
    std::span<LzwPhraseEntry> phrase_workspace) noexcept;

} // namespace marc::dictionary::internal

#endif
