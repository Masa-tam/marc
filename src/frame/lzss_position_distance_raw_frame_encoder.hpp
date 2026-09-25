#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_RAW_FRAME_ENCODER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_RAW_FRAME_ENCODER_HPP
#include "dictionary/lzss_short_length_escape_candidate.hpp"
#include "frame/lzss_position_distance_frame_encoder.hpp"

namespace marc::frame::internal {
enum class LzssPositionDistanceSearch { reference, indexed };
enum class LzssPositionDistanceRawFrameError {
    none, invalid_stream, invalid_position, raw_size_mismatch, invalid_search,
    overlapping_buffers, arithmetic_overflow, workspace_limit, candidate_error, frame_error
};
struct LzssPositionDistanceRawFrameResult {
    LzssPositionDistanceRawFrameError error{};
    dictionary::internal::LzssShortMatchCandidateResult candidate{};
    LzssShortMatchFrameEncodeResult frame{};
};
// Fixed eligibility (3/4/5), no size-based candidate selection. All spans must
// stay stable and mutually disjoint. Scratch can change on failure; output is
// consumable only on success. Empty raw frames are not allowed.
[[nodiscard]] LzssPositionDistanceRawFrameResult encode_lzss_position_distance_raw_frame(
    const TypedContextStreamHeader& stream, const core::DecoderLimits& limits,
    std::uint64_t sequence, std::uint64_t raw_already_committed,
    std::span<const std::byte> raw, std::uint32_t minimum_eligible_length,
    LzssPositionDistanceSearch search,
    std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<context::internal::ModeledOperation> operations,
    std::span<std::byte> finder_workspace, std::span<std::byte> output) noexcept;
}
#endif
