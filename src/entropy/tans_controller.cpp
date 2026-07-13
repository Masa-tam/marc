#include "entropy/tans_controller.hpp"

#include "core/checked_math.hpp"
#include "core/endian.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::entropy::internal {
namespace {

[[nodiscard]] TansControllerError scan_region(
    const std::span<const std::byte> region,
    const std::uint32_t dictionary_size,
    const std::uint32_t block_size,
    const std::uint32_t block_count,
    const std::uint32_t declared_payload_size,
    const core::DecoderLimits& limits,
    const std::span<TansBlockView> views) noexcept {
    std::size_t cursor{};
    std::uint64_t remaining = dictionary_size;
    std::uint64_t payload_offset{};
    for (std::uint32_t block = 0; block < block_count; ++block) {
        std::size_t descriptor_end{};
        if (!core::checked_add(cursor, tans_descriptor_size, descriptor_end)) {
            return TansControllerError::arithmetic_overflow;
        }
        if (descriptor_end > region.size()) {
            return TansControllerError::truncated_descriptor;
        }
        const auto expected_symbols = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(block_size, remaining));
        const std::span<const std::byte, tans_descriptor_size> encoded{
            region.data() + cursor, tans_descriptor_size};
        TansDescriptor descriptor{};
        std::uint32_t block_payload_size{};
        if (!core::load_le(encoded, 4, block_payload_size)) {
            return TansControllerError::invalid_descriptor;
        }
        if (parse_tans_descriptor(
                encoded, expected_symbols, block_payload_size,
                limits, descriptor) != TansFormatError::none) {
            return TansControllerError::invalid_descriptor;
        }
        if (!views.empty()) {
            if (payload_offset > std::numeric_limits<std::uint32_t>::max()) {
                return TansControllerError::arithmetic_overflow;
            }
            views[block] = {
                descriptor, static_cast<std::uint32_t>(payload_offset)};
        }
        if (!core::checked_add(
                payload_offset,
                static_cast<std::uint64_t>(descriptor.payload_size),
                payload_offset)) {
            return TansControllerError::arithmetic_overflow;
        }
        cursor = descriptor_end;
        remaining -= expected_symbols;
    }
    if (cursor != region.size()) {
        return TansControllerError::trailing_descriptor_bytes;
    }
    if (payload_offset != declared_payload_size) {
        return TansControllerError::payload_size_mismatch;
    }
    return TansControllerError::none;
}

} // namespace

TansControllerResult parse_tans_descriptor_region(
    const std::span<const std::byte> descriptor_region,
    const std::uint32_t dictionary_serialized_size,
    const std::uint32_t entropy_block_size,
    const std::uint32_t declared_block_count,
    const std::uint32_t declared_payload_size,
    const core::DecoderLimits& limits,
    const std::span<TansBlockView> views) noexcept {
    if (dictionary_serialized_size == 0 || entropy_block_size == 0
        || entropy_block_size > tans_max_block_size
        || entropy_block_size > limits.max_block_size) {
        return {0, TansControllerError::invalid_block_count};
    }
    const auto expected_count =
        (static_cast<std::uint64_t>(dictionary_serialized_size)
         + entropy_block_size - 1) / entropy_block_size;
    if (expected_count != declared_block_count
        || expected_count > limits.max_blocks_per_frame) {
        return {static_cast<std::size_t>(expected_count),
                TansControllerError::invalid_block_count};
    }
    const auto count = static_cast<std::size_t>(expected_count);
    if (views.size() < count) {
        return {count, TansControllerError::output_views_too_small};
    }
    std::uint64_t expected_descriptor_bytes{};
    if (!core::checked_multiply(
            expected_count, static_cast<std::uint64_t>(tans_descriptor_size),
            expected_descriptor_bytes)) {
        return {count, TansControllerError::arithmetic_overflow};
    }
    if (descriptor_region.size() < expected_descriptor_bytes) {
        return {count, TansControllerError::truncated_descriptor};
    }
    if (descriptor_region.size() > expected_descriptor_bytes) {
        return {count, TansControllerError::trailing_descriptor_bytes};
    }
    std::uint64_t combined{};
    if (!core::checked_add(
            expected_descriptor_bytes,
            static_cast<std::uint64_t>(declared_payload_size), combined)) {
        return {count, TansControllerError::arithmetic_overflow};
    }
    if (declared_payload_size > limits.max_compressed_payload_size
        || combined > limits.max_internal_buffered_bytes) {
        return {count, TansControllerError::limit_exceeded};
    }
    const auto validation = scan_region(
        descriptor_region, dictionary_serialized_size, entropy_block_size,
        declared_block_count, declared_payload_size, limits, {});
    if (validation != TansControllerError::none) return {count, validation};
    const auto populated = scan_region(
        descriptor_region, dictionary_serialized_size, entropy_block_size,
        declared_block_count, declared_payload_size, limits,
        views.first(count));
    return {count, populated};
}

} // namespace marc::entropy::internal
