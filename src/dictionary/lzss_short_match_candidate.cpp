#include "dictionary/lzss_short_match_candidate.hpp"

#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include "dictionary/lzss_match_finder.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::dictionary::internal {
namespace {

template <LzssMatchFinder Finder>
[[nodiscard]] LzssShortMatchCandidateResult parse_with_finder(
    const std::span<const std::byte> input,
    const std::uint32_t eligibility,
    const std::span<LzssTypedToken> output,
    Finder& finder) noexcept {
    LzssShortMatchCandidateResult result{};
    result.input_size = input.size();
    std::size_t position{};
    while (position < input.size()) {
        const auto match = finder.find_match(position);
        const bool use_match = match.length >= eligibility;
        const std::size_t advance = use_match ? match.length : 1U;
        if (advance == 0 || advance > input.size() - position
            || result.token_count == std::numeric_limits<std::size_t>::max()) {
            result.error = LzssShortMatchCandidateError::internal_error;
            return result;
        }
        if (!output.empty()) {
            output[result.token_count] = use_match
                ? LzssTypedToken{LzssTypedTokenKind::match, 0,
                                 match.distance, match.length}
                : LzssTypedToken{
                      LzssTypedTokenKind::literal,
                      std::to_integer<std::uint8_t>(input[position]), 0, 0};
        }
        ++result.token_count;
        finder.advance(position, position + advance);
        position += advance;
    }
    if (!core::checked_multiply(result.token_count,
                                sizeof(LzssTypedToken),
                                result.token_storage_size)) {
        result.error = LzssShortMatchCandidateError::arithmetic_overflow;
    }
    return result;
}

[[nodiscard]] LzssShortMatchCandidateResult validate_input(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint32_t eligibility) noexcept {
    LzssShortMatchCandidateResult result{};
    result.input_size = input.size();
    if (eligibility < 3 || eligibility > 5) {
        result.error = LzssShortMatchCandidateError::invalid_eligibility;
        return result;
    }
    result.token_error = validate_lzss_typed_parameters(
        parameters, limits,
        LzssTypedTokenVariant::field_context_64k_short_match);
    if (result.token_error != LzssTypedTokenError::none) {
        result.error = result.token_error == LzssTypedTokenError::limit_exceeded
            ? LzssShortMatchCandidateError::input_limit_exceeded
            : LzssShortMatchCandidateError::invalid_parameters;
        return result;
    }
    if (input.size() > 65536 || input.size() > limits.max_frame_size
        || input.size() > limits.max_block_size
        || input.size() > limits.max_total_output_size) {
        result.error = LzssShortMatchCandidateError::input_limit_exceeded;
        return result;
    }
    return result;
}

[[nodiscard]] LzssShortMatchCandidateResult preflight(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint32_t eligibility) noexcept {
    auto result = validate_input(input, parameters, limits, eligibility);
    if (result.error != LzssShortMatchCandidateError::none) return result;
    LzssExhaustiveMatchFinder finder{input, parameters};
    result = parse_with_finder(input, eligibility, {}, finder);
    if (result.error != LzssShortMatchCandidateError::none) return result;
    std::size_t aggregate{};
    if (!core::checked_add(input.size(), result.token_storage_size,
                           aggregate)) {
        result.error = LzssShortMatchCandidateError::arithmetic_overflow;
    } else if (aggregate > limits.max_internal_buffered_bytes) {
        result.error =
            LzssShortMatchCandidateError::token_storage_limit_exceeded;
    }
    return result;
}

void set_finder_error(LzssShortMatchCandidateResult& result,
                      const LzssShortPrefixError error) noexcept {
    result.finder_error = error;
    switch (error) {
    case LzssShortPrefixError::none:
        return;
    case LzssShortPrefixError::invalid_parameters:
        result.error = LzssShortMatchCandidateError::invalid_parameters;
        return;
    case LzssShortPrefixError::input_limit_exceeded:
        result.error = LzssShortMatchCandidateError::input_limit_exceeded;
        return;
    case LzssShortPrefixError::arithmetic_overflow:
        result.error = LzssShortMatchCandidateError::arithmetic_overflow;
        return;
    case LzssShortPrefixError::workspace_limit_exceeded:
        result.error =
            LzssShortMatchCandidateError::token_storage_limit_exceeded;
        return;
    case LzssShortPrefixError::workspace_too_small:
        result.error = LzssShortMatchCandidateError::workspace_too_small;
        return;
    case LzssShortPrefixError::misaligned_workspace:
        result.error = LzssShortMatchCandidateError::misaligned_workspace;
        return;
    case LzssShortPrefixError::overlapping_buffers:
        result.error = LzssShortMatchCandidateError::overlapping_buffers;
        return;
    }
    result.error = LzssShortMatchCandidateError::internal_error;
}

} // namespace

LzssShortMatchCandidateResult plan_lzss_short_match_candidate(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint32_t minimum_eligible_length) noexcept {
    return preflight(input, parameters, limits, minimum_eligible_length);
}

