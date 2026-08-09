#ifndef MARC_FRAME_LZSS_CONTEXTUAL_RANS_COMPACT_FRAME_DECODER_HPP
#define MARC_FRAME_LZSS_CONTEXTUAL_RANS_COMPACT_FRAME_DECODER_HPP

#include "frame/lzss_contextual_rans_frame_decoder.hpp"

namespace marc::frame::internal {

struct LzssContextualRansCompactFrameDecodeResult {
    std::size_t serialized_consumed{};
    std::size_t required_table_entries{};
    std::size_t required_token_count{};
    std::size_t required_raw_size{};
    LzssContextualRansCompactFramePreflightResult preflight{};
    context::internal::LzssContextualRansCompactDecodeResult token_decode{};
    dictionary::internal::LzssTypedReconstructResult reconstruction{};
    LzssContextualRansFrameDecodeError error{
        LzssContextualRansFrameDecodeError::none};
};

[[nodiscard]] LzssContextualRansCompactFrameDecodeResult
decode_lzss_contextual_rans_compact_frame(
    std::span<const std::byte> serialized_frame,
    const LzssContextualRansFrameValidationContext& context,
    std::span<entropy::internal::RansDecodeEntry> private_tables,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<std::byte> private_raw_output) noexcept;

} // namespace marc::frame::internal

#endif
