#ifndef MARC_ENTROPY_BLOCKED_HUFFMAN_CONTROLLER_HPP
#define MARC_ENTROPY_BLOCKED_HUFFMAN_CONTROLLER_HPP

#include "core/limits.hpp"
#include "entropy/blocked_huffman_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

struct BlockedHuffmanBlockView {
    BlockedHuffmanDescriptor descriptor{};
    std::uint32_t model_offset{};
    std::uint32_t payload_offset{};
};

enum class BlockedHuffmanControllerError : std::uint8_t {
    none,
    invalid_block_count,
    output_views_too_small,
    truncated_descriptor,
    truncated_model,
    trailing_descriptor_bytes,
    invalid_descriptor,
    invalid_model,
    code_length_limit,
    decode_table_limit,
    payload_size_mismatch,
    limit_exceeded,
    arithmetic_overflow,
    internal_error,
};

struct BlockedHuffmanControllerResult {
    std::size_t block_count{};
    BlockedHuffmanControllerError error{BlockedHuffmanControllerError::none};
};

[[nodiscard]] BlockedHuffmanControllerResult
parse_blocked_huffman_descriptor_region(
    std::span<const std::byte> descriptor_region,
    std::uint32_t dictionary_serialized_size,
    std::uint32_t entropy_block_size,
    std::uint32_t declared_block_count,
    std::uint32_t declared_payload_size,
    const core::DecoderLimits& limits,
    std::span<BlockedHuffmanBlockView> views) noexcept;

} // namespace marc::entropy::internal

#endif
