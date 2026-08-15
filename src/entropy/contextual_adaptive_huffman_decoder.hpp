#ifndef MARC_ENTROPY_CONTEXTUAL_ADAPTIVE_HUFFMAN_DECODER_HPP
#define MARC_ENTROPY_CONTEXTUAL_ADAPTIVE_HUFFMAN_DECODER_HPP

#include "core/limits.hpp"
#include "entropy/contextual_adaptive_huffman_format.hpp"
#include "entropy/contextual_adaptive_huffman_model.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

enum class ContextualAdaptiveHuffmanDecodeError : std::uint8_t {
    none,
    invalid_context_variant,
    invalid_descriptor,
    payload_size_mismatch,
    node_workspace_too_small,
    symbol_workspace_too_small,
    overlapping_buffers,
    limit_exceeded,
    nonzero_padding,
    invalid_context,
    invalid_alphabet,
    invalid_bypass_width,
    decision_count_exceeded,
    truncated_bits,
    invalid_path,
    invalid_nyt_symbol,
    tree_error,
    arithmetic_overflow,
    count_mismatch,
    trailing_bits,
    not_started,
    already_finished,
};

struct ContextualAdaptiveHuffmanDecodeResult {
    std::uint32_t event_count{};
    std::uint32_t decision_count{};
    std::size_t bits_consumed{};
    ContextualAdaptiveHuffmanDecodeError error{
        ContextualAdaptiveHuffmanDecodeError::none};
};

class ContextualAdaptiveHuffmanDecoder {
public:
    [[nodiscard]] ContextualAdaptiveHuffmanDecodeResult begin(
        const ContextualAdaptiveHuffmanDescriptor& descriptor,
        std::span<const std::byte> payload,
        const core::DecoderLimits& limits,
        std::span<AdaptiveHuffmanNode> node_storage,
        std::span<std::uint16_t> symbol_storage,
        context::internal::LzssFieldContextVariant variant =
            context::internal::LzssFieldContextVariant::
                field_context_64k) noexcept;

    [[nodiscard]] ContextualAdaptiveHuffmanDecodeResult decode_symbol(
        std::uint16_t expected_context,
        std::uint16_t expected_alphabet,
        std::uint32_t& value) noexcept;

    [[nodiscard]] ContextualAdaptiveHuffmanDecodeResult decode_bypass(
        std::uint8_t expected_bit_count,
        std::uint32_t& value) noexcept;

    [[nodiscard]] ContextualAdaptiveHuffmanDecodeResult finish(
        std::uint32_t expected_event_count,
        std::uint32_t expected_decision_count) noexcept;

private:
    void reset() noexcept;
    [[nodiscard]] ContextualAdaptiveHuffmanDecodeResult result() const noexcept;
    [[nodiscard]] ContextualAdaptiveHuffmanDecodeResult fail(
        ContextualAdaptiveHuffmanDecodeError error) noexcept;
    [[nodiscard]] bool read_bit(std::size_t& offset,
                                std::uint8_t& bit) const noexcept;

    std::span<const std::byte> payload_{};
    ContextualAdaptiveHuffmanModelBank models_{};
    context::internal::LzssFieldContextLayout layout_{};
    std::size_t total_bits_{};
    std::size_t bit_offset_{};
    std::uint32_t expected_decisions_{};
    std::uint32_t event_count_{};
    std::uint32_t decision_count_{};
    ContextualAdaptiveHuffmanDecodeError error_{
        ContextualAdaptiveHuffmanDecodeError::not_started};
    bool started_{};
    bool finished_{};
};

} // namespace marc::entropy::internal

#endif
