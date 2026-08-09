#ifndef MARC_FRAME_LZSS_CONTEXTUAL_RANS_FRAME_DECODER_HPP
#define MARC_FRAME_LZSS_CONTEXTUAL_RANS_FRAME_DECODER_HPP

#include "context/lzss_contextual_rans_decoder.hpp"
#include "dictionary/lzss_typed_reconstructor.hpp"
#include "frame/lzss_contextual_rans_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame::internal {

enum class LzssContextualRansFrameDecodeError : std::uint8_t {
    none,
    preflight_error,
    output_size_unsupported,
    table_output_too_small,
    token_output_too_small,
    raw_output_too_small,
    overlapping_workspaces,
    arithmetic_overflow,
    token_decode_error,
    reconstruction_error,
};

struct LzssContextualRansFrameDecodeResult {
    std::size_t serialized_consumed{};
    std::size_t required_table_entries{};
    std::size_t required_token_count{};
    std::size_t required_raw_size{};
    LzssContextualRansFramePreflightResult preflight{};
    context::internal::LzssContextualRansDecodeResult token_decode{};
    dictionary::internal::LzssTypedReconstructResult reconstruction{};
    LzssContextualRansFrameDecodeError error{
        LzssContextualRansFrameDecodeError::none};
};

[[nodiscard]] LzssContextualRansFrameDecodeResult
decode_lzss_contextual_rans_frame(
    std::span<const std::byte> serialized_frame,
    const LzssContextualRansFrameValidationContext& context,
    std::span<entropy::internal::RansDecodeEntry> private_tables,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<std::byte> private_raw_output) noexcept;

} // namespace marc::frame::internal

#endif
