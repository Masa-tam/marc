#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_4M_FRAME_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_4M_FRAME_HPP

#include "frame/lzss_short_match_frame_decoder.hpp"
#include "frame/lzss_short_match_frame_encoder.hpp"
#include "frame/lzss_position_distance_4m_preflight.hpp"

namespace marc::frame::internal {

// Private complete-frame helpers for 2/10 + 1/11 + 3/2. No public admission.
// Inputs/configuration remain stable throughout each call. Operations are
// private workspace; encoded output remains unchanged on validation failure.
[[nodiscard]] LzssShortMatchFrameEncodeResult encode_lzss_position_distance_4m_frame(
    const TypedContextStreamHeader& stream, const core::DecoderLimits& limits,
    std::uint64_t sequence, std::uint64_t raw_already_committed,
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    std::span<context::internal::ModeledOperation> operations,
    std::span<std::byte> serialized_output) noexcept;

// Failed transactional calls preserve both token and raw output.
[[nodiscard]] LzssShortMatchFrameDecodeResult decode_lzss_position_distance_4m_frame(
    std::span<const std::byte> serialized_frame,
    const TypedContextFrameValidationContext& context,
    std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<std::byte> raw_output) noexcept;

// Failed scratch calls may modify tokens, but always preserve raw output.
// Never consume failed token scratch. Success consumes exactly one frame.
[[nodiscard]] LzssShortMatchFrameDecodeResult decode_lzss_position_distance_4m_frame_scratch(
    std::span<const std::byte> serialized_frame,
    const TypedContextFrameValidationContext& context,
    std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<std::byte> raw_output) noexcept;

} // namespace marc::frame::internal
#endif
