#include "dictionary/lz78_validator.hpp"

#include "core/checked_math.hpp"

#include <algorithm>

namespace marc::dictionary::internal {

std::size_t lz78_validation_workspace_entries(
    const std::size_t serialized_size,
    const Lz78Parameters& parameters) noexcept {
    const auto token_count = serialized_size / lz78_token_size;
    return std::min(token_count,
                    static_cast<std::size_t>(parameters.maximum_entries));
}

Lz78ValidationResult validate_lz78_token_stream(
    const std::span<const std::byte> input,
    const Lz78Parameters& parameters,
    const std::uint64_t declared_frame_size,
    const core::DecoderLimits& limits,
    const std::span<Lz78PhraseEntry> phrase_workspace) noexcept {
    Lz78ValidationResult result{};
    if (core::validate_limits(limits) != core::LimitError::none) {
        result.error = Lz78ValidationError::limit_exceeded;
        return result;
    }
    result.format_error = validate_lz78_parameters(parameters, limits);
    if (result.format_error != Lz78FormatError::none) {
        result.error = Lz78ValidationError::invalid_parameters;
        return result;
    }
    if (declared_frame_size > limits.max_frame_size
        || input.size() > limits.max_internal_buffered_bytes
        || input.size() > limits.max_dictionary_serialized_size) {
        result.error = Lz78ValidationError::limit_exceeded;
        return result;
    }
    if (input.size() % lz78_token_size != 0) {
        result.token_count = input.size() / lz78_token_size;
        result.token_index = result.token_count;
        result.input_offset = result.token_count * lz78_token_size;
        result.error = Lz78ValidationError::truncated_token;
        return result;
    }
    result.token_count = input.size() / lz78_token_size;
    if (phrase_workspace.size()
        < lz78_validation_workspace_entries(input.size(), parameters)) {
        result.error = Lz78ValidationError::workspace_too_small;
        return result;
    }

    for (std::size_t index = 0; index < result.token_count; ++index) {
        result.token_index = index;
        result.input_offset = index * lz78_token_size;
        if (result.output_size == declared_frame_size) {
            result.error = Lz78ValidationError::trailing_tokens;
            return result;
        }
        const std::span<const std::byte, lz78_token_size> encoded{
            input.data() + result.input_offset, lz78_token_size};
        Lz78Token token{};
        result.format_error = parse_lz78_token(encoded, token);
        if (result.format_error != Lz78FormatError::none) {
            result.error = Lz78ValidationError::token_error;
            return result;
        }
        if (token.phrase_index > result.dictionary_entries) {
            result.format_error = Lz78FormatError::invalid_phrase_index;
            result.error = Lz78ValidationError::token_error;
            return result;
        }

        std::uint64_t phrase_length{};
        if (token.phrase_index != 0)
            phrase_length = phrase_workspace[token.phrase_index - 1].length;
        if (token.tag == Lz78TokenTag::pair) {
            if (!core::checked_add(phrase_length, UINT64_C(1), phrase_length)) {
                result.format_error = Lz78FormatError::arithmetic_overflow;
                result.error = Lz78ValidationError::token_error;
                return result;
            }
        } else if (token.phrase_index == 0) {
            result.format_error = Lz78FormatError::invalid_final_index;
            result.error = Lz78ValidationError::token_error;
            return result;
        }

        std::uint64_t next_output{};
        if (!core::checked_add(result.output_size, phrase_length, next_output)) {
            result.format_error = Lz78FormatError::arithmetic_overflow;
            result.error = Lz78ValidationError::token_error;
            return result;
        }
        if (next_output > declared_frame_size) {
            result.format_error = Lz78FormatError::output_size_mismatch;
            result.error = Lz78ValidationError::token_error;
            return result;
        }
        if (token.tag == Lz78TokenTag::final_index
            && (next_output != declared_frame_size
                || index + 1 != result.token_count)) {
            result.format_error = Lz78FormatError::output_size_mismatch;
            result.error = index + 1 != result.token_count
                ? Lz78ValidationError::trailing_tokens
                : Lz78ValidationError::token_error;
            return result;
        }

        if (token.tag == Lz78TokenTag::pair
            && result.dictionary_entries < parameters.maximum_entries) {
            phrase_workspace[result.dictionary_entries] = {
                token.phrase_index, token.symbol, phrase_length};
            ++result.dictionary_entries;
        }
        result.output_size = next_output;
    }

    result.token_index = result.token_count;
    result.input_offset = input.size();
    if (result.output_size != declared_frame_size)
        result.error = Lz78ValidationError::premature_end;
    return result;
}

} // namespace marc::dictionary::internal
