#ifndef MARC_ENTROPY_LZSS_POSITION_DISTANCE_RANGE_DECODER_HPP
#define MARC_ENTROPY_LZSS_POSITION_DISTANCE_RANGE_DECODER_HPP

#include "entropy/lzss_position_distance_range_state.hpp"
#include "core/limits.hpp"

namespace marc::entropy::internal {

// Private grammar-aware operation decoder. Frame/history validation is separate.
// finish checks counts, consumed extent, model invariants and canonical bytes
// replayed from the decoded intervals, including the five final carry shifts.
class LzssPositionDistanceRangeDecoder {
public:
    [[nodiscard]] ContextualDynamicRangeDecodeResult begin(
        const ContextualDynamicRangeDescriptor& descriptor,
        std::span<const std::byte> payload, const core::DecoderLimits& limits) noexcept;
    [[nodiscard]] ContextualDynamicRangeDecodeResult decode_next(
        context::internal::ModeledOperation& operation) noexcept;
    [[nodiscard]] ContextualDynamicRangeDecodeResult finish(
        std::uint32_t event_count, std::uint32_t decision_count) noexcept;
private:
    friend struct LzssPositionDistanceRangeDecoderTestAccess;
    friend struct LzssPositionDistanceRangeDecoderBenchmarkAccess;
    template<bool Reference>
    [[nodiscard]] ContextualDynamicRangeDecodeResult decode_next_impl(
        context::internal::ModeledOperation& operation) noexcept;
    [[nodiscard]] ContextualDynamicRangeDecodeResult decode_next_reference(
        context::internal::ModeledOperation& operation) noexcept;
    [[nodiscard]] ContextualDynamicRangeDecodeResult decode_distance_extra_reference(
        std::uint8_t bit_count, std::uint32_t& value) noexcept;
    [[nodiscard]] ContextualDynamicRangeDecodeResult decode_symbol(
        std::uint16_t context_id, std::uint16_t alphabet, std::uint32_t& value) noexcept;
    [[nodiscard]] ContextualDynamicRangeDecodeResult decode_bypass(
        std::uint8_t bit_count, std::uint32_t& value) noexcept;
    [[nodiscard]] ContextualDynamicRangeDecodeResult decode_distance_extra(
        std::uint8_t bit_count, std::uint32_t& value) noexcept;
    [[nodiscard]] ContextualDynamicRangeDecodeResult result() const noexcept;
    [[nodiscard]] ContextualDynamicRangeDecodeResult fail(ContextualDynamicRangeDecodeError error) noexcept;
    [[nodiscard]] bool decode_interval(std::uint32_t cumulative, std::uint16_t frequency,
        std::uint32_t total) noexcept;
    [[nodiscard]] bool advance_interval(std::uint32_t cumulative,
        std::uint16_t frequency, std::uint32_t unit) noexcept;
    [[nodiscard]] bool validate_models() const noexcept;
    [[nodiscard]] bool canonical_shift_low() noexcept;
    void canonical_emit(std::uint8_t value) noexcept;
    LzssPositionDistanceRangeState state_{};
};
static_assert(sizeof(LzssPositionDistanceRangeDecoder) == sizeof(LzssPositionDistanceRangeState));

} // namespace marc::entropy::internal
#endif
