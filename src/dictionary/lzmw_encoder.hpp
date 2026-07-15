#ifndef MARC_DICTIONARY_LZMW_ENCODER_HPP
#define MARC_DICTIONARY_LZMW_ENCODER_HPP

#include "dictionary/lzmw_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

struct LzmwEncoderEntry {
    std::size_t input_offset{};
    std::size_t length{};
};

enum class LzmwEncodeError : std::uint8_t {
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

struct LzmwEncodeResult {
    std::size_t input_size{};
    std::size_t output_size{};
    std::size_t token_count{};
    std::uint32_t dictionary_entries{};
    LzmwFormatError format_error{LzmwFormatError::none};
    LzmwEncodeError error{LzmwEncodeError::none};
};

[[nodiscard]] std::size_t lzmw_encoder_workspace_entries(
    std::size_t input_size, const LzmwParameters& parameters) noexcept;

[[nodiscard]] LzmwEncodeResult plan_lzmw_token_stream(
    std::span<const std::byte> input, const LzmwParameters& parameters,
    const core::DecoderLimits& limits,
    std::span<LzmwEncoderEntry> dictionary_workspace) noexcept;

[[nodiscard]] LzmwEncodeResult encode_lzmw_token_stream(
    std::span<const std::byte> input, const LzmwParameters& parameters,
    const core::DecoderLimits& limits,
    std::span<LzmwEncoderEntry> dictionary_workspace,
    std::span<std::byte> output) noexcept;

} // namespace marc::dictionary::internal

#endif
