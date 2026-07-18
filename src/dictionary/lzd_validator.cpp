#include "dictionary/lzd_validator.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <limits>

namespace marc::dictionary::internal {
namespace {

[[nodiscard]] bool reference_length(
    const std::uint32_t reference, const std::uint32_t dictionary_entries,
    const std::span<LzdPhraseEntry> phrase_workspace,
    std::uint64_t& length) noexcept {
    if (reference < lzd_first_phrase_reference) {
        length = 1;
        return true;
    }
    if (reference == lzd_absent_reference) return false;
    const auto phrase_index = reference - lzd_first_phrase_reference;
    if (phrase_index >= dictionary_entries) return false;
    length = phrase_workspace[phrase_index].length;
    return true;
}

} // namespace

std::size_t lzd_validation_workspace_entries(
    const std::size_t serialized_size,
    const LzdParameters& parameters) noexcept {
    const auto token_count = serialized_size / lzd_token_size;
    return std::min(token_count,
                    static_cast<std::size_t>(parameters.maximum_entries));
}

std::size_t lzd_validation_workspace_entries(
    const std::size_t serialized_size,
    const std::uint64_t declared_frame_size,
    const LzdParameters& parameters) noexcept {
    const auto token_count = serialized_size / lzd_token_size;
    const auto raw_phrase_bound = declared_frame_size / 2;
    constexpr auto size_max = std::numeric_limits<std::size_t>::max();
    const auto size_bound = raw_phrase_bound > size_max
        ? size_max : static_cast<std::size_t>(raw_phrase_bound);
    return std::min({token_count, size_bound,
                     static_cast<std::size_t>(parameters.maximum_entries)});
}

LzdValidationResult validate_lzd_token_stream(
    const std::span<const std::byte> input,
    const LzdParameters& parameters,
    const std::uint64_t declared_frame_size,
    const core::DecoderLimits& limits,
    const std::span<LzdPhraseEntry> phrase_workspace) noexcept {
    LzdValidationResult result{};
    if (core::validate_limits(limits) != core::LimitError::none) {
        result.error = LzdValidationError::limit_exceeded;
        return result;
    }
    result.format_error = validate_lzd_parameters(parameters, limits);
    if (result.format_error != LzdFormatError::none) {
        result.error = LzdValidationError::invalid_parameters;
        return result;
    }
    if (declared_frame_size > limits.max_frame_size
        || input.size() > limits.max_dictionary_serialized_size
        || input.size() > limits.max_internal_buffered_bytes) {
        result.error = LzdValidationError::limit_exceeded;
        return result;
    }
    if (input.size() % lzd_token_size != 0) {
        result.token_count = input.size() / lzd_token_size;
        result.token_index = result.token_count;
        result.input_offset = result.token_count * lzd_token_size;
        result.error = LzdValidationError::truncated_token;
        return result;
    }
    result.token_count = input.size() / lzd_token_size;
    if (declared_frame_size == 0) {
        if (!input.empty()) result.error = LzdValidationError::trailing_tokens;
        return result;
    }

    const auto required_entries = lzd_validation_workspace_entries(
        input.size(), declared_frame_size, parameters);
    std::uint64_t workspace_bytes{};
    std::uint64_t aggregate_bytes{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(required_entries),
            static_cast<std::uint64_t>(sizeof(LzdPhraseEntry)),
            workspace_bytes)
        || !core::checked_add(static_cast<std::uint64_t>(input.size()),
                              workspace_bytes, aggregate_bytes)
        || aggregate_bytes > limits.max_internal_buffered_bytes) {
        result.error = LzdValidationError::limit_exceeded;
        return result;
    }
    if (phrase_workspace.size() < required_entries) {
        result.error = LzdValidationError::workspace_too_small;
        return result;
    }

    for (std::size_t index = 0; index < result.token_count; ++index) {
        result.token_index = index;
        result.input_offset = index * lzd_token_size;
        if (result.output_size == declared_frame_size) {
            result.error = LzdValidationError::trailing_tokens;
            return result;
        }

        const std::span<const std::byte, lzd_token_size> encoded{
            input.data() + result.input_offset, lzd_token_size};
        LzdToken token{};
        result.format_error = parse_lzd_token(encoded, token);
        if (result.format_error != LzdFormatError::none) {
            result.error = LzdValidationError::token_error;
            return result;
        }

        std::uint64_t left_length{};
        if (!reference_length(token.left_reference, result.dictionary_entries,
                              phrase_workspace, left_length)) {
            result.format_error = LzdFormatError::invalid_phrase_reference;
            result.error = LzdValidationError::token_error;
            return result;
        }
        std::uint64_t phrase_length = left_length;
        const bool terminal = token.right_reference == lzd_absent_reference;
        if (!terminal) {
            std::uint64_t right_length{};
            if (!reference_length(token.right_reference,
                                  result.dictionary_entries,
                                  phrase_workspace, right_length)) {
                result.format_error = LzdFormatError::invalid_phrase_reference;
                result.error = LzdValidationError::token_error;
                return result;
            }
            if (!core::checked_add(phrase_length, right_length,
                                   phrase_length)) {
                result.format_error = LzdFormatError::arithmetic_overflow;
                result.error = LzdValidationError::token_error;
                return result;
            }
        }

        std::uint64_t next_output{};
        if (!core::checked_add(result.output_size, phrase_length,
                               next_output)) {
            result.format_error = LzdFormatError::arithmetic_overflow;
            result.error = LzdValidationError::token_error;
            return result;
        }
        if (next_output > declared_frame_size) {
            result.format_error = LzdFormatError::output_size_mismatch;
            result.error = LzdValidationError::token_error;
            return result;
        }
        if (terminal
            && (next_output != declared_frame_size
                || index + 1 != result.token_count)) {
            result.format_error = LzdFormatError::invalid_terminal_reference;
            result.error = LzdValidationError::token_error;
            return result;
        }

        if (!terminal
            && result.dictionary_entries < parameters.maximum_entries) {
            phrase_workspace[result.dictionary_entries] = {
                token.left_reference, token.right_reference, phrase_length};
            ++result.dictionary_entries;
        }
        result.output_size = next_output;
    }

    result.token_index = result.token_count;
    result.input_offset = input.size();
    if (result.output_size != declared_frame_size)
        result.error = LzdValidationError::premature_end;
    return result;
}

} // namespace marc::dictionary::internal
