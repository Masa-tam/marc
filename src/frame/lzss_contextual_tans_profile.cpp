#include "frame/lzss_contextual_tans_profile.hpp"

#include "core/checked_math.hpp"
#include "entropy/contextual_tans_encode_core.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace marc::frame::internal {
namespace {

inline constexpr std::uint64_t bits_per_decision = 12;
inline constexpr std::uint64_t initial_state_bytes = 2;

[[nodiscard]] bool to_size(
    const std::uint64_t value, std::size_t& result) noexcept {
    if (value > std::numeric_limits<std::size_t>::max()) return false;
    result = static_cast<std::size_t>(value);
    return true;
}

[[nodiscard]] bool align_up(
    const std::uint64_t value, const std::uint64_t alignment,
    std::uint64_t& result) noexcept {
    if (alignment == 0) return false;
    const auto remainder = value % alignment;
    return remainder == 0
        ? (result = value, true)
        : core::checked_add(value, alignment - remainder, result);
}

[[nodiscard]] bool aligned(
    const void* const pointer, const std::size_t alignment) noexcept {
    return alignment != 0
        && reinterpret_cast<std::uintptr_t>(pointer) % alignment == 0;
}

[[nodiscard]] bool payload_ceiling(
    const std::uint64_t raw_bytes,
    const std::uint64_t decisions_per_raw_byte,
    std::uint64_t& payload_bytes) noexcept {
    std::uint64_t decisions{};
    std::uint64_t bits{};
    return core::checked_multiply(
               raw_bytes, decisions_per_raw_byte, decisions)
        && core::checked_multiply(decisions, bits_per_decision, bits)
        && core::checked_add(bits, UINT64_C(7), bits)
        && core::checked_add(
            bits / 8, initial_state_bytes, payload_bytes);
}

[[nodiscard]] bool encoder_view_layout(
    const std::uint64_t token_count, const std::uint64_t table_count,
    const std::uint64_t finder_bytes, const std::uint64_t finder_alignment,
    std::uint64_t& table_offset, std::uint64_t& finder_offset,
    std::uint64_t& views_bytes) noexcept {
    std::uint64_t token_bytes{};
    std::uint64_t table_bytes{};
    return core::checked_multiply(
               token_count,
               static_cast<std::uint64_t>(
                   sizeof(dictionary::internal::LzssTypedToken)),
               token_bytes)
        && core::checked_multiply(
               table_count, static_cast<std::uint64_t>(sizeof(std::uint16_t)),
               table_bytes)
        && align_up(token_bytes, alignof(std::uint16_t), table_offset)
        && core::checked_add(table_offset, table_bytes, views_bytes)
        && align_up(views_bytes, finder_alignment, finder_offset)
        && core::checked_add(finder_offset, finder_bytes, views_bytes);
}

[[nodiscard]] bool decoder_view_layout(
    const std::uint64_t table_count, const std::uint64_t token_count,
    std::uint64_t& token_offset, std::uint64_t& views_bytes) noexcept {
    std::uint64_t table_bytes{};
    std::uint64_t token_bytes{};
    return core::checked_multiply(
               table_count,
               static_cast<std::uint64_t>(
                   sizeof(entropy::internal::TansDecodeEntry)),
               table_bytes)
        && core::checked_multiply(
               token_count,
               static_cast<std::uint64_t>(
                   sizeof(dictionary::internal::LzssTypedToken)),
               token_bytes)
        && align_up(
               table_bytes,
               alignof(dictionary::internal::LzssTypedToken), token_offset)
        && core::checked_add(token_offset, token_bytes, views_bytes);
}

[[nodiscard]] constexpr std::size_t decoder_alignment() noexcept {
    return std::max(
        alignof(entropy::internal::TansDecodeEntry),
        alignof(dictionary::internal::LzssTypedToken));
}

[[nodiscard]] context::internal::LzssFieldContextLayoutResult profile_layout(
    const LzssContextualTansProfileVariant variant) noexcept {
    switch (variant) {
    case LzssContextualTansProfileVariant::field_context_64k:
        return context::internal::get_lzss_field_context_layout(
            context::internal::LzssFieldContextVariant::field_context_64k);
    case LzssContextualTansProfileVariant::field_context_1m:
        return context::internal::get_lzss_field_context_layout(
            context::internal::LzssFieldContextVariant::field_context_1m);
    case LzssContextualTansProfileVariant::field_context_4m:
        return context::internal::get_lzss_field_context_layout(
            context::internal::LzssFieldContextVariant::field_context_4m);
    case LzssContextualTansProfileVariant::field_context_16m:
        return context::internal::get_lzss_field_context_layout(
            context::internal::LzssFieldContextVariant::field_context_16m);
    case LzssContextualTansProfileVariant::field_context_64m:
        return context::internal::get_lzss_field_context_layout(
            context::internal::LzssFieldContextVariant::field_context_64m);
    }
    return {{}, context::internal::LzssFieldContextLayoutError::
                    unsupported_context_variant};
}

[[nodiscard]] constexpr std::size_t maximum_descriptor_size(
    const context::internal::LzssFieldContextVariant variant) noexcept {
    switch (variant) {
    case context::internal::LzssFieldContextVariant::field_context_64k:
        return entropy::internal::contextual_tans_max_descriptor_size_v1;
    case context::internal::LzssFieldContextVariant::field_context_1m:
        return entropy::internal::contextual_tans_max_descriptor_size_v2;
    case context::internal::LzssFieldContextVariant::field_context_4m:
        return entropy::internal::contextual_tans_max_descriptor_size_v3;
    case context::internal::LzssFieldContextVariant::field_context_16m:
        return entropy::internal::contextual_tans_max_descriptor_size_v4;
    case context::internal::LzssFieldContextVariant::field_context_64m:
        return entropy::internal::contextual_tans_max_descriptor_size_v5;
    }
    return 0;
}

} // namespace

