#ifndef MARC_ENTROPY_CONTEXTUAL_BLOCKED_HUFFMAN_ENCODER_HPP
#define MARC_ENTROPY_CONTEXTUAL_BLOCKED_HUFFMAN_ENCODER_HPP

#include "context/lzss_field_context.hpp"
#include "entropy/contextual_blocked_huffman_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

enum class ContextualBlockedHuffmanEncodeError : std::uint8_t {
    none,
    invalid_operation,
    frequency_overflow,
    huffman_build_error,
    payload_output_too_small,
    overlapping_buffers,
    limit_exceeded,
    arithmetic_overflow,
    internal_error,
};

struct ContextualBlockedHuffmanEncodeResult {
    std::size_t operation_count{};
    std::size_t operation_index{};
    std::uint32_t decision_count{};
    std::size_t descriptor_size{};
    std::size_t payload_size{};
    ContextualBlockedHuffmanEncodeError error{
        ContextualBlockedHuffmanEncodeError::none};
};

[[nodiscard]] ContextualBlockedHuffmanEncodeResult
plan_contextual_blocked_huffman_operations(
    std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits,
    ContextualBlockedHuffmanDescriptor& descriptor) noexcept;

[[nodiscard]] ContextualBlockedHuffmanEncodeResult
encode_contextual_blocked_huffman_operations(
    std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits, std::span<std::byte> payload_output,
    ContextualBlockedHuffmanDescriptor& descriptor) noexcept;

} // namespace marc::entropy::internal

#endif
