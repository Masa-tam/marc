#include "frame/lzw_dynamic_range_profile.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace marc::frame {
namespace {

inline constexpr std::uint64_t profile_max_frame_size = UINT64_C(1) << 20;
inline constexpr std::uint64_t payload_bytes_per_packed_byte = 2;
inline constexpr std::uint64_t range_termination_bytes = 5;

[[nodiscard]] bool to_size(const std::uint64_t value,
                           std::size_t& result) noexcept {
    if (value > std::numeric_limits<std::size_t>::max()) {
        return false;
    }
    result = static_cast<std::size_t>(value);
    return true;
}

[[nodiscard]] bool aligned(const void* const pointer,
                           const std::size_t alignment) noexcept {
    return alignment != 0
        && reinterpret_cast<std::uintptr_t>(pointer) % alignment == 0;
}

[[nodiscard]] std::uint64_t
maximum_supported_entries(const std::uint64_t local_limit) noexcept {
    std::uint64_t result{};
    for (std::uint32_t width = dictionary::internal::lzw_minimum_code_width;
         width <= dictionary::internal::lzw_maximum_code_width; ++width) {
        const auto capacity =
            (UINT64_C(1) << width) - dictionary::internal::lzw_first_free_code;
        if (capacity > local_limit) {
            break;
        }
        result = capacity;
    }
    return result;
}

[[nodiscard]] bool maximum_phrase_entries(const std::uint64_t packed_bytes,
                                           const std::uint64_t capacity,
                                           std::uint64_t& entries) noexcept {
    std::uint64_t whole_codes{};
    if (!core::checked_multiply(packed_bytes / 9, UINT64_C(8), whole_codes)) {
        return false;
    }
    const auto partial_codes = (packed_bytes % 9 * 8) / 9;
    std::uint64_t maximum_codes{};
    if (!core::checked_add(whole_codes, partial_codes, maximum_codes)) {
        return false;
    }
    entries = maximum_codes == 0 ? 0 : std::min(maximum_codes - 1, capacity);
    return true;
}

template <typename Record>
[[nodiscard]] LzwDynamicRangeWorkspaceError partition_records(
    const std::size_t count,
    const std::size_t required_bytes,
    const std::size_t required_alignment,
    const std::span<std::byte> storage,
    std::span<Record>& records) noexcept {
    records = {};
    std::size_t expected_bytes{};
    if (!core::checked_multiply(count, sizeof(Record), expected_bytes)) {
        return LzwDynamicRangeWorkspaceError::arithmetic_overflow;
    }
    if (expected_bytes == 0) {
        return required_bytes == 0 && required_alignment == 1
            ? LzwDynamicRangeWorkspaceError::none
            : LzwDynamicRangeWorkspaceError::invalid_requirements;
    }
    if (expected_bytes != required_bytes
        || required_alignment != alignof(Record)) {
        return LzwDynamicRangeWorkspaceError::invalid_requirements;
    }
    if (storage.size() < expected_bytes) {
        return LzwDynamicRangeWorkspaceError::too_small;
    }
    if (!aligned(storage.data(), required_alignment)) {
        return LzwDynamicRangeWorkspaceError::misaligned;
    }
    records = {reinterpret_cast<Record*>(storage.data()), count};
    return LzwDynamicRangeWorkspaceError::none;
}

} // namespace

LzwDynamicRangeProfileError make_lzw_dynamic_range_profile(
    const LzwDynamicRangeProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzwDynamicRangeEncoderWorkspaceRequirements& workspace) noexcept {
    stream = {};
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none
        || config.frame_size == 0) {
        return LzwDynamicRangeProfileError::invalid_configuration;
    }
    const auto parameter_error =
        dictionary::internal::validate_lzw_parameters(
            config.parameters, limits);
    if (parameter_error != dictionary::internal::LzwFormatError::none) {
        return parameter_error
                   == dictionary::internal::LzwFormatError::limit_exceeded
            ? LzwDynamicRangeProfileError::limit_exceeded
            : LzwDynamicRangeProfileError::invalid_configuration;
    }
    if (config.original_size > limits.max_total_output_size
        || config.frame_size > limits.max_frame_size
        || config.frame_size > profile_max_frame_size) {
        return LzwDynamicRangeProfileError::limit_exceeded;
    }

    stream.dictionary_algorithm = DictionaryAlgorithm::lzw;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::dynamic_range;
    stream.entropy_variant = 1;
    stream.frame_size = config.frame_size;
    stream.dictionary_parameters_size =
        dictionary::internal::lzw_parameter_size;
    stream.original_size = config.original_size;
    if (validate_stream_header(stream, limits) != StreamHeaderError::none) {
        return LzwDynamicRangeProfileError::unsupported;
    }

    const auto largest_frame = std::min<std::uint64_t>(
        config.original_size, config.frame_size);
    if (largest_frame == 0) {
        return LzwDynamicRangeProfileError::none;
    }
    const auto encoder_entries = static_cast<std::uint64_t>(
        dictionary::internal::lzw_encoder_workspace_entries(
            static_cast<std::size_t>(largest_frame), config.parameters));

    std::uint64_t packed_bits{};
    std::uint64_t rounded_bits{};
    std::uint64_t packed_bytes{};
    std::uint64_t payload_bytes{};
    std::uint64_t entropy_buffered_bytes{};
    std::uint64_t entry_bytes{};
    std::uint64_t encoded_bytes{frame_header_size};
    std::uint64_t aggregate_bytes{};
    if (!core::checked_multiply(
            largest_frame,
            static_cast<std::uint64_t>(config.parameters.maximum_code_width),
            packed_bits)
        || !core::checked_add(packed_bits, UINT64_C(7), rounded_bits)
        || !core::checked_multiply(
            rounded_bits / 8, payload_bytes_per_packed_byte, payload_bytes)
        || !core::checked_add(
            payload_bytes, range_termination_bytes, payload_bytes)
        || !core::checked_add(
            static_cast<std::uint64_t>(
                entropy::internal::dynamic_range_descriptor_size),
            payload_bytes, entropy_buffered_bytes)
        || !core::checked_multiply(
            encoder_entries,
            static_cast<std::uint64_t>(
                sizeof(dictionary::internal::LzwEncoderEntry)),
            entry_bytes)
        || !core::checked_add(
            encoded_bytes, entropy_buffered_bytes, encoded_bytes)
        || !core::checked_add(
            largest_frame, rounded_bits / 8, aggregate_bytes)
        || !core::checked_add(
            aggregate_bytes, encoded_bytes, aggregate_bytes)
        || !core::checked_add(
            aggregate_bytes, entry_bytes, aggregate_bytes)) {
        return LzwDynamicRangeProfileError::arithmetic_overflow;
    }
    packed_bytes = rounded_bits / 8;
    if (packed_bytes > entropy::internal::dynamic_range_max_frame_size
        || packed_bytes > limits.max_dictionary_serialized_size
        || payload_bytes > limits.max_compressed_payload_size
        || entropy_buffered_bytes > limits.max_internal_buffered_bytes
        || aggregate_bytes > limits.max_internal_buffered_bytes) {
        return LzwDynamicRangeProfileError::limit_exceeded;
    }
    if (!to_size(largest_frame, workspace.frame_input_bytes)
        || !to_size(packed_bytes, workspace.dictionary_staging_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(encoder_entries, workspace.encoder_entry_count)
        || !to_size(entry_bytes, workspace.views_bytes)) {
        workspace = {};
        return LzwDynamicRangeProfileError::arithmetic_overflow;
    }
    if (encoder_entries != 0) {
        workspace.views_alignment =
            alignof(dictionary::internal::LzwEncoderEntry);
    }
    return LzwDynamicRangeProfileError::none;
}

