#ifndef MARC_DICTIONARY_LZD_FORMAT_HPP
#define MARC_DICTIONARY_LZD_FORMAT_HPP

#include "core/limits.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

inline constexpr std::size_t lzd_parameter_size = 16;
inline constexpr std::size_t lzd_token_size = 8;
inline constexpr std::uint32_t lzd_first_phrase_reference = 256;
inline constexpr std::uint32_t lzd_absent_reference = UINT32_MAX;
inline constexpr std::uint32_t lzd_maximum_phrase_entries =
    UINT32_MAX - lzd_first_phrase_reference;
inline constexpr std::uint32_t lzd_default_maximum_entries =
    UINT32_C(1) << 16;

struct LzdParameters {
    std::uint32_t maximum_entries{lzd_default_maximum_entries};
    std::uint32_t flags{};
    std::uint64_t reserved{};
};

struct LzdToken {
    std::uint32_t left_reference{};
    std::uint32_t right_reference{lzd_absent_reference};
};

struct LzdPhraseEntry {
    std::uint32_t left_reference{};
    std::uint32_t right_reference{};
    std::uint64_t length{};
};

enum class LzdFormatError : std::uint8_t {
    none,
    invalid_maximum_entries,
    unknown_flags,
    nonzero_reserved,
    invalid_left_reference,
    invalid_phrase_reference,
    invalid_terminal_reference,
    output_size_mismatch,
    arithmetic_overflow,
    limit_exceeded,
};

[[nodiscard]] LzdFormatError validate_lzd_parameters(
    const LzdParameters& parameters,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] LzdFormatError parse_lzd_parameters(
    std::span<const std::byte, lzd_parameter_size> input,
    const core::DecoderLimits& limits,
    LzdParameters& parameters) noexcept;

[[nodiscard]] LzdFormatError serialize_lzd_parameters(
    const LzdParameters& parameters, const core::DecoderLimits& limits,
    std::span<std::byte, lzd_parameter_size> output) noexcept;

[[nodiscard]] LzdFormatError parse_lzd_token(
    std::span<const std::byte, lzd_token_size> input,
    LzdToken& token) noexcept;

[[nodiscard]] LzdFormatError serialize_lzd_token(
    const LzdToken& token,
    std::span<std::byte, lzd_token_size> output) noexcept;

} // namespace marc::dictionary::internal

#endif