LzssContextualTansProfileError make_lzss_contextual_tans_profile(
    const LzssContextualTansProfileConfig& config,
    const core::DecoderLimits& limits,
    LzssContextualTansStreamHeader& stream,
    LzssContextualTansEncoderWorkspaceRequirements& workspace) noexcept {
    stream = {};
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none
        || config.frame_size == 0) {
        return LzssContextualTansProfileError::invalid_configuration;
    }
    const auto selected = profile_layout(config.variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        return LzssContextualTansProfileError::unsupported;
    }
    const auto dictionary_error =
        dictionary::internal::validate_lzss_typed_parameters(
            config.dictionary, limits, selected.layout.dictionary_variant);
    if (dictionary_error
        != dictionary::internal::LzssTypedTokenError::none) {
        return dictionary_error
                   == dictionary::internal::LzssTypedTokenError::limit_exceeded
            ? LzssContextualTansProfileError::limit_exceeded
            : LzssContextualTansProfileError::unsupported;
    }
    if (config.original_size > limits.max_total_output_size
        || config.frame_size > limits.max_frame_size) {
        return LzssContextualTansProfileError::limit_exceeded;
    }

    stream.frame_size = config.frame_size;
    stream.original_size = config.original_size;
    stream.dictionary = config.dictionary;
    stream.dictionary_variant = static_cast<std::uint16_t>(
        selected.layout.dictionary_variant);
    stream.context_variant = static_cast<std::uint16_t>(
        selected.layout.context_variant);
    stream.frequency_entry_count = static_cast<std::uint32_t>(
        selected.layout.frequency_entries);
    const auto stream_error = validate_lzss_contextual_tans_stream_header(
        stream, limits);
    if (stream_error != LzssContextualTansStreamHeaderError::none) {
        return stream_error
                   == LzssContextualTansStreamHeaderError::limit_exceeded
            ? LzssContextualTansProfileError::limit_exceeded
            : LzssContextualTansProfileError::unsupported;
    }

    const auto largest_frame = std::min<std::uint64_t>(
        config.original_size, config.frame_size);
    if (!dictionary::internal::is_supported_lzss_match_finder_strategy(
            config.match_finder_strategy)) {
        return LzssContextualTansProfileError::unsupported;
    }
    if (largest_frame == 0) {
        workspace.match_finder_strategy = config.match_finder_strategy;
        return LzssContextualTansProfileError::none;
    }
    std::uint64_t maximum_decisions{};
    if (!core::checked_multiply(
            largest_frame,
            static_cast<std::uint64_t>(
                selected.layout.maximum_decisions_per_raw_byte),
            maximum_decisions)) {
        return LzssContextualTansProfileError::arithmetic_overflow;
    }
    if (largest_frame > limits.max_block_size
        || maximum_decisions > limits.max_block_size
        || entropy::internal::contextual_tans_encode_table_entries
            > limits.max_entropy_table_entries) {
        return LzssContextualTansProfileError::limit_exceeded;
    }

    const std::uint64_t token_count{largest_frame};
    const std::uint64_t table_count{
        entropy::internal::contextual_tans_encode_table_entries};
    const auto descriptor_size = maximum_descriptor_size(
        selected.layout.context_variant);
    std::uint64_t payload_bytes{};
    std::uint64_t frame_encoded_bytes{};
    std::uint64_t table_offset{};
    std::uint64_t finder_offset{};
    std::uint64_t views_bytes{};
    std::uint64_t aggregate_bytes{};
    std::size_t largest_frame_size{};
    if (!to_size(largest_frame, largest_frame_size))
        return LzssContextualTansProfileError::arithmetic_overflow;
    const auto finder = dictionary::internal::calculate_lzss_match_finder_workspace(
        config.match_finder_strategy, largest_frame_size,
        config.dictionary, limits);
    if (finder.error
        != dictionary::internal::LzssMatchFinderWorkspaceError::none) {
        switch (finder.error) {
        case dictionary::internal::LzssMatchFinderWorkspaceError::
                workspace_limit_exceeded:
        case dictionary::internal::LzssMatchFinderWorkspaceError::
                input_limit_exceeded:
            return LzssContextualTansProfileError::limit_exceeded;
        case dictionary::internal::LzssMatchFinderWorkspaceError::
                unsupported_strategy:
            return LzssContextualTansProfileError::unsupported;
        case dictionary::internal::LzssMatchFinderWorkspaceError::
                invalid_configuration:
            return LzssContextualTansProfileError::invalid_configuration;
        case dictionary::internal::LzssMatchFinderWorkspaceError::
                arithmetic_overflow:
            return LzssContextualTansProfileError::arithmetic_overflow;
        case dictionary::internal::LzssMatchFinderWorkspaceError::none:
            break;
        }
        return LzssContextualTansProfileError::arithmetic_overflow;
    }
    if (!payload_ceiling(
            largest_frame,
            selected.layout.maximum_decisions_per_raw_byte, payload_bytes)
        || !core::checked_add(
            static_cast<std::uint64_t>(
                lzss_contextual_tans_frame_header_size),
            static_cast<std::uint64_t>(
                descriptor_size),
            frame_encoded_bytes)
        || !core::checked_add(
            frame_encoded_bytes, payload_bytes, frame_encoded_bytes)
        || !encoder_view_layout(
            token_count, table_count, finder.workspace_size,
            finder.workspace_alignment, table_offset, finder_offset,
            views_bytes)
        || !core::checked_add(
            largest_frame, views_bytes, aggregate_bytes)
        || !core::checked_add(
            aggregate_bytes, frame_encoded_bytes, aggregate_bytes)) {
        return LzssContextualTansProfileError::arithmetic_overflow;
    }
    if (token_count > std::numeric_limits<std::uint32_t>::max()
        || payload_bytes > std::numeric_limits<std::uint32_t>::max()
        || payload_bytes > limits.max_compressed_payload_size
        || aggregate_bytes > limits.max_internal_buffered_bytes) {
        return LzssContextualTansProfileError::limit_exceeded;
    }
    if (!to_size(largest_frame, workspace.frame_input_bytes)
        || !to_size(frame_encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(token_count, workspace.token_count)
        || !to_size(table_count, workspace.table_count)
        || !to_size(table_offset, workspace.table_offset)
        || !to_size(finder_offset, workspace.match_finder_offset)
        || !to_size(finder.workspace_size, workspace.match_finder_bytes)
        || !to_size(views_bytes, workspace.views_bytes)) {
        workspace = {};
        return LzssContextualTansProfileError::arithmetic_overflow;
    }
    workspace.views_alignment = std::max(
        std::max(alignof(dictionary::internal::LzssTypedToken),
                 alignof(std::uint16_t)),
        finder.workspace_alignment);
    workspace.match_finder_alignment = finder.workspace_alignment;
    workspace.match_finder_strategy = config.match_finder_strategy;
    return LzssContextualTansProfileError::none;
}

