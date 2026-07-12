#include "entropy/blocked_huffman_frame_decoder.hpp"

#include "core/checked_math.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {
namespace {

struct FrameTotals {
    std::size_t output_size{};
};

[[nodiscard]] BlockedHuffmanFrameDecodeResult validate_frame_blocks(
    const std::span<const std::byte> descriptor_region,
    const std::span<const std::byte> payload_region,
    const std::span<const BlockedHuffmanBlockView> views,
    const core::DecoderLimits& limits,
    FrameTotals& totals) noexcept {
    std::size_t output_size{};
    std::size_t expected_payload_offset{};
    for (std::size_t block = 0; block < views.size(); ++block) {
        const auto& view = views[block];
        if (view.payload_offset != expected_payload_offset) {
            return {output_size, block, BlockedHuffmanDecodeError::none,
                    BlockedHuffmanFrameDecodeError::invalid_view};
        }
        std::size_t model_end{};
        std::size_t payload_end{};
        if (!core::checked_add(
                static_cast<std::size_t>(view.model_offset),
                static_cast<std::size_t>(view.descriptor.model_size),
                model_end)
            || !core::checked_add(
                static_cast<std::size_t>(view.payload_offset),
                static_cast<std::size_t>(view.descriptor.payload_size),
                payload_end)
            || !core::checked_add(
                output_size,
                static_cast<std::size_t>(view.descriptor.symbol_count),
                output_size)) {
            return {output_size, block, BlockedHuffmanDecodeError::none,
                    BlockedHuffmanFrameDecodeError::arithmetic_overflow};
        }
        if (model_end > descriptor_region.size()
            || payload_end > payload_region.size()) {
            return {output_size, block, BlockedHuffmanDecodeError::none,
                    BlockedHuffmanFrameDecodeError::invalid_view};
        }
        const auto model = descriptor_region.subspan(
            view.model_offset, view.descriptor.model_size);
        const auto payload = payload_region.subspan(
            view.payload_offset, view.descriptor.payload_size);
        const auto validated = validate_blocked_huffman_block(
            view.descriptor, model, payload, limits);
        if (validated.error != BlockedHuffmanDecodeError::none) {
            return {output_size, block, validated.error,
                    BlockedHuffmanFrameDecodeError::block_error};
        }
        expected_payload_offset = payload_end;
    }
    if (expected_payload_offset != payload_region.size()) {
        return {output_size, views.size(), BlockedHuffmanDecodeError::none,
                BlockedHuffmanFrameDecodeError::invalid_view};
    }
    totals.output_size = output_size;
    return {output_size, views.size(), BlockedHuffmanDecodeError::none,
            BlockedHuffmanFrameDecodeError::none};
}

} // namespace

BlockedHuffmanFrameDecodeResult decode_blocked_huffman_frame(
    const std::span<const std::byte> descriptor_region,
    const std::span<const std::byte> payload_region,
    const std::span<const BlockedHuffmanBlockView> views,
    const core::DecoderLimits& limits,
    const std::span<std::byte> output) noexcept {
    if (views.empty()) {
        return {0, 0, BlockedHuffmanDecodeError::none,
                BlockedHuffmanFrameDecodeError::empty_views};
    }
    FrameTotals totals{};
    const auto validation = validate_frame_blocks(
        descriptor_region, payload_region, views, limits, totals);
    if (validation.error != BlockedHuffmanFrameDecodeError::none) {
        return validation;
    }
    if (totals.output_size > limits.max_dictionary_serialized_size) {
        return {totals.output_size, 0, BlockedHuffmanDecodeError::none,
                BlockedHuffmanFrameDecodeError::output_limit};
    }
    if (output.size() < totals.output_size) {
        return {totals.output_size, 0, BlockedHuffmanDecodeError::none,
                BlockedHuffmanFrameDecodeError::output_too_small};
    }

    std::size_t output_offset{};
    for (std::size_t block = 0; block < views.size(); ++block) {
        const auto& view = views[block];
        const auto model = descriptor_region.subspan(
            view.model_offset, view.descriptor.model_size);
        const auto payload = payload_region.subspan(
            view.payload_offset, view.descriptor.payload_size);
        const auto decoded = decode_blocked_huffman_block(
            view.descriptor, model, payload, limits,
            output.subspan(output_offset, view.descriptor.symbol_count));
        if (decoded.error != BlockedHuffmanDecodeError::none) {
            return {totals.output_size, block, decoded.error,
                    BlockedHuffmanFrameDecodeError::internal_error};
        }
        output_offset += decoded.output_size;
    }
    return {totals.output_size, views.size(),
            BlockedHuffmanDecodeError::none,
            BlockedHuffmanFrameDecodeError::none};
}

} // namespace marc::entropy::internal
