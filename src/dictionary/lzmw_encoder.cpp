#include "dictionary/lzmw_encoder.hpp"

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
    const std::span<const LzmwEncoderEntry> entries) noexcept {
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
                lzmw_first_phrase_reference + slot);
            best.length = entry.length;
        }
    }
    return best;
}

template <typename Consumer>
[[nodiscard]] LzmwEncodeResult run(
    const std::span<const std::byte> input,
    const LzmwParameters& parameters,
    const std::span<LzmwEncoderEntry> dictionary_workspace,
    Consumer&& consume) noexcept {
    std::size_t position{};
    std::size_t previous_offset{};
    std::size_t previous_length{};
    std::size_t token_count{};
    std::uint32_t entry_count{};
    while (position < input.size()) {
        const auto match = find_longest_phrase(
            input, position, dictionary_workspace.first(entry_count));
        if (!consume(match.reference, token_count)) {
            return {input.size(), 0, token_count, entry_count,
                    LzmwFormatError::none, LzmwEncodeError::internal_error};
        }
        if (token_count != 0 && entry_count < parameters.maximum_entries) {
            std::size_t combined_length{};
            if (!core::checked_add(previous_length, match.length,
                                   combined_length)) {
                return {input.size(), 0, token_count, entry_count,
                        LzmwFormatError::none,
                        LzmwEncodeError::arithmetic_overflow};
            }
            dictionary_workspace[entry_count] = {
                previous_offset, combined_length};
            ++entry_count;
        }
        previous_offset = position;
        previous_length = match.length;
        position += match.length;
        ++token_count;
    }

    std::size_t output_size{};
    if (!core::checked_multiply(token_count, lzmw_token_size, output_size)) {
        return {input.size(), 0, token_count, entry_count,
                LzmwFormatError::none,
                LzmwEncodeError::arithmetic_overflow};
    }
    return {input.size(), output_size, token_count, entry_count,
            LzmwFormatError::none, LzmwEncodeError::none};
}

[[nodiscard]] LzmwEncodeResult preflight(
    const std::span<const std::byte> input,
    const LzmwParameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<LzmwEncoderEntry> dictionary_workspace) noexcept {
    if (core::validate_limits(limits) != core::LimitError::none) {
        return {input.size(), 0, 0, 0, LzmwFormatError::none,
                LzmwEncodeError::input_limit_exceeded};
    }
    const auto parameter_error = validate_lzmw_parameters(parameters, limits);
    if (parameter_error != LzmwFormatError::none) {
        return {input.size(), 0, 0, 0, parameter_error,
                LzmwEncodeError::invalid_parameters};
    }
    if (input.size() > limits.max_frame_size
        || input.size() > limits.max_total_output_size) {
        return {input.size(), 0, 0, 0, LzmwFormatError::none,
                LzmwEncodeError::input_limit_exceeded};
    }

    const auto required_entries =
        lzmw_encoder_workspace_entries(input.size(), parameters);
    std::size_t workspace_bytes{};
    std::size_t aggregate_bytes{};
    if (!core::checked_multiply(required_entries, sizeof(LzmwEncoderEntry),
                                workspace_bytes)
        || !core::checked_add(input.size(), workspace_bytes,
                              aggregate_bytes)) {
        return {input.size(), 0, 0, 0, LzmwFormatError::none,
                LzmwEncodeError::arithmetic_overflow};
    }
    if (aggregate_bytes > limits.max_internal_buffered_bytes) {
        return {input.size(), 0, 0, 0, LzmwFormatError::none,
                LzmwEncodeError::workspace_limit_exceeded};
    }
    if (dictionary_workspace.size() < required_entries) {
        return {input.size(), 0, 0, 0, LzmwFormatError::none,
                LzmwEncodeError::workspace_too_small};
    }

    const auto planned = run(
        input, parameters, dictionary_workspace,
        [](std::uint32_t, std::size_t) noexcept { return true; });
    if (planned.error != LzmwEncodeError::none) return planned;
    if (planned.output_size > limits.max_dictionary_serialized_size
        || planned.output_size > limits.max_internal_buffered_bytes) {
        auto limited = planned;
        limited.error = LzmwEncodeError::serialized_limit_exceeded;
        return limited;
    }
    return planned;
}

} // namespace

std::size_t lzmw_encoder_workspace_entries(
    const std::size_t input_size,
    const LzmwParameters& parameters) noexcept {
    const auto maximum_from_input = input_size == 0 ? 0 : input_size - 1;
    return std::min(maximum_from_input,
                    static_cast<std::size_t>(parameters.maximum_entries));
}

LzmwEncodeResult plan_lzmw_token_stream(
    const std::span<const std::byte> input,
    const LzmwParameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<LzmwEncoderEntry> dictionary_workspace) noexcept {
    return preflight(input, parameters, limits, dictionary_workspace);
}

LzmwEncodeResult encode_lzmw_token_stream(
    const std::span<const std::byte> input,
    const LzmwParameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<LzmwEncoderEntry> dictionary_workspace,
    const std::span<std::byte> output) noexcept {
    const auto planned = preflight(
        input, parameters, limits, dictionary_workspace);
    if (planned.error != LzmwEncodeError::none) return planned;
    if (output.size() < planned.output_size) {
        auto short_output = planned;
        short_output.error = LzmwEncodeError::output_too_small;
        return short_output;
    }

    const auto encoded = run(
        input, parameters, dictionary_workspace,
        [output](const std::uint32_t reference,
                 const std::size_t index) noexcept {
            const std::span<std::byte, lzmw_token_size> destination{
                output.data() + index * lzmw_token_size, lzmw_token_size};
            return serialize_lzmw_token(reference, destination)
                == LzmwFormatError::none;
        });
    if (encoded.error != LzmwEncodeError::none
        || encoded.output_size != planned.output_size
        || encoded.token_count != planned.token_count
        || encoded.dictionary_entries != planned.dictionary_entries) {
        auto failed = planned;
        failed.error = LzmwEncodeError::internal_error;
        return failed;
    }
    return encoded;
}

} // namespace marc::dictionary::internal
