#include "frame/lzw_profile.hpp"

#include "core/checked_math.hpp"
#include "dictionary/lzw_encoder.hpp"
#include "frame/frame_header.hpp"

#include <algorithm>
#include <limits>

namespace marc::frame {
namespace {

[[nodiscard]] bool to_size(const std::uint64_t value,
                           std::size_t& result) noexcept {
    if (value > std::numeric_limits<std::size_t>::max()) return false;
    result = static_cast<std::size_t>(value);
    return true;
}

[[nodiscard]] std::uint64_t maximum_supported_entries(
    const std::uint64_t local_limit) noexcept {
    std::uint64_t result{};
    for (std::uint32_t width = dictionary::internal::lzw_minimum_code_width;
         width <= dictionary::internal::lzw_maximum_code_width; ++width) {
        const auto capacity =
            (UINT64_C(1) << width)
            - dictionary::internal::lzw_first_free_code;
        if (capacity > local_limit) break;
        result = capacity;
    }
    return result;
}

[[nodiscard]] bool maximum_phrase_entries(
    const std::uint64_t payload_bytes,
    const std::uint64_t capacity,
    std::uint64_t& entries) noexcept {
    std::uint64_t whole_codes{};
    if (!core::checked_multiply(payload_bytes / 9, UINT64_C(8), whole_codes))
        return false;
    const auto partial_codes = (payload_bytes % 9 * 8) / 9;
    std::uint64_t maximum_codes{};
    if (!core::checked_add(whole_codes, partial_codes, maximum_codes))
        return false;
    entries = maximum_codes == 0
        ? 0 : std::min(maximum_codes - 1, capacity);
    return true;
}

[[nodiscard]] bool decoder_payload_fits(
    const std::uint64_t payload_bytes,
    const std::uint64_t maximum_entries,
    const std::uint64_t internal_limit) noexcept {
    std::uint64_t entries{};
    std::uint64_t dictionary_bytes{};
    std::uint64_t buffered_bytes{frame_header_size};
    return maximum_phrase_entries(payload_bytes, maximum_entries, entries)
        && core::checked_multiply(
               entries,
               static_cast<std::uint64_t>(
                   sizeof(dictionary::internal::LzwPhraseEntry)),
               dictionary_bytes)
        && core::checked_add(buffered_bytes, payload_bytes, buffered_bytes)
        && core::checked_add(buffered_bytes, UINT64_C(1), buffered_bytes)
        && core::checked_add(buffered_bytes, dictionary_bytes, buffered_bytes)
        && buffered_bytes <= internal_limit;
}

[[nodiscard]] std::uint64_t maximum_decoder_payload(
    const core::DecoderLimits& limits,
    const std::uint64_t maximum_entries) noexcept {
    const auto fixed_bytes =
        static_cast<std::uint64_t>(frame_header_size) + 1;
    if (limits.max_internal_buffered_bytes < fixed_bytes) return 0;
    const auto upper = std::min(
        {limits.max_dictionary_serialized_size,
         limits.max_compressed_payload_size,
         limits.max_internal_buffered_bytes - fixed_bytes});
    std::uint64_t low{};
    std::uint64_t high{upper};
    while (low < high) {
        const auto distance = high - low;
        const auto middle = low + distance / 2 + distance % 2;
        if (decoder_payload_fits(
                middle, maximum_entries,
                limits.max_internal_buffered_bytes)) {
            low = middle;
        } else {
            high = middle - 1;
        }
    }
    return low;
}

} // namespace

LzwProfileError make_lzw_profile(
    const LzwProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzwEncoderWorkspaceRequirements& workspace) noexcept {
    stream = {};
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none
        || config.frame_size == 0) {
        return LzwProfileError::invalid_configuration;
    }
    const auto parameter_error = dictionary::internal::validate_lzw_parameters(
        config.parameters, limits);
    if (parameter_error != dictionary::internal::LzwFormatError::none) {
        return parameter_error
                   == dictionary::internal::LzwFormatError::limit_exceeded
            ? LzwProfileError::limit_exceeded
            : LzwProfileError::invalid_configuration;
    }
    if (config.original_size > limits.max_total_output_size
        || config.frame_size > limits.max_frame_size) {
        return LzwProfileError::limit_exceeded;
    }
    stream.dictionary_algorithm = DictionaryAlgorithm::lzw;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::none;
    stream.entropy_variant = 0;
    stream.frame_size = config.frame_size;
    stream.dictionary_parameters_size =
        dictionary::internal::lzw_parameter_size;
    stream.original_size = config.original_size;
    if (validate_stream_header(stream, limits) != StreamHeaderError::none) {
        return LzwProfileError::unsupported;
    }

