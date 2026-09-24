#ifndef MARC_FRAME_LZSS_SHORT_MATCH_CANDIDATE_SELECTOR_HPP
#define MARC_FRAME_LZSS_SHORT_MATCH_CANDIDATE_SELECTOR_HPP

#include "dictionary/lzss_short_match_candidate.hpp"
#include "frame/lzss_short_match_frame_encoder.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame::internal {

enum class LzssShortMatchSelectionError : std::uint8_t {
    none,
    overlapping_workspaces,
    arithmetic_overflow,
    candidate_error,
    frame_error,
    serialized_output_too_small,
    internal_error,
};

struct LzssShortMatchSelectionResult {
    std::array<std::size_t, 3> candidate_frame_sizes{};
    std::uint32_t selected_minimum_length{};
    std::size_t selected_token_count{};
    std::size_t selected_frame_size{};
    std::size_t candidate_index{};
    dictionary::internal::LzssShortMatchCandidateResult candidate{};
    LzssShortMatchFrameEncodeResult frame{};
    LzssShortMatchSelectionError error{LzssShortMatchSelectionError::none};
};

// Private, exact-size experiment. Compare greedy eligibility 3, 4, and 5
// using the same reserved frame identity; choose the smallest full frame,
// preferring the higher minimum on ties. This is not a public encoder policy.
[[nodiscard]] LzssShortMatchSelectionResult
plan_lzss_short_match_candidate_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t raw_already_committed,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<context::internal::ModeledOperation> operations) noexcept;

[[nodiscard]] LzssShortMatchSelectionResult
encode_lzss_short_match_candidate_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t raw_already_committed,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<context::internal::ModeledOperation> operations,
    std::span<std::byte> serialized_output) noexcept;

} // namespace marc::frame::internal

#endif