LzssShortMatchCandidateResult tokenize_lzss_short_match_candidate(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint32_t minimum_eligible_length,
    const std::span<LzssTypedToken> output) noexcept {
    auto result = preflight(input, parameters, limits,
                            minimum_eligible_length);
    if (result.error != LzssShortMatchCandidateError::none) return result;
    if (output.size() < result.token_count) {
        result.error = LzssShortMatchCandidateError::output_too_small;
        return result;
    }
    std::size_t output_bytes{};
    if (!core::checked_multiply(output.size(), sizeof(LzssTypedToken),
                                output_bytes)) {
        result.error = LzssShortMatchCandidateError::arithmetic_overflow;
        return result;
    }
    const auto overlap = core::check_buffer_overlap(
        input.data(), input.size(), output.data(), output_bytes);
    if (overlap == core::BufferOverlap::arithmetic_overflow) {
        result.error = LzssShortMatchCandidateError::arithmetic_overflow;
        return result;
    }
    if (overlap == core::BufferOverlap::overlap) {
        result.error = LzssShortMatchCandidateError::overlapping_buffers;
        return result;
    }
    LzssExhaustiveMatchFinder finder{input, parameters};
    const auto written = parse_with_finder(
        input, minimum_eligible_length, output.first(result.token_count),
        finder);
    if (written.error != LzssShortMatchCandidateError::none
        || written.token_count != result.token_count
        || written.token_storage_size != result.token_storage_size) {
        result.error = LzssShortMatchCandidateError::internal_error;
        return result;
    }
    return result;
}

LzssShortMatchCandidateResult plan_lzss_short_match_candidate_indexed(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint32_t minimum_eligible_length,
    const std::span<std::byte> finder_workspace) noexcept {
    auto result = validate_input(input, parameters, limits,
                                 minimum_eligible_length);
    if (result.error != LzssShortMatchCandidateError::none) return result;
    const auto required = calculate_lzss_short_prefix_workspace(
        input.size(), parameters, limits);
    if (required.error != LzssShortPrefixError::none) {
        set_finder_error(result, required.error);
        return result;
    }
    LzssShortPrefixMatchFinder finder{};
    const auto initialized = initialize_lzss_short_prefix_match_finder(
        input, parameters, limits, finder_workspace, finder);
    if (initialized != LzssShortPrefixError::none) {
        set_finder_error(result, initialized);
        return result;
    }
    result = parse_with_finder(input, minimum_eligible_length, {}, finder);
    if (result.error != LzssShortMatchCandidateError::none) return result;
    std::size_t aggregate{};
    if (!core::checked_add(input.size(), result.token_storage_size, aggregate)
        || !core::checked_add(aggregate, required.workspace_size, aggregate)) {
        result.error = LzssShortMatchCandidateError::arithmetic_overflow;
    } else if (aggregate > limits.max_internal_buffered_bytes) {
        result.error =
            LzssShortMatchCandidateError::token_storage_limit_exceeded;
    }
    return result;
}

LzssShortMatchCandidateResult tokenize_lzss_short_match_candidate_indexed(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::uint32_t minimum_eligible_length,
    const std::span<LzssTypedToken> output,
    const std::span<std::byte> finder_workspace) noexcept {
    LzssShortMatchCandidateResult result{};
    result.input_size = input.size();
    std::size_t output_bytes{};
    if (!core::checked_multiply(output.size(), sizeof(LzssTypedToken),
                                output_bytes)) {
        result.error = LzssShortMatchCandidateError::arithmetic_overflow;
        return result;
    }
    const auto input_output = core::check_buffer_overlap(
        input.data(), input.size(), output.data(), output_bytes);
    const auto input_workspace = core::check_buffer_overlap(
        input.data(), input.size(), finder_workspace.data(),
        finder_workspace.size());
    const auto output_workspace = core::check_buffer_overlap(
        output.data(), output_bytes, finder_workspace.data(),
        finder_workspace.size());
    if (input_output == core::BufferOverlap::arithmetic_overflow
        || input_workspace == core::BufferOverlap::arithmetic_overflow
        || output_workspace == core::BufferOverlap::arithmetic_overflow) {
        result.error = LzssShortMatchCandidateError::arithmetic_overflow;
        return result;
    }
    if (input_output == core::BufferOverlap::overlap
        || input_workspace == core::BufferOverlap::overlap
        || output_workspace == core::BufferOverlap::overlap) {
        result.error = LzssShortMatchCandidateError::overlapping_buffers;
        return result;
    }
    result = plan_lzss_short_match_candidate_indexed(
        input, parameters, limits, minimum_eligible_length,
        finder_workspace);
    if (result.error != LzssShortMatchCandidateError::none) return result;
    if (output.size() < result.token_count) {
        result.error = LzssShortMatchCandidateError::output_too_small;
        return result;
    }
    LzssShortPrefixMatchFinder finder{};
    const auto initialized = initialize_lzss_short_prefix_match_finder(
        input, parameters, limits, finder_workspace, finder);
    if (initialized != LzssShortPrefixError::none) {
        set_finder_error(result, initialized);
        return result;
    }
    const auto written = parse_with_finder(
        input, minimum_eligible_length, output.first(result.token_count),
        finder);
    if (written.error != LzssShortMatchCandidateError::none
        || written.token_count != result.token_count
        || written.token_storage_size != result.token_storage_size) {
        result.error = LzssShortMatchCandidateError::internal_error;
    }
    return result;
}

} // namespace marc::dictionary::internal
