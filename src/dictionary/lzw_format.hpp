#ifndef MARC_DICTIONARY_LZW_FORMAT_HPP
#define MARC_DICTIONARY_LZW_FORMAT_HPP

#include "core/limits.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

inline constexpr std::size_t lzw_parameter_size = 16;
inline constexpr std::uint32_t lzw_minimum_code_width = 9;
inline constexpr std::uint32_t lzw_default_maximum_code_width = 16;
inline constexpr std::uint32_t lzw_maximum_code_width = 24;
inline constexpr std::uint32_t lzw_first_free_code = 256;

struct LzwParameters {
    std::uint32_t maximum_code_width{lzw_default_maximum_code_width};
    std::uint32_t flags{};
    std::uint64_t reserved{};
};

struct LzwPhraseEntry {
    std::uint32_t prefix_code{};
    std::uint8_t trailing_byte{};
    std::uint8_t first_byte{};
    std::uint64_t length{};
};

enum class LzwFormatError : std::uint8_t {
    none,
    invalid_code_width,
    unknown_flags,
    nonzero_reserved,
    invalid_first_code,
    invalid_code,
    output_size_mismatch,
    arithmetic_overflow,
    nonzero_padding,
    limit_exceeded,
};

[[nodiscard]] LzwFormatError validate_lzw_parameters(
    const LzwParameters& parameters,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] LzwFormatError parse_lzw_parameters(
    std::span<const std::byte, lzw_parameter_size> input,
    const core::DecoderLimits& limits,
    LzwParameters& parameters) noexcept;

[[nodiscard]] LzwFormatError serialize_lzw_parameters(
    const LzwParameters& parameters, const core::DecoderLimits& limits,
    std::span<std::byte, lzw_parameter_size> output) noexcept;

[[nodiscard]] constexpr std::uint32_t lzw_code_limit(
    const LzwParameters& parameters) noexcept {
    return UINT32_C(1) << parameters.maximum_code_width;
}

} // namespace marc::dictionary::internal

#endif
