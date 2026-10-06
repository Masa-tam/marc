#ifndef MARC_ENTROPY_POSITION_RANS_1M_ENCODER_HPP
#define MARC_ENTROPY_POSITION_RANS_1M_ENCODER_HPP
#include "entropy/position_rans_1m_format.hpp"
#include "entropy/rans_format.hpp"

namespace marc::entropy::internal {
enum class PositionRans1mEncodeError : std::uint8_t {
    none, invalid_context, invalid_symbol, arithmetic_overflow,
    invalid_model, output_too_small, count_mismatch, invalid_state, already_finished
};
class PositionRans1mModelBuilder {
public:
    [[nodiscard]] PositionRans1mEncodeError add(int context_id, std::uint32_t symbol) noexcept;
    [[nodiscard]] PositionRans1mEncodeError finish(PositionRans1mDescriptor& output) const noexcept;
private:
    std::array<std::uint32_t,2566> counts_{};
    std::array<std::uint32_t,44> totals_{};
    std::uint32_t decisions_{};
};
// A reverse writer writes only caller-provided PRIVATE scratch. It does not
// provide frame publication. Empty scratch measures the exact payload size.
class PositionRans1mReverseWriter {
public:
    PositionRans1mReverseWriter(const PositionRans1mDescriptor& descriptor,
        const core::DecoderLimits& limits, std::span<std::byte> scratch) noexcept;
    [[nodiscard]] PositionRans1mEncodeError write(int context_id, std::uint32_t symbol) noexcept;
    [[nodiscard]] PositionRans1mEncodeError finish(std::size_t& payload_size) noexcept;
private:
    const PositionRans1mDescriptor& descriptor_;
    std::span<std::byte> scratch_{};
    std::uint64_t state_{rans_lower_bound};
    std::size_t cursor_{}, emitted_{};
    std::uint32_t count_{};
    PositionRans1mEncodeError error_{PositionRans1mEncodeError::none};
    bool finished_{};
};
} // namespace marc::entropy::internal
#endif
