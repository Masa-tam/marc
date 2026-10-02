#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_4M_PHASE_SPLIT_OWNED_ENCODER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_4M_PHASE_SPLIT_OWNED_ENCODER_HPP
#include "frame/lzss_position_distance_4m_phase_split_frame_streaming_encoder.hpp"
#include <memory>
#include <optional>

namespace marc::frame::internal {
// Private fixed-policy owner. All allocations follow a complete budget check;
// process neither allocates nor grows buffers. Not a public codec factory.
class LzssPositionDistance4mPhaseSplitOwnedEncoder final : public core::Transform {
public:
    [[nodiscard]] static core::ErrorCode requirements(const TypedContextStreamHeader&,
        const core::DecoderLimits&, LzssPositionDistanceWorkspaceRequirements&) noexcept;
    [[nodiscard]] static std::unique_ptr<LzssPositionDistance4mPhaseSplitOwnedEncoder> create(
        const TypedContextStreamHeader&, const core::DecoderLimits&, core::ErrorCode&,
        std::uint32_t eligibility = 3, bool timed = false) noexcept;
    LzssPositionDistance4mPhaseSplitOwnedEncoder(const LzssPositionDistance4mPhaseSplitOwnedEncoder&) = delete;
    LzssPositionDistance4mPhaseSplitOwnedEncoder& operator=(const LzssPositionDistance4mPhaseSplitOwnedEncoder&) = delete;
    [[nodiscard]] core::ProcessResult process(std::span<const std::byte>,
        std::span<std::byte>, std::uint32_t) noexcept override;
    [[nodiscard]] bool diagnostic_report(PreparationSplitReport&) const noexcept;
private:
    LzssPositionDistance4mPhaseSplitOwnedEncoder() noexcept = default;
    std::unique_ptr<std::byte[]> raw_, serialized_, aligned_;
    std::optional<LzssPositionDistance4mPhaseSplitFrameStreamingEncoder> encoder_;
    core::StreamError error_{};
    bool ended_{};
};
}
#endif
