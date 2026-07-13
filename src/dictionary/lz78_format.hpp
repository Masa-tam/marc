#ifndef MARC_DICTIONARY_LZ78_FORMAT_HPP
#define MARC_DICTIONARY_LZ78_FORMAT_HPP

#include "core/limits.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

inline constexpr std::size_t lz78_parameter_size = 16;
inline constexpr std::size_t lz78_token_size = 8;
inline constexpr std::uint32_t lz78_default_maximum_entries =
    UINT32_C(1) << 16;

struct Lz78Parameters {
    std::uint32_t maximum_entries{lz78_default_maximum_entries};
    std::uint32_t flags{};
    std::uint64_t reserved{};
};

enum class Lz78TokenTag : std::uint8_t {
    pair = 0,
    final_index = 1,
};

struct Lz78Token {
    Lz78TokenTag tag{Lz78TokenTag::pair};
    std::uint8_t symbol{};
    std::uint32_t phrase_index{};
};

struct Lz78PhraseEntry {
    std::uint32_t prefix_index{};
    std::uint8_t symbol{};
    std::uint64_t length{};
};

enum class Lz78FormatError : std::uint8_t {
    none,
    invalid_maximum_entries,
    unknown_flags,
    nonzero_reserved,
    unknown_tag,
    nonzero_unused_field,
    invalid_phrase_index,
    invalid_final_index,
    output_size_mismatch,
    arithmetic_overflow,
    limit_exceeded,
};

[[nodiscard]] Lz78FormatError validate_lz78_parameters(
    const Lz78Parameters& parameters,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] Lz78FormatError parse_lz78_parameters(
    std::span<const std::byte, lz78_parameter_size> input,
    const core::DecoderLimits& limits,
    Lz78Parameters& parameters) noexcept;

[[nodiscard]] Lz78FormatError serialize_lz78_parameters(
    const Lz78Parameters& parameters, const core::DecoderLimits& limits,
    std::span<std::byte, lz78_parameter_size> output) noexcept;

[[nodiscard]] Lz78FormatError parse_lz78_token(
    std::span<const std::byte, lz78_token_size> input,
    Lz78Token& token) noexcept;

[[nodiscard]] Lz78FormatError serialize_lz78_token(
    const Lz78Token& token,
    std::span<std::byte, lz78_token_size> output) noexcept;

} // namespace marc::dictionary::internal

#endif
