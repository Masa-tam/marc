#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_4M_OWNED_DECODER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_4M_OWNED_DECODER_HPP
#include "frame/lzss_position_distance_4m_frame_streaming_decoder.hpp"
#include "frame/lzss_position_distance_4m_workspace.hpp"
#include <memory>
#include <optional>

namespace marc::frame::internal {
// Private owner, not a public codec factory. Creation preflights every retained
// byte before allocation; process performs no allocation or workspace growth.
class LzssPositionDistance4mOwnedDecoder final : public core::Transform {
public:
    [[nodiscard]] static core::ErrorCode requirements(std::uint32_t frame_capacity,
        const core::DecoderLimits& limits,LzssPositionDistance4mDecodeWorkspace& result) noexcept;
    [[nodiscard]] static std::unique_ptr<LzssPositionDistance4mOwnedDecoder> create(
        std::uint32_t frame_capacity,const core::DecoderLimits& limits,core::ErrorCode& error) noexcept;
    LzssPositionDistance4mOwnedDecoder(const LzssPositionDistance4mOwnedDecoder&) = delete;
    LzssPositionDistance4mOwnedDecoder& operator=(const LzssPositionDistance4mOwnedDecoder&) = delete;
    [[nodiscard]] core::ProcessResult process(std::span<const std::byte> input,
        std::span<std::byte> output,std::uint32_t flags) noexcept override;
private:
    LzssPositionDistance4mOwnedDecoder() noexcept = default;
    std::unique_ptr<std::byte[]> serialized_,raw_;
    std::unique_ptr<dictionary::internal::LzssTypedToken[]> tokens_;
    // Declared last among storage members so the decoder dies before buffers.
    std::optional<LzssPositionDistance4mFrameStreamingDecoder> decoder_;
    core::StreamError error_{};
    bool ended_{};
};
}
#endif
