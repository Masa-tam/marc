#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_4M_RANGE_PREPARED_FRAME_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_4M_RANGE_PREPARED_FRAME_HPP
#include "frame/lzss_position_distance_4m_frame.hpp"
namespace marc::frame::internal {
// Internal prepared path. Same output/error contract as the retained frame encoder.
// Scalar token mapping; retained range metadata is explicitly charged.
// Failed range preparation retries the scalar path after releasing the plan.
[[nodiscard]] LzssShortMatchFrameEncodeResult encode_lzss_position_distance_4m_range_prepared_frame(
    const TypedContextStreamHeader&,const core::DecoderLimits&,std::uint64_t,std::uint64_t,
    std::span<const dictionary::internal::LzssTypedToken>,
    std::span<context::internal::ModeledOperation>,std::span<std::byte>) noexcept;
}
#endif
