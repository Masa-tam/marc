#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_1M_PREPARED_FRAME_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_1M_PREPARED_FRAME_HPP
#include "frame/lzss_position_distance_1m_frame.hpp"
namespace marc::frame::internal {
// Internal prepared path. Same output/error contract as the retained frame encoder.
// Prepared mapping state is transient and fits the existing model-state charge.
[[nodiscard]] LzssShortMatchFrameEncodeResult encode_lzss_position_distance_1m_prepared_frame(
    const TypedContextStreamHeader&,const core::DecoderLimits&,std::uint64_t,std::uint64_t,
    std::span<const dictionary::internal::LzssTypedToken>,
    std::span<context::internal::ModeledOperation>,std::span<std::byte>) noexcept;
}
#endif
