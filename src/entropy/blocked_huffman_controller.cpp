#include "entropy/blocked_huffman_controller.hpp"

#include "core/checked_math.hpp"
#include "entropy/canonical_huffman.hpp"
#include "entropy/huffman_decode_table.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::entropy::internal {
namespace {

[[nodiscard]] BlockedHuffmanControllerError scan_descriptor_region(
    const std::span<const std::byte> region,
    const std::uint32_t dictionary_size,
    const std::uint32_t block_size,
    const std::uint32_t block_count,
    const std::uint32_t declared_payload_size,
    const core::DecoderLimits& limits,
    const std::span<BlockedHuffmanBlockView> views) noexcept {
    std::size_t cursor{};
    std::uint64_t remaining = dictionary_size;
    std::uint64_t payload_offset{};

    for (std::uint32_t block = 0; block < block_count; ++block) {
        std::size_t descriptor_end{};
        if (!core::checked_add(
                cursor, blocked_huffman_descriptor_size, descriptor_end)) {
            return BlockedHuffmanControllerError::arithmetic_overflow;
        }
        if (descriptor_end > region.size()) {
            return BlockedHuffmanControllerError::truncated_descriptor;
        }

        const auto expected_symbols = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(block_size, remaining));
        BlockedHuffmanDescriptor descriptor{};
        const std::span<const std::byte, blocked_huffman_descriptor_size>
            encoded_descriptor{region.data() + cursor,
                               blocked_huffman_descriptor_size};
        if (parse_block_descriptor(
                encoded_descriptor, expected_symbols, limits, descriptor)
            != BlockedHuffmanFormatError::none) {
            return BlockedHuffmanControllerError::invalid_descriptor;
        }
        cursor = descriptor_end;

        const auto model_offset = cursor;
        std::size_t model_end{};
        if (!core::checked_add(
                cursor, static_cast<std::size_t>(descriptor.model_size),
                model_end)) {
            return BlockedHuffmanControllerError::arithmetic_overflow;
        }
        if (model_end > region.size()) {
            return BlockedHuffmanControllerError::truncated_model;
        }
        if (descriptor.model_size != 0) {
            HuffmanCodeLengths lengths{};
            for (std::size_t symbol = 0; symbol < lengths.size(); ++symbol) {
                lengths[symbol] = std::to_integer<std::uint8_t>(
                    region[model_offset + symbol]);
                if (lengths[symbol] > limits.max_huffman_code_length) {
                    return BlockedHuffmanControllerError::code_length_limit;
                }
            }
            HuffmanDecodeTable table{};
            if (build_decode_table(lengths, table)
                != HuffmanTableError::none) {
                return BlockedHuffmanControllerError::invalid_model;
            }
            if (table.node_count > limits.max_entropy_table_entries) {
                return BlockedHuffmanControllerError::decode_table_limit;
            }
        }

        if (!views.empty()) {
            if (model_offset > std::numeric_limits<std::uint32_t>::max()
                || payload_offset
                    > std::numeric_limits<std::uint32_t>::max()) {
                return BlockedHuffmanControllerError::arithmetic_overflow;
            }
            views[block] = {
                descriptor,
                static_cast<std::uint32_t>(model_offset),
                static_cast<std::uint32_t>(payload_offset)};
        }
        cursor = model_end;
        if (!core::checked_add(
                payload_offset,
                static_cast<std::uint64_t>(descriptor.payload_size),
                payload_offset)) {
            return BlockedHuffmanControllerError::arithmetic_overflow;
        }
        remaining -= expected_symbols;
    }

    if (cursor != region.size()) {
        return BlockedHuffmanControllerError::trailing_descriptor_bytes;
    }
    if (payload_offset != declared_payload_size) {
        return BlockedHuffmanControllerError::payload_size_mismatch;
    }
    return BlockedHuffmanControllerError::none;
}

} // namespace

BlockedHuffmanControllerResult parse_blocked_huffman_descriptor_region(
    const std::span<const std::byte> descriptor_region,
    const std::uint32_t dictionary_serialized_size,
    const std::uint32_t entropy_block_size,
    const std::uint32_t declared_block_count,
    const std::uint32_t declared_payload_size,
    const core::DecoderLimits& limits,
    const std::span<BlockedHuffmanBlockView> views) noexcept {
    if (dictionary_serialized_size == 0 || entropy_block_size == 0
        || entropy_block_size > limits.max_block_size) {
        return {0, BlockedHuffmanControllerError::invalid_block_count};
    }
    const auto expected_count =
        (static_cast<std::uint64_t>(dictionary_serialized_size)
         + entropy_block_size - 1)
        / entropy_block_size;
    if (expected_count != declared_block_count
        || expected_count > limits.max_blocks_per_frame) {
        return {static_cast<std::size_t>(expected_count),
                BlockedHuffmanControllerError::invalid_block_count};
    }
    const auto block_count = static_cast<std::size_t>(expected_count);
    if (views.size() < block_count) {
        return {block_count,
                BlockedHuffmanControllerError::output_views_too_small};
    }
    if (descriptor_region.size() > limits.max_internal_buffered_bytes
        || declared_payload_size > limits.max_compressed_payload_size) {
        return {block_count, BlockedHuffmanControllerError::limit_exceeded};
    }
    std::uint64_t combined_size{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(descriptor_region.size()),
            static_cast<std::uint64_t>(declared_payload_size),
            combined_size)) {
        return {block_count,
                BlockedHuffmanControllerError::arithmetic_overflow};
    }
    if (combined_size > limits.max_internal_buffered_bytes) {
        return {block_count, BlockedHuffmanControllerError::limit_exceeded};
    }

    const auto validation = scan_descriptor_region(
        descriptor_region, dictionary_serialized_size, entropy_block_size,
        declared_block_count, declared_payload_size, limits, {});
    if (validation != BlockedHuffmanControllerError::none) {
        return {block_count, validation};
    }
    const auto populated = scan_descriptor_region(
        descriptor_region, dictionary_serialized_size, entropy_block_size,
        declared_block_count, declared_payload_size, limits,
        views.first(block_count));
    return {block_count, populated};
}

} // namespace marc::entropy::internal
