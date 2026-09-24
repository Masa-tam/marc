#include "frame/lzss_short_match_candidate_selector.hpp"

#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::frame::internal {
namespace {

struct Region {
    const void* data{};
    std::size_t size{};
};

[[nodiscard]] LzssShortMatchSelectionError check_regions(
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> tokens,
    const std::span<context::internal::ModeledOperation> operations,
    const std::span<std::byte> serialized_output) noexcept {
    std::size_t token_bytes{};
    std::size_t operation_bytes{};
    if (!core::checked_multiply(
            tokens.size(), sizeof(dictionary::internal::LzssTypedToken),
            token_bytes)
        || !core::checked_multiply(
            operations.size(), sizeof(context::internal::ModeledOperation),
            operation_bytes)) {
        return LzssShortMatchSelectionError::arithmetic_overflow;
    }
    const std::array regions{
        Region{raw_input.data(), raw_input.size()},
        Region{tokens.data(), token_bytes},
        Region{operations.data(), operation_bytes},
        Region{serialized_output.data(), serialized_output.size()}};
    for (std::size_t first = 0; first < regions.size(); ++first) {
        for (std::size_t second = first + 1; second < regions.size();
             ++second) {
            const auto overlap = core::check_buffer_overlap(
                regions[first].data, regions[first].size,
                regions[second].data, regions[second].size);
            if (overlap == core::BufferOverlap::arithmetic_overflow) {
                return LzssShortMatchSelectionError::arithmetic_overflow;
            }
            if (overlap == core::BufferOverlap::overlap) {
                return LzssShortMatchSelectionError::overlapping_workspaces;
            }
        }
    }
    return LzssShortMatchSelectionError::none;
}

} // namespace

LzssShortMatchSelectionResult plan_lzss_short_match_candidate_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t raw_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> tokens,
    const std::span<context::internal::ModeledOperation> operations) noexcept {
    LzssShortMatchSelectionResult result{};
    result.error = check_regions(raw_input, tokens, operations, {});
    if (result.error != LzssShortMatchSelectionError::none) return result;
    if (raw_input.size() > std::numeric_limits<std::uint32_t>::max()) {
        result.error = LzssShortMatchSelectionError::arithmetic_overflow;
        return result;
    }
    for (std::size_t index = 0; index < 3; ++index) {
        result.candidate_index = index;
        const auto eligibility = static_cast<std::uint32_t>(index + 3);
        result.candidate = dictionary::internal::
            tokenize_lzss_short_match_candidate(
                raw_input, stream.dictionary, limits, eligibility, tokens);
        if (result.candidate.error
            != dictionary::internal::LzssShortMatchCandidateError::none) {
            result.error = LzssShortMatchSelectionError::candidate_error;
            return result;
        }
        result.frame = plan_lzss_short_match_frame(
            stream, limits, sequence, raw_already_committed,
            tokens.first(result.candidate.token_count), operations);
        if (result.frame.error != LzssShortMatchFrameEncodeError::none) {
            result.error = LzssShortMatchSelectionError::frame_error;
            return result;
        }
        result.candidate_frame_sizes[index] = result.frame.serialized_size;
        if (result.selected_minimum_length == 0
            || result.frame.serialized_size <= result.selected_frame_size) {
            result.selected_minimum_length = eligibility;
            result.selected_frame_size = result.frame.serialized_size;
            result.selected_token_count = result.candidate.token_count;
        }
    }
    result.candidate_index = result.selected_minimum_length - 3U;
    return result;
}

LzssShortMatchSelectionResult encode_lzss_short_match_candidate_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t raw_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> tokens,
    const std::span<context::internal::ModeledOperation> operations,
    const std::span<std::byte> serialized_output) noexcept {
    LzssShortMatchSelectionResult result{};
    result.error = check_regions(raw_input, tokens, operations,
                                 serialized_output);
    if (result.error != LzssShortMatchSelectionError::none) return result;
    result = plan_lzss_short_match_candidate_frame(
        stream, limits, sequence, raw_already_committed,
        raw_input, tokens, operations);
    if (result.error != LzssShortMatchSelectionError::none) return result;
    if (serialized_output.size() < result.selected_frame_size) {
        result.error =
            LzssShortMatchSelectionError::serialized_output_too_small;
        return result;
    }
    result.candidate = dictionary::internal::
        tokenize_lzss_short_match_candidate(
            raw_input, stream.dictionary, limits,
            result.selected_minimum_length, tokens);
    if (result.candidate.error
            != dictionary::internal::LzssShortMatchCandidateError::none
        || result.candidate.token_count != result.selected_token_count) {
        result.error = LzssShortMatchSelectionError::internal_error;
        return result;
    }
    result.frame = encode_lzss_short_match_frame(
        stream, limits, sequence, raw_already_committed,
        tokens.first(result.selected_token_count), operations,
        serialized_output.first(result.selected_frame_size));
    if (result.frame.error != LzssShortMatchFrameEncodeError::none
        || result.frame.serialized_size != result.selected_frame_size) {
        result.error = LzssShortMatchSelectionError::internal_error;
    }
    return result;
}

} // namespace marc::frame::internal
