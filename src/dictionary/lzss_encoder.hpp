#ifndef MARC_DICTIONARY_LZSS_ENCODER_HPP
#define MARC_DICTIONARY_LZSS_ENCODER_HPP

#include "dictionary/lzss_format.hpp"
#include "dictionary/lzss_hash_chain_match_finder.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

enum class LzssEncodeError : std::uint8_t {
    none,
    invalid_parameters,
    input_limit_exceeded,
    serialized_limit_exceeded,
    output_too_small,
    overlapping_buffers,
    arithmetic_overflow,
    match_finder_error,
    internal_error,
};

struct LzssEncodeResult {
    std::size_t input_size{};
    std::size_t output_size{};
    std::size_t token_count{};
    LzssFormatError format_error{LzssFormatError::none};
    LzssEncodeError error{LzssEncodeError::none};
    LzssHashChainError match_finder_error{LzssHashChainError::none};
};

[[nodiscard]] LzssEncodeResult plan_lzss_token_stream(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] LzssEncodeResult encode_lzss_token_stream(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits, std::span<std::byte> output) noexcept;

[[nodiscard]] LzssEncodeResult plan_lzss_token_stream_hash_chain(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    std::span<std::byte> match_finder_workspace) noexcept;

[[nodiscard]] LzssEncodeResult encode_lzss_token_stream_hash_chain(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits, std::span<std::byte> output,
    std::span<std::byte> match_finder_workspace) noexcept;

} // namespace marc::dictionary::internal

#endif
