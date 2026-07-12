#include "entropy/adaptive_huffman_encoder.hpp"

#include "core/checked_math.hpp"
#include "entropy/adaptive_huffman_tree.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::entropy::internal {
namespace {

[[nodiscard]] AdaptiveHuffmanTreeError path_for(
    const AdaptiveHuffmanTree& tree,
    const std::uint8_t symbol,
    const std::span<std::uint8_t> path,
    std::size_t& path_size) noexcept {
    return tree.contains(symbol)
        ? tree.path_for_symbol(symbol, path, path_size)
        : tree.path_for_nyt(path, path_size);
}

[[nodiscard]] AdaptiveHuffmanTreeError observe(
    AdaptiveHuffmanTree& tree, const std::uint8_t symbol) noexcept {
    return tree.contains(symbol) ? tree.observe_existing(symbol)
                                 : tree.observe_new(symbol);
}

void write_bit(const std::uint8_t bit, std::uint64_t& bit_offset,
               const std::span<std::byte> output) noexcept {
    if (bit != 0) {
        const auto byte_index = static_cast<std::size_t>(bit_offset / 8);
        const auto bit_index = static_cast<unsigned int>(bit_offset % 8);
        output[byte_index] |= static_cast<std::byte>(1U << bit_index);
    }
    ++bit_offset;
}

} // namespace

AdaptiveHuffmanEncodeResult plan_adaptive_huffman_frame(
    const std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    AdaptiveHuffmanDescriptor& descriptor) noexcept {
    if (input.empty()) return {0, 0, AdaptiveHuffmanEncodeError::empty_input};
    if (input.size() > adaptive_huffman_max_frame_size
        || input.size() > std::numeric_limits<std::uint32_t>::max()) {
        return {0, 0, AdaptiveHuffmanEncodeError::frame_too_large};
    }
    if (input.size() > limits.max_frame_size) {
        return {0, 0, AdaptiveHuffmanEncodeError::limit_exceeded};
    }

    AdaptiveHuffmanTree tree;
    std::array<std::uint8_t, 256> path{};
    std::uint64_t payload_bits{};
    for (const auto byte : input) {
        const auto symbol = std::to_integer<std::uint8_t>(byte);
        const bool is_new = !tree.contains(symbol);
        std::size_t path_size{};
        if (path_for(tree, symbol, path, path_size)
            != AdaptiveHuffmanTreeError::none) {
            return {0, 0, AdaptiveHuffmanEncodeError::internal_error};
        }
        if (!core::checked_add(
                payload_bits, static_cast<std::uint64_t>(path_size),
                payload_bits)
            || (is_new && !core::checked_add(
                payload_bits, UINT64_C(8), payload_bits))) {
            return {0, 0, AdaptiveHuffmanEncodeError::arithmetic_overflow};
        }
        if (observe(tree, symbol) != AdaptiveHuffmanTreeError::none) {
            return {0, 0, AdaptiveHuffmanEncodeError::internal_error};
        }
    }
    std::uint64_t rounded_bits{};
    if (!core::checked_add(payload_bits, UINT64_C(7), rounded_bits)) {
        return {0, payload_bits,
                AdaptiveHuffmanEncodeError::arithmetic_overflow};
    }
    const auto payload_size_u64 = rounded_bits / 8;
    if (payload_size_u64 > std::numeric_limits<std::uint32_t>::max()
        || payload_size_u64 > std::numeric_limits<std::size_t>::max()) {
        return {0, payload_bits,
                AdaptiveHuffmanEncodeError::arithmetic_overflow};
    }
    const auto payload_size = static_cast<std::size_t>(payload_size_u64);
    if (payload_size_u64 > limits.max_compressed_payload_size
        || payload_size_u64 > limits.max_internal_buffered_bytes) {
        return {payload_size, payload_bits,
                AdaptiveHuffmanEncodeError::limit_exceeded};
    }

    AdaptiveHuffmanDescriptor planned{};
    planned.symbol_count = static_cast<std::uint32_t>(input.size());
    planned.payload_size = static_cast<std::uint32_t>(payload_size);
    planned.final_valid_bits = static_cast<std::uint8_t>(
        payload_bits % 8 == 0 ? 8 : payload_bits % 8);
    if (validate_adaptive_huffman_descriptor(
            planned, planned.symbol_count, planned.payload_size, limits)
        != AdaptiveHuffmanFormatError::none) {
        return {payload_size, payload_bits,
                AdaptiveHuffmanEncodeError::internal_error};
    }
    descriptor = planned;
    return {payload_size, payload_bits, AdaptiveHuffmanEncodeError::none};
}

AdaptiveHuffmanEncodeResult encode_adaptive_huffman_frame(
    const std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    const std::span<std::byte> payload_output,
    AdaptiveHuffmanDescriptor& descriptor) noexcept {
    AdaptiveHuffmanDescriptor planned{};
    const auto result = plan_adaptive_huffman_frame(input, limits, planned);
    if (result.error != AdaptiveHuffmanEncodeError::none) return result;
    if (payload_output.size() < result.payload_size) {
        return {result.payload_size, result.payload_bits,
                AdaptiveHuffmanEncodeError::payload_output_too_small};
    }
    auto payload = payload_output.first(result.payload_size);
    std::ranges::fill(payload, std::byte{});
    AdaptiveHuffmanTree tree;
    std::array<std::uint8_t, 256> path{};
    std::uint64_t bit_offset{};
    for (const auto byte : input) {
        const auto symbol = std::to_integer<std::uint8_t>(byte);
        const bool is_new = !tree.contains(symbol);
        std::size_t path_size{};
        if (path_for(tree, symbol, path, path_size)
            != AdaptiveHuffmanTreeError::none) {
            return {result.payload_size, result.payload_bits,
                    AdaptiveHuffmanEncodeError::internal_error};
        }
        for (std::size_t bit = 0; bit < path_size; ++bit) {
            write_bit(path[bit], bit_offset, payload);
        }
        if (is_new) {
            for (std::uint8_t bit = 0; bit < 8; ++bit) {
                write_bit(static_cast<std::uint8_t>((symbol >> bit) & 1U),
                          bit_offset, payload);
            }
        }
        if (observe(tree, symbol) != AdaptiveHuffmanTreeError::none) {
            return {result.payload_size, result.payload_bits,
                    AdaptiveHuffmanEncodeError::internal_error};
        }
    }
    if (bit_offset != result.payload_bits) {
        return {result.payload_size, result.payload_bits,
                AdaptiveHuffmanEncodeError::internal_error};
    }
    descriptor = planned;
    return result;
}

} // namespace marc::entropy::internal
