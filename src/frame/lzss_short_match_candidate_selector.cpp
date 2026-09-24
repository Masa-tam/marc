#include "frame/lzss_short_match_candidate_selector.hpp"
#include "frame/lzss_short_length_escape_candidate_selector.hpp"

#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include "dictionary/lzss_short_length_escape_candidate.hpp"
#include "frame/lzss_short_length_escape_frame_encoder.hpp"

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
    const std::span<std::byte> serialized_output,
    const std::span<std::byte> finder_workspace = {}) noexcept {
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
        Region{serialized_output.data(), serialized_output.size()},
        Region{finder_workspace.data(), finder_workspace.size()}};
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

[[nodiscard]] dictionary::internal::LzssTypedTokenVariant token_variant(
    const bool length_escape) noexcept {
    return length_escape
        ? dictionary::internal::LzssTypedTokenVariant::
            field_context_64k_short_length_escape
        : dictionary::internal::LzssTypedTokenVariant::
            field_context_64k_short_match;
}

[[nodiscard]] dictionary::internal::LzssShortMatchCandidateResult
tokenize_candidate(
    const std::span<const std::byte> raw_input,
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint32_t eligibility,
    const std::span<dictionary::internal::LzssTypedToken> tokens,
    const bool length_escape) noexcept {
    return length_escape
        ? dictionary::internal::tokenize_lzss_short_length_escape_candidate(
            raw_input, stream.dictionary, limits, eligibility, tokens)
        : dictionary::internal::tokenize_lzss_short_match_candidate(
            raw_input, stream.dictionary, limits, eligibility, tokens);
}

[[nodiscard]] dictionary::internal::LzssShortMatchCandidateResult
tokenize_candidate_indexed(
    const std::span<const std::byte> raw_input,
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint32_t eligibility,
    const std::span<dictionary::internal::LzssTypedToken> tokens,
    const std::span<std::byte> finder_workspace,
    const bool length_escape) noexcept {
    return length_escape
        ? dictionary::internal::tokenize_lzss_short_length_escape_candidate_indexed(
            raw_input, stream.dictionary, limits, eligibility,
            tokens, finder_workspace)
        : dictionary::internal::tokenize_lzss_short_match_candidate_indexed(
            raw_input, stream.dictionary, limits, eligibility,
            tokens, finder_workspace);
}

[[nodiscard]] LzssShortMatchFrameEncodeResult plan_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t raw_already_committed,
    const std::span<const dictionary::internal::LzssTypedToken> tokens,
    const std::span<context::internal::ModeledOperation> operations,
    const bool length_escape) noexcept {
    return length_escape
        ? plan_lzss_short_length_escape_frame(
            stream, limits, sequence, raw_already_committed,
            tokens, operations)
        : plan_lzss_short_match_frame(
            stream, limits, sequence, raw_already_committed,
            tokens, operations);
}

[[nodiscard]] LzssShortMatchFrameEncodeResult encode_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t raw_already_committed,
    const std::span<const dictionary::internal::LzssTypedToken> tokens,
    const std::span<context::internal::ModeledOperation> operations,
    const std::span<std::byte> serialized_output,
    const bool length_escape) noexcept {
    return length_escape
        ? encode_lzss_short_length_escape_frame(
            stream, limits, sequence, raw_already_committed,
            tokens, operations, serialized_output)
        : encode_lzss_short_match_frame(
            stream, limits, sequence, raw_already_committed,
            tokens, operations, serialized_output);
}

