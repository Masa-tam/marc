#ifndef MARC_DICTIONARY_LZ78_ENCODER_HPP
#define MARC_DICTIONARY_LZ78_ENCODER_HPP

#include "dictionary/lz78_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

struct Lz78EncoderEntry {
    std::size_t input_offset{};
    std::uint64_t length{};
};

enum class Lz78EncodeError : std::uint8_t {
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

struct Lz78EncodeResult {
    std::size_t input_size{};
    std::size_t output_size{};
    std::size_t token_count{};
    std::uint32_t dictionary_entries{};
    Lz78FormatError format_error{Lz78FormatError::none};
    Lz78EncodeError error{Lz78EncodeError::none};
};

[[nodiscard]] std::size_t lz78_encoder_workspace_entries(
    std::size_t input_size, const Lz78Parameters& parameters) noexcept;

[[nodiscard]] Lz78EncodeResult plan_lz78_token_stream(
    std::span<const std::byte> input, const Lz78Parameters& parameters,
    const core::DecoderLimits& limits,
    std::span<Lz78EncoderEntry> dictionary_workspace) noexcept;

[[nodiscard]] Lz78EncodeResult encode_lz78_token_stream(
    std::span<const std::byte> input, const Lz78Parameters& parameters,
    const core::DecoderLimits& limits,
    std::span<Lz78EncoderEntry> dictionary_workspace,
    std::span<std::byte> output) noexcept;

} // namespace marc::dictionary::internal

#endif
