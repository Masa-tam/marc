#include "entropy/blocked_huffman_frame_encoder.hpp"

#include "core/checked_math.hpp"
#include "entropy/blocked_huffman_format.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::entropy::internal {

BlockedHuffmanFrameEncodeResult plan_blocked_huffman_frame(
    const std::span<const std::byte> input,
    const std::uint32_t block_size,
    const core::DecoderLimits& limits) noexcept {
    if (input.empty()) {
        return {0, 0, 0, 0, BlockedHuffmanEncodeError::none,
                BlockedHuffmanFrameEncodeError::empty_input};
    }
    if (block_size == 0 || block_size > limits.max_block_size) {
        return {0, 0, 0, 0, BlockedHuffmanEncodeError::none,
                BlockedHuffmanFrameEncodeError::invalid_block_size};
    }
    if (input.size() > std::numeric_limits<std::uint32_t>::max()
        || input.size() > limits.max_dictionary_serialized_size) {
        return {0, 0, 0, 0, BlockedHuffmanEncodeError::none,
                BlockedHuffmanFrameEncodeError::input_limit};
    }
    const auto block_count = (input.size() - 1) / block_size + 1;
    if (block_count > limits.max_blocks_per_frame) {
        return {block_count, 0, 0, 0, BlockedHuffmanEncodeError::none,
                BlockedHuffmanFrameEncodeError::limit_exceeded};
    }

    std::size_t descriptor_size{};
    std::size_t payload_size{};
    for (std::size_t block = 0; block < block_count; ++block) {
        const auto input_offset = block * static_cast<std::size_t>(block_size);
        const auto current_size = std::min<std::size_t>(
            block_size, input.size() - input_offset);
        BlockedHuffmanDescriptor descriptor{};
        const auto planned = plan_blocked_huffman_block(
            input.subspan(input_offset, current_size), limits, descriptor);
        if (planned.error != BlockedHuffmanEncodeError::none) {
            return {block_count, descriptor_size, payload_size, block,
                    planned.error,
                    BlockedHuffmanFrameEncodeError::block_error};
        }
        if (!core::checked_add(
                descriptor_size,
                blocked_huffman_descriptor_size + planned.model_size,
                descriptor_size)
            || !core::checked_add(
                payload_size, planned.payload_size, payload_size)) {
            return {block_count, descriptor_size, payload_size, block,
                    BlockedHuffmanEncodeError::none,
                    BlockedHuffmanFrameEncodeError::arithmetic_overflow};
        }
    }

    std::size_t buffered_size{};
    if (!core::checked_add(
            descriptor_size, payload_size, buffered_size)) {
        return {block_count, descriptor_size, payload_size, block_count,
                BlockedHuffmanEncodeError::none,
                BlockedHuffmanFrameEncodeError::arithmetic_overflow};
    }
    if (buffered_size > limits.max_internal_buffered_bytes
        || payload_size > limits.max_compressed_payload_size) {
        return {block_count, descriptor_size, payload_size, block_count,
                BlockedHuffmanEncodeError::none,
                BlockedHuffmanFrameEncodeError::limit_exceeded};
    }
    return {block_count, descriptor_size, payload_size, block_count,
            BlockedHuffmanEncodeError::none,
            BlockedHuffmanFrameEncodeError::none};
}

BlockedHuffmanFrameEncodeResult encode_blocked_huffman_frame(
    const std::span<const std::byte> input,
    const std::uint32_t block_size,
    const core::DecoderLimits& limits,
    const std::span<std::byte> descriptor_output,
    const std::span<std::byte> payload_output) noexcept {
    const auto plan = plan_blocked_huffman_frame(input, block_size, limits);
    if (plan.error != BlockedHuffmanFrameEncodeError::none) {
        return plan;
    }
    if (descriptor_output.size() < plan.descriptor_region_size) {
        auto result = plan;
        result.error =
            BlockedHuffmanFrameEncodeError::descriptor_output_too_small;
        return result;
    }
    if (payload_output.size() < plan.payload_size) {
        auto result = plan;
        result.error =
            BlockedHuffmanFrameEncodeError::payload_output_too_small;
        return result;
    }

    std::size_t descriptor_offset{};
    std::size_t payload_offset{};
    for (std::size_t block = 0; block < plan.block_count; ++block) {
        const auto input_offset = block * static_cast<std::size_t>(block_size);
        const auto current_size = std::min<std::size_t>(
            block_size, input.size() - input_offset);
        BlockedHuffmanDescriptor descriptor{};
        const auto planned = plan_blocked_huffman_block(
            input.subspan(input_offset, current_size), limits, descriptor);
        if (planned.error != BlockedHuffmanEncodeError::none) {
            auto result = plan;
            result.block_index = block;
            result.block_error = planned.error;
            result.error = BlockedHuffmanFrameEncodeError::internal_error;
            return result;
        }
        const auto model_offset =
            descriptor_offset + blocked_huffman_descriptor_size;
        const auto encoded = encode_blocked_huffman_block(
            input.subspan(input_offset, current_size), limits,
            descriptor_output.subspan(model_offset, planned.model_size),
            payload_output.subspan(payload_offset, planned.payload_size),
            descriptor);
        if (encoded.error != BlockedHuffmanEncodeError::none) {
            auto result = plan;
            result.block_index = block;
            result.block_error = encoded.error;
            result.error = BlockedHuffmanFrameEncodeError::internal_error;
            return result;
        }
        std::span<std::byte, blocked_huffman_descriptor_size> serialized{
            descriptor_output.data() + descriptor_offset,
            blocked_huffman_descriptor_size};
        if (serialize_block_descriptor(
                descriptor, descriptor.symbol_count, limits, serialized)
            != BlockedHuffmanFormatError::none) {
            auto result = plan;
            result.block_index = block;
            result.error = BlockedHuffmanFrameEncodeError::internal_error;
            return result;
        }
        descriptor_offset +=
            blocked_huffman_descriptor_size + planned.model_size;
        payload_offset += planned.payload_size;
    }
    return plan;
}

} // namespace marc::entropy::internal
