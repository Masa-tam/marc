#include "frame/lzss_typed_context_profile.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace marc::frame::internal {
namespace {

// Format 2 admits at most two operations and six arithmetic decisions per
// raw byte. Before every range decision range >= 2^24, while model totals are
// <= 2^15, so even a unit-frequency update needs at most two byte shifts.
inline constexpr std::uint64_t operations_per_raw_byte = 2;
inline constexpr std::uint64_t decisions_per_raw_byte = 6;
inline constexpr std::uint64_t payload_bytes_per_decision = 2;
inline constexpr std::uint64_t range_termination_bytes = 5;

[[nodiscard]] bool to_size(const std::uint64_t value,
                           std::size_t& result) noexcept {
    if (value > std::numeric_limits<std::size_t>::max()) return false;
    result = static_cast<std::size_t>(value);
    return true;
}

[[nodiscard]] bool align_up(const std::uint64_t value,
                            const std::uint64_t alignment,
                            std::uint64_t& result) noexcept {
    if (alignment == 0) return false;
    const auto remainder = value % alignment;
    return remainder == 0
        ? (result = value, true)
        : core::checked_add(value, alignment - remainder, result);
}

[[nodiscard]] bool aligned(const void* const pointer,
                           const std::size_t alignment) noexcept {
    return alignment != 0
        && reinterpret_cast<std::uintptr_t>(pointer) % alignment == 0;
}

[[nodiscard]] bool encoder_view_layout(
    const std::uint64_t token_count,
    const std::uint64_t operation_count,
    std::uint64_t& operation_offset,
    std::uint64_t& views_bytes) noexcept {
    std::uint64_t token_bytes{};
    std::uint64_t operation_bytes{};
    return core::checked_multiply(
               token_count,
               static_cast<std::uint64_t>(
                   sizeof(dictionary::internal::LzssTypedToken)),
               token_bytes)
        && core::checked_multiply(
               operation_count,
               static_cast<std::uint64_t>(
                   sizeof(context::internal::ModeledOperation)),
               operation_bytes)
        && align_up(
               token_bytes,
               alignof(context::internal::ModeledOperation),
               operation_offset)
        && core::checked_add(operation_offset, operation_bytes, views_bytes);
}

[[nodiscard]] bool decoder_view_layout(
    const std::uint64_t token_count,
    std::uint64_t& views_bytes) noexcept {
    return core::checked_multiply(
        token_count,
        static_cast<std::uint64_t>(
            sizeof(dictionary::internal::LzssTypedToken)),
        views_bytes);
}

} // namespace

