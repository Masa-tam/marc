#include "dictionary/lz77_validator.hpp"

namespace marc::dictionary::internal {

Lz77ValidationResult validate_lz77_token_stream(
    const std::span<const std::byte> input,
    const Lz77Parameters& parameters,
    const std::uint64_t declared_frame_size,
    const core::DecoderLimits& limits) noexcept {
    Lz77ValidationResult result{};
    result.output_size = 0;
    const auto parameter_error = validate_lz77_parameters(parameters, limits);
    if (parameter_error != Lz77FormatError::none) {
        result.format_error = parameter_error;
        result.error = Lz77ValidationError::invalid_parameters;
        return result;
    }
    if (declared_frame_size > limits.max_frame_size
        || input.size() > limits.max_internal_buffered_bytes
        || input.size() > limits.max_compressed_payload_size) {
        result.error = Lz77ValidationError::limit_exceeded;
        return result;
    }
    if (input.size() % lz77_token_size != 0) {
        result.token_count = input.size() / lz77_token_size;
        result.token_index = result.token_count;
        result.error = Lz77ValidationError::truncated_token;
        return result;
    }
    result.token_count = input.size() / lz77_token_size;
    for (std::size_t index = 0; index < result.token_count; ++index) {
        result.token_index = index;
        if (result.output_size == declared_frame_size) {
            result.error = Lz77ValidationError::trailing_tokens;
            return result;
        }
        const std::span<const std::byte, lz77_token_size> encoded{
            input.data() + index * lz77_token_size, lz77_token_size};
        Lz77Token token{};
        result.format_error = parse_lz77_token(encoded, token);
        if (result.format_error != Lz77FormatError::none) {
            result.error = Lz77ValidationError::token_error;
            return result;
        }
        std::uint64_t next_size{};
        result.format_error = validate_lz77_token(
            token, parameters, {result.output_size, declared_frame_size},
            limits, next_size);
        if (result.format_error != Lz77FormatError::none) {
            result.error = Lz77ValidationError::token_error;
            return result;
        }
        result.output_size = next_size;
        if (token.tag == Lz77TokenTag::terminal_match
            && index + 1 != result.token_count) {
            result.error = Lz77ValidationError::trailing_tokens;
            return result;
        }
    }
    result.token_index = result.token_count;
    if (result.output_size != declared_frame_size) {
        result.error = Lz77ValidationError::premature_end;
        return result;
    }
    return result;
}

} // namespace marc::dictionary::internal
