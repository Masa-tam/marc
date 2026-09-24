#ifndef MARC_FRAME_LZSS_SHORT_MATCH_FRAME_DECODER_HPP
#define MARC_FRAME_LZSS_SHORT_MATCH_FRAME_DECODER_HPP

#include "context/lzss_short_match_range_tokens.hpp"
#include "dictionary/lzss_typed_reconstructor.hpp"
#include "frame/lzss_short_match_preflight.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame::internal {

enum class LzssShortMatchFrameDecodeError : std::uint8_t {
    none,
    preflight_error,
    token_output_too_small,
    raw_output_too_small,
    overlapping_workspaces,
    arithmetic_overflow,
    token_decode_error,
    reconstruction_error,
};

struct LzssShortMatchFrameDecodeResult {
    std::size_t serialized_consumed{};
    std::size_t required_token_count{};
    std::size_t required_raw_size{};
    LzssShortMatchPreflightError preflight_error{
        LzssShortMatchPreflightError::none};
    context::internal::LzssContextualRangeDecodeResult token_decode{};
    dictionary::internal::LzssTypedReconstructResult reconstruction{};
    LzssShortMatchFrameDecodeError error{
        LzssShortMatchFrameDecodeError::none};
};

// Frame-local private decoder. The public stream parser still rejects the
// reserved variant pair; callers must supply nonoverlapping private buffers.
[[nodiscard]] LzssShortMatchFrameDecodeResult decode_lzss_short_match_frame(
    std::span<const std::byte> serialized_frame,
    const TypedContextFrameValidationContext& context,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<std::byte> private_raw_output) noexcept;

} // namespace marc::frame::internal

#endif
