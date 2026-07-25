#include "frame/lz78_dynamic_range_profile.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace marc::frame {
namespace {

inline constexpr std::uint64_t profile_max_frame_size = UINT64_C(1) << 21;
inline constexpr std::uint64_t payload_bytes_per_token_byte = 2;
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

template <typename Record>
[[nodiscard]] Lz78DynamicRangeWorkspaceError partition_records(
    const std::size_t count,
    const std::size_t required_bytes,
    const std::size_t required_alignment,
    const std::span<std::byte> storage,
    std::span<Record>& records) noexcept {
    records = {};
    std::size_t expected_bytes{};
    if (!core::checked_multiply(count, sizeof(Record), expected_bytes)) {
        return Lz78DynamicRangeWorkspaceError::arithmetic_overflow;
    }
    if (expected_bytes == 0) {
        return required_bytes == 0 && required_alignment == 1
            ? Lz78DynamicRangeWorkspaceError::none
            : Lz78DynamicRangeWorkspaceError::invalid_requirements;
    }
    if (expected_bytes != required_bytes
        || required_alignment != alignof(Record)) {
        return Lz78DynamicRangeWorkspaceError::invalid_requirements;
    }
    if (storage.size() < expected_bytes) {
        return Lz78DynamicRangeWorkspaceError::too_small;
    }
    if (!aligned(storage.data(), required_alignment)) {
        return Lz78DynamicRangeWorkspaceError::misaligned;
    }
    records = {reinterpret_cast<Record*>(storage.data()), count};
    return Lz78DynamicRangeWorkspaceError::none;
}

} // namespace

Lz78DynamicRangeProfileError make_lz78_dynamic_range_profile(
    const Lz78DynamicRangeProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    Lz78DynamicRangeEncoderWorkspaceRequirements& workspace) noexcept {
    stream = {};
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none
        || config.frame_size == 0) {
        return Lz78DynamicRangeProfileError::invalid_configuration;
    }
    const auto parameter_error =
        dictionary::internal::validate_lz78_parameters(
            config.parameters, limits);
    if (parameter_error != dictionary::internal::Lz78FormatError::none) {
        return parameter_error
                   == dictionary::internal::Lz78FormatError::limit_exceeded
            ? Lz78DynamicRangeProfileError::limit_exceeded
            : Lz78DynamicRangeProfileError::invalid_configuration;
    }
    if (config.original_size > limits.max_total_output_size
        || config.frame_size > limits.max_frame_size
        || config.frame_size > profile_max_frame_size) {
        return Lz78DynamicRangeProfileError::limit_exceeded;
    }

    stream.dictionary_algorithm = DictionaryAlgorithm::lz78;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::dynamic_range;
    stream.entropy_variant = 1;
    stream.frame_size = config.frame_size;
    stream.dictionary_parameters_size =
        dictionary::internal::lz78_parameter_size;
    stream.original_size = config.original_size;
    if (validate_stream_header(stream, limits) != StreamHeaderError::none) {
        return Lz78DynamicRangeProfileError::unsupported;
    }

    const auto largest_frame = std::min<std::uint64_t>(
        config.original_size, config.frame_size);
    if (largest_frame == 0) {
        return Lz78DynamicRangeProfileError::none;
    }
    const auto encoder_entries = std::min<std::uint64_t>(
        largest_frame, config.parameters.maximum_entries);
    std::uint64_t dictionary_bytes{};
    std::uint64_t payload_bytes{};
    std::uint64_t entropy_buffered_bytes{};
    std::uint64_t entry_bytes{};
    std::uint64_t encoded_bytes{frame_header_size};
    std::uint64_t aggregate_bytes{};
    if (!core::checked_multiply(
            largest_frame,
            static_cast<std::uint64_t>(dictionary::internal::lz78_token_size),
            dictionary_bytes)
        || !core::checked_multiply(
            dictionary_bytes, payload_bytes_per_token_byte, payload_bytes)
        || !core::checked_add(
            payload_bytes, range_termination_bytes, payload_bytes)
        || !core::checked_add(
            static_cast<std::uint64_t>(
                entropy::internal::dynamic_range_descriptor_size),
            payload_bytes, entropy_buffered_bytes)
        || !core::checked_multiply(
            encoder_entries,
            static_cast<std::uint64_t>(
                sizeof(dictionary::internal::Lz78EncoderEntry)),
            entry_bytes)
        || !core::checked_add(
            encoded_bytes, entropy_buffered_bytes, encoded_bytes)
        || !core::checked_add(
            largest_frame, dictionary_bytes, aggregate_bytes)
        || !core::checked_add(
            aggregate_bytes, encoded_bytes, aggregate_bytes)
        || !core::checked_add(
            aggregate_bytes, entry_bytes, aggregate_bytes)) {
        return Lz78DynamicRangeProfileError::arithmetic_overflow;
    }
    if (dictionary_bytes
            > entropy::internal::dynamic_range_max_frame_size
        || dictionary_bytes > limits.max_dictionary_serialized_size
        || payload_bytes > limits.max_compressed_payload_size
        || entropy_buffered_bytes > limits.max_internal_buffered_bytes
        || aggregate_bytes > limits.max_internal_buffered_bytes) {
        return Lz78DynamicRangeProfileError::limit_exceeded;
    }
    if (!to_size(largest_frame, workspace.frame_input_bytes)
        || !to_size(dictionary_bytes, workspace.dictionary_staging_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(encoder_entries, workspace.encoder_entry_count)
        || !to_size(entry_bytes, workspace.views_bytes)) {
        workspace = {};
        return Lz78DynamicRangeProfileError::arithmetic_overflow;
    }
    workspace.views_alignment =
        alignof(dictionary::internal::Lz78EncoderEntry);
    return Lz78DynamicRangeProfileError::none;
}