LzssContextualTansProfileError
calculate_lzss_contextual_tans_decoder_workspace(
    const core::DecoderLimits& limits,
    LzssContextualTansDecoderWorkspaceRequirements& workspace,
    const LzssContextualTansProfileVariant variant) noexcept {
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none) {
        return LzssContextualTansProfileError::invalid_configuration;
    }
    const auto selected = profile_layout(variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        return LzssContextualTansProfileError::unsupported;
    }
    if (lzss_contextual_tans_stream_header_size
            > limits.max_internal_buffered_bytes
        || entropy::internal::contextual_tans_decode_table_entries
        > limits.max_entropy_table_entries) {
        return LzssContextualTansProfileError::limit_exceeded;
    }
    const auto raw_bytes = std::min<std::uint64_t>(
        std::min(limits.max_frame_size, limits.max_block_size),
        std::numeric_limits<std::uint32_t>::max());
    std::uint64_t modeled_payload_bytes{};
    if (!payload_ceiling(
            raw_bytes, selected.layout.maximum_decisions_per_raw_byte,
            modeled_payload_bytes)) {
        return LzssContextualTansProfileError::arithmetic_overflow;
    }
    const auto payload_bytes = std::min<std::uint64_t>(
        std::min(limits.max_compressed_payload_size,
                 limits.max_internal_buffered_bytes),
        std::min<std::uint64_t>(
            modeled_payload_bytes,
            std::numeric_limits<std::uint32_t>::max()));
    std::uint64_t encoded_bytes{};
    std::uint64_t token_offset{};
    std::uint64_t views_bytes{};
    std::uint64_t aggregate_bytes{};
    const auto descriptor_size = maximum_descriptor_size(
        selected.layout.context_variant);
    if (!core::checked_add(
            static_cast<std::uint64_t>(
                lzss_contextual_tans_frame_header_size),
            static_cast<std::uint64_t>(
                descriptor_size),
            encoded_bytes)
        || !core::checked_add(encoded_bytes, payload_bytes, encoded_bytes)
        || !decoder_view_layout(
            entropy::internal::contextual_tans_decode_table_entries,
            raw_bytes, token_offset, views_bytes)
        || !core::checked_add(encoded_bytes, views_bytes, aggregate_bytes)
        || !core::checked_add(
            aggregate_bytes, raw_bytes, aggregate_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(raw_bytes, workspace.frame_decoded_bytes)
        || !to_size(
            entropy::internal::contextual_tans_decode_table_entries,
            workspace.table_count)
        || !to_size(raw_bytes, workspace.token_count)
        || !to_size(token_offset, workspace.token_offset)
        || !to_size(views_bytes, workspace.views_bytes)) {
        workspace = {};
        return LzssContextualTansProfileError::arithmetic_overflow;
    }
    if (aggregate_bytes > limits.max_internal_buffered_bytes) {
        workspace = {};
        return LzssContextualTansProfileError::limit_exceeded;
    }
    workspace.views_alignment = decoder_alignment();
    return LzssContextualTansProfileError::none;
}

