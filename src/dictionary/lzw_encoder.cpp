#include "dictionary/lzw_encoder.hpp"

#include "core/bit_io.hpp"
#include "core/checked_math.hpp"

#include <algorithm>

namespace marc::dictionary::internal {
namespace {

struct PhraseMatch {
    std::uint32_t code{};
    std::size_t length{1};
};

[[nodiscard]] PhraseMatch find_longest_phrase(
    const std::span<const std::byte> input, const std::size_t position,
    const std::span<const LzwEncoderEntry> entries) noexcept {
    PhraseMatch best{
        std::to_integer<std::uint8_t>(input[position]), 1};
    const auto remaining = input.size() - position;
    for (std::size_t slot = 0; slot < entries.size(); ++slot) {
        const auto& entry = entries[slot];
        if (entry.length <= best.length || entry.length > remaining
            || entry.input_offset > input.size()
            || entry.length > input.size() - entry.input_offset)
            continue;
        if (std::equal(input.begin() + entry.input_offset,
                       input.begin() + entry.input_offset + entry.length,
                       input.begin() + position)) {
            best.code = static_cast<std::uint32_t>(
                lzw_first_free_code + slot);
            best.length = entry.length;
        }
    }
    return best;
}

template <typename Consumer>
[[nodiscard]] LzwEncodeResult run(
    const std::span<const std::byte> input,
    const LzwParameters& parameters,
    const std::span<LzwEncoderEntry> dictionary_workspace,
    Consumer&& consume) noexcept {
    std::size_t position{};
    std::size_t code_count{};
    std::size_t bit_count{};
    std::uint32_t entry_count{};
    std::uint32_t width = lzw_minimum_code_width;
    const auto capacity =
        lzw_code_limit(parameters) - lzw_first_free_code;

    while (position < input.size()) {
        const auto match = find_longest_phrase(
            input, position, dictionary_workspace.first(entry_count));
        std::size_t next_bit_count{};
        if (!core::checked_add(bit_count, static_cast<std::size_t>(width),
                               next_bit_count)) {
            return {input.size(), 0, code_count, bit_count, entry_count,
                    LzwFormatError::none,
                    LzwEncodeError::arithmetic_overflow};
        }
        if (!consume(match.code, width)) {
            return {input.size(), 0, code_count, bit_count, entry_count,
                    LzwFormatError::none, LzwEncodeError::internal_error};
        }
        bit_count = next_bit_count;
        ++code_count;

        const auto remaining = input.size() - position;
        if (match.length < remaining && entry_count < capacity) {
            std::size_t inserted_length{};
            if (!core::checked_add(match.length, std::size_t{1},
                                   inserted_length)) {
                return {input.size(), 0, code_count, bit_count, entry_count,
                        LzwFormatError::none,
                        LzwEncodeError::arithmetic_overflow};
            }
            dictionary_workspace[entry_count] = {position, inserted_length};
            ++entry_count;
            const auto next_code = lzw_first_free_code + entry_count;
            if (width < parameters.maximum_code_width
                && next_code == (UINT32_C(1) << width))
                ++width;
        }
        position += match.length;
    }

    std::size_t rounded_bits{};
    if (!core::checked_add(bit_count, std::size_t{7}, rounded_bits)) {
        return {input.size(), 0, code_count, bit_count, entry_count,
                LzwFormatError::none,
                LzwEncodeError::arithmetic_overflow};
    }
    return {input.size(), rounded_bits / 8, code_count, bit_count,
            entry_count, LzwFormatError::none, LzwEncodeError::none};
}

[[nodiscard]] LzwEncodeResult preflight(
    const std::span<const std::byte> input,
    const LzwParameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<LzwEncoderEntry> dictionary_workspace) noexcept {
    if (core::validate_limits(limits) != core::LimitError::none) {
        return {input.size(), 0, 0, 0, 0, LzwFormatError::none,
                LzwEncodeError::input_limit_exceeded};
    }
    const auto parameter_error = validate_lzw_parameters(parameters, limits);
    if (parameter_error != LzwFormatError::none) {
        return {input.size(), 0, 0, 0, 0, parameter_error,
                LzwEncodeError::invalid_parameters};
    }
    if (input.size() > limits.max_frame_size
        || input.size() > limits.max_total_output_size) {
        return {input.size(), 0, 0, 0, 0, LzwFormatError::none,
                LzwEncodeError::input_limit_exceeded};
    }

    const auto required_entries =
        lzw_encoder_workspace_entries(input.size(), parameters);
    std::size_t workspace_bytes{};
    if (!core::checked_multiply(required_entries, sizeof(LzwEncoderEntry),
                                workspace_bytes)) {
        return {input.size(), 0, 0, 0, 0, LzwFormatError::none,
                LzwEncodeError::arithmetic_overflow};
    }
    if (workspace_bytes > limits.max_internal_buffered_bytes) {
        return {input.size(), 0, 0, 0, 0, LzwFormatError::none,
                LzwEncodeError::workspace_limit_exceeded};
    }
    if (dictionary_workspace.size() < required_entries) {
        return {input.size(), 0, 0, 0, 0, LzwFormatError::none,
                LzwEncodeError::workspace_too_small};
    }

    const auto planned = run(
        input, parameters, dictionary_workspace,
        [](std::uint32_t, std::uint32_t) noexcept { return true; });
    if (planned.error != LzwEncodeError::none) return planned;
    if (planned.output_size > limits.max_dictionary_serialized_size
        || planned.output_size > limits.max_internal_buffered_bytes) {
        auto limited = planned;
        limited.error = LzwEncodeError::serialized_limit_exceeded;
        return limited;
    }
    return planned;
}

} // namespace

std::size_t lzw_encoder_workspace_entries(
    const std::size_t input_size,
    const LzwParameters& parameters) noexcept {
    if (input_size == 0
        || parameters.maximum_code_width < lzw_minimum_code_width
        || parameters.maximum_code_width > lzw_maximum_code_width)
        return 0;
    const auto capacity = static_cast<std::size_t>(
        lzw_code_limit(parameters) - lzw_first_free_code);
    return std::min(input_size - 1, capacity);
}

LzwEncodeResult plan_lzw_code_stream(
    const std::span<const std::byte> input,
    const LzwParameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<LzwEncoderEntry> dictionary_workspace) noexcept {
    return preflight(input, parameters, limits, dictionary_workspace);
}

LzwEncodeResult encode_lzw_code_stream(
    const std::span<const std::byte> input,
    const LzwParameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<LzwEncoderEntry> dictionary_workspace,
    const std::span<std::byte> output) noexcept {
    const auto planned = preflight(
        input, parameters, limits, dictionary_workspace);
    if (planned.error != LzwEncodeError::none) return planned;
    if (output.size() < planned.output_size) {
        auto short_output = planned;
        short_output.error = LzwEncodeError::output_too_small;
        return short_output;
    }

    core::BitWriter writer{};
    std::size_t bytes_written{};
    const auto encoded = run(
        input, parameters, dictionary_workspace,
        [&](const std::uint32_t code, const std::uint32_t width) noexcept {
            const auto written = writer.write_bits(
                code, static_cast<std::uint8_t>(width),
                output.subspan(bytes_written));
            bytes_written += written.bytes_produced;
            return written.status == core::BitIoStatus::complete
                && written.bits_consumed == width;
        });
    if (encoded.error != LzwEncodeError::none
        || encoded.output_size != planned.output_size
        || encoded.code_count != planned.code_count
        || encoded.bit_count != planned.bit_count
        || encoded.dictionary_entries != planned.dictionary_entries) {
        auto failed = planned;
        failed.error = LzwEncodeError::internal_error;
        return failed;
    }
    const auto finished = writer.finish(output.subspan(bytes_written));
    bytes_written += finished.bytes_produced;
    if (finished.status != core::BitIoStatus::finished
        || bytes_written != planned.output_size) {
        auto failed = planned;
        failed.error = LzwEncodeError::internal_error;
        return failed;
    }
    return encoded;
}

} // namespace marc::dictionary::internal
