#ifndef MARC_ENTROPY_CONTEXTUAL_DYNAMIC_RANGE_DECODER_HPP
#define MARC_ENTROPY_CONTEXTUAL_DYNAMIC_RANGE_DECODER_HPP

#include "context/lzss_field_context_format.hpp"
#include "core/limits.hpp"
#include "entropy/contextual_dynamic_range_format.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

enum class ContextualDynamicRangeDecodeError : std::uint8_t {
    none,
    invalid_descriptor,
    payload_size_mismatch,
    invalid_interval,
    truncated_payload,
    invalid_context,
    invalid_alphabet,
    invalid_bypass_width,
    decision_count_exceeded,
    count_mismatch,
    trailing_payload,
    invalid_model,
    not_started,
    already_finished,
};

struct ContextualDynamicRangeDecodeResult {
    std::uint32_t event_count{};
    std::uint32_t decision_count{};
    std::size_t payload_consumed{};
    ContextualDynamicRangeDecodeError error{
        ContextualDynamicRangeDecodeError::none};
};

class ContextualDynamicRangeDecoder {
public:
    ContextualDynamicRangeDecoder() noexcept;

    [[nodiscard]] ContextualDynamicRangeDecodeResult begin(
        const ContextualDynamicRangeDescriptor& descriptor,
        std::span<const std::byte> payload,
        const core::DecoderLimits& limits) noexcept;

    [[nodiscard]] ContextualDynamicRangeDecodeResult decode_symbol(
        std::uint16_t expected_context,
        std::uint16_t expected_alphabet,
        std::uint32_t& value) noexcept;

    [[nodiscard]] ContextualDynamicRangeDecodeResult decode_bypass(
        std::uint8_t expected_bit_count,
        std::uint32_t& value) noexcept;

    [[nodiscard]] ContextualDynamicRangeDecodeResult finish(
        std::uint32_t expected_event_count,
        std::uint32_t expected_decision_count) noexcept;

private:
    [[nodiscard]] ContextualDynamicRangeDecodeResult result() const noexcept;
    [[nodiscard]] ContextualDynamicRangeDecodeResult fail(
        ContextualDynamicRangeDecodeError error) noexcept;
    [[nodiscard]] bool decode_interval(std::uint32_t cumulative,
                                       std::uint16_t frequency,
                                       std::uint32_t total) noexcept;
    void reset_models() noexcept;
    [[nodiscard]] bool validate_models() const noexcept;

    std::span<const std::byte> payload_{};
    std::array<std::uint16_t,
               marc::context::internal::lzss_field_context_frequency_entries>
        frequencies_{};
    std::array<std::uint32_t,
               marc::context::internal::lzss_field_context_count> totals_{};
    ContextualDynamicRangeDescriptor descriptor_{};
    std::size_t payload_offset_{};
    std::uint32_t code_{};
    std::uint32_t range_{UINT32_MAX};
    std::uint32_t event_count_{};
    std::uint32_t decision_count_{};
    ContextualDynamicRangeDecodeError error_{
        ContextualDynamicRangeDecodeError::not_started};
    bool started_{};
    bool finished_{};
};

} // namespace marc::entropy::internal

#endif
