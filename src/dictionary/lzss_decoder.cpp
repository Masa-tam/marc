#include "dictionary/lzss_decoder.hpp"

#include <limits>

namespace marc::dictionary::internal {

LzssDecodeResult decode_lzss_token_stream(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const std::uint64_t declared_frame_size,
    const core::DecoderLimits& limits,
    const std::span<std::byte> output) noexcept {
    const auto validated = validate_lzss_token_stream(
        input, parameters, declared_frame_size, limits);
    if (validated.error != LzssValidationError::none) {
        return {0, validated.token_index, validated.input_offset,
                validated.error, validated.format_error,
                LzssDecodeError::invalid_token_stream};
    }
    if (declared_frame_size
        > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return {0, 0, 0, LzssValidationError::none, LzssFormatError::none,
                LzssDecodeError::output_size_unsupported};
    }
    const auto output_size = static_cast<std::size_t>(declared_frame_size);
    if (output.size() < output_size) {
        return {output_size, 0, 0, LzssValidationError::none,
                LzssFormatError::none, LzssDecodeError::output_too_small};
    }

    std::size_t input_offset{};
    std::size_t produced{};
    std::size_t token_index{};
    while (input_offset < input.size()) {
        LzssToken token{};
        std::size_t consumed{};
        if (parse_lzss_token(input.subspan(input_offset), token, consumed)
            != LzssFormatError::none) {
            return {output_size, token_index, input_offset,
                    LzssValidationError::none, LzssFormatError::none,
                    LzssDecodeError::internal_error};
        }
        if (token.tag == LzssTokenTag::literal) {
            output[produced++] = static_cast<std::byte>(token.literal);
        } else {
            for (std::uint32_t copied = 0; copied < token.length; ++copied) {
                output[produced] = output[produced - token.distance];
                ++produced;
            }
        }
        input_offset += consumed;
        ++token_index;
    }
    if (produced != output_size || token_index != validated.token_count
        || input_offset != validated.input_offset) {
        return {output_size, token_index, input_offset,
                LzssValidationError::none, LzssFormatError::none,
                LzssDecodeError::internal_error};
    }
    return {output_size, token_index, input_offset,
            LzssValidationError::none, LzssFormatError::none,
            LzssDecodeError::none};
}

} // namespace marc::dictionary::internal
