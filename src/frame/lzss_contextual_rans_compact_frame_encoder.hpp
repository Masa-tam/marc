#ifndef MARC_FRAME_LZSS_CONTEXTUAL_RANS_COMPACT_FRAME_ENCODER_HPP
#define MARC_FRAME_LZSS_CONTEXTUAL_RANS_COMPACT_FRAME_ENCODER_HPP

#include "frame/lzss_contextual_rans_frame_encoder.hpp"

namespace marc::frame::internal {

struct LzssContextualRansCompactFrameEncodeResult {
    std::size_t serialized_size{};
    std::size_t descriptor_size{};
    std::size_t token_count{};
    std::size_t event_count{};
    std::uint32_t decision_count{};
    std::size_t payload_size{};
    dictionary::internal::LzssTypedEncodeResult token_encode{};
    context::internal::LzssContextualRansEncodeResult entropy_encode{};
    LzssContextualRansFrameHeaderError header_error{
        LzssContextualRansFrameHeaderError::none};
    entropy::internal::ContextualRansCompactFormatError descriptor_error{
        entropy::internal::ContextualRansCompactFormatError::none};
    LzssContextualRansFrameEncodeError error{
        LzssContextualRansFrameEncodeError::none};
};

[[nodiscard]] LzssContextualRansCompactFrameEncodeResult
plan_lzss_contextual_rans_compact_frame(
    const LzssContextualRansStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> private_tokens) noexcept;

[[nodiscard]] LzssContextualRansCompactFrameEncodeResult
encode_lzss_contextual_rans_compact_frame(
    const LzssContextualRansStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<std::byte> serialized_output) noexcept;

} // namespace marc::frame::internal

#endif