Lz78DynamicRangeProfileError
calculate_lz78_dynamic_range_decoder_workspace(
    const core::DecoderLimits& limits,
    Lz78DynamicRangeDecoderWorkspaceRequirements& workspace) noexcept {
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none) {
        return Lz78DynamicRangeProfileError::invalid_configuration;
    }
    const auto raw_bytes = std::min<std::uint64_t>(
        limits.max_frame_size, profile_max_frame_size);
    std::uint64_t profile_dictionary_bytes{};
    if (!core::checked_multiply(
            raw_bytes,
            static_cast<std::uint64_t>(dictionary::internal::lz78_token_size),
            profile_dictionary_bytes)) {
        return Lz78DynamicRangeProfileError::arithmetic_overflow;
    }
    const auto dictionary_bytes = std::min<std::uint64_t>(
        limits.max_dictionary_serialized_size,
        std::min<std::uint64_t>(
            profile_dictionary_bytes,
            entropy::internal::dynamic_range_max_frame_size));
    const auto phrase_entries = std::min<std::uint64_t>(
        dictionary_bytes / dictionary::internal::lz78_token_size,
        std::min<std::uint64_t>(limits.max_dictionary_entries,
                                std::numeric_limits<std::uint32_t>::max()));
    std::uint64_t encoded_bytes{};
    std::uint64_t phrase_bytes{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(frame_header_size),
            limits.max_internal_buffered_bytes, encoded_bytes)
        || !core::checked_multiply(
            phrase_entries,
            static_cast<std::uint64_t>(
                sizeof(dictionary::internal::Lz78PhraseEntry)),
            phrase_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(dictionary_bytes,
                    workspace.dictionary_staging_bytes)
        || !to_size(raw_bytes, workspace.frame_decoded_bytes)
        || !to_size(phrase_entries, workspace.phrase_entry_count)
        || !to_size(phrase_bytes, workspace.views_bytes)) {
        workspace = {};
        return Lz78DynamicRangeProfileError::arithmetic_overflow;
    }
    if (phrase_bytes != 0) {
        workspace.views_alignment =
            alignof(dictionary::internal::Lz78PhraseEntry);
    }
    return Lz78DynamicRangeProfileError::none;
}

Lz78DynamicRangeWorkspaceError
partition_lz78_dynamic_range_encoder_views(
    const Lz78DynamicRangeEncoderWorkspaceRequirements& requirements,
    const std::span<std::byte> storage,
    Lz78DynamicRangeEncoderViews& views) noexcept {
    return partition_records(
        requirements.encoder_entry_count, requirements.views_bytes,
        requirements.views_alignment, storage, views.entries);
}

Lz78DynamicRangeWorkspaceError
partition_lz78_dynamic_range_decoder_views(
    const Lz78DynamicRangeDecoderWorkspaceRequirements& requirements,
    const std::span<std::byte> storage,
    Lz78DynamicRangeDecoderViews& views) noexcept {
    return partition_records(
        requirements.phrase_entry_count, requirements.views_bytes,
        requirements.views_alignment, storage, views.phrases);
}

core::ErrorCode lz78_dynamic_range_profile_error_code(
    const Lz78DynamicRangeProfileError error) noexcept {
    switch (error) {
    case Lz78DynamicRangeProfileError::none:
        return core::ErrorCode::none;
    case Lz78DynamicRangeProfileError::invalid_configuration:
        return core::ErrorCode::invalid_argument;
    case Lz78DynamicRangeProfileError::unsupported:
        return core::ErrorCode::unsupported;
    case Lz78DynamicRangeProfileError::limit_exceeded:
    case Lz78DynamicRangeProfileError::arithmetic_overflow:
        return core::ErrorCode::limit_exceeded;
    }
    return core::ErrorCode::internal_error;
}

} // namespace marc::frame
