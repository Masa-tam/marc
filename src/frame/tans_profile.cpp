#include "frame/tans_profile.hpp"

#include "core/checked_math.hpp"
#include "entropy/tans_format.hpp"
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

TansProfileError make_tans_profile(
    const TansProfileConfig& config, const core::DecoderLimits& limits,
    StreamHeader& stream,
    TansEncoderWorkspaceRequirements& workspace) noexcept {
    stream = {};
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none
        || config.frame_size == 0 || config.block_size == 0) {
        return TansProfileError::invalid_configuration;
    }
    if (config.original_size > limits.max_total_output_size
        || config.frame_size > limits.max_frame_size
        || config.block_size > limits.max_block_size
        || config.block_size > entropy::internal::tans_max_block_size) {
        return TansProfileError::limit_exceeded;
    }
    stream.entropy_algorithm = EntropyAlgorithm::tans;
    stream.entropy_variant = 1;
    stream.frame_size = config.frame_size;
    stream.entropy_block_size = config.block_size;
    stream.original_size = config.original_size;
    if (validate_stream_header(stream, limits) != StreamHeaderError::none) {
        return TansProfileError::unsupported;
    }

    const auto largest_frame = std::min<std::uint64_t>(
        config.original_size, config.frame_size);
    const auto block_count = largest_frame == 0 ? UINT64_C(0)
        : UINT64_C(1) + (largest_frame - 1) / config.block_size;
    if (block_count > limits.max_blocks_per_frame) {
        return TansProfileError::limit_exceeded;
    }
    const auto full_blocks = largest_frame / config.block_size;
    const auto final_symbols = largest_frame % config.block_size;
    std::uint64_t descriptor_bytes{};
    std::uint64_t full_block_bits{};
    std::uint64_t full_block_payload{};
    std::uint64_t full_payloads{};
    std::uint64_t final_bits{};
    std::uint64_t final_payload{};
    std::uint64_t payload_bytes{};
    std::uint64_t buffered_bytes{};
    std::uint64_t encoded_bytes{frame_header_size};
    if (!core::checked_multiply(
            block_count,
            static_cast<std::uint64_t>(entropy::internal::tans_descriptor_size),
            descriptor_bytes)
        || !core::checked_multiply(
            static_cast<std::uint64_t>(config.block_size), UINT64_C(12),
            full_block_bits)
        || !core::checked_add(full_block_bits, UINT64_C(7), full_block_bits)
        || !core::checked_add(
            full_block_bits / 8,
            static_cast<std::uint64_t>(entropy::internal::tans_min_payload_size),
            full_block_payload)
        || !core::checked_multiply(
            full_blocks, full_block_payload, full_payloads)
        || !core::checked_multiply(final_symbols, UINT64_C(12), final_bits)
        || (final_symbols != 0
            && (!core::checked_add(final_bits, UINT64_C(7), final_bits)
                || !core::checked_add(
                    final_bits / 8,
                    static_cast<std::uint64_t>(
                        entropy::internal::tans_min_payload_size),
                    final_payload)))
        || !core::checked_add(full_payloads, final_payload, payload_bytes)
        || !core::checked_add(descriptor_bytes, payload_bytes, buffered_bytes)
        || !core::checked_add(encoded_bytes, buffered_bytes, encoded_bytes)) {
        return TansProfileError::arithmetic_overflow;
    }
    if (payload_bytes > limits.max_compressed_payload_size
        || buffered_bytes > limits.max_internal_buffered_bytes) {
        return TansProfileError::limit_exceeded;
    }
    if (!to_size(largest_frame, workspace.frame_input_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)) {
        workspace = {};
        return TansProfileError::arithmetic_overflow;
    }
    return TansProfileError::none;
}

TansProfileError calculate_tans_decoder_workspace(
    const core::DecoderLimits& limits,
    TansDecoderWorkspaceRequirements& workspace) noexcept {
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none) {
        return TansProfileError::invalid_configuration;
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
        return TansProfileError::arithmetic_overflow;
    }
    return TansProfileError::none;
}

core::ErrorCode tans_profile_error_code(
    const TansProfileError error) noexcept {
    switch (error) {
    case TansProfileError::none: return core::ErrorCode::none;
    case TansProfileError::invalid_configuration:
        return core::ErrorCode::invalid_argument;
    case TansProfileError::unsupported: return core::ErrorCode::unsupported;
    case TansProfileError::limit_exceeded:
    case TansProfileError::arithmetic_overflow:
        return core::ErrorCode::limit_exceeded;
    }
    return core::ErrorCode::internal_error;
}

} // namespace marc::frame
