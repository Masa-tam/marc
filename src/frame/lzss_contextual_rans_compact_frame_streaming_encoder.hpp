#ifndef MARC_FRAME_LZSS_CONTEXTUAL_RANS_COMPACT_FRAME_STREAMING_ENCODER_HPP
#define MARC_FRAME_LZSS_CONTEXTUAL_RANS_COMPACT_FRAME_STREAMING_ENCODER_HPP

#include "frame/lzss_contextual_rans_frame_streaming_encoder.hpp"

namespace marc::frame::internal {

class LzssContextualRansCompactFrameStreamingEncoder final
    : public core::Transform {
public:
    LzssContextualRansCompactFrameStreamingEncoder(
        LzssContextualRansStreamHeader stream,
        core::DecoderLimits limits,
        std::span<std::byte> raw_frame_workspace,
        std::span<dictionary::internal::LzssTypedToken> token_workspace,
        std::span<std::byte> serialized_frame_workspace) noexcept;

    [[nodiscard]] core::ProcessResult process(
        std::span<const std::byte> input,
        std::span<std::byte> output,
        std::uint32_t flags) noexcept override;

private:
    LzssContextualRansFrameStreamingEncoder encoder_;
};

} // namespace marc::frame::internal

#endif
