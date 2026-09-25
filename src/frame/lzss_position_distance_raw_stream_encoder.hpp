#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_RAW_STREAM_ENCODER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_RAW_STREAM_ENCODER_HPP
#include "frame/lzss_position_distance_raw_frame_encoder.hpp"

namespace marc::frame::internal {
enum class LzssPositionDistanceRawStreamError {
    none, invalid_stream, invalid_policy, raw_size_mismatch, overlapping_buffers,
    arithmetic_overflow, frame_error, output_too_small, internal_error
};
struct LzssPositionDistanceRawStreamResult {
    LzssPositionDistanceRawStreamError error{};
    std::size_t serialized_size{};
    std::uint64_t frame_count{};
    std::uint64_t frame_index{};
    LzssPositionDistanceRawFrameResult frame{};
};
// Private one-shot fixed-policy writer. All regions must remain stable and
// disjoint. Scratch is reused per frame. Planning validates the entire input;
// encoding commits the stream header last. Error sizes are not committed bytes.
// Preflight errors preserve output; unexpected write errors can leave unusable
// partial frame bytes. Empty input is header-only. No public admission.
[[nodiscard]] LzssPositionDistanceRawStreamResult plan_lzss_position_distance_raw_stream(
    const TypedContextStreamHeader&, const core::DecoderLimits&,
    std::span<const std::byte> raw, std::uint32_t eligibility, LzssPositionDistanceSearch,
    std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<context::internal::ModeledOperation> operations,
    std::span<std::byte> finder_workspace) noexcept;
[[nodiscard]] LzssPositionDistanceRawStreamResult encode_lzss_position_distance_raw_stream(
    const TypedContextStreamHeader&, const core::DecoderLimits&,
    std::span<const std::byte> raw, std::uint32_t eligibility, LzssPositionDistanceSearch,
    std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<context::internal::ModeledOperation> operations,
    std::span<std::byte> finder_workspace, std::span<std::byte> output) noexcept;
}
#endif
