#include "dictionary/lz77_encoder.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <limits>

namespace marc::dictionary::internal {
namespace {

struct Match {
    std::uint32_t distance{};
    std::uint32_t length{};
};

[[nodiscard]] Match find_match(
    const std::span<const std::byte> input, const std::size_t position,
    const Lz77Parameters& parameters) noexcept {
    Match best{};
    const auto maximum_distance = std::min<std::size_t>(
        position, static_cast<std::size_t>(parameters.window_size));
    const auto maximum_length = std::min<std::size_t>(
        input.size() - position,
        static_cast<std::size_t>(parameters.max_match_length));
    for (std::size_t distance = 1; distance <= maximum_distance; ++distance) {
        std::size_t length{};
        while (length < maximum_length
               && input[position + length]
                      == input[position - distance + length]) {
            ++length;
        }
        if (length >= parameters.min_match_length
            && length > best.length) {
            best.distance = static_cast<std::uint32_t>(distance);
            best.length = static_cast<std::uint32_t>(length);
        }
    }
    return best;
}

template <typename Consumer>
[[nodiscard]] Lz77EncodeResult run(
    const std::span<const std::byte> input,
    const Lz77Parameters& parameters, Consumer&& consume) noexcept {
    std::size_t position{};
    std::size_t token_count{};
    while (position < input.size()) {
        const auto match = find_match(input, position, parameters);
        Lz77Token token{};
        std::size_t advance{1};
        if (match.length == 0) {
            token.literal = std::to_integer<std::uint8_t>(input[position]);
        } else if (match.length == input.size() - position) {
            token = {Lz77TokenTag::terminal_match, match.distance,
                     match.length, 0};
            advance = match.length;
        } else {
            token = {Lz77TokenTag::match_then_literal, match.distance,
                     match.length,
                     std::to_integer<std::uint8_t>(
                         input[position + match.length])};
            advance = static_cast<std::size_t>(match.length) + 1;
        }
        if (!consume(token, token_count)) {
            return {input.size(), 0, token_count, Lz77FormatError::none,
                    Lz77EncodeError::internal_error};
        }
        ++token_count;
        position += advance;
    }
    std::size_t output_size{};
    if (!core::checked_multiply(token_count, lz77_token_size, output_size)) {
        return {input.size(), 0, token_count, Lz77FormatError::none,
                Lz77EncodeError::arithmetic_overflow};
    }
    return {input.size(), output_size, token_count, Lz77FormatError::none,
            Lz77EncodeError::none};
}

[[nodiscard]] Lz77EncodeResult preflight(
    const std::span<const std::byte> input,
    const Lz77Parameters& parameters,
    const core::DecoderLimits& limits) noexcept {
    if (core::validate_limits(limits) != core::LimitError::none) {
        return {input.size(), 0, 0, Lz77FormatError::none,
                Lz77EncodeError::input_limit_exceeded};
    }
    const auto parameter_error = validate_lz77_parameters(parameters, limits);
    if (parameter_error != Lz77FormatError::none) {
        return {input.size(), 0, 0, parameter_error,
                Lz77EncodeError::invalid_parameters};
    }
    if (input.size() > limits.max_frame_size
        || input.size() > limits.max_total_output_size) {
        return {input.size(), 0, 0, Lz77FormatError::none,
                Lz77EncodeError::input_limit_exceeded};
    }
    const auto planned = run(input, parameters,
                             [](const Lz77Token&, std::size_t) noexcept {
                                 return true;
                             });
    if (planned.error != Lz77EncodeError::none) return planned;
    if (planned.output_size > limits.max_dictionary_serialized_size
        || planned.output_size > limits.max_compressed_payload_size
        || planned.output_size > limits.max_internal_buffered_bytes) {
        auto limited = planned;
        limited.error = Lz77EncodeError::serialized_limit_exceeded;
        return limited;
    }
    return planned;
}

} // namespace

Lz77EncodeResult plan_lz77_token_stream(
    const std::span<const std::byte> input,
    const Lz77Parameters& parameters,
    const core::DecoderLimits& limits) noexcept {
    return preflight(input, parameters, limits);
}

Lz77EncodeResult encode_lz77_token_stream(
    const std::span<const std::byte> input,
    const Lz77Parameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<std::byte> output) noexcept {
    const auto planned = preflight(input, parameters, limits);
    if (planned.error != Lz77EncodeError::none) return planned;
    if (output.size() < planned.output_size) {
        auto short_output = planned;
        short_output.error = Lz77EncodeError::output_too_small;
        return short_output;
    }
    const auto encoded = run(
        input, parameters,
        [output](const Lz77Token& token, const std::size_t index) noexcept {
            const std::span<std::byte, lz77_token_size> destination{
                output.data() + index * lz77_token_size, lz77_token_size};
            return serialize_lz77_token(token, destination)
                   == Lz77FormatError::none;
        });
    if (encoded.error != Lz77EncodeError::none
        || encoded.output_size != planned.output_size
        || encoded.token_count != planned.token_count) {
        auto failed = planned;
        failed.error = Lz77EncodeError::internal_error;
        return failed;
    }
    return encoded;
}

} // namespace marc::dictionary::internal
