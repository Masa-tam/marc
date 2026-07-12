#include "frame/rans_profile.hpp"

#include "core/checked_math.hpp"
#include "entropy/rans_format.hpp"
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

RansProfileError make_rans_profile(
    const RansProfileConfig& config, const core::DecoderLimits& limits,
    StreamHeader& stream,
    RansEncoderWorkspaceRequirements& workspace) noexcept {
    stream = {};
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none
        || config.frame_size == 0 || config.block_size == 0) {
        return RansProfileError::invalid_configuration;
    }
    if (config.original_size > limits.max_total_output_size
        || config.frame_size > limits.max_frame_size
        || config.block_size > limits.max_block_size
        || config.block_size > entropy::internal::rans_max_block_size) {
        return RansProfileError::limit_exceeded;
    }
    stream.entropy_algorithm = EntropyAlgorithm::rans;
    stream.entropy_variant = 1;
    stream.frame_size = config.frame_size;
    stream.entropy_block_size = config.block_size;
    stream.original_size = config.original_size;
    if (validate_stream_header(stream, limits) != StreamHeaderError::none) {
        return RansProfileError::unsupported;
    }

    const auto largest_frame = std::min<std::uint64_t>(
        config.original_size, config.frame_size);
    const auto block_count = largest_frame == 0 ? UINT64_C(0)
        : UINT64_C(1) + (largest_frame - 1) / config.block_size;
    if (block_count > limits.max_blocks_per_frame) {
        return RansProfileError::limit_exceeded;
    }
    std::uint64_t descriptor_bytes{};
    std::uint64_t state_bytes{};
    std::uint64_t payload_bytes{};
    std::uint64_t buffered_bytes{};
    std::uint64_t encoded_bytes{frame_header_size};
    if (!core::checked_multiply(
            block_count,
            static_cast<std::uint64_t>(entropy::internal::rans_descriptor_size),
            descriptor_bytes)
        || !core::checked_multiply(
            block_count,
            static_cast<std::uint64_t>(entropy::internal::rans_min_payload_size),
            state_bytes)
        || !core::checked_add(largest_frame, state_bytes, payload_bytes)
        || !core::checked_add(descriptor_bytes, payload_bytes, buffered_bytes)
        || !core::checked_add(encoded_bytes, buffered_bytes, encoded_bytes)) {
        return RansProfileError::arithmetic_overflow;
    }
    if (payload_bytes > limits.max_compressed_payload_size
        || buffered_bytes > limits.max_internal_buffered_bytes) {
        return RansProfileError::limit_exceeded;
    }
    if (!to_size(largest_frame, workspace.frame_input_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)) {
        workspace = {};
        return RansProfileError::arithmetic_overflow;
    }
    return RansProfileError::none;
}

RansProfileError calculate_rans_decoder_workspace(
    const core::DecoderLimits& limits,
    RansDecoderWorkspaceRequirements& workspace) noexcept {
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none) {
        return RansProfileError::invalid_configuration;
    }
    std::uint64_t encoded_bytes{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(frame_header_size),
            limits.max_internal_buffered_bytes, encoded_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(limits.max_frame_size, workspace.frame_decoded_bytes)
        || !to_size(limits.max_blocks_per_frame,
                    workspace.block_view_count)) {
        workspace = {};
        return RansProfileError::arithmetic_overflow;
    }
    return RansProfileError::none;
}

core::ErrorCode rans_profile_error_code(
    const RansProfileError error) noexcept {
    switch (error) {
    case RansProfileError::none: return core::ErrorCode::none;
    case RansProfileError::invalid_configuration:
        return core::ErrorCode::invalid_argument;
    case RansProfileError::unsupported: return core::ErrorCode::unsupported;
    case RansProfileError::limit_exceeded:
    case RansProfileError::arithmetic_overflow:
        return core::ErrorCode::limit_exceeded;
    }
    return core::ErrorCode::internal_error;
}

} // namespace marc::frame
