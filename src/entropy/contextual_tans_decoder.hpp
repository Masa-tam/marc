#ifndef MARC_ENTROPY_CONTEXTUAL_TANS_DECODER_HPP
#define MARC_ENTROPY_CONTEXTUAL_TANS_DECODER_HPP

#include "core/limits.hpp"
#include "entropy/contextual_tans_decode_tables.hpp"
#include "entropy/contextual_tans_format.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

enum class ContextualTansDecodeError : std::uint8_t {
    none,
    invalid_descriptor,
    payload_size_mismatch,
    table_output_too_small,
    invalid_state,
    invalid_table,
    truncated_bits,
    trailing_bits,
    nonzero_padding,
    invalid_context,
    invalid_alphabet,
    inactive_context,
    invalid_bypass_width,
    decision_count_exceeded,
    count_mismatch,
    unused_context,
    invalid_terminal_state,
    not_started,
    already_finished,
};

struct ContextualTansDecodeResult {
    std::uint32_t event_count{};
    std::uint32_t decision_count{};
    std::size_t bits_consumed{};
    ContextualTansDecodeError error{ContextualTansDecodeError::none};
};

class ContextualTansDecoder {
public:
    [[nodiscard]] ContextualTansDecodeResult begin(
        const ContextualTansDescriptor& descriptor,
        std::span<const std::byte> payload,
        const core::DecoderLimits& limits,
        std::span<TansDecodeEntry> table_output) noexcept;

    [[nodiscard]] ContextualTansDecodeResult decode_symbol(
        std::uint16_t expected_context,
        std::uint16_t expected_alphabet,
        std::uint32_t& value) noexcept;

    [[nodiscard]] ContextualTansDecodeResult decode_bypass(
        std::uint8_t expected_bit_count,
        std::uint32_t& value) noexcept;

    [[nodiscard]] ContextualTansDecodeResult finish(
        std::uint32_t expected_event_count,
        std::uint32_t expected_decision_count) noexcept;

private:
    void reset() noexcept;
    [[nodiscard]] ContextualTansDecodeResult result() const noexcept;
    [[nodiscard]] ContextualTansDecodeResult fail(
        ContextualTansDecodeError error) noexcept;
    [[nodiscard]] ContextualTansDecodeError decode_transition(
        std::size_t table_index,
        std::uint16_t expected_alphabet,
        std::uint32_t& symbol) noexcept;

    std::span<const std::byte> payload_{};
    std::span<const TansDecodeEntry> tables_{};
    std::array<bool, contextual_tans_context_count> active_contexts_{};
    std::array<bool, contextual_tans_context_count> requested_contexts_{};
    std::uint32_t state_{};
    std::size_t total_bits_{};
    std::size_t bit_offset_{};
    std::uint32_t expected_decisions_{};
    std::uint32_t event_count_{};
    std::uint32_t decision_count_{};
    ContextualTansDecodeError error_{ContextualTansDecodeError::not_started};
    bool started_{};
    bool finished_{};
};

} // namespace marc::entropy::internal

#endif
