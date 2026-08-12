#include "frame/lzss_dynamic_range_profile.hpp"

#include "core/checked_math.hpp"
#include "dictionary/lzss_hash_chain_match_finder.hpp"

#include <algorithm>
#include <limits>

namespace marc::frame {
namespace {

inline constexpr std::uint64_t profile_max_frame_size = UINT64_C(1) << 23;
inline constexpr std::uint64_t dictionary_bytes_per_raw_byte = 2;
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

} // namespace

LzssDynamicRangeProfileError make_lzss_dynamic_range_profile(
    const LzssDynamicRangeProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzssDynamicRangeEncoderWorkspaceRequirements& workspace) noexcept {
    stream = {};
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none
        || config.frame_size == 0) {
        return LzssDynamicRangeProfileError::invalid_configuration;
    }
    const auto parameter_error =
        dictionary::internal::validate_lzss_parameters(
            config.parameters, limits);
    if (parameter_error != dictionary::internal::LzssFormatError::none) {
        return parameter_error
                   == dictionary::internal::LzssFormatError::limit_exceeded
            ? LzssDynamicRangeProfileError::limit_exceeded
            : LzssDynamicRangeProfileError::invalid_configuration;
    }
    if (config.original_size > limits.max_total_output_size
        || config.frame_size > limits.max_frame_size
        || config.frame_size > profile_max_frame_size) {
        return LzssDynamicRangeProfileError::limit_exceeded;
    }

    stream.dictionary_algorithm = DictionaryAlgorithm::lzss;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::dynamic_range;
    stream.entropy_variant = 1;
    stream.frame_size = config.frame_size;
    stream.dictionary_parameters_size =
        dictionary::internal::lzss_parameter_size;
    stream.original_size = config.original_size;
    if (validate_stream_header(stream, limits) != StreamHeaderError::none) {
        return LzssDynamicRangeProfileError::unsupported;
    }

    const auto largest_frame = std::min<std::uint64_t>(
        config.original_size, config.frame_size);
    if (largest_frame == 0) {
        return LzssDynamicRangeProfileError::none;
    }

    std::size_t largest_frame_size{};
    if (!to_size(largest_frame, largest_frame_size)) {
        return LzssDynamicRangeProfileError::arithmetic_overflow;
    }
    const auto finder = dictionary::internal::
        calculate_lzss_hash_chain_workspace(
            largest_frame_size, config.parameters, limits);
    if (finder.error != dictionary::internal::LzssHashChainError::none) {
        return finder.error
                == dictionary::internal::LzssHashChainError::
                    workspace_limit_exceeded
            ? LzssDynamicRangeProfileError::limit_exceeded
            : LzssDynamicRangeProfileError::arithmetic_overflow;
    }

    std::uint64_t dictionary_bytes{};
    std::uint64_t payload_bytes{};
    std::uint64_t entropy_buffered_bytes{};
    std::uint64_t frame_encoded_bytes{frame_header_size};
    std::uint64_t aggregate_bytes{};
    if (!core::checked_multiply(
            largest_frame, dictionary_bytes_per_raw_byte, dictionary_bytes)
        || !core::checked_multiply(
            dictionary_bytes, payload_bytes_per_token_byte, payload_bytes)
        || !core::checked_add(
            payload_bytes, range_termination_bytes, payload_bytes)
        || !core::checked_add(
            static_cast<std::uint64_t>(
                entropy::internal::dynamic_range_descriptor_size),
            payload_bytes, entropy_buffered_bytes)
        || !core::checked_add(
            frame_encoded_bytes, entropy_buffered_bytes,
            frame_encoded_bytes)
        || !core::checked_add(
            largest_frame, dictionary_bytes, aggregate_bytes)
        || !core::checked_add(
            aggregate_bytes,
            static_cast<std::uint64_t>(finder.workspace_size),
            aggregate_bytes)
        || !core::checked_add(
            aggregate_bytes, frame_encoded_bytes, aggregate_bytes)) {
        return LzssDynamicRangeProfileError::arithmetic_overflow;
    }
    if (dictionary_bytes > entropy::internal::dynamic_range_max_frame_size
        || dictionary_bytes > limits.max_dictionary_serialized_size
        || payload_bytes > limits.max_compressed_payload_size
        || entropy_buffered_bytes > limits.max_internal_buffered_bytes
        || aggregate_bytes > limits.max_internal_buffered_bytes) {
        return LzssDynamicRangeProfileError::limit_exceeded;
    }
    if (!to_size(largest_frame, workspace.frame_input_bytes)
        || !to_size(dictionary_bytes, workspace.dictionary_staging_bytes)
        || !to_size(frame_encoded_bytes, workspace.frame_encoded_bytes)) {
        workspace = {};
        return LzssDynamicRangeProfileError::arithmetic_overflow;
    }
    workspace.match_finder_bytes = finder.workspace_size;
    workspace.match_finder_alignment = finder.workspace_size == 0
        ? 1 : finder.workspace_alignment;
    return LzssDynamicRangeProfileError::none;
}

LzssDynamicRangeProfileError
calculate_lzss_dynamic_range_decoder_workspace(
    const core::DecoderLimits& limits,
    LzssDynamicRangeDecoderWorkspaceRequirements& workspace) noexcept {
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none) {
        return LzssDynamicRangeProfileError::invalid_configuration;
    }
    const auto raw_bytes = std::min<std::uint64_t>(
        limits.max_frame_size, profile_max_frame_size);
    std::uint64_t profile_dictionary_bytes{};
    if (!core::checked_multiply(
            raw_bytes, dictionary_bytes_per_raw_byte,
            profile_dictionary_bytes)) {
        return LzssDynamicRangeProfileError::arithmetic_overflow;
    }
    const auto dictionary_bytes = std::min<std::uint64_t>(
        limits.max_dictionary_serialized_size,
        std::min<std::uint64_t>(
            profile_dictionary_bytes,
            entropy::internal::dynamic_range_max_frame_size));
    std::uint64_t encoded_bytes{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(frame_header_size),
            limits.max_internal_buffered_bytes, encoded_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(dictionary_bytes,
                    workspace.dictionary_staging_bytes)
        || !to_size(raw_bytes, workspace.frame_decoded_bytes)) {
        workspace = {};
        return LzssDynamicRangeProfileError::arithmetic_overflow;
    }
    return LzssDynamicRangeProfileError::none;
}

core::ErrorCode lzss_dynamic_range_profile_error_code(
    const LzssDynamicRangeProfileError error) noexcept {
    switch (error) {
    case LzssDynamicRangeProfileError::none:
        return core::ErrorCode::none;
    case LzssDynamicRangeProfileError::invalid_configuration:
        return core::ErrorCode::invalid_argument;
    case LzssDynamicRangeProfileError::unsupported:
        return core::ErrorCode::unsupported;
    case LzssDynamicRangeProfileError::limit_exceeded:
    case LzssDynamicRangeProfileError::arithmetic_overflow:
        return core::ErrorCode::limit_exceeded;
    }
    return core::ErrorCode::internal_error;
}

} // namespace marc::frame
