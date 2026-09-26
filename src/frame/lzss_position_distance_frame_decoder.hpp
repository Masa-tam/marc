#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_FRAME_DECODER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_FRAME_DECODER_HPP

#include "frame/lzss_short_match_frame_decoder.hpp"

namespace marc::frame::internal {

// Transactional complete-frame reference for private 2/8 + 1/9 + 3/2 decoding.
[[nodiscard]] LzssShortMatchFrameDecodeResult
decode_lzss_position_distance_frame(
    std::span<const std::byte> serialized_frame,
    const TypedContextFrameValidationContext& context,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<std::byte> private_raw_output) noexcept;

// Frame-atomic streaming adapter. Token scratch may change on failure; raw
// output remains unchanged. Discard failed scratch and publish only on success.
[[nodiscard]] LzssShortMatchFrameDecodeResult
decode_lzss_position_distance_frame_scratch(
    std::span<const std::byte> serialized_frame,
    const TypedContextFrameValidationContext& context,
    std::span<dictionary::internal::LzssTypedToken> token_scratch,
    std::span<std::byte> private_raw_output) noexcept;

} // namespace marc::frame::internal

#endif
