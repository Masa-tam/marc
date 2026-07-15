#ifndef MARC_DICTIONARY_LZMW_FORMAT_HPP
#define MARC_DICTIONARY_LZMW_FORMAT_HPP

#include "core/limits.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

inline constexpr std::size_t lzmw_parameter_size = 16;
inline constexpr std::size_t lzmw_token_size = 4;
inline constexpr std::uint32_t lzmw_first_phrase_reference = 256;
inline constexpr std::uint32_t lzmw_maximum_phrase_entries =
    UINT32_MAX - UINT32_C(255);
inline constexpr std::uint32_t lzmw_default_maximum_entries =
    UINT32_C(1) << 16;

struct LzmwParameters {
    std::uint32_t maximum_entries{lzmw_default_maximum_entries};
    std::uint32_t flags{};
    std::uint64_t reserved{};
};

struct LzmwPhraseEntry {
    std::uint32_t left_reference{};
    std::uint32_t right_reference{};
    std::uint64_t length{};
};

enum class LzmwFormatError : std::uint8_t {
    none,
    invalid_maximum_entries,
    unknown_flags,
    nonzero_reserved,
    invalid_phrase_reference,
    output_size_mismatch,
    arithmetic_overflow,
    limit_exceeded,
};

[[nodiscard]] bool lzmw_maximum_token_stream_size(
    std::uint64_t raw_size, std::size_t& serialized_size) noexcept;

[[nodiscard]] LzmwFormatError validate_lzmw_parameters(
    const LzmwParameters& parameters,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] LzmwFormatError parse_lzmw_parameters(
    std::span<const std::byte, lzmw_parameter_size> input,
    const core::DecoderLimits& limits,
    LzmwParameters& parameters) noexcept;

[[nodiscard]] LzmwFormatError serialize_lzmw_parameters(
    const LzmwParameters& parameters, const core::DecoderLimits& limits,
    std::span<std::byte, lzmw_parameter_size> output) noexcept;

[[nodiscard]] LzmwFormatError parse_lzmw_token(
    std::span<const std::byte, lzmw_token_size> input,
    std::uint32_t& reference) noexcept;

[[nodiscard]] LzmwFormatError serialize_lzmw_token(
    std::uint32_t reference,
    std::span<std::byte, lzmw_token_size> output) noexcept;

} // namespace marc::dictionary::internal

#endif
