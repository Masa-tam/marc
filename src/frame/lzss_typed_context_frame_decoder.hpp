#ifndef MARC_FRAME_LZSS_TYPED_CONTEXT_FRAME_DECODER_HPP
#define MARC_FRAME_LZSS_TYPED_CONTEXT_FRAME_DECODER_HPP

#include "context/lzss_contextual_range_decoder.hpp"
#include "dictionary/lzss_typed_reconstructor.hpp"
#include "frame/typed_context_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame::internal {

enum class LzssTypedContextFrameDecodeError : std::uint8_t {
    none,
    preflight_error,
    output_size_unsupported,
    token_output_too_small,
    raw_output_too_small,
    overlapping_workspaces,
    arithmetic_overflow,
    token_decode_error,
    reconstruction_error,
};

struct LzssTypedContextFrameDecodeResult {
    std::size_t serialized_consumed{};
    std::size_t required_token_count{};
    std::size_t required_raw_size{};
    TypedContextFramePreflightResult preflight{};
    context::internal::LzssContextualRangeDecodeResult token_decode{};
    dictionary::internal::LzssTypedReconstructResult reconstruction{};
    LzssTypedContextFrameDecodeError error{
        LzssTypedContextFrameDecodeError::none};
};

[[nodiscard]] LzssTypedContextFrameDecodeResult
decode_lzss_typed_context_frame(
    std::span<const std::byte> serialized_frame,
    const TypedContextFrameValidationContext& context,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<std::byte> private_raw_output) noexcept;

} // namespace marc::frame::internal

#endif
