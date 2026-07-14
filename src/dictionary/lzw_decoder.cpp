#include "dictionary/lzw_decoder.hpp"

#include "core/bit_io.hpp"
#include "core/checked_math.hpp"

#include <limits>

namespace marc::dictionary::internal {
namespace {

struct PhraseView {
    std::uint8_t first_byte{};
    std::uint64_t length{};
};

} // namespace

LzwDecodeResult decode_lzw_code_stream(
    const std::span<const std::byte> input,
    const LzwParameters& parameters,
    const std::uint64_t declared_frame_size,
    const core::DecoderLimits& limits,
    const std::span<LzwPhraseEntry> phrase_workspace,
    const std::span<std::byte> output) noexcept {
    const auto validated = validate_lzw_code_stream(
        input, parameters, declared_frame_size, limits, phrase_workspace);
    if (validated.error != LzwValidationError::none) {
        return {0, validated.code_index, validated.input_offset,
                validated.input_bit_offset, validated.error,
                validated.format_error, LzwDecodeError::invalid_code_stream};
    }
    if (declared_frame_size
        > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return {0, 0, 0, 0, LzwValidationError::none,
                LzwFormatError::none,
                LzwDecodeError::output_size_unsupported};
    }
    const auto output_size = static_cast<std::size_t>(declared_frame_size);
    if (output.size() < output_size) {
        return {output_size, 0, 0, 0, LzwValidationError::none,
                LzwFormatError::none, LzwDecodeError::output_too_small};
    }
    if (validated.code_count == 0)
        return {0, 0, validated.input_offset, validated.input_bit_offset,
                LzwValidationError::none, LzwFormatError::none,
                LzwDecodeError::none};

    core::BitReader reader{};
    std::size_t loaded_bytes{};
    std::uint64_t code_bits{};
    std::size_t produced{};
    std::uint32_t width = lzw_minimum_code_width;
    const auto code_limit = lzw_code_limit(parameters);
    std::uint32_t next_code = lzw_first_free_code;

    auto internal_error = [&](const std::size_t code_index,
                              const std::uint64_t bit_offset) noexcept {
        return LzwDecodeResult{
            output_size, code_index,
            static_cast<std::size_t>(bit_offset / 8), bit_offset,
            LzwValidationError::none, LzwFormatError::none,
            LzwDecodeError::internal_error};
    };
    auto read_code = [&](const std::uint32_t bit_width,
                         std::uint32_t& code) noexcept {
        const auto read = reader.read_bits(
            input.subspan(loaded_bytes),
            static_cast<std::uint8_t>(bit_width));
        loaded_bytes += read.bytes_consumed;
        if (read.status != core::BitIoStatus::complete) return false;
        code_bits += bit_width;
        code = static_cast<std::uint32_t>(read.value);
        return true;
    };
    auto view = [&](const std::uint32_t code,
                    PhraseView& phrase) noexcept {
        if (code < lzw_first_free_code) {
            phrase = {static_cast<std::uint8_t>(code), 1};
            return true;
        }
        const auto index = static_cast<std::size_t>(
            code - lzw_first_free_code);
        if (index >= validated.dictionary_entries
            || index >= phrase_workspace.size())
            return false;
        const auto& entry = phrase_workspace[index];
        phrase = {entry.first_byte, entry.length};
        return true;
    };
    auto write_phrase = [&](const std::uint32_t code,
                            const PhraseView phrase) noexcept {
        if (phrase.length
            > static_cast<std::uint64_t>(output_size - produced))
            return false;
        auto write_position = produced + static_cast<std::size_t>(phrase.length);
        auto phrase_code = code;
        while (phrase_code >= lzw_first_free_code) {
            const auto index = static_cast<std::size_t>(
                phrase_code - lzw_first_free_code);
            if (index >= validated.dictionary_entries
                || index >= phrase_workspace.size()
                || write_position == produced)
                return false;
            const auto& entry = phrase_workspace[index];
            if (entry.prefix_code >= phrase_code) return false;
            output[--write_position] =
                static_cast<std::byte>(entry.trailing_byte);
            phrase_code = entry.prefix_code;
        }
        if (write_position == produced) return false;
        output[--write_position] = static_cast<std::byte>(phrase_code);
        if (write_position != produced) return false;
        produced += static_cast<std::size_t>(phrase.length);
        return true;
    };

    std::uint32_t previous_code{};
    if (!read_code(width, previous_code)
        || previous_code >= lzw_first_free_code)
        return internal_error(0, 0);
    PhraseView previous{static_cast<std::uint8_t>(previous_code), 1};
    if (!write_phrase(previous_code, previous))
        return internal_error(0, 0);

    for (std::size_t index = 1; index < validated.code_count; ++index) {
        const auto code_start = code_bits;
        if (width < parameters.maximum_code_width
            && next_code == (UINT32_C(1) << width) - 1)
            ++width;
        std::uint32_t code{};
        if (!read_code(width, code)) return internal_error(index, code_start);

        PhraseView current{};
        if (code > next_code || (code == next_code && next_code >= code_limit)
            || !view(code, current))
            return internal_error(index, code_start);
        if (!write_phrase(code, current))
            return internal_error(index, code_start);

        if (next_code < code_limit) {
            const auto entry_index = static_cast<std::size_t>(
                next_code - lzw_first_free_code);
            if (entry_index >= validated.dictionary_entries
                || entry_index >= phrase_workspace.size())
                return internal_error(index, code_start);
            std::uint64_t expected_length{};
            if (!core::checked_add(previous.length, UINT64_C(1),
                                   expected_length))
                return internal_error(index, code_start);
            const auto& entry = phrase_workspace[entry_index];
            if (entry.prefix_code != previous_code
                || entry.trailing_byte != current.first_byte
                || entry.first_byte != previous.first_byte
                || entry.length != expected_length)
                return internal_error(index, code_start);
            ++next_code;
        }
        previous_code = code;
        previous = current;
    }

    if (produced != output_size
        || code_bits != validated.input_bit_offset
        || loaded_bytes != input.size())
        return internal_error(validated.code_count, code_bits);
    return {output_size, validated.code_count, validated.input_offset,
            validated.input_bit_offset, LzwValidationError::none,
            LzwFormatError::none, LzwDecodeError::none};
}

} // namespace marc::dictionary::internal
