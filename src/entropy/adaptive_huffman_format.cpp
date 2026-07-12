#include "entropy/adaptive_huffman_format.hpp"

#include "core/endian.hpp"

#include <algorithm>

namespace marc::entropy::internal {

AdaptiveHuffmanFormatError validate_adaptive_huffman_descriptor(
    const AdaptiveHuffmanDescriptor& descriptor,
    const std::uint32_t expected_symbol_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits) noexcept {
    if (descriptor.symbol_count == 0
        || descriptor.symbol_count > adaptive_huffman_max_frame_size) {
        return AdaptiveHuffmanFormatError::invalid_symbol_count;
    }
    if (descriptor.payload_size == 0) {
        return AdaptiveHuffmanFormatError::invalid_payload_size;
    }
    if (descriptor.final_valid_bits == 0
        || descriptor.final_valid_bits > 8) {
        return AdaptiveHuffmanFormatError::invalid_final_bits;
    }
    if (descriptor.flags != 0) {
        return AdaptiveHuffmanFormatError::unknown_flags;
    }
    if (descriptor.symbol_count != expected_symbol_count
        || descriptor.payload_size != expected_payload_size) {
        return AdaptiveHuffmanFormatError::contradictory_size;
    }
    if (descriptor.symbol_count > limits.max_frame_size
        || descriptor.payload_size > limits.max_compressed_payload_size
        || descriptor.payload_size > limits.max_internal_buffered_bytes) {
        return AdaptiveHuffmanFormatError::limit_exceeded;
    }
    return AdaptiveHuffmanFormatError::none;
}

AdaptiveHuffmanFormatError parse_adaptive_huffman_descriptor(
    const std::span<const std::byte, adaptive_huffman_descriptor_size> input,
    const std::uint32_t expected_symbol_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    AdaptiveHuffmanDescriptor& descriptor) noexcept {
    AdaptiveHuffmanDescriptor parsed{};
    if (!core::load_le(input, 0, parsed.symbol_count)
        || !core::load_le(input, 4, parsed.payload_size)) {
        return AdaptiveHuffmanFormatError::contradictory_size;
    }
    parsed.final_valid_bits = std::to_integer<std::uint8_t>(input[8]);
    parsed.flags = std::to_integer<std::uint8_t>(input[9]);
    if (!std::all_of(input.begin() + 10, input.end(),
                     [](const std::byte value) { return value == std::byte{}; })) {
        return AdaptiveHuffmanFormatError::nonzero_reserved;
    }
    const auto error = validate_adaptive_huffman_descriptor(
        parsed, expected_symbol_count, expected_payload_size, limits);
    if (error == AdaptiveHuffmanFormatError::none) descriptor = parsed;
    return error;
}

AdaptiveHuffmanFormatError serialize_adaptive_huffman_descriptor(
    const AdaptiveHuffmanDescriptor& descriptor,
    const std::uint32_t expected_symbol_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    const std::span<std::byte, adaptive_huffman_descriptor_size> output)
    noexcept {
    const auto error = validate_adaptive_huffman_descriptor(
        descriptor, expected_symbol_count, expected_payload_size, limits);
    if (error != AdaptiveHuffmanFormatError::none) return error;
    std::fill(output.begin(), output.end(), std::byte{});
    if (!core::store_le(output, 0, descriptor.symbol_count)
        || !core::store_le(output, 4, descriptor.payload_size)) {
        return AdaptiveHuffmanFormatError::contradictory_size;
    }
    output[8] = static_cast<std::byte>(descriptor.final_valid_bits);
    output[9] = static_cast<std::byte>(descriptor.flags);
    return AdaptiveHuffmanFormatError::none;
}

} // namespace marc::entropy::internal
