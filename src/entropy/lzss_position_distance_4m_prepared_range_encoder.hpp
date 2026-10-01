// Private range-only trial; no frame or public admission.
#ifndef MARC_ENTROPY_LZSS_POSITION_DISTANCE_4M_PREPARED_RANGE_ENCODER_HPP
#define MARC_ENTROPY_LZSS_POSITION_DISTANCE_4M_PREPARED_RANGE_ENCODER_HPP
#include "entropy/contextual_dynamic_range_encoder.hpp"

namespace marc::entropy::internal {
// Includes retained plan metadata and the scalar model/writer/cursor workspace.
[[nodiscard]] std::size_t lzss_position_distance_4m_prepared_range_state_bytes() noexcept;

// Borrowed operations must remain alive and unchanged through write. No owned
// buffer or allocation. Preparation failures preserve the caller descriptor.
// Output and descriptor must be disjoint from operations and this object.
// Ordinary write attempts consume readiness; alias rejection involving this
// object or descriptor preserves it. Only successful output may be consumed.
class PreparedLzssPositionDistance4mEncode {
public:
    PreparedLzssPositionDistance4mEncode() noexcept = default;
    PreparedLzssPositionDistance4mEncode(const PreparedLzssPositionDistance4mEncode&) = delete;
    PreparedLzssPositionDistance4mEncode& operator=(const PreparedLzssPositionDistance4mEncode&) = delete;
    PreparedLzssPositionDistance4mEncode(PreparedLzssPositionDistance4mEncode&&) = delete;
    PreparedLzssPositionDistance4mEncode& operator=(PreparedLzssPositionDistance4mEncode&&) = delete;
    [[nodiscard]] ContextualDynamicRangeEncodeResult prepare(
        std::span<const context::internal::ModeledOperation> operations,
        const core::DecoderLimits& limits, ContextualDynamicRangeDescriptor& descriptor) noexcept;
    [[nodiscard]] ContextualDynamicRangeEncodeResult write(
        std::span<std::byte> output, ContextualDynamicRangeDescriptor& descriptor) noexcept;
private:
    std::span<const context::internal::ModeledOperation> operations_{};
    ContextualDynamicRangeEncodeResult plan_{};
    ContextualDynamicRangeDescriptor descriptor_{};
    bool ready_{};
};
} // namespace marc::entropy::internal
#endif
