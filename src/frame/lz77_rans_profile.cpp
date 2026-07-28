#include "frame/lz77_rans_profile.hpp"

#include "core/checked_math.hpp"
#include "entropy/rans_format.hpp"

#include <algorithm>
#include <limits>

namespace marc::frame {
namespace {

inline constexpr std::uint64_t profile_max_frame_size = UINT64_C(1) << 20;

[[nodiscard]] bool to_size(
    const std::uint64_t value, std::size_t& result) noexcept {
    if (value > std::numeric_limits<std::size_t>::max()) {
        return false;
    }
    result = static_cast<std::size_t>(value);
    return true;
}

} // namespace

Lz77RansProfileError make_lz77_rans_profile(
    const Lz77RansProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    Lz77RansEncoderWorkspaceRequirements& workspace) noexcept {
    stream = {};
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none
        || config.frame_size == 0 || config.entropy_block_size == 0) {
        return Lz77RansProfileError::invalid_configuration;
    }
    const auto parameter_error =
        dictionary::internal::validate_lz77_parameters(
            config.parameters, limits);
    if (parameter_error != dictionary::internal::Lz77FormatError::none) {
        return parameter_error
                   == dictionary::internal::Lz77FormatError::limit_exceeded
            ? Lz77RansProfileError::limit_exceeded
            : Lz77RansProfileError::invalid_configuration;
    }
    if (config.original_size > limits.max_total_output_size
        || config.frame_size > limits.max_frame_size
        || config.frame_size > profile_max_frame_size
        || config.entropy_block_size > limits.max_block_size
        || config.entropy_block_size
               > entropy::internal::rans_max_block_size) {
        return Lz77RansProfileError::limit_exceeded;
    }

    stream.dictionary_algorithm = DictionaryAlgorithm::lz77;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::rans;
    stream.entropy_variant = 1;
    stream.frame_size = config.frame_size;
    stream.entropy_block_size = config.entropy_block_size;
    stream.dictionary_parameters_size =
        dictionary::internal::lz77_parameter_size;
    stream.original_size = config.original_size;
    if (validate_stream_header(stream, limits) != StreamHeaderError::none) {
        return Lz77RansProfileError::unsupported;
    }

    const auto largest_frame = std::min<std::uint64_t>(
        config.original_size, config.frame_size);
    if (largest_frame == 0) {
        return Lz77RansProfileError::none;
    }

    std::uint64_t dictionary_bytes{};
    if (!core::checked_multiply(
            largest_frame,
            static_cast<std::uint64_t>(
                dictionary::internal::lz77_token_size),
            dictionary_bytes)) {
        return Lz77RansProfileError::arithmetic_overflow;
    }
    const auto block_count =
        UINT64_C(1)
        + (dictionary_bytes - 1) / config.entropy_block_size;
    if (block_count > limits.max_blocks_per_frame
        || block_count > std::numeric_limits<std::uint32_t>::max()) {
        return Lz77RansProfileError::limit_exceeded;
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
        return Lz77RansProfileError::arithmetic_overflow;
    }
    if (descriptor_bytes > std::numeric_limits<std::uint32_t>::max()
        || payload_bytes > std::numeric_limits<std::uint32_t>::max()
        || dictionary_bytes > std::numeric_limits<std::uint32_t>::max()
        || dictionary_bytes > limits.max_dictionary_serialized_size
        || payload_bytes > limits.max_compressed_payload_size
        || entropy_buffered_bytes > limits.max_internal_buffered_bytes
        || aggregate_bytes > limits.max_internal_buffered_bytes) {
        return Lz77RansProfileError::limit_exceeded;
    }
    if (!to_size(largest_frame, workspace.frame_input_bytes)
        || !to_size(
            dictionary_bytes, workspace.dictionary_staging_bytes)
        || !to_size(frame_encoded_bytes, workspace.frame_encoded_bytes)) {
        workspace = {};
        return Lz77RansProfileError::arithmetic_overflow;
    }
    return Lz77RansProfileError::none;
}

Lz77RansProfileError calculate_lz77_rans_decoder_workspace(
    const core::DecoderLimits& limits,
    Lz77RansDecoderWorkspaceRequirements& workspace) noexcept {
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none) {
        return Lz77RansProfileError::invalid_configuration;
    }
    const auto raw_bytes = std::min<std::uint64_t>(
        limits.max_frame_size, profile_max_frame_size);
    std::uint64_t profile_dictionary_bytes{};
    if (!core::checked_multiply(
            raw_bytes,
            static_cast<std::uint64_t>(
                dictionary::internal::lz77_token_size),
            profile_dictionary_bytes)) {
        return Lz77RansProfileError::arithmetic_overflow;
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
        return Lz77RansProfileError::arithmetic_overflow;
    }
    return Lz77RansProfileError::none;
}

core::ErrorCode lz77_rans_profile_error_code(
    const Lz77RansProfileError error) noexcept {
    switch (error) {
    case Lz77RansProfileError::none:
        return core::ErrorCode::none;
    case Lz77RansProfileError::invalid_configuration:
        return core::ErrorCode::invalid_argument;
    case Lz77RansProfileError::unsupported:
        return core::ErrorCode::unsupported;
    case Lz77RansProfileError::limit_exceeded:
    case Lz77RansProfileError::arithmetic_overflow:
        return core::ErrorCode::limit_exceeded;
    }
    return core::ErrorCode::internal_error;
}

} // namespace marc::frame
