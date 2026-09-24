#ifndef MARC_FRAME_LZSS_SHORT_LENGTH_ESCAPE_RAW_STREAM_ENCODER_HPP
#define MARC_FRAME_LZSS_SHORT_LENGTH_ESCAPE_RAW_STREAM_ENCODER_HPP

#include "frame/lzss_short_length_escape_candidate_selector.hpp"

namespace marc::frame::internal {

enum class LzssShortLengthEscapeRawStreamEncodeError : std::uint8_t {
    none,
    invalid_stream,
    raw_size_mismatch,
    selection_error,
    output_too_small,
    overlapping_workspaces,
    arithmetic_overflow,
    internal_error,
};

struct LzssShortLengthEscapeRawStreamEncodeResult {
    std::size_t serialized_size{};
    std::uint64_t frame_count{};
    std::size_t frame_index{};
    LzssShortMatchPreflightError stream_error{
        LzssShortMatchPreflightError::none};
    LzssShortMatchSelectionResult selection{};
    LzssShortLengthEscapeRawStreamEncodeError error{
        LzssShortLengthEscapeRawStreamEncodeError::none};
};

// Private one-shot assembly from a stable caller-owned raw span. Token and
// operation storage is reused per frame; indexed mode also reuses an exact
// prefix-finder workspace. Neither mode admits the identity publicly.
[[nodiscard]] LzssShortLengthEscapeRawStreamEncodeResult
plan_lzss_short_length_escape_raw_stream(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<context::internal::ModeledOperation> operations) noexcept;

[[nodiscard]] LzssShortLengthEscapeRawStreamEncodeResult
encode_lzss_short_length_escape_raw_stream(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<context::internal::ModeledOperation> operations,
    std::span<std::byte> serialized_output) noexcept;

[[nodiscard]] LzssShortLengthEscapeRawStreamEncodeResult
plan_lzss_short_length_escape_raw_stream_indexed(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<context::internal::ModeledOperation> operations,
    std::span<std::byte> finder_workspace) noexcept;

[[nodiscard]] LzssShortLengthEscapeRawStreamEncodeResult
encode_lzss_short_length_escape_raw_stream_indexed(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<context::internal::ModeledOperation> operations,
    std::span<std::byte> finder_workspace,
    std::span<std::byte> serialized_output) noexcept;

} // namespace marc::frame::internal

#endif
