#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_1M_PREPARED_OWNED_ENCODER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_1M_PREPARED_OWNED_ENCODER_HPP
#include "frame/lzss_position_distance_1m_prepared_frame_streaming_encoder.hpp"
#include <memory>
#include <optional>

namespace marc::frame::internal {
// Separate diagnostic-only prepared-mapping path; no public factory selects it.

// Private fixed-policy owner. All allocations follow a complete budget check;
// process neither allocates nor grows buffers. Not a public codec factory.
class LzssPositionDistance1mPreparedOwnedEncoder final : public core::Transform {
public:
    [[nodiscard]] static core::ErrorCode requirements(const TypedContextStreamHeader&,
        const core::DecoderLimits&, LzssPositionDistanceWorkspaceRequirements&,
        dictionary::internal::LzssPositionDistance1mSearch search = dictionary::internal::LzssPositionDistance1mSearch::indexed) noexcept;
    [[nodiscard]] static std::unique_ptr<LzssPositionDistance1mPreparedOwnedEncoder> create(
        const TypedContextStreamHeader&, const core::DecoderLimits&, core::ErrorCode&,
        std::uint32_t eligibility = 3,
        dictionary::internal::LzssPositionDistance1mSearch search = dictionary::internal::LzssPositionDistance1mSearch::indexed) noexcept;
    LzssPositionDistance1mPreparedOwnedEncoder(const LzssPositionDistance1mPreparedOwnedEncoder&) = delete;
    LzssPositionDistance1mPreparedOwnedEncoder& operator=(const LzssPositionDistance1mPreparedOwnedEncoder&) = delete;
    [[nodiscard]] core::ProcessResult process(std::span<const std::byte>,
        std::span<std::byte>, std::uint32_t) noexcept override;
private:
    LzssPositionDistance1mPreparedOwnedEncoder() noexcept = default;
    std::unique_ptr<std::byte[]> raw_, serialized_, aligned_;
    std::optional<LzssPositionDistance1mPreparedFrameStreamingEncoder> encoder_;
    core::StreamError error_{};
    bool ended_{};
};
}
#endif
