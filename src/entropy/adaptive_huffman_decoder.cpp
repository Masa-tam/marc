#include "entropy/adaptive_huffman_decoder.hpp"

#include "core/checked_math.hpp"
#include "entropy/adaptive_huffman_tree.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {
namespace {

struct DecodePassResult {
    std::uint64_t bits_consumed{};
    AdaptiveHuffmanDecodeError error{AdaptiveHuffmanDecodeError::none};
};

[[nodiscard]] bool read_bit(const std::span<const std::byte> payload,
                            const std::uint64_t valid_bits,
                            std::uint64_t& bit_offset,
                            std::uint8_t& bit) noexcept {
    if (bit_offset >= valid_bits) return false;
    const auto byte_index = static_cast<std::size_t>(bit_offset / 8);
    const auto bit_index = static_cast<unsigned int>(bit_offset % 8);
    bit = static_cast<std::uint8_t>(
        (std::to_integer<std::uint8_t>(payload[byte_index]) >> bit_index) & 1U);
    ++bit_offset;
    return true;
}

[[nodiscard]] DecodePassResult decode_pass(
    const AdaptiveHuffmanDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const std::uint64_t valid_bits,
    const std::span<std::byte> output) noexcept {
    AdaptiveHuffmanTree tree;
    std::uint64_t bit_offset{};
    for (std::uint32_t produced = 0; produced < descriptor.symbol_count;
         ++produced) {
        auto cursor = tree.root();
        while (tree.node(cursor).kind == AdaptiveHuffmanNodeKind::internal) {
            std::uint8_t bit{};
            if (!read_bit(payload, valid_bits, bit_offset, bit)) {
                return {bit_offset,
                        AdaptiveHuffmanDecodeError::truncated_payload};
            }
            cursor = bit == 0 ? tree.node(cursor).left
                              : tree.node(cursor).right;
            if (cursor == adaptive_huffman_invalid_node
                || cursor >= tree.node_count()) {
                return {bit_offset, AdaptiveHuffmanDecodeError::invalid_tree};
            }
        }

        std::uint8_t symbol{};
        if (tree.node(cursor).kind == AdaptiveHuffmanNodeKind::nyt) {
            for (std::uint8_t bit_index = 0; bit_index < 8; ++bit_index) {
                std::uint8_t bit{};
                if (!read_bit(payload, valid_bits, bit_offset, bit)) {
                    return {bit_offset,
                            AdaptiveHuffmanDecodeError::truncated_payload};
                }
                symbol |= static_cast<std::uint8_t>(bit << bit_index);
            }
            if (tree.contains(symbol)) {
                return {bit_offset,
                        AdaptiveHuffmanDecodeError::duplicate_nyt_symbol};
            }
            if (tree.observe_new(symbol) != AdaptiveHuffmanTreeError::none) {
                return {bit_offset, AdaptiveHuffmanDecodeError::invalid_tree};
            }
        } else if (tree.node(cursor).kind == AdaptiveHuffmanNodeKind::symbol) {
            symbol = static_cast<std::uint8_t>(tree.node(cursor).symbol);
            if (tree.observe_existing(symbol)
                != AdaptiveHuffmanTreeError::none) {
                return {bit_offset, AdaptiveHuffmanDecodeError::invalid_tree};
            }
        } else {
            return {bit_offset, AdaptiveHuffmanDecodeError::invalid_tree};
        }
        if (!output.empty()) output[produced] = static_cast<std::byte>(symbol);
    }
    if (bit_offset != valid_bits) {
        return {bit_offset, AdaptiveHuffmanDecodeError::trailing_bits};
    }
    if (!tree.validate()) {
        return {bit_offset, AdaptiveHuffmanDecodeError::invalid_tree};
    }
    return {bit_offset, AdaptiveHuffmanDecodeError::none};
}

} // namespace

AdaptiveHuffmanDecodeResult decode_adaptive_huffman_frame(
    const AdaptiveHuffmanDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const core::DecoderLimits& limits,
    const std::span<std::byte> output) noexcept {
    if (validate_adaptive_huffman_descriptor(
            descriptor, descriptor.symbol_count, descriptor.payload_size,
            limits) != AdaptiveHuffmanFormatError::none) {
        return {descriptor.symbol_count, 0,
                AdaptiveHuffmanDecodeError::invalid_descriptor};
    }
    core::FrameBounds bounds{};
    bounds.uncompressed_size = descriptor.symbol_count;
    bounds.dictionary_serialized_size = descriptor.symbol_count;
    bounds.compressed_payload_size = descriptor.payload_size;
    bounds.payload_buffered_bytes = descriptor.payload_size;
    bounds.block_count = 1;
    if (core::validate_frame_bounds(limits, bounds, 0)
        != core::LimitError::none) {
        return {descriptor.symbol_count, 0,
                AdaptiveHuffmanDecodeError::invalid_descriptor};
    }
    if (payload.size() != descriptor.payload_size) {
        return {descriptor.symbol_count, 0,
                AdaptiveHuffmanDecodeError::payload_size_mismatch};
    }
    if (output.size() < descriptor.symbol_count) {
        return {descriptor.symbol_count, 0,
                AdaptiveHuffmanDecodeError::output_too_small};
    }
    std::uint64_t leading_bits{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(descriptor.payload_size - 1),
            UINT64_C(8), leading_bits)) {
        return {descriptor.symbol_count, 0,
                AdaptiveHuffmanDecodeError::arithmetic_overflow};
    }
    std::uint64_t valid_bits{};
    if (!core::checked_add(
            leading_bits,
            static_cast<std::uint64_t>(descriptor.final_valid_bits),
            valid_bits)) {
        return {descriptor.symbol_count, 0,
                AdaptiveHuffmanDecodeError::arithmetic_overflow};
    }
    if (descriptor.final_valid_bits < 8) {
        const auto last = std::to_integer<std::uint8_t>(payload.back());
        const auto valid_mask = static_cast<std::uint8_t>(
            (1U << descriptor.final_valid_bits) - 1U);
        if ((last & static_cast<std::uint8_t>(~valid_mask)) != 0) {
            return {descriptor.symbol_count, 0,
                    AdaptiveHuffmanDecodeError::nonzero_padding};
        }
    }

    const auto validation = decode_pass(descriptor, payload, valid_bits, {});
    if (validation.error != AdaptiveHuffmanDecodeError::none) {
        return {descriptor.symbol_count, validation.bits_consumed,
                validation.error};
    }
    const auto decoded = decode_pass(
        descriptor, payload, valid_bits,
        output.first(descriptor.symbol_count));
    if (decoded.error != AdaptiveHuffmanDecodeError::none) {
        return {descriptor.symbol_count, decoded.bits_consumed,
                AdaptiveHuffmanDecodeError::internal_error};
    }
    return {descriptor.symbol_count, decoded.bits_consumed,
            AdaptiveHuffmanDecodeError::none};
}

} // namespace marc::entropy::internal
