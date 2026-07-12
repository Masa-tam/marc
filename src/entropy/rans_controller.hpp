#ifndef MARC_ENTROPY_RANS_CONTROLLER_HPP
#define MARC_ENTROPY_RANS_CONTROLLER_HPP

#include "core/limits.hpp"
#include "entropy/rans_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

struct RansBlockView {
    RansDescriptor descriptor{};
    std::uint32_t payload_offset{};
};

enum class RansControllerError : std::uint8_t {
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

struct RansControllerResult {
    std::size_t block_count{};
    RansControllerError error{RansControllerError::none};
};

[[nodiscard]] RansControllerResult parse_rans_descriptor_region(
    std::span<const std::byte> descriptor_region,
    std::uint32_t dictionary_serialized_size,
    std::uint32_t entropy_block_size,
    std::uint32_t declared_block_count,
    std::uint32_t declared_payload_size,
    const core::DecoderLimits& limits,
    std::span<RansBlockView> views) noexcept;

} // namespace marc::entropy::internal

#endif
