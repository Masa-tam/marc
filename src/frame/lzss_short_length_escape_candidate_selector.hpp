#ifndef MARC_FRAME_LZSS_SHORT_LENGTH_ESCAPE_CANDIDATE_SELECTOR_HPP
#define MARC_FRAME_LZSS_SHORT_LENGTH_ESCAPE_CANDIDATE_SELECTOR_HPP

#include "frame/lzss_short_match_candidate_selector.hpp"

namespace marc::frame::internal {

// Private exact-size selection for the 2/8 + 1/7 + 3/2 identity. Compare
// complete frames at eligibility 3, 4 and 5; prefer higher eligibility on
// ties. No public stream encoder calls these entry points.
[[nodiscard]] LzssShortMatchSelectionResult
plan_lzss_short_length_escape_candidate_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t raw_already_committed,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<context::internal::ModeledOperation> operations) noexcept;

[[nodiscard]] LzssShortMatchSelectionResult
encode_lzss_short_length_escape_candidate_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t raw_already_committed,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<context::internal::ModeledOperation> operations,
    std::span<std::byte> serialized_output) noexcept;

[[nodiscard]] LzssShortMatchSelectionResult
plan_lzss_short_length_escape_candidate_frame_indexed(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t raw_already_committed,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<context::internal::ModeledOperation> operations,
    std::span<std::byte> finder_workspace) noexcept;

[[nodiscard]] LzssShortMatchSelectionResult
encode_lzss_short_length_escape_candidate_frame_indexed(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t raw_already_committed,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<context::internal::ModeledOperation> operations,
    std::span<std::byte> finder_workspace,
    std::span<std::byte> serialized_output) noexcept;

} // namespace marc::frame::internal

#endif
