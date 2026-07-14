#ifndef MARC_DICTIONARY_LZD_DECODER_HPP
#define MARC_DICTIONARY_LZD_DECODER_HPP

#include "dictionary/lzd_validator.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

enum class LzdDecodeError : std::uint8_t {
    none,
    invalid_token_stream,
    output_too_small,
    output_size_unsupported,
    expansion_workspace_too_small,
    limit_exceeded,
    internal_error,
};

struct LzdDecodeResult {
    std::size_t output_size{};
    std::size_t expansion_workspace_entries{};
    std::size_t token_index{};
    std::size_t input_offset{};
    LzdValidationError validation_error{LzdValidationError::none};
    LzdFormatError format_error{LzdFormatError::none};
    LzdDecodeError error{LzdDecodeError::none};
};

[[nodiscard]] std::size_t lzd_expansion_workspace_entries(
    std::size_t dictionary_entries, bool has_output) noexcept;

[[nodiscard]] LzdDecodeResult decode_lzd_token_stream(
    std::span<const std::byte> input, const LzdParameters& parameters,
    std::uint64_t declared_frame_size, const core::DecoderLimits& limits,
    std::span<LzdPhraseEntry> phrase_workspace,
    std::span<std::uint32_t> expansion_workspace,
    std::span<std::byte> output) noexcept;

} // namespace marc::dictionary::internal

#endif
