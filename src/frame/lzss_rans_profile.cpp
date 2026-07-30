#include "frame/lzss_rans_profile.hpp"

#include "core/checked_math.hpp"
#include "entropy/rans_format.hpp"

#include <algorithm>
#include <limits>

namespace marc::frame {
namespace {

inline constexpr std::uint64_t profile_max_frame_size = UINT64_C(1) << 20;
inline constexpr std::uint64_t dictionary_bytes_per_raw_byte = 2;

[[nodiscard]] bool to_size(
    const std::uint64_t value, std::size_t& result) noexcept {
    if (value > std::numeric_limits<std::size_t>::max()) {
        return false;
    }
    result = static_cast<std::size_t>(value);
    return true;
}

} // namespace

LzssRansProfileError make_lzss_rans_profile(
    const LzssRansProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzssRansEncoderWorkspaceRequirements& workspace) noexcept {
    stream = {};
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none
        || config.frame_size == 0 || config.entropy_block_size == 0) {
        return LzssRansProfileError::invalid_configuration;
    }
    const auto parameter_error =
        dictionary::internal::validate_lzss_parameters(
            config.parameters, limits);
    if (parameter_error != dictionary::internal::LzssFormatError::none) {
        return parameter_error
                   == dictionary::internal::LzssFormatError::limit_exceeded
            ? LzssRansProfileError::limit_exceeded
            : LzssRansProfileError::invalid_configuration;
    }
    if (config.original_size > limits.max_total_output_size
        || config.frame_size > limits.max_frame_size
        || config.frame_size > profile_max_frame_size
        || config.entropy_block_size > limits.max_block_size
        || config.entropy_block_size
               > entropy::internal::rans_max_block_size) {
        return LzssRansProfileError::limit_exceeded;
    }

    stream.dictionary_algorithm = DictionaryAlgorithm::lzss;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::rans;
    stream.entropy_variant = 1;
    stream.frame_size = config.frame_size;
    stream.entropy_block_size = config.entropy_block_size;
    stream.dictionary_parameters_size =
        dictionary::internal::lzss_parameter_size;
    stream.original_size = config.original_size;
    if (validate_stream_header(stream, limits) != StreamHeaderError::none) {
        return LzssRansProfileError::unsupported;
    }

    const auto largest_frame = std::min<std::uint64_t>(
        config.original_size, config.frame_size);
    if (largest_frame == 0) {
        return LzssRansProfileError::none;
    }

    std::uint64_t dictionary_bytes{};
    if (!core::checked_multiply(
            largest_frame, dictionary_bytes_per_raw_byte,
            dictionary_bytes)) {
        return LzssRansProfileError::arithmetic_overflow;
    }
    const auto block_count =
        UINT64_C(1)
        + (dictionary_bytes - 1) / config.entropy_block_size;
    if (block_count > limits.max_blocks_per_frame
        || block_count > std::numeric_limits<std::uint32_t>::max()) {
        return LzssRansProfileError::limit_exceeded;
    }

    std::uint64_t descriptor_bytes{};
    std::uint64_t state_bytes{};
    std::uint64_t payload_bytes{};
    std::uint64_t entropy_buffered_bytes{};
    std::uint64_t frame_encoded_bytes{frame_header_size};
    std::uint64_t aggregate_bytes{};
    if (!core::checked_multiply(
            block_count,
            static_cast<std::uint64_t>(
                entropy::internal::rans_descriptor_size),
            descriptor_bytes)
        || !core::checked_multiply(
            block_count,
            static_cast<std::uint64_t>(
                entropy::internal::rans_min_payload_size),
            state_bytes)
        || !core::checked_add(
            dictionary_bytes, state_bytes, payload_bytes)
        || !core::checked_add(
            descriptor_bytes, payload_bytes, entropy_buffered_bytes)
        || !core::checked_add(
            frame_encoded_bytes, entropy_buffered_bytes,
            frame_encoded_bytes)
        || !core::checked_add(
            largest_frame, dictionary_bytes, aggregate_bytes)
        || !core::checked_add(
            aggregate_bytes, frame_encoded_bytes, aggregate_bytes)) {
        return LzssRansProfileError::arithmetic_overflow;
    }
    if (descriptor_bytes > std::numeric_limits<std::uint32_t>::max()
        || payload_bytes > std::numeric_limits<std::uint32_t>::max()
        || dictionary_bytes > std::numeric_limits<std::uint32_t>::max()
        || dictionary_bytes > limits.max_dictionary_serialized_size
        || payload_bytes > limits.max_compressed_payload_size
        || entropy_buffered_bytes > limits.max_internal_buffered_bytes
        || aggregate_bytes > limits.max_internal_buffered_bytes) {
        return LzssRansProfileError::limit_exceeded;
    }
    if (!to_size(largest_frame, workspace.frame_input_bytes)
        || !to_size(
            dictionary_bytes, workspace.dictionary_staging_bytes)
        || !to_size(frame_encoded_bytes, workspace.frame_encoded_bytes)) {
        workspace = {};
        return LzssRansProfileError::arithmetic_overflow;
    }
    return LzssRansProfileError::none;
}

LzssRansProfileError calculate_lzss_rans_decoder_workspace(
    const core::DecoderLimits& limits,
    LzssRansDecoderWorkspaceRequirements& workspace) noexcept {
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none) {
        return LzssRansProfileError::invalid_configuration;
    }
    const auto raw_bytes = std::min<std::uint64_t>(
        limits.max_frame_size, profile_max_frame_size);
    std::uint64_t profile_dictionary_bytes{};
    if (!core::checked_multiply(
            raw_bytes, dictionary_bytes_per_raw_byte,
            profile_dictionary_bytes)) {
        return LzssRansProfileError::arithmetic_overflow;
    }
    const auto dictionary_bytes = std::min<std::uint64_t>(
        limits.max_dictionary_serialized_size,
        profile_dictionary_bytes);
    std::uint64_t encoded_bytes{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(frame_header_size),
            limits.max_internal_buffered_bytes, encoded_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(
            dictionary_bytes, workspace.dictionary_staging_bytes)
        || !to_size(raw_bytes, workspace.frame_decoded_bytes)
        || !to_size(
            limits.max_blocks_per_frame, workspace.block_view_count)) {
        workspace = {};
        return LzssRansProfileError::arithmetic_overflow;
    }
    return LzssRansProfileError::none;
}

core::ErrorCode lzss_rans_profile_error_code(
    const LzssRansProfileError error) noexcept {
    switch (error) {
    case LzssRansProfileError::none:
        return core::ErrorCode::none;
    case LzssRansProfileError::invalid_configuration:
        return core::ErrorCode::invalid_argument;
    case LzssRansProfileError::unsupported:
        return core::ErrorCode::unsupported;
    case LzssRansProfileError::limit_exceeded:
    case LzssRansProfileError::arithmetic_overflow:
        return core::ErrorCode::limit_exceeded;
    }
    return core::ErrorCode::internal_error;
}

} // namespace marc::frame
