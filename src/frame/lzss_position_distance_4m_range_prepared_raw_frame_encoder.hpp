#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_4M_RANGE_PREPARED_RAW_FRAME_ENCODER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_4M_RANGE_PREPARED_RAW_FRAME_ENCODER_HPP
#include "dictionary/lzss_position_distance_4m_five_prefix_candidate.hpp"
#include "frame/lzss_position_distance_4m_frame.hpp"
#include "frame/lzss_position_distance_raw_frame_encoder.hpp"

namespace marc::frame::internal {
// Private raw-frame encoder. Fixed eligibility 3/4/5. Charge all supplied
// capacities before parsing; tokens/operations/finder are discardable scratch.
// Serialized output is unchanged on validation/capacity failure. Consume only
// successful output. All inputs/configuration remain stable during the call.
[[nodiscard]] LzssPositionDistanceRawFrameResult encode_lzss_position_distance_4m_range_prepared_raw_frame(
    const TypedContextStreamHeader& stream,const core::DecoderLimits& limits,
    std::uint64_t sequence,std::uint64_t committed,std::span<const std::byte> raw,
    std::uint32_t eligibility,
    std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<context::internal::ModeledOperation> operations,
    std::span<std::byte> finder,std::span<std::byte> output) noexcept;
}
#endif
