#include "entropy/contextual_adaptive_huffman_format.hpp"

#include "core/endian.hpp"

#include <algorithm>
#include <array>

namespace marc::entropy::internal {

ContextualAdaptiveHuffmanFormatError
validate_contextual_adaptive_huffman_descriptor(
    const ContextualAdaptiveHuffmanDescriptor& descriptor,
    const std::uint32_t expected_decision_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits) noexcept {
    if (descriptor.decision_count == 0
        || descriptor.decision_count
            > contextual_adaptive_huffman_max_decision_count) {
        return ContextualAdaptiveHuffmanFormatError::invalid_decision_count;
    }
    if (descriptor.payload_size == 0) {
        return ContextualAdaptiveHuffmanFormatError::invalid_payload_size;
    }
    if (descriptor.context_count
        != contextual_adaptive_huffman_context_count) {
        return ContextualAdaptiveHuffmanFormatError::invalid_context_count;
    }
    if (descriptor.final_valid_bits == 0
        || descriptor.final_valid_bits > 8) {
        return ContextualAdaptiveHuffmanFormatError::invalid_final_bits;
    }
    if (descriptor.flags != 0) {
        return ContextualAdaptiveHuffmanFormatError::unknown_flags;
    }
    if (descriptor.decision_count != expected_decision_count
        || descriptor.payload_size != expected_payload_size) {
        return ContextualAdaptiveHuffmanFormatError::contradictory_size;
    }
    if (descriptor.payload_size > limits.max_compressed_payload_size
        || descriptor.payload_size > limits.max_internal_buffered_bytes) {
        return ContextualAdaptiveHuffmanFormatError::limit_exceeded;
    }
    return ContextualAdaptiveHuffmanFormatError::none;
}

ContextualAdaptiveHuffmanFormatError
parse_contextual_adaptive_huffman_descriptor(
    const std::span<const std::byte,
                    contextual_adaptive_huffman_descriptor_size> input,
    const std::uint32_t expected_decision_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    ContextualAdaptiveHuffmanDescriptor& descriptor) noexcept {
    ContextualAdaptiveHuffmanDescriptor parsed{};
    if (!core::load_le(input, 0, parsed.decision_count)
        || !core::load_le(input, 4, parsed.payload_size)
        || !core::load_le(input, 8, parsed.context_count)) {
        return ContextualAdaptiveHuffmanFormatError::contradictory_size;
    }
    parsed.final_valid_bits = std::to_integer<std::uint8_t>(input[10]);
    parsed.flags = std::to_integer<std::uint8_t>(input[11]);
    if (!std::all_of(input.begin() + 12, input.end(),
                     [](const std::byte value) { return value == std::byte{}; })) {
        return ContextualAdaptiveHuffmanFormatError::nonzero_reserved;
    }
    const auto error = validate_contextual_adaptive_huffman_descriptor(
        parsed, expected_decision_count, expected_payload_size, limits);
    if (error == ContextualAdaptiveHuffmanFormatError::none) descriptor = parsed;
    return error;
}

ContextualAdaptiveHuffmanFormatError
serialize_contextual_adaptive_huffman_descriptor(
    const ContextualAdaptiveHuffmanDescriptor& descriptor,
    const std::uint32_t expected_decision_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    const std::span<std::byte,
                    contextual_adaptive_huffman_descriptor_size> output)
    noexcept {
    const auto error = validate_contextual_adaptive_huffman_descriptor(
        descriptor, expected_decision_count, expected_payload_size, limits);
    if (error != ContextualAdaptiveHuffmanFormatError::none) return error;
    std::array<std::byte, contextual_adaptive_huffman_descriptor_size> encoded{};
    if (!core::store_le(encoded, 0, descriptor.decision_count)
        || !core::store_le(encoded, 4, descriptor.payload_size)
        || !core::store_le(encoded, 8, descriptor.context_count)) {
        return ContextualAdaptiveHuffmanFormatError::contradictory_size;
    }
    encoded[10] = static_cast<std::byte>(descriptor.final_valid_bits);
    encoded[11] = static_cast<std::byte>(descriptor.flags);
    std::copy(encoded.begin(), encoded.end(), output.begin());
    return ContextualAdaptiveHuffmanFormatError::none;
}

} // namespace marc::entropy::internal