    const auto largest_frame = std::min<std::uint64_t>(
        config.original_size, config.frame_size);
    const auto dictionary_entries = largest_frame == 0
        ? UINT64_C(0)
        : std::min(
              largest_frame - 1,
              static_cast<std::uint64_t>(
                  dictionary::internal::lzw_code_limit(config.parameters)
                  - dictionary::internal::lzw_first_free_code));
    std::uint64_t payload_bits{};
    std::uint64_t rounded_bits{};
    std::uint64_t payload_bytes{};
    std::uint64_t encoded_bytes{frame_header_size};
    std::uint64_t dictionary_bytes{};
    std::uint64_t buffered_bytes{};
    if (!core::checked_multiply(
            largest_frame,
            static_cast<std::uint64_t>(config.parameters.maximum_code_width),
            payload_bits)
        || !core::checked_add(payload_bits, UINT64_C(7), rounded_bits)) {
        return LzwProfileError::arithmetic_overflow;
    }
    payload_bytes = rounded_bits / 8;
    if (!core::checked_add(encoded_bytes, payload_bytes, encoded_bytes)
        || !core::checked_multiply(
            dictionary_entries,
            static_cast<std::uint64_t>(
                sizeof(dictionary::internal::LzwEncoderEntry)),
            dictionary_bytes)
        || !core::checked_add(largest_frame, encoded_bytes, buffered_bytes)
        || !core::checked_add(
            buffered_bytes, dictionary_bytes, buffered_bytes)) {
        return LzwProfileError::arithmetic_overflow;
    }
    if (payload_bytes > limits.max_dictionary_serialized_size
        || payload_bytes > limits.max_compressed_payload_size
        || buffered_bytes > limits.max_internal_buffered_bytes) {
        return LzwProfileError::limit_exceeded;
    }
    if (!to_size(largest_frame, workspace.frame_input_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(dictionary_entries, workspace.dictionary_entries)) {
        workspace = {};
        return LzwProfileError::arithmetic_overflow;
    }
    return LzwProfileError::none;
}

LzwProfileError calculate_lzw_decoder_workspace(
    const core::DecoderLimits& limits,
    LzwDecoderWorkspaceRequirements& workspace) noexcept {
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none) {
        return LzwProfileError::invalid_configuration;
    }
    const auto maximum_entries =
        maximum_supported_entries(limits.max_dictionary_entries);
    if (maximum_entries == 0) return LzwProfileError::limit_exceeded;
    const auto payload_bytes = maximum_decoder_payload(limits, maximum_entries);
    std::uint64_t dictionary_entries{};
    std::uint64_t encoded_bytes{};
    if (!maximum_phrase_entries(
            payload_bytes, maximum_entries, dictionary_entries)
        || !core::checked_add(static_cast<std::uint64_t>(frame_header_size),
                              payload_bytes, encoded_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(limits.max_frame_size, workspace.frame_decoded_bytes)
        || !to_size(dictionary_entries, workspace.dictionary_entries)) {
        workspace = {};
        return LzwProfileError::arithmetic_overflow;
    }
    return LzwProfileError::none;
}

core::ErrorCode lzw_profile_error_code(
    const LzwProfileError error) noexcept {
    switch (error) {
    case LzwProfileError::none: return core::ErrorCode::none;
    case LzwProfileError::invalid_configuration:
        return core::ErrorCode::invalid_argument;
    case LzwProfileError::unsupported: return core::ErrorCode::unsupported;
    case LzwProfileError::limit_exceeded:
    case LzwProfileError::arithmetic_overflow:
        return core::ErrorCode::limit_exceeded;
    }
    return core::ErrorCode::internal_error;
}

} // namespace marc::frame
