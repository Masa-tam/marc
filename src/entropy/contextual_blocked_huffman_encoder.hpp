#ifndef MARC_ENTROPY_CONTEXTUAL_BLOCKED_HUFFMAN_ENCODER_HPP
#define MARC_ENTROPY_CONTEXTUAL_BLOCKED_HUFFMAN_ENCODER_HPP

#include "context/lzss_field_context.hpp"
#include "entropy/contextual_blocked_huffman_format.hpp"

#include <array>
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

class ContextualBlockedHuffmanModelBuilder final {
public:
    [[nodiscard]] ContextualBlockedHuffmanEncodeError add_symbol(
        std::uint16_t context_id, std::uint16_t alphabet_size,
        std::uint32_t value) noexcept;
    [[nodiscard]] ContextualBlockedHuffmanEncodeError add_bypass(
        std::uint8_t bit_count, std::uint32_t value) noexcept;
    [[nodiscard]] ContextualBlockedHuffmanEncodeResult finish(
        const core::DecoderLimits& limits,
        ContextualBlockedHuffmanDescriptor& descriptor) const noexcept;

    [[nodiscard]] std::size_t operation_count() const noexcept {
        return operation_count_;
    }

private:
    std::array<HuffmanFrequencies,
               context::internal::lzss_field_context_count>
        context_frequencies_{};
    std::array<HuffmanFrequencies,
               contextual_blocked_huffman_field_table_count>
        field_frequencies_{};
    std::uint64_t decision_count_{};
    std::uint64_t bypass_bits_{};
    std::size_t operation_count_{};
    ContextualBlockedHuffmanEncodeError error_{
        ContextualBlockedHuffmanEncodeError::none};
};

class ContextualBlockedHuffmanWriter final {
public:
    ContextualBlockedHuffmanWriter(
        const ContextualBlockedHuffmanDescriptor& descriptor,
        std::span<std::byte> payload_output) noexcept;

    [[nodiscard]] ContextualBlockedHuffmanEncodeError encode_symbol(
        std::uint16_t context_id, std::uint16_t alphabet_size,
        std::uint32_t value) noexcept;
    [[nodiscard]] ContextualBlockedHuffmanEncodeError encode_bypass(
        std::uint8_t bit_count, std::uint32_t value) noexcept;
    [[nodiscard]] ContextualBlockedHuffmanEncodeError finish(
        std::size_t expected_operation_count,
        std::uint32_t expected_decision_count) noexcept;

private:
    const ContextualBlockedHuffmanDescriptor* descriptor_{};
    std::span<std::byte> output_{};
    std::array<CanonicalHuffmanTable,
               contextual_blocked_huffman_max_table_count>
        tables_{};
    std::array<bool, contextual_blocked_huffman_max_table_count> built_{};
    std::uint64_t bit_offset_{};
    std::uint64_t decision_count_{};
    std::size_t operation_count_{};
    ContextualBlockedHuffmanEncodeError error_{
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
