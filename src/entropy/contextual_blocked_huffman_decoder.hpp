#ifndef MARC_ENTROPY_CONTEXTUAL_BLOCKED_HUFFMAN_DECODER_HPP
#define MARC_ENTROPY_CONTEXTUAL_BLOCKED_HUFFMAN_DECODER_HPP

#include "core/limits.hpp"
#include "entropy/contextual_blocked_huffman_format.hpp"
#include "entropy/huffman_decode_table.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

enum class ContextualBlockedHuffmanDecodeError : std::uint8_t {
    none,
    invalid_descriptor,
    payload_size_mismatch,
    table_output_too_small,
    overlapping_buffers,
    nonzero_padding,
    invalid_table,
    truncated_bits,
    invalid_code,
    trailing_bits,
    invalid_context,
    invalid_alphabet,
    inactive_context,
    invalid_bypass_width,
    decision_count_exceeded,
    arithmetic_overflow,
    count_mismatch,
    unused_override,
    not_started,
    already_finished,
    internal_error,
};

struct ContextualBlockedHuffmanDecodeResult {
    std::uint32_t event_count{};
    std::uint32_t decision_count{};
    std::size_t bits_consumed{};
    ContextualBlockedHuffmanDecodeError error{
        ContextualBlockedHuffmanDecodeError::none};
};

[[nodiscard]] std::size_t
contextual_blocked_huffman_required_decode_table_count(
    const ContextualBlockedHuffmanDescriptor& descriptor) noexcept;

class ContextualBlockedHuffmanDecoder {
public:
    [[nodiscard]] ContextualBlockedHuffmanDecodeResult begin(
        const ContextualBlockedHuffmanDescriptor& descriptor,
        std::span<const std::byte> payload,
        const core::DecoderLimits& limits,
        std::span<HuffmanDecodeTable> table_output) noexcept;

    [[nodiscard]] ContextualBlockedHuffmanDecodeResult decode_symbol(
        std::uint16_t expected_context,
        std::uint16_t expected_alphabet,
        std::uint32_t& value) noexcept;

    [[nodiscard]] ContextualBlockedHuffmanDecodeResult decode_bypass(
        std::uint8_t expected_bit_count,
        std::uint32_t& value) noexcept;

    [[nodiscard]] ContextualBlockedHuffmanDecodeResult finish(
        std::uint32_t expected_event_count,
        std::uint32_t expected_decision_count) noexcept;

private:
    struct ModelSelection {
        std::uint16_t single_symbol{
            contextual_blocked_huffman_no_single_symbol};
        std::uint8_t table_index{UINT8_MAX};
        bool active{};
    };

    void reset() noexcept;
    [[nodiscard]] ContextualBlockedHuffmanDecodeResult result() const noexcept;
    [[nodiscard]] ContextualBlockedHuffmanDecodeResult fail(
        ContextualBlockedHuffmanDecodeError error) noexcept;
    [[nodiscard]] ContextualBlockedHuffmanDecodeError decode_model(
        const ModelSelection& model, std::uint32_t& value) noexcept;

    std::span<const std::byte> payload_{};
    std::span<const HuffmanDecodeTable> tables_{};
    std::array<ModelSelection,
               contextual_blocked_huffman_field_table_count>
        field_models_{};
    std::array<ModelSelection,
               context::internal::lzss_field_context_count>
        override_models_{};
    std::array<bool, context::internal::lzss_field_context_count>
        requested_overrides_{};
    std::uint32_t override_mask_{};
    std::size_t total_bits_{};
    std::size_t bit_offset_{};
    std::uint32_t expected_decisions_{};
    std::uint32_t event_count_{};
    std::uint32_t decision_count_{};
    ContextualBlockedHuffmanDecodeError error_{
        ContextualBlockedHuffmanDecodeError::not_started};
    bool started_{};
    bool finished_{};
};

} // namespace marc::entropy::internal

#endif
