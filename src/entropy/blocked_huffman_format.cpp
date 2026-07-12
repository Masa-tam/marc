#include "entropy/blocked_huffman_format.hpp"

#include "core/checked_math.hpp"
#include "core/endian.hpp"
#include "entropy/canonical_huffman.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {
namespace {

[[nodiscard]] bool is_raw(
    const BlockedHuffmanDescriptor& descriptor) noexcept {
    return (descriptor.flags & blocked_huffman_raw_flag) != 0;
}

} // namespace

BlockedHuffmanFormatError validate_block_descriptor(
    const BlockedHuffmanDescriptor& descriptor,
    const std::uint32_t expected_symbol_count,
    const core::DecoderLimits& limits) noexcept {
    if (descriptor.symbol_count == 0
        || descriptor.symbol_count != expected_symbol_count) {
        return BlockedHuffmanFormatError::invalid_symbol_count;
    }
    if (descriptor.symbol_count > limits.max_block_size) {
        return BlockedHuffmanFormatError::limit_exceeded;
    }
    if ((descriptor.flags & ~blocked_huffman_raw_flag) != 0) {
        return BlockedHuffmanFormatError::unknown_flags;
    }
    if (descriptor.payload_size == 0) {
        return BlockedHuffmanFormatError::invalid_payload_size;
    }
    if (descriptor.payload_size > limits.max_compressed_payload_size) {
        return BlockedHuffmanFormatError::limit_exceeded;
    }

    if (is_raw(descriptor)) {
        if (descriptor.model_size != 0) {
            return BlockedHuffmanFormatError::invalid_model_size;
        }
        if (descriptor.payload_size != descriptor.symbol_count) {
            return BlockedHuffmanFormatError::contradictory_representation;
        }
        return descriptor.final_valid_bits == 8
            ? BlockedHuffmanFormatError::none
            : BlockedHuffmanFormatError::invalid_final_bits;
    }

    if (descriptor.model_size != blocked_huffman_model_size) {
        return BlockedHuffmanFormatError::invalid_model_size;
    }
    if (descriptor.final_valid_bits == 0
        || descriptor.final_valid_bits > 8) {
        return BlockedHuffmanFormatError::invalid_final_bits;
    }

    std::uint64_t payload_bits{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(descriptor.payload_size - 1),
            UINT64_C(8), payload_bits)
        || !core::checked_add(
            payload_bits,
            static_cast<std::uint64_t>(descriptor.final_valid_bits),
            payload_bits)) {
        return BlockedHuffmanFormatError::arithmetic_overflow;
    }
    std::uint64_t maximum_bits{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(descriptor.symbol_count),
            static_cast<std::uint64_t>(huffman_max_code_length),
            maximum_bits)) {
        return BlockedHuffmanFormatError::arithmetic_overflow;
    }
    if (payload_bits < descriptor.symbol_count
        || payload_bits > maximum_bits) {
        return BlockedHuffmanFormatError::contradictory_representation;
    }

    std::uint64_t stored_huffman_size{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(descriptor.model_size),
            static_cast<std::uint64_t>(descriptor.payload_size),
            stored_huffman_size)) {
        return BlockedHuffmanFormatError::arithmetic_overflow;
    }
    return stored_huffman_size < descriptor.symbol_count
        ? BlockedHuffmanFormatError::none
        : BlockedHuffmanFormatError::contradictory_representation;
}

BlockedHuffmanFormatError parse_block_descriptor(
    const std::span<const std::byte, blocked_huffman_descriptor_size> input,
    const std::uint32_t expected_symbol_count,
    const core::DecoderLimits& limits,
    BlockedHuffmanDescriptor& descriptor) noexcept {
    BlockedHuffmanDescriptor parsed{};
    if (!core::load_le(input, 0, parsed.symbol_count)
        || !core::load_le(input, 4, parsed.payload_size)
        || !core::load_le(input, 8, parsed.model_size)) {
        return BlockedHuffmanFormatError::descriptor_size_mismatch;
    }
    parsed.flags = std::to_integer<std::uint8_t>(input[10]);
    parsed.final_valid_bits = std::to_integer<std::uint8_t>(input[11]);
    if (std::ranges::any_of(
            input.subspan<12>(),
            [](const std::byte value) { return value != std::byte{0}; })) {
        return BlockedHuffmanFormatError::nonzero_reserved;
    }

    const auto error = validate_block_descriptor(
        parsed, expected_symbol_count, limits);
    if (error == BlockedHuffmanFormatError::none) {
        descriptor = parsed;
    }
    return error;
}

