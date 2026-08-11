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
    invalid_descriptor,
    not_started,
    already_finished,
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

class ContextualAdaptiveHuffmanForwardEncoder final {
public:
    [[nodiscard]] ContextualAdaptiveHuffmanEncodeResult begin_plan(
        const core::DecoderLimits& limits,
        std::span<AdaptiveHuffmanNode> node_workspace,
        std::span<std::uint16_t> symbol_workspace) noexcept;

    [[nodiscard]] ContextualAdaptiveHuffmanEncodeResult begin_write(
        const ContextualAdaptiveHuffmanDescriptor& descriptor,
        const core::DecoderLimits& limits,
        std::span<AdaptiveHuffmanNode> node_workspace,
        std::span<std::uint16_t> symbol_workspace,
        std::span<std::byte> payload_output) noexcept;

    [[nodiscard]] ContextualAdaptiveHuffmanEncodeResult encode_symbol(
        std::uint16_t context_id, std::uint16_t alphabet_size,
        std::uint32_t value) noexcept;
    [[nodiscard]] ContextualAdaptiveHuffmanEncodeResult encode_bypass(
        std::uint8_t bit_count, std::uint32_t value) noexcept;

    [[nodiscard]] ContextualAdaptiveHuffmanEncodeResult finish_plan(
        ContextualAdaptiveHuffmanDescriptor& descriptor) noexcept;
    [[nodiscard]] ContextualAdaptiveHuffmanEncodeResult finish_write(
        std::size_t expected_operation_count,
        std::uint32_t expected_decision_count,
        std::size_t expected_payload_bits) noexcept;

private:
    [[nodiscard]] ContextualAdaptiveHuffmanEncodeResult begin_common(
        const core::DecoderLimits& limits,
        std::span<AdaptiveHuffmanNode> node_workspace,
        std::span<std::uint16_t> symbol_workspace,
        std::span<std::byte> payload_output) noexcept;
    [[nodiscard]] ContextualAdaptiveHuffmanEncodeResult fail(
        ContextualAdaptiveHuffmanEncodeError error) noexcept;

    ContextualAdaptiveHuffmanModelBank models_{};
    core::DecoderLimits limits_{};
    std::span<std::byte> output_{};
    ContextualAdaptiveHuffmanDescriptor expected_{};
    ContextualAdaptiveHuffmanEncodeResult result_{};
    bool started_{};
    bool writing_{};
    bool finished_{};
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
