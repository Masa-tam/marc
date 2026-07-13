#include "dictionary/lz78_encoder.hpp"

#include "core/checked_math.hpp"

#include <algorithm>

namespace marc::dictionary::internal {
namespace {

struct PhraseMatch {
    std::uint32_t index{};
    std::size_t length{};
};

[[nodiscard]] PhraseMatch find_longest_phrase(
    const std::span<const std::byte> input, const std::size_t position,
    const std::span<const Lz78EncoderEntry> entries) noexcept {
    PhraseMatch best{};
    const auto remaining = input.size() - position;
    for (std::size_t slot = 0; slot < entries.size(); ++slot) {
        const auto& entry = entries[slot];
        if (entry.length <= best.length || entry.length > remaining)
            continue;
        const auto length = static_cast<std::size_t>(entry.length);
        if (entry.input_offset > input.size()
            || length > input.size() - entry.input_offset)
            continue;
        if (std::equal(input.begin() + entry.input_offset,
                       input.begin() + entry.input_offset + length,
                       input.begin() + position)) {
            best.index = static_cast<std::uint32_t>(slot + 1);
            best.length = length;
        }
    }
    return best;
}

template <typename Consumer>
[[nodiscard]] Lz78EncodeResult run(
    const std::span<const std::byte> input,
    const Lz78Parameters& parameters,
    const std::span<Lz78EncoderEntry> dictionary_workspace,
    Consumer&& consume) noexcept {
    std::size_t position{};
    std::size_t token_count{};
    std::uint32_t entry_count{};
    while (position < input.size()) {
        const auto entries = dictionary_workspace.first(entry_count);
        const auto match = find_longest_phrase(input, position, entries);
        const auto remaining = input.size() - position;
        Lz78Token token{};
        std::size_t advance{};
        if (match.length == remaining) {
            token = {Lz78TokenTag::final_index, 0, match.index};
            advance = match.length;
        } else {
            token = {Lz78TokenTag::pair,
                     std::to_integer<std::uint8_t>(
                         input[position + match.length]),
                     match.index};
            if (!core::checked_add(match.length, std::size_t{1}, advance)) {
                return {input.size(), 0, token_count, entry_count,
                        Lz78FormatError::none,
                        Lz78EncodeError::arithmetic_overflow};
            }
        }
        if (!consume(token, token_count)) {
            return {input.size(), 0, token_count, entry_count,
                    Lz78FormatError::none, Lz78EncodeError::internal_error};
        }
        if (token.tag == Lz78TokenTag::pair
            && entry_count < parameters.maximum_entries) {
            dictionary_workspace[entry_count] = {position, advance};
            ++entry_count;
        }
        position += advance;
        ++token_count;
    }

    std::size_t output_size{};
    if (!core::checked_multiply(token_count, lz78_token_size, output_size)) {
        return {input.size(), 0, token_count, entry_count,
                Lz78FormatError::none, Lz78EncodeError::arithmetic_overflow};
    }
    return {input.size(), output_size, token_count, entry_count,
            Lz78FormatError::none, Lz78EncodeError::none};
}

[[nodiscard]] Lz78EncodeResult preflight(
    const std::span<const std::byte> input,
    const Lz78Parameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<Lz78EncoderEntry> dictionary_workspace) noexcept {
    if (core::validate_limits(limits) != core::LimitError::none) {
        return {input.size(), 0, 0, 0, Lz78FormatError::none,
                Lz78EncodeError::input_limit_exceeded};
    }
    const auto parameter_error = validate_lz78_parameters(parameters, limits);
    if (parameter_error != Lz78FormatError::none) {
        return {input.size(), 0, 0, 0, parameter_error,
                Lz78EncodeError::invalid_parameters};
    }
    if (input.size() > limits.max_frame_size
        || input.size() > limits.max_total_output_size) {
        return {input.size(), 0, 0, 0, Lz78FormatError::none,
                Lz78EncodeError::input_limit_exceeded};
    }

    const auto required_entries =
        lz78_encoder_workspace_entries(input.size(), parameters);
    std::size_t workspace_bytes{};
    if (!core::checked_multiply(required_entries, sizeof(Lz78EncoderEntry),
                                workspace_bytes)) {
        return {input.size(), 0, 0, 0, Lz78FormatError::none,
                Lz78EncodeError::arithmetic_overflow};
    }
    if (workspace_bytes > limits.max_internal_buffered_bytes) {
        return {input.size(), 0, 0, 0, Lz78FormatError::none,
                Lz78EncodeError::workspace_limit_exceeded};
    }
    if (dictionary_workspace.size() < required_entries) {
        return {input.size(), 0, 0, 0, Lz78FormatError::none,
                Lz78EncodeError::workspace_too_small};
    }

    const auto planned = run(
        input, parameters, dictionary_workspace,
        [](const Lz78Token&, std::size_t) noexcept { return true; });
    if (planned.error != Lz78EncodeError::none) return planned;
    if (planned.output_size > limits.max_dictionary_serialized_size
        || planned.output_size > limits.max_internal_buffered_bytes) {
        auto limited = planned;
        limited.error = Lz78EncodeError::serialized_limit_exceeded;
        return limited;
    }
    return planned;
}

} // namespace

std::size_t lz78_encoder_workspace_entries(
    const std::size_t input_size,
    const Lz78Parameters& parameters) noexcept {
    return std::min(input_size,
                    static_cast<std::size_t>(parameters.maximum_entries));
}

Lz78EncodeResult plan_lz78_token_stream(
    const std::span<const std::byte> input,
    const Lz78Parameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<Lz78EncoderEntry> dictionary_workspace) noexcept {
    return preflight(input, parameters, limits, dictionary_workspace);
}

Lz78EncodeResult encode_lz78_token_stream(
    const std::span<const std::byte> input,
    const Lz78Parameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<Lz78EncoderEntry> dictionary_workspace,
    const std::span<std::byte> output) noexcept {
    const auto planned = preflight(
        input, parameters, limits, dictionary_workspace);
    if (planned.error != Lz78EncodeError::none) return planned;
    if (output.size() < planned.output_size) {
        auto short_output = planned;
        short_output.error = Lz78EncodeError::output_too_small;
        return short_output;
    }

    const auto encoded = run(
        input, parameters, dictionary_workspace,
        [output](const Lz78Token& token, const std::size_t index) noexcept {
            const std::span<std::byte, lz78_token_size> destination{
                output.data() + index * lz78_token_size, lz78_token_size};
            return serialize_lz78_token(token, destination)
                == Lz78FormatError::none;
        });
    if (encoded.error != Lz78EncodeError::none
        || encoded.output_size != planned.output_size
        || encoded.token_count != planned.token_count
        || encoded.dictionary_entries != planned.dictionary_entries) {
        auto failed = planned;
        failed.error = Lz78EncodeError::internal_error;
        return failed;
    }
    return encoded;
}

} // namespace marc::dictionary::internal
