#ifndef MARC_ENTROPY_TANS_DECODER_HPP
#define MARC_ENTROPY_TANS_DECODER_HPP

#include "core/limits.hpp"
#include "entropy/tans_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

enum class TansDecodeError : std::uint8_t {
    none,
    invalid_descriptor,
    invalid_table,
    payload_size_mismatch,
    invalid_state,
    truncated_bits,
    trailing_bits,
    nonzero_padding,
    invalid_terminal_state,
    output_too_small,
    internal_error,
};

struct TansDecodeResult {
    std::size_t output_size{};
    std::size_t bits_consumed{};
    TansDecodeError error{TansDecodeError::none};
};

[[nodiscard]] TansDecodeResult validate_tans_block(
    const TansDescriptor& descriptor, std::span<const std::byte> payload,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] TansDecodeResult decode_tans_block(
    const TansDescriptor& descriptor, std::span<const std::byte> payload,
    const core::DecoderLimits& limits,
    std::span<std::byte> output) noexcept;

} // namespace marc::entropy::internal

#endif
