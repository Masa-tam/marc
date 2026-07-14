#ifndef MARC_DICTIONARY_LZW_ENCODER_HPP
#define MARC_DICTIONARY_LZW_ENCODER_HPP

#include "dictionary/lzw_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

struct LzwEncoderEntry {
    std::size_t input_offset{};
    std::size_t length{};
};

enum class LzwEncodeError : std::uint8_t {
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

struct LzwEncodeResult {
    std::size_t input_size{};
    std::size_t output_size{};
    std::size_t code_count{};
    std::size_t bit_count{};
    std::uint32_t dictionary_entries{};
    LzwFormatError format_error{LzwFormatError::none};
    LzwEncodeError error{LzwEncodeError::none};
};

[[nodiscard]] std::size_t lzw_encoder_workspace_entries(
    std::size_t input_size, const LzwParameters& parameters) noexcept;

[[nodiscard]] LzwEncodeResult plan_lzw_code_stream(
    std::span<const std::byte> input, const LzwParameters& parameters,
    const core::DecoderLimits& limits,
    std::span<LzwEncoderEntry> dictionary_workspace) noexcept;

[[nodiscard]] LzwEncodeResult encode_lzw_code_stream(
    std::span<const std::byte> input, const LzwParameters& parameters,
    const core::DecoderLimits& limits,
    std::span<LzwEncoderEntry> dictionary_workspace,
    std::span<std::byte> output) noexcept;

} // namespace marc::dictionary::internal

#endif
