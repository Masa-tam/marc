#ifndef MARC_ENTROPY_POSITION_RANS_8M_FULL_LITERAL_DECODER_HPP
#define MARC_ENTROPY_POSITION_RANS_8M_FULL_LITERAL_DECODER_HPP
#include "entropy/position_rans_8m_full_literal_format.hpp"

namespace marc::entropy::internal {
enum class PositionRans8mFullLiteralDecodeError : std::uint8_t {
    none, invalid_descriptor, invalid_state, invalid_context, inactive_context,
    truncated_payload, decision_count_exceeded, count_mismatch, unused_context,
    invalid_terminal_state, trailing_payload, not_started, already_finished
};
class PositionRans8mFullLiteralDecoder {
public:
    [[nodiscard]] PositionRans8mFullLiteralDecodeError begin(
        std::span<const std::byte> descriptor, std::uint32_t expected_decisions,
        std::span<const std::byte> payload, const core::DecoderLimits& limits) noexcept;
    // -1 selects a uniform binary decision. Ordinary IDs are 0..43.
    [[nodiscard]] PositionRans8mFullLiteralDecodeError read(
        int context_id, std::uint32_t& symbol) noexcept;
    [[nodiscard]] PositionRans8mFullLiteralDecodeError finish() noexcept;
private:
    PositionRans8mFullLiteralDescriptor descriptor_{};
    std::span<const std::byte> payload_{};
    std::array<bool, 55> used_{};
    std::uint64_t state_{};
    std::size_t cursor_{};
    std::uint32_t count_{};
    PositionRans8mFullLiteralDecodeError error_{PositionRans8mFullLiteralDecodeError::not_started};
    bool finished_{};
};
} // namespace marc::entropy::internal
#endif
