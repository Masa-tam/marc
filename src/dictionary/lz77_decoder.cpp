#include "dictionary/lz77_decoder.hpp"

#include <limits>

namespace marc::dictionary::internal {

Lz77DecodeResult decode_lz77_token_stream(
    const std::span<const std::byte> input,
    const Lz77Parameters& parameters,
    const std::uint64_t declared_frame_size,
    const core::DecoderLimits& limits,
    const std::span<std::byte> output) noexcept {
    const auto validated = validate_lz77_token_stream(
        input, parameters, declared_frame_size, limits);
    if (validated.error != Lz77ValidationError::none) {
        return {0, validated.token_index, validated.error,
                validated.format_error,
                Lz77DecodeError::invalid_token_stream};
    }
    if (declared_frame_size
        > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return {0, 0, Lz77ValidationError::none, Lz77FormatError::none,
                Lz77DecodeError::output_size_unsupported};
    }
    const auto output_size = static_cast<std::size_t>(declared_frame_size);
    if (output.size() < output_size) {
        return {output_size, 0, Lz77ValidationError::none,
                Lz77FormatError::none, Lz77DecodeError::output_too_small};
    }

    std::size_t produced{};
    for (std::size_t index = 0; index < validated.token_count; ++index) {
        const std::span<const std::byte, lz77_token_size> encoded{
            input.data() + index * lz77_token_size, lz77_token_size};
        Lz77Token token{};
        if (parse_lz77_token(encoded, token) != Lz77FormatError::none) {
            return {output_size, index, Lz77ValidationError::none,
                    Lz77FormatError::none, Lz77DecodeError::internal_error};
        }
        if (token.tag == Lz77TokenTag::literal) {
            output[produced++] = static_cast<std::byte>(token.literal);
            continue;
        }
        for (std::uint32_t copied = 0; copied < token.length; ++copied) {
            output[produced] = output[produced - token.distance];
            ++produced;
        }
        if (token.tag == Lz77TokenTag::match_then_literal) {
            output[produced++] = static_cast<std::byte>(token.literal);
        }
    }
    if (produced != output_size) {
        return {output_size, validated.token_count,
                Lz77ValidationError::none, Lz77FormatError::none,
                Lz77DecodeError::internal_error};
    }
    return {output_size, validated.token_count, Lz77ValidationError::none,
            Lz77FormatError::none, Lz77DecodeError::none};
}

} // namespace marc::dictionary::internal
