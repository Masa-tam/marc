#include "dictionary/lzd_encoder.hpp"

#include "core/checked_math.hpp"

#include <algorithm>

namespace marc::dictionary::internal {
namespace {

struct PhraseMatch {
    std::uint32_t reference{};
    std::size_t length{1};
};

[[nodiscard]] PhraseMatch find_longest_phrase(
    const std::span<const std::byte> input, const std::size_t position,
    const std::span<const LzdEncoderEntry> entries) noexcept {
    PhraseMatch best{
        std::to_integer<std::uint8_t>(input[position]), 1};
    const auto remaining = input.size() - position;
    for (std::size_t slot = 0; slot < entries.size(); ++slot) {
        const auto& entry = entries[slot];
        if (entry.length <= best.length || entry.length > remaining
            || entry.input_offset > input.size()
            || entry.length > input.size() - entry.input_offset) {
            continue;
        }
        if (std::equal(input.begin() + entry.input_offset,
                       input.begin() + entry.input_offset + entry.length,
                       input.begin() + position)) {
            best.reference = static_cast<std::uint32_t>(
                lzd_first_phrase_reference + slot);
            best.length = entry.length;
        }
    }
    return best;
}

template <typename Consumer>
[[nodiscard]] LzdEncodeResult run(
    const std::span<const std::byte> input,
    const LzdParameters& parameters,
    const std::span<LzdEncoderEntry> dictionary_workspace,
    Consumer&& consume) noexcept {
    std::size_t position{};
    std::size_t token_count{};
    std::uint32_t entry_count{};
    while (position < input.size()) {
        const auto entries = dictionary_workspace.first(entry_count);
        const auto left = find_longest_phrase(input, position, entries);
        const auto remaining = input.size() - position;
        LzdToken token{left.reference, lzd_absent_reference};
        std::size_t advance = left.length;
        if (left.length != remaining) {
            const auto right = find_longest_phrase(
                input, position + left.length, entries);
            token.right_reference = right.reference;
            if (!core::checked_add(left.length, right.length, advance)) {
                return {input.size(), 0, token_count, entry_count,
                        LzdFormatError::none,
                        LzdEncodeError::arithmetic_overflow};
            }
        }
        if (!consume(token, token_count)) {
            return {input.size(), 0, token_count, entry_count,
                    LzdFormatError::none, LzdEncodeError::internal_error};
        }
        if (token.right_reference != lzd_absent_reference
            && entry_count < parameters.maximum_entries) {
            dictionary_workspace[entry_count] = {position, advance};
            ++entry_count;
        }
        position += advance;
        ++token_count;
    }

    std::size_t output_size{};
    if (!core::checked_multiply(token_count, lzd_token_size, output_size)) {
        return {input.size(), 0, token_count, entry_count,
                LzdFormatError::none, LzdEncodeError::arithmetic_overflow};
    }
    return {input.size(), output_size, token_count, entry_count,
            LzdFormatError::none, LzdEncodeError::none};
}

[[nodiscard]] LzdEncodeResult preflight(
    const std::span<const std::byte> input,
    const LzdParameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<LzdEncoderEntry> dictionary_workspace) noexcept {
    if (core::validate_limits(limits) != core::LimitError::none) {
        return {input.size(), 0, 0, 0, LzdFormatError::none,
                LzdEncodeError::input_limit_exceeded};
    }
    const auto parameter_error = validate_lzd_parameters(parameters, limits);
    if (parameter_error != LzdFormatError::none) {
        return {input.size(), 0, 0, 0, parameter_error,
                LzdEncodeError::invalid_parameters};
    }
    if (input.size() > limits.max_frame_size
        || input.size() > limits.max_total_output_size) {
        return {input.size(), 0, 0, 0, LzdFormatError::none,
                LzdEncodeError::input_limit_exceeded};
    }

    const auto required_entries =
        lzd_encoder_workspace_entries(input.size(), parameters);
    std::size_t workspace_bytes{};
    std::size_t aggregate_bytes{};
    if (!core::checked_multiply(required_entries, sizeof(LzdEncoderEntry),
                                workspace_bytes)
        || !core::checked_add(input.size(), workspace_bytes,
                              aggregate_bytes)) {
        return {input.size(), 0, 0, 0, LzdFormatError::none,
                LzdEncodeError::arithmetic_overflow};
    }
    if (aggregate_bytes > limits.max_internal_buffered_bytes) {
        return {input.size(), 0, 0, 0, LzdFormatError::none,
                LzdEncodeError::workspace_limit_exceeded};
    }
    if (dictionary_workspace.size() < required_entries) {
        return {input.size(), 0, 0, 0, LzdFormatError::none,
                LzdEncodeError::workspace_too_small};
    }

    const auto planned = run(
        input, parameters, dictionary_workspace,
        [](const LzdToken&, std::size_t) noexcept { return true; });
    if (planned.error != LzdEncodeError::none) return planned;
    if (planned.output_size > limits.max_dictionary_serialized_size
        || planned.output_size > limits.max_internal_buffered_bytes) {
        auto limited = planned;
        limited.error = LzdEncodeError::serialized_limit_exceeded;
        return limited;
    }
    return planned;
}

} // namespace

std::size_t lzd_encoder_workspace_entries(
    const std::size_t input_size,
    const LzdParameters& parameters) noexcept {
    return std::min(input_size / 2,
                    static_cast<std::size_t>(parameters.maximum_entries));
}

LzdEncodeResult plan_lzd_token_stream(
    const std::span<const std::byte> input,
    const LzdParameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<LzdEncoderEntry> dictionary_workspace) noexcept {
    return preflight(input, parameters, limits, dictionary_workspace);
}

LzdEncodeResult encode_lzd_token_stream(
    const std::span<const std::byte> input,
    const LzdParameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<LzdEncoderEntry> dictionary_workspace,
    const std::span<std::byte> output) noexcept {
    const auto planned = preflight(
        input, parameters, limits, dictionary_workspace);
    if (planned.error != LzdEncodeError::none) return planned;
    if (output.size() < planned.output_size) {
        auto short_output = planned;
        short_output.error = LzdEncodeError::output_too_small;
        return short_output;
    }

    const auto encoded = run(
        input, parameters, dictionary_workspace,
        [output](const LzdToken& token, const std::size_t index) noexcept {
            const std::span<std::byte, lzd_token_size> destination{
                output.data() + index * lzd_token_size, lzd_token_size};
            return serialize_lzd_token(token, destination)
                == LzdFormatError::none;
        });
    if (encoded.error != LzdEncodeError::none
        || encoded.output_size != planned.output_size
        || encoded.token_count != planned.token_count
        || encoded.dictionary_entries != planned.dictionary_entries) {
        auto failed = planned;
        failed.error = LzdEncodeError::internal_error;
        return failed;
    }
    return encoded;
}

} // namespace marc::dictionary::internal
