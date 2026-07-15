#include "dictionary/lzmw_validator.hpp"

#include "core/checked_math.hpp"

#include <algorithm>

namespace marc::dictionary::internal {
namespace {

[[nodiscard]] bool reference_length(
    const std::uint32_t reference, const std::uint32_t dictionary_entries,
    const std::span<LzmwPhraseEntry> phrase_workspace,
    std::uint64_t& length) noexcept {
    if (reference < lzmw_first_phrase_reference) {
        length = 1;
        return true;
    }
    const auto phrase_index = reference - lzmw_first_phrase_reference;
    if (phrase_index >= dictionary_entries) return false;
    length = phrase_workspace[phrase_index].length;
    return true;
}

} // namespace

std::size_t lzmw_validation_workspace_entries(
    const std::size_t serialized_size,
    const LzmwParameters& parameters) noexcept {
    const auto token_count = serialized_size / lzmw_token_size;
    const auto possible_entries = token_count == 0 ? std::size_t{0}
                                                    : token_count - 1;
    return std::min(possible_entries,
                    static_cast<std::size_t>(parameters.maximum_entries));
}

LzmwValidationResult validate_lzmw_token_stream(
    const std::span<const std::byte> input,
    const LzmwParameters& parameters,
    const std::uint64_t declared_frame_size,
    const core::DecoderLimits& limits,
    const std::span<LzmwPhraseEntry> phrase_workspace) noexcept {
    LzmwValidationResult result{};
    if (core::validate_limits(limits) != core::LimitError::none) {
        result.error = LzmwValidationError::limit_exceeded;
        return result;
    }
    result.format_error = validate_lzmw_parameters(parameters, limits);
    if (result.format_error != LzmwFormatError::none) {
        result.error = LzmwValidationError::invalid_parameters;
        return result;
    }
    if (declared_frame_size > limits.max_frame_size
        || input.size() > limits.max_dictionary_serialized_size
        || input.size() > limits.max_internal_buffered_bytes) {
        result.error = LzmwValidationError::limit_exceeded;
        return result;
    }
    if (input.size() % lzmw_token_size != 0) {
        result.token_count = input.size() / lzmw_token_size;
        result.token_index = result.token_count;
        result.input_offset = result.token_count * lzmw_token_size;
        result.error = LzmwValidationError::truncated_token;
        return result;
    }
    result.token_count = input.size() / lzmw_token_size;
    if (declared_frame_size == 0) {
        if (!input.empty()) result.error = LzmwValidationError::trailing_tokens;
        return result;
    }

    const auto required_entries =
        lzmw_validation_workspace_entries(input.size(), parameters);
    std::uint64_t workspace_bytes{};
    std::uint64_t aggregate_bytes{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(required_entries),
            static_cast<std::uint64_t>(sizeof(LzmwPhraseEntry)),
            workspace_bytes)
        || !core::checked_add(static_cast<std::uint64_t>(input.size()),
                              workspace_bytes, aggregate_bytes)
        || aggregate_bytes > limits.max_internal_buffered_bytes) {
        result.error = LzmwValidationError::limit_exceeded;
        return result;
    }
    if (phrase_workspace.size() < required_entries) {
        result.error = LzmwValidationError::workspace_too_small;
        return result;
    }

    std::uint32_t previous_reference{};
    std::uint64_t previous_length{};
    for (std::size_t index = 0; index < result.token_count; ++index) {
        result.token_index = index;
        result.input_offset = index * lzmw_token_size;
        if (result.output_size == declared_frame_size) {
            result.error = LzmwValidationError::trailing_tokens;
            return result;
        }

        const std::span<const std::byte, lzmw_token_size> encoded{
            input.data() + result.input_offset, lzmw_token_size};
        std::uint32_t reference{};
        result.format_error = parse_lzmw_token(encoded, reference);
        if (result.format_error != LzmwFormatError::none) {
            result.error = LzmwValidationError::token_error;
            return result;
        }
        std::uint64_t phrase_length{};
        if (!reference_length(reference, result.dictionary_entries,
                              phrase_workspace, phrase_length)) {
            result.format_error = LzmwFormatError::invalid_phrase_reference;
            result.error = LzmwValidationError::token_error;
            return result;
        }

        std::uint64_t next_output{};
        if (!core::checked_add(result.output_size, phrase_length,
                               next_output)) {
            result.format_error = LzmwFormatError::arithmetic_overflow;
            result.error = LzmwValidationError::token_error;
            return result;
        }
        if (next_output > declared_frame_size) {
            result.format_error = LzmwFormatError::output_size_mismatch;
            result.error = LzmwValidationError::token_error;
            return result;
        }

        if (index != 0
            && result.dictionary_entries < parameters.maximum_entries) {
            std::uint64_t combined_length{};
            if (!core::checked_add(previous_length, phrase_length,
                                   combined_length)) {
                result.format_error = LzmwFormatError::arithmetic_overflow;
                result.error = LzmwValidationError::token_error;
                return result;
            }
            phrase_workspace[result.dictionary_entries] = {
                previous_reference, reference, combined_length};
            ++result.dictionary_entries;
        }
        previous_reference = reference;
        previous_length = phrase_length;
        result.output_size = next_output;
    }

    result.token_index = result.token_count;
    result.input_offset = input.size();
    if (result.output_size != declared_frame_size)
        result.error = LzmwValidationError::premature_end;
    return result;
}

} // namespace marc::dictionary::internal
