#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_STREAM_DECODER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_STREAM_DECODER_HPP
#include "frame/lzss_short_match_stream_decoder.hpp"

namespace marc::frame::internal {
// Private strict one-shot decoder for 2/8 + 1/9 + 3/2, not public admission.
// Input and disjoint caller-owned workspaces must remain stable for both passes.
// Malformed input publishes no whole-stream raw output; workspaces may change.
[[nodiscard]] LzssShortMatchStreamDecodeResult decode_lzss_position_distance_stream(
    std::span<const std::byte> serialized_stream,
    const core::DecoderLimits& limits,
    std::span<dictionary::internal::LzssTypedToken> token_workspace,
    std::span<std::byte> raw_frame_workspace,
    std::span<std::byte> raw_stream_output) noexcept;
}
#endif
