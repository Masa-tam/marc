#ifndef MARC_ENTROPY_POSITION_RANS_16M_FULL_LITERAL_ENCODER_HPP
#define MARC_ENTROPY_POSITION_RANS_16M_FULL_LITERAL_ENCODER_HPP
#include "entropy/position_rans_16m_full_literal_format.hpp"
#include "entropy/rans_format.hpp"

namespace marc::entropy::internal {
enum class PositionRans16mFullLiteralEncodeError : std::uint8_t {
    none, invalid_context, invalid_symbol, arithmetic_overflow,
    invalid_model, output_too_small, count_mismatch, invalid_state, already_finished
};
class PositionRans16mFullLiteralModelBuilder {
public:
    [[nodiscard]] PositionRans16mFullLiteralEncodeError add(int context_id, std::uint32_t symbol) noexcept;
    [[nodiscard]] PositionRans16mFullLiteralEncodeError finish(PositionRans16mFullLiteralDescriptor& output) const noexcept;
private:
    std::array<std::uint32_t,4658> counts_{};
    std::array<std::uint32_t,56> totals_{};
    std::uint32_t decisions_{};
};
// A reverse writer writes only caller-provided PRIVATE scratch. It does not
// provide frame publication. Empty scratch measures the exact payload size.
class PositionRans16mFullLiteralReverseWriter {
public:
    PositionRans16mFullLiteralReverseWriter(const PositionRans16mFullLiteralDescriptor& descriptor,
        const core::DecoderLimits& limits, std::span<std::byte> scratch) noexcept;
    [[nodiscard]] PositionRans16mFullLiteralEncodeError write(int context_id, std::uint32_t symbol) noexcept;
    [[nodiscard]] PositionRans16mFullLiteralEncodeError finish(std::size_t& payload_size) noexcept;
private:
    const PositionRans16mFullLiteralDescriptor& descriptor_;
    std::span<std::byte> scratch_{};
    std::uint64_t state_{rans_lower_bound};
    std::size_t cursor_{}, emitted_{};
    std::uint32_t count_{};
    PositionRans16mFullLiteralEncodeError error_{PositionRans16mFullLiteralEncodeError::none};
    bool finished_{};
};
} // namespace marc::entropy::internal
#endif
