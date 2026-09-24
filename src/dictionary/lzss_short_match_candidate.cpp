#include "dictionary/lzss_short_match_candidate.hpp"

#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include "dictionary/lzss_match_finder.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::dictionary::internal {
namespace {

[[nodiscard]] LzssShortMatchCandidateResult parse(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const std::uint32_t eligibility,
    const std::span<LzssTypedToken> output) noexcept {
    LzssShortMatchCandidateResult result{};
    result.input_size = input.size();
    LzssExhaustiveMatchFinder finder{input, parameters};
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

[[nodiscard]] LzssShortMatchCandidateResult preflight(
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
    result = parse(input, parameters, eligibility, {});
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
    const auto written = parse(input, parameters, minimum_eligible_length,
                               output.first(result.token_count));
    if (written.error != LzssShortMatchCandidateError::none
        || written.token_count != result.token_count
        || written.token_storage_size != result.token_storage_size) {
        result.error = LzssShortMatchCandidateError::internal_error;
        return result;
    }
    return result;
}

} // namespace marc::dictionary::internal
