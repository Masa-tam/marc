#include "dictionary/lzss_validator.hpp"

namespace marc::dictionary::internal {

LzssValidationResult validate_lzss_token_stream(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const std::uint64_t declared_frame_size,
    const core::DecoderLimits& limits) noexcept {
    LzssValidationResult result{};
    if (core::validate_limits(limits) != core::LimitError::none) {
        result.error = LzssValidationError::limit_exceeded;
        return result;
    }
    const auto parameter_error = validate_lzss_parameters(parameters, limits);
    if (parameter_error != LzssFormatError::none) {
        result.format_error = parameter_error;
        result.error = LzssValidationError::invalid_parameters;
        return result;
    }
    if (declared_frame_size > limits.max_frame_size
        || input.size() > limits.max_internal_buffered_bytes
        || input.size() > limits.max_dictionary_serialized_size) {
        result.error = LzssValidationError::limit_exceeded;
        return result;
    }

    while (result.input_offset < input.size()) {
        result.token_index = result.token_count;
        if (result.output_size == declared_frame_size) {
            result.error = LzssValidationError::trailing_tokens;
            return result;
        }
        LzssToken token{};
        std::size_t consumed{};
        result.format_error = parse_lzss_token(
            input.subspan(result.input_offset), token, consumed);
        if (result.format_error != LzssFormatError::none) {
            result.error = result.format_error == LzssFormatError::truncated_token
                ? LzssValidationError::truncated_token
                : LzssValidationError::token_error;
            return result;
        }
        std::uint64_t next_size{};
        result.format_error = validate_lzss_token(
            token, parameters, {result.output_size, declared_frame_size},
            limits, next_size);
        if (result.format_error != LzssFormatError::none) {
            result.error = LzssValidationError::token_error;
            return result;
        }
        result.input_offset += consumed;
        result.output_size = next_size;
        ++result.token_count;
    }
    result.token_index = result.token_count;
    if (result.output_size != declared_frame_size)
        result.error = LzssValidationError::premature_end;
    return result;
}

} // namespace marc::dictionary::internal