LzssContextualTansWorkspaceError
partition_lzss_contextual_tans_encoder_views(
    const LzssContextualTansEncoderWorkspaceRequirements& requirements,
    const std::span<std::byte> storage,
    LzssContextualTansEncoderViews& views) noexcept {
    views = {};
    std::uint64_t expected_offset{};
    std::uint64_t expected_finder_offset{};
    std::uint64_t expected_bytes{};
    if (!encoder_view_layout(
            requirements.token_count, requirements.table_count,
            requirements.match_finder_bytes,
            requirements.match_finder_alignment,
            expected_offset, expected_finder_offset, expected_bytes)) {
        return LzssContextualTansWorkspaceError::arithmetic_overflow;
    }
    if (expected_bytes == 0) {
        return requirements.table_offset == 0
                && requirements.match_finder_offset == 0
                && requirements.match_finder_bytes == 0
                && requirements.match_finder_alignment == 1
                && dictionary::internal::
                    is_supported_lzss_match_finder_strategy(
                        requirements.match_finder_strategy)
                && requirements.views_bytes == 0
                && requirements.views_alignment == 1
            ? LzssContextualTansWorkspaceError::none
            : LzssContextualTansWorkspaceError::invalid_requirements;
    }
    const auto expected_alignment = std::max(
        std::max(alignof(dictionary::internal::LzssTypedToken),
                 alignof(std::uint16_t)),
        requirements.match_finder_alignment);
    const auto canonical_match_finder_alignment =
        dictionary::internal::lzss_match_finder_workspace_alignment(
            requirements.match_finder_strategy);
    if (requirements.table_count
            != entropy::internal::contextual_tans_encode_table_entries
        || expected_offset != requirements.table_offset
        || expected_finder_offset != requirements.match_finder_offset
        || expected_bytes != requirements.views_bytes
        || canonical_match_finder_alignment == 0
        || requirements.match_finder_alignment
            != canonical_match_finder_alignment
        || requirements.views_alignment != expected_alignment) {
        return LzssContextualTansWorkspaceError::invalid_requirements;
    }
    if (storage.size() < requirements.views_bytes) {
        return LzssContextualTansWorkspaceError::too_small;
    }
    if (!aligned(storage.data(), requirements.views_alignment)) {
        return LzssContextualTansWorkspaceError::misaligned;
    }
    views.tokens = {
        reinterpret_cast<dictionary::internal::LzssTypedToken*>(
            storage.data()),
        requirements.token_count};
    views.tables = {
        reinterpret_cast<std::uint16_t*>(
            storage.data() + requirements.table_offset),
        requirements.table_count};
    views.match_finder = storage.subspan(
        requirements.match_finder_offset, requirements.match_finder_bytes);
    return LzssContextualTansWorkspaceError::none;
}