BlockedHuffmanFormatError serialize_block_descriptor(
    const BlockedHuffmanDescriptor& descriptor,
    const std::uint32_t expected_symbol_count,
    const core::DecoderLimits& limits,
    const std::span<std::byte, blocked_huffman_descriptor_size> output) noexcept {
    const auto error = validate_block_descriptor(
        descriptor, expected_symbol_count, limits);
    if (error != BlockedHuffmanFormatError::none) {
        return error;
    }

    std::ranges::fill(output, std::byte{0});
    const auto stored = core::store_le(output, 0, descriptor.symbol_count)
        && core::store_le(output, 4, descriptor.payload_size)
        && core::store_le(output, 8, descriptor.model_size);
    if (!stored) {
        return BlockedHuffmanFormatError::descriptor_size_mismatch;
    }
    output[10] = static_cast<std::byte>(descriptor.flags);
    output[11] = static_cast<std::byte>(descriptor.final_valid_bits);
    return BlockedHuffmanFormatError::none;
}

BlockedHuffmanFormatError validate_blocked_huffman_layout(
    const std::span<const BlockedHuffmanDescriptor> descriptors,
    const std::uint32_t dictionary_serialized_size,
    const std::uint32_t entropy_block_size,
    const std::uint32_t declared_descriptor_bytes,
    const std::uint32_t declared_payload_bytes,
    const core::DecoderLimits& limits) noexcept {
    if (dictionary_serialized_size == 0 || entropy_block_size == 0) {
        return BlockedHuffmanFormatError::invalid_block_count;
    }
    if (entropy_block_size > limits.max_block_size) {
        return BlockedHuffmanFormatError::limit_exceeded;
    }
    const auto expected_count =
        (static_cast<std::uint64_t>(dictionary_serialized_size)
         + entropy_block_size - 1)
        / entropy_block_size;
    if (descriptors.size() != expected_count
        || descriptors.size() > limits.max_blocks_per_frame) {
        return BlockedHuffmanFormatError::invalid_block_count;
    }

    std::uint64_t descriptor_bytes{};
    std::uint64_t payload_bytes{};
    std::uint64_t remaining = dictionary_serialized_size;
    for (const auto& descriptor : descriptors) {
        const auto expected_symbols = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(entropy_block_size, remaining));
        const auto error = validate_block_descriptor(
            descriptor, expected_symbols, limits);
        if (error != BlockedHuffmanFormatError::none) {
            return error;
        }
        if (!core::checked_add(
                descriptor_bytes,
                static_cast<std::uint64_t>(blocked_huffman_descriptor_size)
                    + descriptor.model_size,
                descriptor_bytes)
            || !core::checked_add(
                payload_bytes,
                static_cast<std::uint64_t>(descriptor.payload_size),
                payload_bytes)) {
            return BlockedHuffmanFormatError::arithmetic_overflow;
        }
        remaining -= expected_symbols;
    }
    if (descriptor_bytes != declared_descriptor_bytes) {
        return BlockedHuffmanFormatError::descriptor_size_mismatch;
    }
    if (payload_bytes != declared_payload_bytes) {
        return BlockedHuffmanFormatError::payload_size_mismatch;
    }
    std::uint64_t buffered_bytes{};
    if (!core::checked_add(
            descriptor_bytes, payload_bytes, buffered_bytes)) {
        return BlockedHuffmanFormatError::arithmetic_overflow;
    }
    if (buffered_bytes > limits.max_internal_buffered_bytes
        || payload_bytes > limits.max_compressed_payload_size) {
        return BlockedHuffmanFormatError::limit_exceeded;
    }
    return BlockedHuffmanFormatError::none;
}

} // namespace marc::entropy::internal
