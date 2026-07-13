#include "dictionary/lz78_decoder.hpp"

#include <limits>

namespace marc::dictionary::internal {

Lz78DecodeResult decode_lz78_token_stream(
    const std::span<const std::byte> input,
    const Lz78Parameters& parameters,
    const std::uint64_t declared_frame_size,
    const core::DecoderLimits& limits,
    const std::span<Lz78PhraseEntry> phrase_workspace,
    const std::span<std::byte> output) noexcept {
    const auto validated = validate_lz78_token_stream(
        input, parameters, declared_frame_size, limits, phrase_workspace);
    if (validated.error != Lz78ValidationError::none) {
        return {0, validated.token_index, validated.input_offset,
                validated.error, validated.format_error,
                Lz78DecodeError::invalid_token_stream};
    }
    if (declared_frame_size
        > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return {0, 0, 0, Lz78ValidationError::none, Lz78FormatError::none,
                Lz78DecodeError::output_size_unsupported};
    }
    const auto output_size = static_cast<std::size_t>(declared_frame_size);
    if (output.size() < output_size) {
        return {output_size, 0, 0, Lz78ValidationError::none,
                Lz78FormatError::none, Lz78DecodeError::output_too_small};
    }

    std::size_t produced{};
    for (std::size_t index = 0; index < validated.token_count; ++index) {
        const auto input_offset = index * lz78_token_size;
        const std::span<const std::byte, lz78_token_size> encoded{
            input.data() + input_offset, lz78_token_size};
        Lz78Token token{};
        if (parse_lz78_token(encoded, token) != Lz78FormatError::none) {
            return {output_size, index, input_offset,
                    Lz78ValidationError::none, Lz78FormatError::none,
                    Lz78DecodeError::internal_error};
        }

        std::size_t phrase_length{};
        if (token.phrase_index != 0) {
            if (token.phrase_index > validated.dictionary_entries
                || token.phrase_index > phrase_workspace.size()) {
                return {output_size, index, input_offset,
                        Lz78ValidationError::none, Lz78FormatError::none,
                        Lz78DecodeError::internal_error};
            }
            const auto stored_length =
                phrase_workspace[token.phrase_index - 1].length;
            if (stored_length > static_cast<std::uint64_t>(
                                    std::numeric_limits<std::size_t>::max())) {
                return {output_size, index, input_offset,
                        Lz78ValidationError::none, Lz78FormatError::none,
                        Lz78DecodeError::internal_error};
            }
            phrase_length = static_cast<std::size_t>(stored_length);
        }

        if (produced > output_size || phrase_length > output_size - produced) {
            return {output_size, index, input_offset,
                    Lz78ValidationError::none, Lz78FormatError::none,
                    Lz78DecodeError::internal_error};
        }
        auto write_position = produced + phrase_length;
        auto phrase_index = token.phrase_index;
        while (phrase_index != 0) {
            if (phrase_index > validated.dictionary_entries
                || phrase_index > phrase_workspace.size()) {
                return {output_size, index, input_offset,
                        Lz78ValidationError::none, Lz78FormatError::none,
                        Lz78DecodeError::internal_error};
            }
            const auto& entry = phrase_workspace[phrase_index - 1];
            if (write_position == produced) {
                return {output_size, index, input_offset,
                        Lz78ValidationError::none, Lz78FormatError::none,
                        Lz78DecodeError::internal_error};
            }
            output[--write_position] = static_cast<std::byte>(entry.symbol);
            phrase_index = entry.prefix_index;
        }
        if (write_position != produced) {
            return {output_size, index, input_offset,
                    Lz78ValidationError::none, Lz78FormatError::none,
                    Lz78DecodeError::internal_error};
        }
        produced += phrase_length;
        if (token.tag == Lz78TokenTag::pair) {
            if (produced >= output_size) {
                return {output_size, index, input_offset,
                        Lz78ValidationError::none, Lz78FormatError::none,
                        Lz78DecodeError::internal_error};
            }
            output[produced++] = static_cast<std::byte>(token.symbol);
        }
    }

    if (produced != output_size) {
        return {output_size, validated.token_count, validated.input_offset,
                Lz78ValidationError::none, Lz78FormatError::none,
                Lz78DecodeError::internal_error};
    }
    return {output_size, validated.token_count, validated.input_offset,
            Lz78ValidationError::none, Lz78FormatError::none,
            Lz78DecodeError::none};
}

} // namespace marc::dictionary::internal
