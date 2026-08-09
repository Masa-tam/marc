#ifndef MARC_FRAME_LZSS_CONTEXTUAL_RANS_COMPACT_FRAME_STREAMING_DECODER_HPP
#define MARC_FRAME_LZSS_CONTEXTUAL_RANS_COMPACT_FRAME_STREAMING_DECODER_HPP

#include "frame/lzss_contextual_rans_frame_streaming_decoder.hpp"

namespace marc::frame::internal {

class LzssContextualRansCompactFrameStreamingDecoder final
    : public core::Transform {
public:
    LzssContextualRansCompactFrameStreamingDecoder(
        core::DecoderLimits limits,
        std::span<std::byte> serialized_frame_workspace,
        std::span<entropy::internal::RansDecodeEntry> table_workspace,
        std::span<dictionary::internal::LzssTypedToken> token_workspace,
        std::span<std::byte> raw_frame_workspace) noexcept;

    [[nodiscard]] core::ProcessResult process(
        std::span<const std::byte> input,
        std::span<std::byte> output,
        std::uint32_t flags) noexcept override;

private:
    LzssContextualRansFrameStreamingDecoder decoder_;
};

} // namespace marc::frame::internal

#endif
