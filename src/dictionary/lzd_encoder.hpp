#ifndef MARC_DICTIONARY_LZD_ENCODER_HPP
#define MARC_DICTIONARY_LZD_ENCODER_HPP

#include "dictionary/lzd_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

struct LzdEncoderEntry {
    std::size_t input_offset{};
    std::size_t length{};
};

enum class LzdEncodeError : std::uint8_t {
    none,
    invalid_parameters,
    input_limit_exceeded,
    serialized_limit_exceeded,
    workspace_too_small,
    workspace_limit_exceeded,
    output_too_small,
    arithmetic_overflow,
    internal_error,
};

struct LzdEncodeResult {
    std::size_t input_size{};
    std::size_t output_size{};
    std::size_t token_count{};
    std::uint32_t dictionary_entries{};
    LzdFormatError format_error{LzdFormatError::none};
    LzdEncodeError error{LzdEncodeError::none};
};

[[nodiscard]] std::size_t lzd_encoder_workspace_entries(
    std::size_t input_size, const LzdParameters& parameters) noexcept;

[[nodiscard]] LzdEncodeResult plan_lzd_token_stream(
    std::span<const std::byte> input, const LzdParameters& parameters,
    const core::DecoderLimits& limits,
    std::span<LzdEncoderEntry> dictionary_workspace) noexcept;

[[nodiscard]] LzdEncodeResult encode_lzd_token_stream(
    std::span<const std::byte> input, const LzdParameters& parameters,
    const core::DecoderLimits& limits,
    std::span<LzdEncoderEntry> dictionary_workspace,
    std::span<std::byte> output) noexcept;

} // namespace marc::dictionary::internal

#endif
