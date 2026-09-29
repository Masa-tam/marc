#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_1M_BOUNDED18_OWNED_ENCODER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_1M_BOUNDED18_OWNED_ENCODER_HPP
#include "frame/lzss_position_distance_1m_bounded18_frame_streaming_encoder.hpp"
#include <memory>
#include <optional>

namespace marc::frame::internal {
// Diagnostic-only adapter: indexed_five_prefix selects the private 18-bit
// candidate here. Other policies retain reference behavior; no public factory
// selects this type or these functions. The wire representation is unchanged.

// Private fixed-policy owner. All allocations follow a complete budget check;
// process neither allocates nor grows buffers. Not a public codec factory.
class LzssPositionDistance1mBounded18OwnedEncoder final : public core::Transform {
public:
    [[nodiscard]] static core::ErrorCode requirements(const TypedContextStreamHeader&,
        const core::DecoderLimits&, LzssPositionDistanceWorkspaceRequirements&,
        dictionary::internal::LzssPositionDistance1mSearch search = dictionary::internal::LzssPositionDistance1mSearch::indexed_five_prefix) noexcept;
    [[nodiscard]] static std::unique_ptr<LzssPositionDistance1mBounded18OwnedEncoder> create(
        const TypedContextStreamHeader&, const core::DecoderLimits&, core::ErrorCode&,
        std::uint32_t eligibility = 3,
        dictionary::internal::LzssPositionDistance1mSearch search = dictionary::internal::LzssPositionDistance1mSearch::indexed_five_prefix) noexcept;
    LzssPositionDistance1mBounded18OwnedEncoder(const LzssPositionDistance1mBounded18OwnedEncoder&) = delete;
    LzssPositionDistance1mBounded18OwnedEncoder& operator=(const LzssPositionDistance1mBounded18OwnedEncoder&) = delete;
    [[nodiscard]] core::ProcessResult process(std::span<const std::byte>,
        std::span<std::byte>, std::uint32_t) noexcept override;
private:
    LzssPositionDistance1mBounded18OwnedEncoder() noexcept = default;
    std::unique_ptr<std::byte[]> raw_, serialized_, aligned_;
    std::optional<LzssPositionDistance1mBounded18FrameStreamingEncoder> encoder_;
    core::StreamError error_{};
    bool ended_{};
};
}
#endif
