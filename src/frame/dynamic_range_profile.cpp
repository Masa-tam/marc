#include "frame/dynamic_range_profile.hpp"

#include "core/checked_math.hpp"
#include "entropy/dynamic_range_format.hpp"
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

} // namespace

DynamicRangeProfileError make_dynamic_range_profile(
    const DynamicRangeProfileConfig& config,
    const core::DecoderLimits& limits, StreamHeader& stream,
    DynamicRangeEncoderWorkspaceRequirements& workspace) noexcept {
    stream = {};
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none
        || config.frame_size == 0) {
        return DynamicRangeProfileError::invalid_configuration;
    }
    if (config.original_size > limits.max_total_output_size
        || config.frame_size > limits.max_frame_size
        || config.frame_size
            > entropy::internal::dynamic_range_max_frame_size
        || limits.max_range_model_total
            < entropy::internal::dynamic_range_model_total_limit) {
        return DynamicRangeProfileError::limit_exceeded;
    }
    stream.entropy_algorithm = EntropyAlgorithm::dynamic_range;
    stream.entropy_variant = 1;
    stream.frame_size = config.frame_size;
    stream.original_size = config.original_size;
    if (validate_stream_header(stream, limits) != StreamHeaderError::none) {
        return DynamicRangeProfileError::unsupported;
    }

    const auto largest_frame = std::min<std::uint64_t>(
        config.original_size, config.frame_size);
    if (largest_frame == 0) return DynamicRangeProfileError::none;
    std::uint64_t payload_bytes{};
    std::uint64_t buffered_bytes{};
    std::uint64_t encoded_bytes{};
    if (!core::checked_multiply(
            largest_frame, UINT64_C(2), payload_bytes)
        || !core::checked_add(payload_bytes, UINT64_C(5), payload_bytes)) {
        return DynamicRangeProfileError::arithmetic_overflow;
    }
    if (!core::checked_add(
            static_cast<std::uint64_t>(
                entropy::internal::dynamic_range_descriptor_size),
            payload_bytes, buffered_bytes)
        || !core::checked_add(
            static_cast<std::uint64_t>(frame_header_size), buffered_bytes,
            encoded_bytes)) {
        return DynamicRangeProfileError::arithmetic_overflow;
    }
    if (payload_bytes > limits.max_compressed_payload_size
        || buffered_bytes > limits.max_internal_buffered_bytes) {
        return DynamicRangeProfileError::limit_exceeded;
    }
    if (!to_size(largest_frame, workspace.frame_input_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)) {
        workspace = {};
        return DynamicRangeProfileError::arithmetic_overflow;
    }
    return DynamicRangeProfileError::none;
}

DynamicRangeProfileError calculate_dynamic_range_decoder_workspace(
    const core::DecoderLimits& limits,
    DynamicRangeDecoderWorkspaceRequirements& workspace) noexcept {
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none) {
        return DynamicRangeProfileError::invalid_configuration;
    }
    if (limits.max_range_model_total
        < entropy::internal::dynamic_range_model_total_limit) {
        return DynamicRangeProfileError::limit_exceeded;
    }
    std::uint64_t encoded_bytes{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(frame_header_size),
            limits.max_internal_buffered_bytes, encoded_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(std::min<std::uint64_t>(
                        limits.max_frame_size,
                        entropy::internal::dynamic_range_max_frame_size),
                    workspace.frame_decoded_bytes)) {
        workspace = {};
        return DynamicRangeProfileError::arithmetic_overflow;
    }
    return DynamicRangeProfileError::none;
}

core::ErrorCode dynamic_range_profile_error_code(
    const DynamicRangeProfileError error) noexcept {
    switch (error) {
    case DynamicRangeProfileError::none:
        return core::ErrorCode::none;
    case DynamicRangeProfileError::invalid_configuration:
        return core::ErrorCode::invalid_argument;
    case DynamicRangeProfileError::unsupported:
        return core::ErrorCode::unsupported;
    case DynamicRangeProfileError::limit_exceeded:
    case DynamicRangeProfileError::arithmetic_overflow:
        return core::ErrorCode::limit_exceeded;
    }
    return core::ErrorCode::internal_error;
}

} // namespace marc::frame