LzssTypedContextProfileError make_lzss_typed_context_profile(
    const LzssTypedContextProfileConfig& config,
    const core::DecoderLimits& limits,
    TypedContextStreamHeader& stream,
    LzssTypedContextEncoderWorkspaceRequirements& workspace) noexcept {
    stream = {};
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none
        || config.frame_size == 0) {
        return LzssTypedContextProfileError::invalid_configuration;
    }
    const auto dictionary_error =
        dictionary::internal::validate_lzss_parameters(
            config.dictionary, limits);
    if (dictionary_error != dictionary::internal::LzssFormatError::none) {
        return dictionary_error
                   == dictionary::internal::LzssFormatError::limit_exceeded
            ? LzssTypedContextProfileError::limit_exceeded
            : LzssTypedContextProfileError::invalid_configuration;
    }
    if (config.dictionary.min_match_length != 5
        || config.dictionary.max_match_length > 258
        || config.dictionary.window_size > 65536) {
        return LzssTypedContextProfileError::unsupported;
    }
    if (config.original_size > limits.max_total_output_size
        || config.frame_size > limits.max_frame_size) {
        return LzssTypedContextProfileError::limit_exceeded;
    }

    stream.frame_size = config.frame_size;
    stream.original_size = config.original_size;
    stream.dictionary = config.dictionary;
    stream.range_model_total = typed_context_model_total;
    stream.context_count = typed_context_count;
    const auto stream_error =
        validate_typed_context_stream_header(stream, limits);
    if (stream_error != TypedContextStreamHeaderError::none) {
        return stream_error == TypedContextStreamHeaderError::limit_exceeded
            ? LzssTypedContextProfileError::limit_exceeded
            : LzssTypedContextProfileError::unsupported;
    }

    const auto largest_frame = std::min<std::uint64_t>(
        config.original_size, config.frame_size);
    if (largest_frame == 0) return LzssTypedContextProfileError::none;
    if (largest_frame > limits.max_block_size) {
        return LzssTypedContextProfileError::limit_exceeded;
    }

    std::uint64_t token_count{largest_frame};
    std::uint64_t operation_count{};
    std::uint64_t decision_count{};
    std::uint64_t payload_bytes{};
    std::uint64_t frame_encoded_bytes{};
    std::uint64_t operation_offset{};
    std::uint64_t views_bytes{};
    std::uint64_t aggregate_bytes{};
    if (!core::checked_multiply(
            largest_frame, operations_per_raw_byte, operation_count)
        || !core::checked_multiply(
            largest_frame, decisions_per_raw_byte, decision_count)
        || !core::checked_multiply(
            decision_count, payload_bytes_per_decision, payload_bytes)
        || !core::checked_add(
            payload_bytes, range_termination_bytes, payload_bytes)
        || !core::checked_add(
            static_cast<std::uint64_t>(typed_context_frame_header_size),
            static_cast<std::uint64_t>(
                typed_context_range_descriptor_size),
            frame_encoded_bytes)
        || !core::checked_add(
            frame_encoded_bytes, payload_bytes, frame_encoded_bytes)
        || !encoder_view_layout(
            token_count, operation_count, operation_offset, views_bytes)
        || !core::checked_add(
            largest_frame, views_bytes, aggregate_bytes)
        || !core::checked_add(
            aggregate_bytes, frame_encoded_bytes, aggregate_bytes)) {
        return LzssTypedContextProfileError::arithmetic_overflow;
    }
    if (token_count > std::numeric_limits<std::uint32_t>::max()
        || operation_count > std::numeric_limits<std::uint32_t>::max()
        || decision_count > std::numeric_limits<std::uint32_t>::max()
        || payload_bytes > std::numeric_limits<std::uint32_t>::max()
        || payload_bytes > limits.max_compressed_payload_size
        || payload_bytes > limits.max_internal_buffered_bytes
        || views_bytes > limits.max_internal_buffered_bytes
        || aggregate_bytes > limits.max_internal_buffered_bytes) {
        return LzssTypedContextProfileError::limit_exceeded;
    }
    if (!to_size(largest_frame, workspace.frame_input_bytes)
        || !to_size(frame_encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(token_count, workspace.token_count)
        || !to_size(operation_count, workspace.operation_count)
        || !to_size(operation_offset, workspace.operation_offset)
        || !to_size(views_bytes, workspace.views_bytes)) {
        workspace = {};
        return LzssTypedContextProfileError::arithmetic_overflow;
    }
    workspace.views_alignment = std::max(
        alignof(dictionary::internal::LzssTypedToken),
        alignof(context::internal::ModeledOperation));
    return LzssTypedContextProfileError::none;
}

LzssTypedContextProfileError
calculate_lzss_typed_context_decoder_workspace(
    const core::DecoderLimits& limits,
    LzssTypedContextDecoderWorkspaceRequirements& workspace) noexcept {
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none) {
        return LzssTypedContextProfileError::invalid_configuration;
    }
    if (typed_context_stream_header_size > limits.max_internal_buffered_bytes
        || typed_context_model_total > limits.max_range_model_total
        || typed_context_table_entries > limits.max_entropy_table_entries) {
        return LzssTypedContextProfileError::limit_exceeded;
    }
    const auto raw_bytes = std::min<std::uint64_t>(
        std::min(limits.max_frame_size, limits.max_block_size),
        std::numeric_limits<std::uint32_t>::max());
    const auto payload_bytes = std::min<std::uint64_t>(
        std::min(limits.max_compressed_payload_size,
                 limits.max_internal_buffered_bytes),
        std::numeric_limits<std::uint32_t>::max());
    std::uint64_t encoded_bytes{};
    std::uint64_t views_bytes{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(typed_context_frame_header_size),
            static_cast<std::uint64_t>(
                typed_context_range_descriptor_size),
            encoded_bytes)
        || !core::checked_add(encoded_bytes, payload_bytes, encoded_bytes)
        || !decoder_view_layout(raw_bytes, views_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(raw_bytes, workspace.frame_decoded_bytes)
        || !to_size(raw_bytes, workspace.token_count)
        || !to_size(views_bytes, workspace.views_bytes)) {
        workspace = {};
        return LzssTypedContextProfileError::arithmetic_overflow;
    }
    if (workspace.token_count != 0) {
        workspace.views_alignment =
            alignof(dictionary::internal::LzssTypedToken);
    }
    return LzssTypedContextProfileError::none;
}

