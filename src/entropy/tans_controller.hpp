#ifndef MARC_ENTROPY_TANS_CONTROLLER_HPP
#define MARC_ENTROPY_TANS_CONTROLLER_HPP

#include "core/limits.hpp"
#include "entropy/tans_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

struct TansBlockView {
    TansDescriptor descriptor{};
    std::uint32_t payload_offset{};
};

enum class TansControllerError : std::uint8_t {
    none,
    invalid_block_count,
    output_views_too_small,
    truncated_descriptor,
    trailing_descriptor_bytes,
    invalid_descriptor,
    payload_size_mismatch,
    limit_exceeded,
    arithmetic_overflow,
};

struct TansControllerResult {
    std::size_t block_count{};
    TansControllerError error{TansControllerError::none};
};

[[nodiscard]] TansControllerResult parse_tans_descriptor_region(
    std::span<const std::byte> descriptor_region,
    std::uint32_t dictionary_serialized_size,
    std::uint32_t entropy_block_size,
    std::uint32_t declared_block_count,
    std::uint32_t declared_payload_size,
    const core::DecoderLimits& limits,
    std::span<TansBlockView> views) noexcept;

} // namespace marc::entropy::internal

#endif
