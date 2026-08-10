#ifndef MARC_FRAME_LZSS_CONTEXTUAL_TANS_FRAME_ENCODER_HPP
#define MARC_FRAME_LZSS_CONTEXTUAL_TANS_FRAME_ENCODER_HPP

#include "context/lzss_contextual_tans_encoder.hpp"
#include "dictionary/lzss_typed_encoder.hpp"
#include "frame/lzss_contextual_tans_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame::internal {

enum class LzssContextualTansFrameEncodeError : std::uint8_t {
    none,
    invalid_stream,
    input_size_mismatch,
    token_staging_too_small,
    table_staging_too_small,
    serialized_output_too_small,
    overlapping_workspaces,
    workspace_limit,
    token_encode_error,
    entropy_encode_error,
    header_error,
    descriptor_error,
    arithmetic_overflow,
    internal_error,
};

struct LzssContextualTansFrameEncodeResult {
    std::size_t serialized_size{};
    std::size_t descriptor_size{};
    std::size_t token_count{};
    std::size_t event_count{};
    std::uint32_t decision_count{};
    std::size_t payload_size{};
    std::size_t required_table_entries{
        entropy::internal::contextual_tans_encode_table_entries};
    dictionary::internal::LzssTypedEncodeResult token_encode{};
    context::internal::LzssContextualTansEncodeResult entropy_encode{};
    LzssContextualTansFrameHeaderError header_error{
        LzssContextualTansFrameHeaderError::none};
    entropy::internal::ContextualTansFormatError descriptor_error{
        entropy::internal::ContextualTansFormatError::none};
    LzssContextualTansFrameEncodeError error{
        LzssContextualTansFrameEncodeError::none};
};

// Planning materializes private tokens and encode tables, but never writes
// serialized output.
[[nodiscard]] LzssContextualTansFrameEncodeResult
plan_lzss_contextual_tans_frame(
    const LzssContextualTansStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<std::uint16_t> private_encode_tables) noexcept;

[[nodiscard]] LzssContextualTansFrameEncodeResult
encode_lzss_contextual_tans_frame(
    const LzssContextualTansStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<std::uint16_t> private_encode_tables,
    std::span<std::byte> serialized_output) noexcept;

} // namespace marc::frame::internal

#endif
