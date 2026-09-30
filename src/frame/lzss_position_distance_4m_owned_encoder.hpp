#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_4M_OWNED_ENCODER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_4M_OWNED_ENCODER_HPP
#include "frame/lzss_position_distance_4m_frame_streaming_encoder.hpp"
#include <memory>
#include <optional>

namespace marc::frame::internal {
// Private fixed-policy owner. All allocations follow a complete budget check;
// process neither allocates nor grows buffers. Not a public codec factory.
class LzssPositionDistance4mOwnedEncoder final : public core::Transform {
public:
    [[nodiscard]] static core::ErrorCode requirements(const TypedContextStreamHeader&,
        const core::DecoderLimits&, LzssPositionDistanceWorkspaceRequirements&,
        dictionary::internal::LzssPositionDistance4mSearch search = dictionary::internal::LzssPositionDistance4mSearch::indexed_reference) noexcept;
    [[nodiscard]] static std::unique_ptr<LzssPositionDistance4mOwnedEncoder> create(
        const TypedContextStreamHeader&, const core::DecoderLimits&, core::ErrorCode&,
        std::uint32_t eligibility = 3,
        dictionary::internal::LzssPositionDistance4mSearch search = dictionary::internal::LzssPositionDistance4mSearch::indexed_reference) noexcept;
    LzssPositionDistance4mOwnedEncoder(const LzssPositionDistance4mOwnedEncoder&) = delete;
    LzssPositionDistance4mOwnedEncoder& operator=(const LzssPositionDistance4mOwnedEncoder&) = delete;
    [[nodiscard]] core::ProcessResult process(std::span<const std::byte>,
        std::span<std::byte>, std::uint32_t) noexcept override;
private:
    LzssPositionDistance4mOwnedEncoder() noexcept = default;
    std::unique_ptr<std::byte[]> raw_, serialized_, aligned_;
    std::optional<LzssPositionDistance4mFrameStreamingEncoder> encoder_;
    core::StreamError error_{};
    bool ended_{};
};
}
#endif
