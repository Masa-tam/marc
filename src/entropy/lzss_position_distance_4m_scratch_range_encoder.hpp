#ifndef MARC_ENTROPY_LZSS_POSITION_DISTANCE_4M_SCRATCH_RANGE_ENCODER_HPP
#define MARC_ENTROPY_LZSS_POSITION_DISTANCE_4M_SCRATCH_RANGE_ENCODER_HPP
#include "entropy/lzss_position_distance_4m_range_encoder.hpp"
namespace marc::entropy::internal {
// Private discardable scratch writer. Operations/configuration remain stable.
// A failed writing run may modify scratch; descriptor is committed only on success.
// Conservative bound/capacity/limit failures use the scalar encoder. No allocation.
[[nodiscard]] ContextualDynamicRangeEncodeResult encode_lzss_position_distance_4m_range_operations_scratch(
    std::span<const context::internal::ModeledOperation>,const core::DecoderLimits&,
    std::span<std::byte>,ContextualDynamicRangeDescriptor&) noexcept;
}
#endif