[[nodiscard]] LzssShortMatchSelectionResult plan_candidate_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t raw_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> tokens,
    const std::span<context::internal::ModeledOperation> operations,
    const bool length_escape) noexcept {
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
        result.candidate = tokenize_candidate(
            raw_input, stream, limits, eligibility, tokens, length_escape);
        if (result.candidate.error
            != dictionary::internal::LzssShortMatchCandidateError::none) {
            result.error = LzssShortMatchSelectionError::candidate_error;
            return result;
        }
        result.frame = plan_frame(
            stream, limits, sequence, raw_already_committed,
            tokens.first(result.candidate.token_count), operations,
            length_escape);
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

[[nodiscard]] LzssShortMatchSelectionResult encode_candidate_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t raw_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> tokens,
    const std::span<context::internal::ModeledOperation> operations,
    const std::span<std::byte> serialized_output,
    const bool length_escape) noexcept {
    LzssShortMatchSelectionResult result{};
    result.error = check_regions(raw_input, tokens, operations,
                                 serialized_output);
    if (result.error != LzssShortMatchSelectionError::none) return result;
    result = plan_candidate_frame(
        stream, limits, sequence, raw_already_committed,
        raw_input, tokens, operations, length_escape);
    if (result.error != LzssShortMatchSelectionError::none) return result;
    if (serialized_output.size() < result.selected_frame_size) {
        result.error =
            LzssShortMatchSelectionError::serialized_output_too_small;
        return result;
    }
    result.candidate = tokenize_candidate(
        raw_input, stream, limits, result.selected_minimum_length,
        tokens, length_escape);
    if (result.candidate.error
            != dictionary::internal::LzssShortMatchCandidateError::none
        || result.candidate.token_count != result.selected_token_count) {
        result.error = LzssShortMatchSelectionError::internal_error;
        return result;
    }
    result.frame = encode_frame(
        stream, limits, sequence, raw_already_committed,
        tokens.first(result.selected_token_count), operations,
        serialized_output.first(result.selected_frame_size), length_escape);
    if (result.frame.error != LzssShortMatchFrameEncodeError::none
        || result.frame.serialized_size != result.selected_frame_size) {
        result.error = LzssShortMatchSelectionError::internal_error;
    }
    return result;
}

