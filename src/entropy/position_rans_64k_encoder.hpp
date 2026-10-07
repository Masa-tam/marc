#ifndef MARC_ENTROPY_POSITION_RANS_64K_ENCODER_HPP
#define MARC_ENTROPY_POSITION_RANS_64K_ENCODER_HPP
#include "entropy/position_rans_64k_format.hpp"
#include "entropy/rans_format.hpp"

namespace marc::entropy::internal {
enum class PositionRans64kEncodeError : std::uint8_t {
    none, invalid_context, invalid_symbol, arithmetic_overflow,
    invalid_model, output_too_small, count_mismatch, invalid_state, already_finished
};
class PositionRans64kModelBuilder {
public:
    [[nodiscard]] PositionRans64kEncodeError add(int context_id, std::uint32_t symbol) noexcept;
    [[nodiscard]] PositionRans64kEncodeError finish(PositionRans64kDescriptor& output) const noexcept;
private:
    std::array<std::uint32_t,2522> counts_{};
    std::array<std::uint32_t,40> totals_{};
    std::uint32_t decisions_{};
};
// A reverse writer writes only caller-provided PRIVATE scratch. It does not
// provide frame publication. Empty scratch measures the exact payload size.
class PositionRans64kReverseWriter {
public:
    PositionRans64kReverseWriter(const PositionRans64kDescriptor& descriptor,
        const core::DecoderLimits& limits, std::span<std::byte> scratch) noexcept;
    [[nodiscard]] PositionRans64kEncodeError write(int context_id, std::uint32_t symbol) noexcept;
    [[nodiscard]] PositionRans64kEncodeError finish(std::size_t& payload_size) noexcept;
private:
    const PositionRans64kDescriptor& descriptor_;
    std::span<std::byte> scratch_{};
    std::uint64_t state_{rans_lower_bound};
    std::size_t cursor_{}, emitted_{};
    std::uint32_t count_{};
    PositionRans64kEncodeError error_{PositionRans64kEncodeError::none};
    bool finished_{};
};
} // namespace marc::entropy::internal
#endif
