#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_FRAME_ENCODER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_FRAME_ENCODER_HPP

#include "frame/lzss_short_match_frame_encoder.hpp"

namespace marc::frame::internal {

// Private complete-frame encoder for the exact 2/8 + 1/9 + 3/2 identity.
// No published stream encoder or public selector calls this entry point.
[[nodiscard]] LzssShortMatchFrameEncodeResult
plan_lzss_position_distance_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t raw_already_committed,
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    std::span<context::internal::ModeledOperation> operations) noexcept;

[[nodiscard]] LzssShortMatchFrameEncodeResult
encode_lzss_position_distance_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t raw_already_committed,
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    std::span<context::internal::ModeledOperation> operations,
    std::span<std::byte> serialized_output) noexcept;

// Retained three-run path for private differential tests and benchmarks.
[[nodiscard]] LzssShortMatchFrameEncodeResult
encode_lzss_position_distance_frame_reference(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t raw_already_committed,
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    std::span<context::internal::ModeledOperation> operations,
    std::span<std::byte> serialized_output) noexcept;

} // namespace marc::frame::internal

#endif
