#ifndef MARC_FRAME_LZSS_SHORT_LENGTH_ESCAPE_FRAME_DECODER_HPP
#define MARC_FRAME_LZSS_SHORT_LENGTH_ESCAPE_FRAME_DECODER_HPP

#include "frame/lzss_short_match_frame_decoder.hpp"

namespace marc::frame::internal {

// Complete-frame private 2/8 + 1/7 + 3/2 decoder. It is not called by the
// published stream decoder or any public API.
[[nodiscard]] LzssShortMatchFrameDecodeResult
decode_lzss_short_length_escape_frame(
    std::span<const std::byte> serialized_frame,
    const TypedContextFrameValidationContext& context,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<std::byte> private_raw_output) noexcept;

} // namespace marc::frame::internal

#endif
