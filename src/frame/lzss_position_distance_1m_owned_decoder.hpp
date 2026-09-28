#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_1M_OWNED_DECODER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_1M_OWNED_DECODER_HPP
#include "frame/lzss_position_distance_1m_frame_streaming_decoder.hpp"
#include "frame/lzss_position_distance_1m_workspace.hpp"
#include <memory>
#include <optional>

namespace marc::frame::internal {
// Private owner, not a public codec factory. Creation preflights every retained
// byte before allocation; process performs no allocation or workspace growth.
class LzssPositionDistance1mOwnedDecoder final : public core::Transform {
public:
    [[nodiscard]] static core::ErrorCode requirements(std::uint32_t frame_capacity,
        const core::DecoderLimits& limits,LzssPositionDistance1mDecodeWorkspace& result) noexcept;
    [[nodiscard]] static std::unique_ptr<LzssPositionDistance1mOwnedDecoder> create(
        std::uint32_t frame_capacity,const core::DecoderLimits& limits,core::ErrorCode& error) noexcept;
    LzssPositionDistance1mOwnedDecoder(const LzssPositionDistance1mOwnedDecoder&) = delete;
    LzssPositionDistance1mOwnedDecoder& operator=(const LzssPositionDistance1mOwnedDecoder&) = delete;
    [[nodiscard]] core::ProcessResult process(std::span<const std::byte> input,
        std::span<std::byte> output,std::uint32_t flags) noexcept override;
private:
    LzssPositionDistance1mOwnedDecoder() noexcept = default;
    std::unique_ptr<std::byte[]> serialized_,raw_;
    std::unique_ptr<dictionary::internal::LzssTypedToken[]> tokens_;
    // Declared last among storage members so the decoder dies before buffers.
    std::optional<LzssPositionDistance1mFrameStreamingDecoder> decoder_;
    core::StreamError error_{};
    bool ended_{};
};
}
#endif
