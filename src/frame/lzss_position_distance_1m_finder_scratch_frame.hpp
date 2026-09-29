#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_1M_FINDER_SCRATCH_FRAME_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_1M_FINDER_SCRATCH_FRAME_HPP
#include "frame/lzss_position_distance_1m_prepared_frame.hpp"
namespace marc::frame::internal {
// Diagnostic metadata is not a public ABI or additional persistent workspace.
struct LzssPositionDistance1mFinderScratchFrameResult : LzssShortMatchFrameEncodeResult {
    bool used_finder_scratch{};
    LzssPositionDistance1mFinderScratchFrameResult() = default;
    LzssPositionDistance1mFinderScratchFrameResult(LzssShortMatchFrameEncodeResult r) noexcept
        : LzssShortMatchFrameEncodeResult(r) {}
};
// Optional scratch is discardable. Ineligible scratch falls back to the admitted
// frame path. All failure results preserve serialized output. Inputs/configuration
// must remain stable; used_finder_scratch means a payload was generated there,
// including when subsequent frame preflight fails without publishing it.
[[nodiscard]] LzssPositionDistance1mFinderScratchFrameResult
encode_lzss_position_distance_1m_finder_scratch_frame(
    const TypedContextStreamHeader&,const core::DecoderLimits&,std::uint64_t,std::uint64_t,
    std::span<const dictionary::internal::LzssTypedToken>,
    std::span<context::internal::ModeledOperation>,std::span<std::byte> output,
    std::span<std::byte> scratch) noexcept;
}
#endif
