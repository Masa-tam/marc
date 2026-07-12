#ifndef MARC_ENTROPY_BLOCKED_HUFFMAN_FORMAT_HPP
#define MARC_ENTROPY_BLOCKED_HUFFMAN_FORMAT_HPP

#include "core/limits.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

inline constexpr std::size_t blocked_huffman_descriptor_size = 16;
inline constexpr std::uint16_t blocked_huffman_model_size = 256;
inline constexpr std::uint8_t blocked_huffman_raw_flag = 1U;

struct BlockedHuffmanDescriptor {
    std::uint32_t symbol_count{};
    std::uint32_t payload_size{};
    std::uint16_t model_size{};
    std::uint8_t flags{};
    std::uint8_t final_valid_bits{};
};

enum class BlockedHuffmanFormatError : std::uint8_t {
    none,
    invalid_symbol_count,
    invalid_payload_size,
    invalid_model_size,
    unknown_flags,
    invalid_final_bits,
    nonzero_reserved,
    contradictory_representation,
    invalid_block_count,
    descriptor_size_mismatch,
    payload_size_mismatch,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] BlockedHuffmanFormatError validate_block_descriptor(
    const BlockedHuffmanDescriptor& descriptor,
    std::uint32_t expected_symbol_count,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] BlockedHuffmanFormatError parse_block_descriptor(
    std::span<const std::byte, blocked_huffman_descriptor_size> input,
    std::uint32_t expected_symbol_count,
    const core::DecoderLimits& limits,
    BlockedHuffmanDescriptor& descriptor) noexcept;

[[nodiscard]] BlockedHuffmanFormatError serialize_block_descriptor(
    const BlockedHuffmanDescriptor& descriptor,
    std::uint32_t expected_symbol_count,
    const core::DecoderLimits& limits,
    std::span<std::byte, blocked_huffman_descriptor_size> output) noexcept;

[[nodiscard]] BlockedHuffmanFormatError validate_blocked_huffman_layout(
    std::span<const BlockedHuffmanDescriptor> descriptors,
    std::uint32_t dictionary_serialized_size,
    std::uint32_t entropy_block_size,
    std::uint32_t declared_descriptor_bytes,
    std::uint32_t declared_payload_bytes,
    const core::DecoderLimits& limits) noexcept;

} // namespace marc::entropy::internal

#endif
