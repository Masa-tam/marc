#include "frame/lzss_contextual_rans_compact_frame_streaming_encoder.hpp"

namespace marc::frame::internal {

LzssContextualRansCompactFrameStreamingEncoder::
LzssContextualRansCompactFrameStreamingEncoder(
    const LzssContextualRansStreamHeader stream,
    const core::DecoderLimits limits,
    const std::span<std::byte> raw_frame_workspace,
    const std::span<dictionary::internal::LzssTypedToken> token_workspace,
    const std::span<std::byte> serialized_frame_workspace) noexcept
    : encoder_(
          stream, limits, raw_frame_workspace, token_workspace,
          serialized_frame_workspace,
          LzssContextualRansFrameStreamingEncoder::Representation::compact) {}

core::ProcessResult LzssContextualRansCompactFrameStreamingEncoder::process(
    const std::span<const std::byte> input,
    const std::span<std::byte> output,
    const std::uint32_t flags) noexcept {
    return encoder_.process(input, output, flags);
}

} // namespace marc::frame::internal
