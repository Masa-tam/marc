#ifndef MARC_ENTROPY_LZSS_REDUCED_LITERAL_RANGE_DECODER_HPP
#define MARC_ENTROPY_LZSS_REDUCED_LITERAL_RANGE_DECODER_HPP

#include "entropy/lzss_reduced_literal_range_state.hpp"
#include "core/limits.hpp"

namespace marc::entropy::internal {

// Private operation decoder. The caller supplies token-grammar context IDs.
// finish checks counts, consumed extent and model invariants; frame-level
// canonical re-encoding remains required before publishing decoded bytes.
class LzssReducedLiteralRangeDecoder {
public:
    [[nodiscard]] ContextualDynamicRangeDecodeResult begin(
        const ContextualDynamicRangeDescriptor& descriptor,
        std::span<const std::byte> payload, const core::DecoderLimits& limits) noexcept;
    [[nodiscard]] ContextualDynamicRangeDecodeResult decode_symbol(
        std::uint16_t context_id, std::uint16_t alphabet, std::uint32_t& value) noexcept;
    [[nodiscard]] ContextualDynamicRangeDecodeResult decode_bypass(
        std::uint8_t bit_count, std::uint32_t& value) noexcept;
    [[nodiscard]] ContextualDynamicRangeDecodeResult finish(
        std::uint32_t event_count, std::uint32_t decision_count) noexcept;
private:
    [[nodiscard]] ContextualDynamicRangeDecodeResult result() const noexcept;
    [[nodiscard]] ContextualDynamicRangeDecodeResult fail(ContextualDynamicRangeDecodeError error) noexcept;
    [[nodiscard]] bool decode_interval(std::uint32_t cumulative, std::uint16_t frequency,
        std::uint32_t total) noexcept;
    [[nodiscard]] bool validate_models() const noexcept;
    LzssReducedLiteralRangeState state_{};
};
static_assert(sizeof(LzssReducedLiteralRangeDecoder) == sizeof(LzssReducedLiteralRangeState));

} // namespace marc::entropy::internal
#endif
