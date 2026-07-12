#ifndef MARC_ENTROPY_TANS_ENCODER_HPP
#define MARC_ENTROPY_TANS_ENCODER_HPP

#include "core/limits.hpp"
#include "entropy/tans_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

enum class TansEncodeError : std::uint8_t {
    none,
    empty_input,
    block_too_large,
    normalization_error,
    table_error,
    arithmetic_overflow,
    limit_exceeded,
    payload_output_too_small,
    internal_error,
};

struct TansEncodeResult {
    std::size_t payload_size{};
    TansEncodeError error{TansEncodeError::none};
};

[[nodiscard]] TansEncodeResult plan_tans_block(
    std::span<const std::byte> input, const core::DecoderLimits& limits,
    TansDescriptor& descriptor) noexcept;

[[nodiscard]] TansEncodeResult encode_tans_block(
    std::span<const std::byte> input, const core::DecoderLimits& limits,
    std::span<std::byte> payload_output,
    TansDescriptor& descriptor) noexcept;

} // namespace marc::entropy::internal

#endif