LzssTypedContextWorkspaceError partition_lzss_typed_context_encoder_views(
    const LzssTypedContextEncoderWorkspaceRequirements& requirements,
    const std::span<std::byte> storage,
    LzssTypedContextEncoderViews& views) noexcept {
    views = {};
    std::uint64_t expected_offset{};
    std::uint64_t expected_bytes{};
    if (!encoder_view_layout(
            requirements.token_count, requirements.operation_count,
            expected_offset, expected_bytes)) {
        return LzssTypedContextWorkspaceError::arithmetic_overflow;
    }
    if (expected_bytes == 0) {
        return requirements.operation_offset == 0
                && requirements.views_bytes == 0
                && requirements.views_alignment == 1
            ? LzssTypedContextWorkspaceError::none
            : LzssTypedContextWorkspaceError::invalid_requirements;
    }
    const auto expected_alignment = std::max(
        alignof(dictionary::internal::LzssTypedToken),
        alignof(context::internal::ModeledOperation));
    if (expected_offset != requirements.operation_offset
        || expected_bytes != requirements.views_bytes
        || requirements.views_alignment != expected_alignment) {
        return LzssTypedContextWorkspaceError::invalid_requirements;
    }
    if (storage.size() < requirements.views_bytes) {
        return LzssTypedContextWorkspaceError::too_small;
    }
    if (!aligned(storage.data(), expected_alignment)) {
        return LzssTypedContextWorkspaceError::misaligned;
    }
    views.tokens = {
        reinterpret_cast<dictionary::internal::LzssTypedToken*>(
            storage.data()),
        requirements.token_count};
    views.operations = {
        reinterpret_cast<context::internal::ModeledOperation*>(
            storage.data() + requirements.operation_offset),
        requirements.operation_count};
    return LzssTypedContextWorkspaceError::none;
}

LzssTypedContextWorkspaceError partition_lzss_typed_context_decoder_views(
    const LzssTypedContextDecoderWorkspaceRequirements& requirements,
    const std::span<std::byte> storage,
    LzssTypedContextDecoderViews& views) noexcept {
    views = {};
    std::uint64_t expected_bytes{};
    if (!decoder_view_layout(requirements.token_count, expected_bytes)) {
        return LzssTypedContextWorkspaceError::arithmetic_overflow;
    }
    if (expected_bytes == 0) {
        return requirements.views_bytes == 0
                && requirements.views_alignment == 1
            ? LzssTypedContextWorkspaceError::none
            : LzssTypedContextWorkspaceError::invalid_requirements;
    }
    const auto expected_alignment =
        alignof(dictionary::internal::LzssTypedToken);
    if (expected_bytes != requirements.views_bytes
        || requirements.views_alignment != expected_alignment) {
        return LzssTypedContextWorkspaceError::invalid_requirements;
    }
    if (storage.size() < requirements.views_bytes) {
        return LzssTypedContextWorkspaceError::too_small;
    }
    if (!aligned(storage.data(), expected_alignment)) {
        return LzssTypedContextWorkspaceError::misaligned;
    }
    views.tokens = {
        reinterpret_cast<dictionary::internal::LzssTypedToken*>(
            storage.data()),
        requirements.token_count};
    return LzssTypedContextWorkspaceError::none;
}

core::ErrorCode lzss_typed_context_profile_error_code(
    const LzssTypedContextProfileError error) noexcept {
    switch (error) {
    case LzssTypedContextProfileError::none:
        return core::ErrorCode::none;
    case LzssTypedContextProfileError::invalid_configuration:
        return core::ErrorCode::invalid_argument;
    case LzssTypedContextProfileError::unsupported:
        return core::ErrorCode::unsupported;
    case LzssTypedContextProfileError::limit_exceeded:
    case LzssTypedContextProfileError::arithmetic_overflow:
        return core::ErrorCode::limit_exceeded;
    }
    return core::ErrorCode::internal_error;
}

} // namespace marc::frame::internal
