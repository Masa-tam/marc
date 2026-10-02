#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_4M_WORD4_OWNED_ENCODER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_4M_WORD4_OWNED_ENCODER_HPP
#include "frame/lzss_position_distance_4m_word4_frame_streaming_encoder.hpp"
#include <memory>
#include <optional>

namespace marc::frame::internal {
// Private fixed-policy owner. All allocations follow a complete budget check;
// process neither allocates nor grows buffers. Not a public codec factory.
class LzssPositionDistance4mWord4OwnedEncoder final : public core::Transform {
public:
    [[nodiscard]] static core::ErrorCode requirements(const TypedContextStreamHeader&,
        const core::DecoderLimits&, LzssPositionDistanceWorkspaceRequirements&) noexcept;
    [[nodiscard]] static std::unique_ptr<LzssPositionDistance4mWord4OwnedEncoder> create(
        const TypedContextStreamHeader&, const core::DecoderLimits&, core::ErrorCode&,
        std::uint32_t eligibility = 3) noexcept;
    LzssPositionDistance4mWord4OwnedEncoder(const LzssPositionDistance4mWord4OwnedEncoder&) = delete;
    LzssPositionDistance4mWord4OwnedEncoder& operator=(const LzssPositionDistance4mWord4OwnedEncoder&) = delete;
    [[nodiscard]] core::ProcessResult process(std::span<const std::byte>,
        std::span<std::byte>, std::uint32_t) noexcept override;
private:
    LzssPositionDistance4mWord4OwnedEncoder() noexcept = default;
    std::unique_ptr<std::byte[]> raw_, serialized_, aligned_;
    std::optional<LzssPositionDistance4mWord4FrameStreamingEncoder> encoder_;
    core::StreamError error_{};
    bool ended_{};
};
}
#endif
