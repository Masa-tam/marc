#include "frame/lzss_contextual_rans_compact_frame_streaming_decoder.hpp"

namespace marc::frame::internal {

LzssContextualRansCompactFrameStreamingDecoder::
LzssContextualRansCompactFrameStreamingDecoder(
    const core::DecoderLimits limits,
    const std::span<std::byte> serialized_frame_workspace,
    const std::span<entropy::internal::RansDecodeEntry> table_workspace,
    const std::span<dictionary::internal::LzssTypedToken> token_workspace,
    const std::span<std::byte> raw_frame_workspace) noexcept
    : decoder_(
          limits, serialized_frame_workspace, table_workspace, token_workspace,
          raw_frame_workspace,
          LzssContextualRansFrameStreamingDecoder::Representation::compact) {}

core::ProcessResult LzssContextualRansCompactFrameStreamingDecoder::process(
    const std::span<const std::byte> input,
    const std::span<std::byte> output,
    const std::uint32_t flags) noexcept {
    return decoder_.process(input, output, flags);
}

} // namespace marc::frame::internal
