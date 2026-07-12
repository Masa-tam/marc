#include "entropy/rans_controller.hpp"

#include "core/checked_math.hpp"
#include "core/endian.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::entropy::internal {
namespace {

[[nodiscard]] RansControllerError scan_region(
    const std::span<const std::byte> region,
    const std::uint32_t dictionary_size,
    const std::uint32_t block_size,
    const std::uint32_t block_count,
    const std::uint32_t declared_payload_size,
    const core::DecoderLimits& limits,
    const std::span<RansBlockView> views) noexcept {
    std::size_t cursor{};
    std::uint64_t remaining = dictionary_size;
    std::uint64_t payload_offset{};
    for (std::uint32_t block = 0; block < block_count; ++block) {
        std::size_t descriptor_end{};
        if (!core::checked_add(cursor, rans_descriptor_size, descriptor_end)) {
            return RansControllerError::arithmetic_overflow;
        }
        if (descriptor_end > region.size()) {
            return RansControllerError::truncated_descriptor;
        }
        const auto expected_symbols = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(block_size, remaining));
        const std::span<const std::byte, rans_descriptor_size> encoded{
            region.data() + cursor, rans_descriptor_size};
        RansDescriptor descriptor{};
        std::uint32_t block_payload_size{};
        if (!core::load_le(encoded, 4, block_payload_size)) {
            return RansControllerError::invalid_descriptor;
        }
        if (parse_rans_descriptor(
                encoded, expected_symbols, block_payload_size,
                limits, descriptor) != RansFormatError::none) {
            return RansControllerError::invalid_descriptor;
        }
        if (!views.empty()) {
            if (payload_offset > std::numeric_limits<std::uint32_t>::max()) {
                return RansControllerError::arithmetic_overflow;
            }
            views[block] = {
                descriptor, static_cast<std::uint32_t>(payload_offset)};
        }
        if (!core::checked_add(
                payload_offset,
                static_cast<std::uint64_t>(descriptor.payload_size),
                payload_offset)) {
            return RansControllerError::arithmetic_overflow;
        }
        cursor = descriptor_end;
        remaining -= expected_symbols;
    }
    if (cursor != region.size()) {
        return RansControllerError::trailing_descriptor_bytes;
    }
    if (payload_offset != declared_payload_size) {
        return RansControllerError::payload_size_mismatch;
    }
    return RansControllerError::none;
}

} // namespace

RansControllerResult parse_rans_descriptor_region(
    const std::span<const std::byte> descriptor_region,
    const std::uint32_t dictionary_serialized_size,
    const std::uint32_t entropy_block_size,
    const std::uint32_t declared_block_count,
    const std::uint32_t declared_payload_size,
    const core::DecoderLimits& limits,
    const std::span<RansBlockView> views) noexcept {
    if (dictionary_serialized_size == 0 || entropy_block_size == 0
        || entropy_block_size > rans_max_block_size
        || entropy_block_size > limits.max_block_size) {
        return {0, RansControllerError::invalid_block_count};
    }
    const auto expected_count =
        (static_cast<std::uint64_t>(dictionary_serialized_size)
         + entropy_block_size - 1) / entropy_block_size;
    if (expected_count != declared_block_count
        || expected_count > limits.max_blocks_per_frame) {
        return {static_cast<std::size_t>(expected_count),
                RansControllerError::invalid_block_count};
    }
    const auto count = static_cast<std::size_t>(expected_count);
    if (views.size() < count) {
        return {count, RansControllerError::output_views_too_small};
    }
    std::uint64_t expected_descriptor_bytes{};
    if (!core::checked_multiply(
            expected_count, static_cast<std::uint64_t>(rans_descriptor_size),
            expected_descriptor_bytes)) {
        return {count, RansControllerError::arithmetic_overflow};
    }
    if (descriptor_region.size() < expected_descriptor_bytes) {
        return {count, RansControllerError::truncated_descriptor};
    }
    if (descriptor_region.size() > expected_descriptor_bytes) {
        return {count, RansControllerError::trailing_descriptor_bytes};
    }
    std::uint64_t combined{};
    if (!core::checked_add(
            expected_descriptor_bytes,
            static_cast<std::uint64_t>(declared_payload_size), combined)) {
        return {count, RansControllerError::arithmetic_overflow};
    }
    if (declared_payload_size > limits.max_compressed_payload_size
        || combined > limits.max_internal_buffered_bytes) {
        return {count, RansControllerError::limit_exceeded};
    }
    const auto validation = scan_region(
        descriptor_region, dictionary_serialized_size, entropy_block_size,
        declared_block_count, declared_payload_size, limits, {});
    if (validation != RansControllerError::none) return {count, validation};
    const auto populated = scan_region(
        descriptor_region, dictionary_serialized_size, entropy_block_size,
        declared_block_count, declared_payload_size, limits,
        views.first(count));
    return {count, populated};
}

} // namespace marc::entropy::internal
