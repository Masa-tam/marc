#ifndef MARC_DICTIONARY_LZ77_FORMAT_HPP
#define MARC_DICTIONARY_LZ77_FORMAT_HPP

#include "core/limits.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

inline constexpr std::size_t lz77_parameter_size = 16;
inline constexpr std::size_t lz77_token_size = 16;
inline constexpr std::uint32_t lz77_default_window_size = UINT32_C(1) << 16;
inline constexpr std::uint32_t lz77_default_min_match = 3;
inline constexpr std::uint32_t lz77_default_max_match = 258;

struct Lz77Parameters {
    std::uint32_t window_size{lz77_default_window_size};
    std::uint32_t min_match_length{lz77_default_min_match};
    std::uint32_t max_match_length{lz77_default_max_match};
    std::uint32_t flags{};
};

enum class Lz77TokenTag : std::uint8_t {
    literal = 0,
    match_then_literal = 1,
    terminal_match = 2,
};

struct Lz77Token {
    Lz77TokenTag tag{Lz77TokenTag::literal};
    std::uint32_t distance{};
    std::uint32_t length{};
    std::uint8_t literal{};
};

enum class Lz77FormatError : std::uint8_t {
    none,
    invalid_window_size,
    invalid_match_range,
    unknown_flags,
    unknown_tag,
    nonzero_reserved,
    nonzero_unused_field,
    invalid_distance,
    invalid_length,
    output_size_mismatch,
    arithmetic_overflow,
    limit_exceeded,
};

struct Lz77TokenContext {
    std::uint64_t output_already_produced{};
    std::uint64_t declared_frame_size{};
};

[[nodiscard]] Lz77FormatError validate_lz77_parameters(
    const Lz77Parameters& parameters,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] Lz77FormatError parse_lz77_parameters(
    std::span<const std::byte, lz77_parameter_size> input,
    const core::DecoderLimits& limits,
    Lz77Parameters& parameters) noexcept;

[[nodiscard]] Lz77FormatError serialize_lz77_parameters(
    const Lz77Parameters& parameters, const core::DecoderLimits& limits,
    std::span<std::byte, lz77_parameter_size> output) noexcept;

[[nodiscard]] Lz77FormatError parse_lz77_token(
    std::span<const std::byte, lz77_token_size> input,
    Lz77Token& token) noexcept;

[[nodiscard]] Lz77FormatError serialize_lz77_token(
    const Lz77Token& token,
    std::span<std::byte, lz77_token_size> output) noexcept;

[[nodiscard]] Lz77FormatError validate_lz77_token(
    const Lz77Token& token, const Lz77Parameters& parameters,
    const Lz77TokenContext& context, const core::DecoderLimits& limits,
    std::uint64_t& output_size) noexcept;

} // namespace marc::dictionary::internal

#endif
