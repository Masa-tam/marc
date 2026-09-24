#ifndef MARC_ENTROPY_LZSS_SHORT_MATCH_RANGE_DECODER_HPP
#define MARC_ENTROPY_LZSS_SHORT_MATCH_RANGE_DECODER_HPP

#include "context/lzss_short_match_context_layout.hpp"
#include "core/limits.hpp"
#include "entropy/contextual_dynamic_range_decoder.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

// Private Format 2.0 32-context model shared by context variants 6 and 7.
// No published stream selector constructs it; the earlier 31-context model
// storage is unchanged.
class LzssShortMatchRangeDecoder {
public:
    [[nodiscard]] ContextualDynamicRangeDecodeResult begin(
        const ContextualDynamicRangeDescriptor& descriptor,
        std::span<const std::byte> payload,
        const core::DecoderLimits& limits) noexcept;

    [[nodiscard]] ContextualDynamicRangeDecodeResult decode_symbol(
        std::uint16_t context_id, std::uint16_t alphabet,
        std::uint32_t& value) noexcept;

    [[nodiscard]] ContextualDynamicRangeDecodeResult decode_bypass(
        std::uint8_t bit_count, std::uint32_t& value) noexcept;

    [[nodiscard]] ContextualDynamicRangeDecodeResult finish(
        std::uint32_t event_count, std::uint32_t decision_count) noexcept;

private:
    [[nodiscard]] ContextualDynamicRangeDecodeResult result() const noexcept;
    [[nodiscard]] ContextualDynamicRangeDecodeResult fail(
        ContextualDynamicRangeDecodeError error) noexcept;
    [[nodiscard]] bool decode_interval(std::uint32_t cumulative,
                                       std::uint16_t frequency,
                                       std::uint32_t total) noexcept;
    [[nodiscard]] bool validate_models() const noexcept;

    std::array<std::uint16_t,
               context::internal::lzss_short_match_frequency_entries>
        frequencies_{};
    std::array<std::uint32_t,
               context::internal::lzss_short_match_context_count>
        totals_{};
    std::span<const std::byte> payload_{};
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
