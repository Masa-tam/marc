#ifndef MARC_FRAME_LZSS_SHORT_LENGTH_ESCAPE_STREAM_DECODER_HPP
#define MARC_FRAME_LZSS_SHORT_LENGTH_ESCAPE_STREAM_DECODER_HPP

#include "frame/lzss_short_match_stream_decoder.hpp"

namespace marc::frame::internal {

// Strict private one-shot decoder for the exact 2/8 + 1/7 + 3/2 identity.
// The public stream parser continues to reject this identity.
[[nodiscard]] LzssShortMatchStreamDecodeResult
decode_lzss_short_length_escape_stream(
    std::span<const std::byte> serialized_stream,
    const core::DecoderLimits& limits,
    std::span<dictionary::internal::LzssTypedToken> token_workspace,
    std::span<std::byte> raw_frame_workspace,
    std::span<std::byte> raw_stream_output) noexcept;

} // namespace marc::frame::internal

#endif
