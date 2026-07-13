#ifndef MARC_DICTIONARY_LZSS_FORMAT_HPP
#define MARC_DICTIONARY_LZSS_FORMAT_HPP

#include "core/limits.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

inline constexpr std::size_t lzss_parameter_size = 16;
inline constexpr std::size_t lzss_literal_size = 2;
inline constexpr std::size_t lzss_match_size = 9;
inline constexpr std::uint32_t lzss_default_window_size = UINT32_C(1) << 16;
inline constexpr std::uint32_t lzss_default_min_match = 5;
inline constexpr std::uint32_t lzss_default_max_match = 258;

struct LzssParameters {
    std::uint32_t window_size{lzss_default_window_size};
    std::uint32_t min_match_length{lzss_default_min_match};
    std::uint32_t max_match_length{lzss_default_max_match};
    std::uint32_t flags{};
};

enum class LzssTokenTag : std::uint8_t {
    literal = 0,
    match = 1,
};

struct LzssToken {
    LzssTokenTag tag{LzssTokenTag::literal};
    std::uint32_t distance{};
    std::uint32_t length{};
    std::uint8_t literal{};
};

enum class LzssFormatError : std::uint8_t {
    none,
    invalid_window_size,
    invalid_match_range,
    unknown_flags,
    unknown_tag,
    truncated_token,
    nonzero_unused_field,
    invalid_distance,
    invalid_length,
    output_size_mismatch,
    output_too_small,
    arithmetic_overflow,
    limit_exceeded,
};

struct LzssTokenContext {
    std::uint64_t output_already_produced{};
    std::uint64_t declared_frame_size{};
};

[[nodiscard]] LzssFormatError validate_lzss_parameters(
    const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] LzssFormatError parse_lzss_parameters(
    std::span<const std::byte, lzss_parameter_size> input,
    const core::DecoderLimits& limits,
    LzssParameters& parameters) noexcept;

[[nodiscard]] LzssFormatError serialize_lzss_parameters(
    const LzssParameters& parameters, const core::DecoderLimits& limits,
    std::span<std::byte, lzss_parameter_size> output) noexcept;

[[nodiscard]] LzssFormatError parse_lzss_token(
    std::span<const std::byte> input, LzssToken& token,
    std::size_t& bytes_consumed) noexcept;

[[nodiscard]] LzssFormatError serialize_lzss_token(
    const LzssToken& token, std::span<std::byte> output,
    std::size_t& bytes_written) noexcept;

[[nodiscard]] LzssFormatError validate_lzss_token(
    const LzssToken& token, const LzssParameters& parameters,
    const LzssTokenContext& context, const core::DecoderLimits& limits,
    std::uint64_t& output_size) noexcept;

} // namespace marc::dictionary::internal

#endif