LzwDynamicRangeProfileError calculate_lzw_dynamic_range_decoder_workspace(
    const core::DecoderLimits& limits,
    LzwDynamicRangeDecoderWorkspaceRequirements& workspace) noexcept {
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none) {
        return LzwDynamicRangeProfileError::invalid_configuration;
    }
    const auto maximum_entries =
        maximum_supported_entries(limits.max_dictionary_entries);
    if (maximum_entries == 0) {
        return LzwDynamicRangeProfileError::limit_exceeded;
    }
    const auto raw_bytes = std::min<std::uint64_t>(
        limits.max_frame_size, profile_max_frame_size);
    const auto packed_bytes = std::min<std::uint64_t>(
        limits.max_dictionary_serialized_size,
        entropy::internal::dynamic_range_max_frame_size);
    std::uint64_t phrase_entries{};
    std::uint64_t encoded_bytes{};
    std::uint64_t phrase_bytes{};
    if (!maximum_phrase_entries(
            packed_bytes, maximum_entries, phrase_entries)
        || !core::checked_add(
            static_cast<std::uint64_t>(frame_header_size),
            limits.max_internal_buffered_bytes, encoded_bytes)
        || !core::checked_multiply(
            phrase_entries,
            static_cast<std::uint64_t>(
                sizeof(dictionary::internal::LzwPhraseEntry)),
            phrase_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(packed_bytes, workspace.dictionary_staging_bytes)
        || !to_size(raw_bytes, workspace.frame_decoded_bytes)
        || !to_size(phrase_entries, workspace.phrase_entry_count)
        || !to_size(phrase_bytes, workspace.views_bytes)) {
        workspace = {};
        return LzwDynamicRangeProfileError::arithmetic_overflow;
    }
    if (phrase_entries != 0) {
        workspace.views_alignment =
            alignof(dictionary::internal::LzwPhraseEntry);
    }
    return LzwDynamicRangeProfileError::none;
}

LzwDynamicRangeWorkspaceError partition_lzw_dynamic_range_encoder_views(
    const LzwDynamicRangeEncoderWorkspaceRequirements& requirements,
    const std::span<std::byte> storage,
    LzwDynamicRangeEncoderViews& views) noexcept {
    return partition_records(
        requirements.encoder_entry_count, requirements.views_bytes,
        requirements.views_alignment, storage, views.entries);
}

LzwDynamicRangeWorkspaceError partition_lzw_dynamic_range_decoder_views(
    const LzwDynamicRangeDecoderWorkspaceRequirements& requirements,
    const std::span<std::byte> storage,
    LzwDynamicRangeDecoderViews& views) noexcept {
    return partition_records(
        requirements.phrase_entry_count, requirements.views_bytes,
        requirements.views_alignment, storage, views.phrases);
}

core::ErrorCode lzw_dynamic_range_profile_error_code(
    const LzwDynamicRangeProfileError error) noexcept {
    switch (error) {
    case LzwDynamicRangeProfileError::none:
        return core::ErrorCode::none;
    case LzwDynamicRangeProfileError::invalid_configuration:
        return core::ErrorCode::invalid_argument;
    case LzwDynamicRangeProfileError::unsupported:
        return core::ErrorCode::unsupported;
    case LzwDynamicRangeProfileError::limit_exceeded:
    case LzwDynamicRangeProfileError::arithmetic_overflow:
        return core::ErrorCode::limit_exceeded;
    }
    return core::ErrorCode::internal_error;
}

} // namespace marc::frame
