#ifndef MARC_ENTROPY_POSITION_RANS_16M_FULL_LITERAL_DECODER_HPP
#define MARC_ENTROPY_POSITION_RANS_16M_FULL_LITERAL_DECODER_HPP
#include "entropy/position_rans_16m_full_literal_format.hpp"

namespace marc::entropy::internal {
enum class PositionRans16mFullLiteralDecodeError : std::uint8_t {
    none, invalid_descriptor, invalid_state, invalid_context, inactive_context,
    truncated_payload, decision_count_exceeded, count_mismatch, unused_context,
    invalid_terminal_state, trailing_payload, not_started, already_finished
};
class PositionRans16mFullLiteralDecoder {
public:
    [[nodiscard]] PositionRans16mFullLiteralDecodeError begin(
        std::span<const std::byte> descriptor, std::uint32_t expected_decisions,
        std::span<const std::byte> payload, const core::DecoderLimits& limits) noexcept;
    // -1 selects a uniform binary decision. Ordinary IDs are 0..55.
    [[nodiscard]] PositionRans16mFullLiteralDecodeError read(
        int context_id, std::uint32_t& symbol) noexcept;
    [[nodiscard]] PositionRans16mFullLiteralDecodeError finish() noexcept;
private:
    PositionRans16mFullLiteralDescriptor descriptor_{};
    std::span<const std::byte> payload_{};
    std::array<bool, 56> used_{};
    std::uint64_t state_{};
    std::size_t cursor_{};
    std::uint32_t count_{};
    PositionRans16mFullLiteralDecodeError error_{PositionRans16mFullLiteralDecodeError::not_started};
    bool finished_{};
};
} // namespace marc::entropy::internal
#endif
