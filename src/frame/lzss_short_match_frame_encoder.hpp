#ifndef MARC_FRAME_LZSS_SHORT_MATCH_FRAME_ENCODER_HPP
#define MARC_FRAME_LZSS_SHORT_MATCH_FRAME_ENCODER_HPP

#include "context/lzss_short_match_operations.hpp"
#include "entropy/lzss_short_match_range_encoder.hpp"
#include "frame/lzss_short_match_preflight.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame::internal {

enum class LzssShortMatchFrameEncodeError : std::uint8_t {
    none,
    invalid_stream,
    invalid_frame_position,
    token_count_unsupported,
    context_error,
    entropy_error,
    preflight_error,
    operation_workspace_too_small,
    serialized_output_too_small,
    overlapping_workspaces,
    workspace_limit,
    arithmetic_overflow,
    internal_error,
};

struct LzssShortMatchFrameEncodeResult {
    std::size_t serialized_size{};
    std::size_t raw_size{};
    std::size_t token_count{};
    std::size_t operation_count{};
    std::uint32_t decision_count{};
    std::size_t payload_size{};
    LzssShortMatchPreflightError preflight_error{
        LzssShortMatchPreflightError::none};
    context::internal::LzssFieldContextResult context{};
    entropy::internal::ContextualDynamicRangeEncodeResult entropy{};
    LzssShortMatchFrameEncodeError error{
        LzssShortMatchFrameEncodeError::none};
};

// Private encoder for already selected, complete typed-token frames. The
// caller retains ownership of token and operation storage, which must remain
// stable throughout planning and encoding. No public stream selector calls it.
[[nodiscard]] LzssShortMatchFrameEncodeResult
plan_lzss_short_match_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t raw_already_committed,
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    std::span<context::internal::ModeledOperation> operations) noexcept;

[[nodiscard]] LzssShortMatchFrameEncodeResult
encode_lzss_short_match_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t raw_already_committed,
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    std::span<context::internal::ModeledOperation> operations,
    std::span<std::byte> serialized_output) noexcept;

} // namespace marc::frame::internal

#endif