[[nodiscard]] LzssShortMatchSelectionResult plan_candidate_frame_indexed(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t raw_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> tokens,
    const std::span<context::internal::ModeledOperation> operations,
    const std::span<std::byte> finder_workspace,
    const bool length_escape) noexcept {
    LzssShortMatchSelectionResult result{};
    result.error = check_regions(raw_input, tokens, operations, {},
                                 finder_workspace);
    if (result.error != LzssShortMatchSelectionError::none) return result;
    if (raw_input.size() > std::numeric_limits<std::uint32_t>::max()) {
        result.error = LzssShortMatchSelectionError::arithmetic_overflow;
        return result;
    }
    const auto needed = dictionary::internal::
        calculate_lzss_short_prefix_workspace(
            raw_input.size(), stream.dictionary, limits,
            token_variant(length_escape));
    if (needed.error != dictionary::internal::LzssShortPrefixError::none) {
        result.candidate.finder_error = needed.error;
        switch (needed.error) {
        case dictionary::internal::LzssShortPrefixError::invalid_parameters:
            result.candidate.error = dictionary::internal::
                LzssShortMatchCandidateError::invalid_parameters;
            break;
        case dictionary::internal::LzssShortPrefixError::input_limit_exceeded:
            result.candidate.error = dictionary::internal::
                LzssShortMatchCandidateError::input_limit_exceeded;
            break;
        case dictionary::internal::LzssShortPrefixError::arithmetic_overflow:
            result.candidate.error = dictionary::internal::
                LzssShortMatchCandidateError::arithmetic_overflow;
            break;
        default:
            result.candidate.error = dictionary::internal::
                LzssShortMatchCandidateError::token_storage_limit_exceeded;
            break;
        }
        result.error = LzssShortMatchSelectionError::candidate_error;
        return result;
    }
    if (needed.workspace_size > limits.max_internal_buffered_bytes
        || limits.max_internal_buffered_bytes - needed.workspace_size
               < limits.max_block_size) {
        result.candidate.error = dictionary::internal::
            LzssShortMatchCandidateError::token_storage_limit_exceeded;
        result.error = LzssShortMatchSelectionError::candidate_error;
        return result;
    }
    auto frame_limits = limits;
    frame_limits.max_internal_buffered_bytes -= needed.workspace_size;
    for (std::size_t index = 0; index < 3; ++index) {
        result.candidate_index = index;
        const auto eligibility = static_cast<std::uint32_t>(index + 3);
        result.candidate = tokenize_candidate_indexed(
            raw_input, stream, limits, eligibility, tokens,
            finder_workspace, length_escape);
        if (result.candidate.error
            != dictionary::internal::LzssShortMatchCandidateError::none) {
            result.error = LzssShortMatchSelectionError::candidate_error;
            return result;
        }
        result.frame = plan_frame(
            stream, frame_limits, sequence, raw_already_committed,
            tokens.first(result.candidate.token_count), operations,
            length_escape);
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

[[nodiscard]] LzssShortMatchSelectionResult encode_candidate_frame_indexed(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t raw_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> tokens,
    const std::span<context::internal::ModeledOperation> operations,
    const std::span<std::byte> finder_workspace,
    const std::span<std::byte> serialized_output,
    const bool length_escape) noexcept {
    LzssShortMatchSelectionResult result{};
    result.error = check_regions(raw_input, tokens, operations,
                                 serialized_output, finder_workspace);
    if (result.error != LzssShortMatchSelectionError::none) return result;
    result = plan_candidate_frame_indexed(
        stream, limits, sequence, raw_already_committed,
        raw_input, tokens, operations, finder_workspace, length_escape);
    if (result.error != LzssShortMatchSelectionError::none) return result;
    if (serialized_output.size() < result.selected_frame_size) {
        result.error =
            LzssShortMatchSelectionError::serialized_output_too_small;
        return result;
    }
    const auto needed = dictionary::internal::
        calculate_lzss_short_prefix_workspace(
            raw_input.size(), stream.dictionary, limits,
            token_variant(length_escape));
    if (needed.error != dictionary::internal::LzssShortPrefixError::none) {
        result.error = LzssShortMatchSelectionError::internal_error;
        return result;
    }
    auto frame_limits = limits;
    frame_limits.max_internal_buffered_bytes -= needed.workspace_size;
    result.candidate = tokenize_candidate_indexed(
        raw_input, stream, limits, result.selected_minimum_length,
        tokens, finder_workspace, length_escape);
    if (result.candidate.error
            != dictionary::internal::LzssShortMatchCandidateError::none
        || result.candidate.token_count != result.selected_token_count) {
        result.error = LzssShortMatchSelectionError::internal_error;
        return result;
    }
    result.frame = encode_frame(
        stream, frame_limits, sequence, raw_already_committed,
        tokens.first(result.selected_token_count), operations,
        serialized_output.first(result.selected_frame_size), length_escape);
    if (result.frame.error != LzssShortMatchFrameEncodeError::none
        || result.frame.serialized_size != result.selected_frame_size) {
        result.error = LzssShortMatchSelectionError::internal_error;
    }
    return result;
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
    return plan_candidate_frame(stream, limits, sequence,
                                raw_already_committed, raw_input, tokens,
                                operations, false);
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
    return encode_candidate_frame(stream, limits, sequence,
                                  raw_already_committed, raw_input, tokens,
                                  operations, serialized_output, false);
}

LzssShortMatchSelectionResult plan_lzss_short_match_candidate_frame_indexed(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t raw_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> tokens,
    const std::span<context::internal::ModeledOperation> operations,
    const std::span<std::byte> finder_workspace) noexcept {
    return plan_candidate_frame_indexed(
        stream, limits, sequence, raw_already_committed, raw_input,
        tokens, operations, finder_workspace, false);
}

LzssShortMatchSelectionResult encode_lzss_short_match_candidate_frame_indexed(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t raw_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> tokens,
    const std::span<context::internal::ModeledOperation> operations,
    const std::span<std::byte> finder_workspace,
    const std::span<std::byte> serialized_output) noexcept {
    return encode_candidate_frame_indexed(
        stream, limits, sequence, raw_already_committed, raw_input,
        tokens, operations, finder_workspace, serialized_output, false);
}

LzssShortMatchSelectionResult plan_lzss_short_length_escape_candidate_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t raw_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> tokens,
    const std::span<context::internal::ModeledOperation> operations) noexcept {
    return plan_candidate_frame(stream, limits, sequence,
                                raw_already_committed, raw_input, tokens,
                                operations, true);
}

LzssShortMatchSelectionResult encode_lzss_short_length_escape_candidate_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t raw_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> tokens,
    const std::span<context::internal::ModeledOperation> operations,
    const std::span<std::byte> serialized_output) noexcept {
    return encode_candidate_frame(stream, limits, sequence,
                                  raw_already_committed, raw_input, tokens,
                                  operations, serialized_output, true);
}

LzssShortMatchSelectionResult plan_lzss_short_length_escape_candidate_frame_indexed(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t raw_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> tokens,
    const std::span<context::internal::ModeledOperation> operations,
    const std::span<std::byte> finder_workspace) noexcept {
    return plan_candidate_frame_indexed(
        stream, limits, sequence, raw_already_committed, raw_input,
        tokens, operations, finder_workspace, true);
}

LzssShortMatchSelectionResult encode_lzss_short_length_escape_candidate_frame_indexed(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t raw_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> tokens,
    const std::span<context::internal::ModeledOperation> operations,
    const std::span<std::byte> finder_workspace,
    const std::span<std::byte> serialized_output) noexcept {
    return encode_candidate_frame_indexed(
        stream, limits, sequence, raw_already_committed, raw_input,
        tokens, operations, finder_workspace, serialized_output, true);
}

} // namespace marc::frame::internal
