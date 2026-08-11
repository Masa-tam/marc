#ifndef MARC_ENTROPY_CONTEXTUAL_RANS_DECODER_HPP
#define MARC_ENTROPY_CONTEXTUAL_RANS_DECODER_HPP

#include "core/limits.hpp"
#include "entropy/contextual_rans_decode_tables.hpp"
#include "entropy/contextual_rans_format.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

enum class ContextualRansDecodeError : std::uint8_t {
    none,
    invalid_descriptor,
    payload_size_mismatch,
    table_output_too_small,
    invalid_state,
    invalid_table,
    truncated_payload,
    invalid_context,
    invalid_alphabet,
    inactive_context,
    invalid_bypass_width,
    decision_count_exceeded,
    arithmetic_overflow,
    count_mismatch,
    unused_context,
    invalid_terminal_state,
    trailing_payload,
    not_started,
    already_finished,
};

struct ContextualRansDecodeResult {
    std::uint32_t event_count{};
    std::uint32_t decision_count{};
    std::size_t payload_consumed{};
    ContextualRansDecodeError error{ContextualRansDecodeError::none};
};

struct ContextualRansBeginResult {
    ContextualRansDecodeResult decode{};
    ContextualRansFormatError format_error{ContextualRansFormatError::none};
};

class ContextualRansDecoder {
public:
    [[nodiscard]] ContextualRansBeginResult begin(
        std::span<const std::byte> serialized_descriptor,
        std::uint32_t expected_decision_count,
        std::uint32_t expected_payload_size,
        std::span<const std::byte> payload,
        const core::DecoderLimits& limits,
        std::span<RansDecodeEntry> table_output) noexcept;

    [[nodiscard]] ContextualRansDecodeResult decode_symbol(
        std::uint16_t expected_context,
        std::uint16_t expected_alphabet,
        std::uint32_t& value) noexcept;

    [[nodiscard]] ContextualRansDecodeResult decode_bypass(
        std::uint8_t expected_bit_count,
        std::uint32_t& value) noexcept;

    [[nodiscard]] ContextualRansDecodeResult finish(
        std::uint32_t expected_event_count,
        std::uint32_t expected_decision_count) noexcept;

private:
    void reset() noexcept;
    [[nodiscard]] ContextualRansDecodeResult begin_validated(
        const ContextualRansDescriptor& descriptor,
        std::span<const std::byte> payload,
        std::span<RansDecodeEntry> table_output) noexcept;
    [[nodiscard]] ContextualRansDecodeResult result() const noexcept;
    [[nodiscard]] ContextualRansDecodeResult fail(
        ContextualRansDecodeError error) noexcept;
    [[nodiscard]] ContextualRansDecodeError decode_range(
        std::uint16_t cumulative, std::uint16_t frequency) noexcept;
    [[nodiscard]] bool boundary_state() const noexcept;

    std::span<const std::byte> payload_{};
    std::span<const RansDecodeEntry> tables_{};
    std::array<bool, contextual_rans_context_count> active_contexts_{};
    std::array<bool, contextual_rans_context_count> requested_contexts_{};
    std::uint64_t state_{};
    std::size_t payload_offset_{};
    std::uint32_t expected_decisions_{};
    std::uint32_t event_count_{};
    std::uint32_t decision_count_{};
    ContextualRansDecodeError error_{ContextualRansDecodeError::not_started};
    bool started_{};
    bool finished_{};
};

} // namespace marc::entropy::internal

#endif