LzssContextualTansWorkspaceError
partition_lzss_contextual_tans_decoder_views(
    const LzssContextualTansDecoderWorkspaceRequirements& requirements,
    const std::span<std::byte> storage,
    LzssContextualTansDecoderViews& views) noexcept {
    views = {};
    std::uint64_t expected_offset{};
    std::uint64_t expected_bytes{};
    if (!decoder_view_layout(
            requirements.table_count, requirements.token_count,
            expected_offset, expected_bytes)) {
        return LzssContextualTansWorkspaceError::arithmetic_overflow;
    }
    if (requirements.table_count
            != entropy::internal::contextual_tans_decode_table_entries
        || expected_offset != requirements.token_offset
        || expected_bytes != requirements.views_bytes
        || requirements.views_alignment != decoder_alignment()) {
        return LzssContextualTansWorkspaceError::invalid_requirements;
    }
    if (storage.size() < requirements.views_bytes) {
        return LzssContextualTansWorkspaceError::too_small;
    }
    if (!aligned(storage.data(), requirements.views_alignment)) {
        return LzssContextualTansWorkspaceError::misaligned;
    }
    views.tables = {
        reinterpret_cast<entropy::internal::TansDecodeEntry*>(storage.data()),
        requirements.table_count};
    views.tokens = {
        reinterpret_cast<dictionary::internal::LzssTypedToken*>(
            storage.data() + requirements.token_offset),
        requirements.token_count};
    return LzssContextualTansWorkspaceError::none;
}

core::ErrorCode lzss_contextual_tans_profile_error_code(
    const LzssContextualTansProfileError error) noexcept {
    switch (error) {
    case LzssContextualTansProfileError::none:
        return core::ErrorCode::none;
    case LzssContextualTansProfileError::invalid_configuration:
        return core::ErrorCode::invalid_argument;
    case LzssContextualTansProfileError::unsupported:
        return core::ErrorCode::unsupported;
    case LzssContextualTansProfileError::limit_exceeded:
    case LzssContextualTansProfileError::arithmetic_overflow:
        return core::ErrorCode::limit_exceeded;
    }
    return core::ErrorCode::internal_error;
}

} // namespace marc::frame::internal
