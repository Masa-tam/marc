#ifndef MARC_DICTIONARY_LZMW_DECODER_HPP
#define MARC_DICTIONARY_LZMW_DECODER_HPP

#include "dictionary/lzmw_validator.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

enum class LzmwDecodeError : std::uint8_t {
    none,
    invalid_token_stream,
    output_too_small,
    output_size_unsupported,
    expansion_workspace_too_small,
    limit_exceeded,
    internal_error,
};

struct LzmwDecodeResult {
    std::size_t output_size{};
    std::size_t expansion_workspace_entries{};
    std::size_t token_index{};
    std::size_t input_offset{};
    LzmwValidationError validation_error{LzmwValidationError::none};
    LzmwFormatError format_error{LzmwFormatError::none};
    LzmwDecodeError error{LzmwDecodeError::none};
};

[[nodiscard]] std::size_t lzmw_expansion_workspace_entries(
    std::size_t dictionary_entries, bool has_output) noexcept;

[[nodiscard]] LzmwDecodeResult decode_lzmw_token_stream(
    std::span<const std::byte> input, const LzmwParameters& parameters,
    std::uint64_t declared_frame_size, const core::DecoderLimits& limits,
    std::span<LzmwPhraseEntry> phrase_workspace,
    std::span<std::uint32_t> expansion_workspace,
    std::span<std::byte> output) noexcept;

} // namespace marc::dictionary::internal

#endif
