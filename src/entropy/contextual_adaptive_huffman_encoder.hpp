#ifndef MARC_ENTROPY_CONTEXTUAL_ADAPTIVE_HUFFMAN_ENCODER_HPP
#define MARC_ENTROPY_CONTEXTUAL_ADAPTIVE_HUFFMAN_ENCODER_HPP

#include "context/lzss_field_context.hpp"
#include "core/limits.hpp"
#include "entropy/contextual_adaptive_huffman_format.hpp"
#include "entropy/contextual_adaptive_huffman_model.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

enum class ContextualAdaptiveHuffmanEncodeError : std::uint8_t {
    none,
    empty_operations,
    invalid_operation_kind,
    invalid_context,
    invalid_alphabet,
    invalid_symbol,
    invalid_bypass_width,
    nonzero_unused_field,
    node_workspace_too_small,
    symbol_workspace_too_small,
    payload_output_too_small,
    overlapping_buffers,
    limit_exceeded,
    arithmetic_overflow,
    tree_error,
    internal_error,
};

struct ContextualAdaptiveHuffmanEncodeResult {
    std::size_t operation_count{};
    std::size_t operation_index{};
    std::uint32_t decision_count{};
    std::size_t payload_size{};
    std::size_t payload_bits{};
    ContextualAdaptiveHuffmanEncodeError error{
        ContextualAdaptiveHuffmanEncodeError::none};
};

[[nodiscard]] ContextualAdaptiveHuffmanEncodeResult
plan_contextual_adaptive_huffman_operations(
    std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits,
    std::span<AdaptiveHuffmanNode> node_workspace,
    std::span<std::uint16_t> symbol_workspace,
    ContextualAdaptiveHuffmanDescriptor& descriptor) noexcept;

[[nodiscard]] ContextualAdaptiveHuffmanEncodeResult
encode_contextual_adaptive_huffman_operations(
    std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits,
    std::span<AdaptiveHuffmanNode> node_workspace,
    std::span<std::uint16_t> symbol_workspace,
    std::span<std::byte> payload_output,
    ContextualAdaptiveHuffmanDescriptor& descriptor) noexcept;

} // namespace marc::entropy::internal

#endif
