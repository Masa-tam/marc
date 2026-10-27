#include "c_api/common.hpp"
#include "c_api/configuration.hpp"
#include "frame/lzss_profile.hpp"
#include "frame/lzss_streaming_decoder.hpp"
#include "frame/lzss_streaming_encoder.hpp"
#include "frame/lzss_blocked_huffman_frame_streaming_decoder.hpp"
#include "frame/lzss_blocked_huffman_frame_streaming_encoder.hpp"
#include "frame/lzss_blocked_huffman_profile.hpp"
#include "frame/lzss_adaptive_huffman_frame_streaming_decoder.hpp"
#include "frame/lzss_adaptive_huffman_frame_streaming_encoder.hpp"
#include "frame/lzss_adaptive_huffman_profile.hpp"
#include "frame/lzss_dynamic_range_frame_streaming_decoder.hpp"
#include "frame/lzss_dynamic_range_frame_streaming_encoder.hpp"
#include "frame/lzss_dynamic_range_profile.hpp"
#include "frame/lzss_rans_frame_streaming_decoder.hpp"
#include "frame/lzss_rans_frame_streaming_encoder.hpp"
#include "frame/lzss_rans_profile.hpp"
#include "frame/lzss_tans_frame_streaming_decoder.hpp"
#include "frame/lzss_tans_frame_streaming_encoder.hpp"
#include "frame/lzss_tans_profile.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <span>

namespace {
using marc::c_api::status_for;
using namespace marc::c_api::detail;

bool load_config(const marc_lzss_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || config->reserved != 0 || config->reserved2 != 0) return false;
    copy_frame_limits(*config, limits);
    copy_payload_limits(*config, limits, config->max_dictionary_serialized_size);
    copy_match_limits(*config, limits);
    limits.max_block_size = std::min(
        limits.max_block_size, limits.max_internal_buffered_bytes);
    return true;
}

bool load_config(const marc_lzss_rans_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || config->reserved != 0 || config->reserved2 != 0) return false;
    copy_frame_limits(*config, limits);
    limits.max_block_size = config->max_block_size;
    copy_payload_limits(*config, limits, config->max_dictionary_serialized_size);
    limits.max_blocks_per_frame = config->max_blocks_per_frame;
    copy_match_limits(*config, limits);
    return true;
}

bool load_config(const marc_lzss_tans_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || config->reserved != 0 || config->reserved2 != 0) return false;
    copy_frame_limits(*config, limits);
    limits.max_block_size = config->max_block_size;
    copy_payload_limits(*config, limits, config->max_dictionary_serialized_size);
    limits.max_blocks_per_frame = config->max_blocks_per_frame;
    copy_match_limits(*config, limits);
    return true;
}

bool load_config(const marc_lzss_blocked_huffman_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || config->reserved != 0 || config->reserved2 != 0) return false;
    copy_frame_limits(*config, limits);
    limits.max_block_size = config->max_block_size;
    copy_payload_limits(*config, limits, config->max_dictionary_serialized_size);
    limits.max_blocks_per_frame = config->max_blocks_per_frame;
    copy_match_limits(*config, limits);
    return true;
}

bool load_config(const marc_lzss_adaptive_huffman_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || config->reserved != 0 || config->reserved2 != 0) return false;
    copy_frame_limits(*config, limits);
    copy_payload_limits(*config, limits, config->max_dictionary_serialized_size);
    copy_match_limits(*config, limits);
    limits.max_block_size = std::min(
        limits.max_block_size, limits.max_internal_buffered_bytes);
    return true;
}

bool load_config(const marc_lzss_dynamic_range_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || config->reserved != 0 || config->reserved2 != 0) return false;
    copy_frame_limits(*config, limits);
    copy_payload_limits(*config, limits, config->max_dictionary_serialized_size);
    copy_match_limits(*config, limits);
    limits.max_block_size = std::min(
        limits.max_block_size, limits.max_internal_buffered_bytes);
    return true;
}

} // namespace

extern "C" {

marc_status marc_lzss_rans_config_init(
    const marc_direction direction,
    marc_lzss_rans_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 16;
    config->entropy_block_size = UINT32_C(1) << 16;
    config->window_size = UINT32_C(1) << 16;
    config->min_match_length =
        marc::dictionary::internal::lzss_default_min_match;
    config->max_match_length = 258;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_block_size = limits.max_block_size;
    config->max_compressed_payload_size =
        limits.max_compressed_payload_size;
    config->max_dictionary_serialized_size =
        limits.max_dictionary_serialized_size;
    config->max_internal_buffered_bytes =
        limits.max_internal_buffered_bytes;
    config->max_blocks_per_frame = limits.max_blocks_per_frame;
    config->max_lz_distance = limits.max_lz_distance;
    config->max_lz_match_length = limits.max_lz_match_length;
    return MARC_STATUS_OK;
}

marc_status marc_lzss_rans_workspace_requirements(
    const marc_lzss_rans_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::LzssRansEncoderWorkspaceRequirements needed{};
        const marc::dictionary::internal::LzssParameters parameters{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        const auto error = marc::frame::make_lzss_rans_profile(
            {config->original_size, config->frame_size,
             config->entropy_block_size, parameters},
            limits, stream, needed);
        if (error != marc::frame::LzssRansProfileError::none) {
            return status_for(
                marc::frame::lzss_rans_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_input_bytes;
        if (!marc::core::checked_add(
                needed.match_finder_bytes,
                needed.match_finder_alignment - 1,
                requirements->secondary_bytes)
            || !marc::core::checked_add(
                requirements->secondary_bytes,
                needed.dictionary_staging_bytes,
                requirements->secondary_bytes)
            || !marc::core::checked_add(
                requirements->secondary_bytes,
                needed.frame_encoded_bytes,
                requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        using View = marc::entropy::internal::RansBlockView;
        marc::frame::LzssRansDecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_lzss_rans_decoder_workspace(
                limits, needed);
        if (error != marc::frame::LzssRansProfileError::none) {
            return status_for(
                marc::frame::lzss_rans_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_encoded_bytes;
        if (!marc::core::checked_add(
                needed.dictionary_staging_bytes,
                needed.frame_decoded_bytes,
                requirements->secondary_bytes)
            || !marc::core::checked_multiply(
                needed.block_view_count, sizeof(View),
                requirements->views_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        requirements->views_alignment = alignof(View);
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_lzss_rans_create(
    const marc_lzss_rans_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query = marc_lzss_rans_workspace_requirements(
        config, &required);
    if (query != MARC_STATUS_OK) return query;
    if (!valid_buffer(primary_workspace.data, primary_workspace.size)
        || !valid_buffer(secondary_workspace.data, secondary_workspace.size)
        || !valid_buffer(views_workspace.data, views_workspace.size)
        || primary_workspace.size < required.primary_bytes
        || secondary_workspace.size < required.secondary_bytes
        || views_workspace.size < required.views_bytes
        || (required.views_bytes != 0
            && reinterpret_cast<std::uintptr_t>(views_workspace.data)
                % required.views_alignment != 0)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    auto* const secondary =
        reinterpret_cast<std::byte*>(secondary_workspace.data);
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::LzssRansEncoderWorkspaceRequirements needed{};
        const marc::dictionary::internal::LzssParameters parameters{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        if (marc::frame::make_lzss_rans_profile(
                {config->original_size, config->frame_size,
                 config->entropy_block_size, parameters},
                limits, stream, needed)
            != marc::frame::LzssRansProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        std::span<std::byte> finder_workspace{};
        std::size_t finder_extent{};
        if (!aligned_buffer_region(
                secondary_workspace, needed.match_finder_bytes,
                needed.match_finder_alignment, finder_workspace,
                finder_extent)
            || finder_extent > secondary_workspace.size
            || needed.dictionary_staging_bytes
                > secondary_workspace.size - finder_extent
            || needed.frame_encoded_bytes
                > secondary_workspace.size - finder_extent
                    - needed.dictionary_staging_bytes) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        auto* const dictionary = finder_extent == 0
            ? secondary : secondary + finder_extent;
        auto* const encoded = needed.dictionary_staging_bytes == 0
            ? dictionary
            : dictionary + needed.dictionary_staging_bytes;
        implementation = new (std::nothrow)
            marc::frame::LzssRansFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_input_bytes},
                {dictionary, needed.dictionary_staging_bytes},
                finder_workspace,
                {encoded, needed.frame_encoded_bytes});
    } else {
        using View = marc::entropy::internal::RansBlockView;
        marc::frame::LzssRansDecoderWorkspaceRequirements needed{};
        if (marc::frame::calculate_lzss_rans_decoder_workspace(
                limits, needed)
            != marc::frame::LzssRansProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        implementation = new (std::nothrow)
            marc::frame::LzssRansFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_encoded_bytes},
                {reinterpret_cast<View*>(views_workspace.data),
                 needed.block_view_count},
                {secondary, needed.dictionary_staging_bytes},
                {secondary + needed.dictionary_staging_bytes,
                 needed.frame_decoded_bytes});
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lzss_tans_config_init(
    const marc_direction direction,
    marc_lzss_tans_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 16;
    config->entropy_block_size = UINT32_C(1) << 16;
    config->window_size = UINT32_C(1) << 16;
    config->min_match_length =
        marc::dictionary::internal::lzss_default_min_match;
    config->max_match_length = 258;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_block_size = limits.max_block_size;
    config->max_compressed_payload_size =
        limits.max_compressed_payload_size;
    config->max_dictionary_serialized_size =
        limits.max_dictionary_serialized_size;
    config->max_internal_buffered_bytes =
        limits.max_internal_buffered_bytes;
    config->max_blocks_per_frame = limits.max_blocks_per_frame;
    config->max_lz_distance = limits.max_lz_distance;
    config->max_lz_match_length = limits.max_lz_match_length;
    return MARC_STATUS_OK;
}

marc_status marc_lzss_tans_workspace_requirements(
    const marc_lzss_tans_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::LzssTansEncoderWorkspaceRequirements needed{};
        const marc::dictionary::internal::LzssParameters parameters{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        const auto error = marc::frame::make_lzss_tans_profile(
            {config->original_size, config->frame_size,
             config->entropy_block_size, parameters},
            limits, stream, needed);
        if (error != marc::frame::LzssTansProfileError::none) {
            return status_for(
                marc::frame::lzss_tans_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_input_bytes;
        if (!marc::core::checked_add(
                needed.match_finder_bytes,
                needed.match_finder_alignment - 1,
                requirements->secondary_bytes)
            || !marc::core::checked_add(
                requirements->secondary_bytes,
                needed.dictionary_staging_bytes,
                requirements->secondary_bytes)
            || !marc::core::checked_add(
                requirements->secondary_bytes,
                needed.frame_encoded_bytes,
                requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        using View = marc::entropy::internal::TansBlockView;
        marc::frame::LzssTansDecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_lzss_tans_decoder_workspace(
                limits, needed);
        if (error != marc::frame::LzssTansProfileError::none) {
            return status_for(
                marc::frame::lzss_tans_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_encoded_bytes;
        if (!marc::core::checked_add(
                needed.dictionary_staging_bytes,
                needed.frame_decoded_bytes,
                requirements->secondary_bytes)
            || !marc::core::checked_multiply(
                needed.block_view_count, sizeof(View),
                requirements->views_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        requirements->views_alignment = alignof(View);
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_lzss_tans_create(
    const marc_lzss_tans_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query = marc_lzss_tans_workspace_requirements(
        config, &required);
    if (query != MARC_STATUS_OK) return query;
    if (!valid_buffer(primary_workspace.data, primary_workspace.size)
        || !valid_buffer(secondary_workspace.data, secondary_workspace.size)
        || !valid_buffer(views_workspace.data, views_workspace.size)
        || primary_workspace.size < required.primary_bytes
        || secondary_workspace.size < required.secondary_bytes
        || views_workspace.size < required.views_bytes
        || (required.views_bytes != 0
            && reinterpret_cast<std::uintptr_t>(views_workspace.data)
                % required.views_alignment != 0)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    auto* const secondary =
        reinterpret_cast<std::byte*>(secondary_workspace.data);
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::LzssTansEncoderWorkspaceRequirements needed{};
        const marc::dictionary::internal::LzssParameters parameters{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        if (marc::frame::make_lzss_tans_profile(
                {config->original_size, config->frame_size,
                 config->entropy_block_size, parameters},
                limits, stream, needed)
            != marc::frame::LzssTansProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        std::span<std::byte> finder_workspace{};
        std::size_t finder_extent{};
        if (!aligned_buffer_region(
                secondary_workspace, needed.match_finder_bytes,
                needed.match_finder_alignment, finder_workspace,
                finder_extent)
            || finder_extent > secondary_workspace.size
            || needed.dictionary_staging_bytes
                > secondary_workspace.size - finder_extent
            || needed.frame_encoded_bytes
                > secondary_workspace.size - finder_extent
                    - needed.dictionary_staging_bytes) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        auto* const dictionary = finder_extent == 0
            ? secondary : secondary + finder_extent;
        auto* const encoded = needed.dictionary_staging_bytes == 0
            ? dictionary
            : dictionary + needed.dictionary_staging_bytes;
        implementation = new (std::nothrow)
            marc::frame::LzssTansFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_input_bytes},
                {dictionary, needed.dictionary_staging_bytes},
                finder_workspace,
                {encoded, needed.frame_encoded_bytes});
    } else {
        using View = marc::entropy::internal::TansBlockView;
        marc::frame::LzssTansDecoderWorkspaceRequirements needed{};
        if (marc::frame::calculate_lzss_tans_decoder_workspace(
                limits, needed)
            != marc::frame::LzssTansProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        implementation = new (std::nothrow)
            marc::frame::LzssTansFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_encoded_bytes},
                {reinterpret_cast<View*>(views_workspace.data),
                 needed.block_view_count},
                {secondary, needed.dictionary_staging_bytes},
                {secondary + needed.dictionary_staging_bytes,
                 needed.frame_decoded_bytes});
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lzss_config_init(
    const marc_direction direction, marc_lzss_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE))
        return MARC_STATUS_INVALID_ARGUMENT;
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 20;
    config->window_size = UINT32_C(1) << 16;
    config->min_match_length = 5;
    config->max_match_length = 258;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_compressed_payload_size =
        limits.max_compressed_payload_size;
    config->max_dictionary_serialized_size =
        limits.max_dictionary_serialized_size;
    config->max_internal_buffered_bytes = limits.max_internal_buffered_bytes;
    config->max_lz_distance = limits.max_lz_distance;
    config->max_lz_match_length = limits.max_lz_match_length;
    return MARC_STATUS_OK;
}

marc_status marc_lzss_workspace_requirements(
    const marc_lzss_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::LzssEncoderWorkspaceRequirements needed{};
        const marc::dictionary::internal::LzssParameters parameters{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        const auto error = marc::frame::make_lzss_profile(
            {config->original_size, config->frame_size, parameters}, limits,
            stream, needed);
        if (error != marc::frame::LzssProfileError::none)
            return status_for(marc::frame::lzss_profile_error_code(error));
        requirements->primary_bytes = needed.frame_input_bytes;
        if (!marc::core::checked_add(
                needed.match_finder_bytes,
                needed.match_finder_alignment - 1,
                requirements->secondary_bytes)
            || !marc::core::checked_add(
                requirements->secondary_bytes,
                needed.frame_encoded_bytes,
                requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::LzssDecoderWorkspaceRequirements needed{};
        const auto error = marc::frame::calculate_lzss_decoder_workspace(
            limits, needed);
        if (error != marc::frame::LzssProfileError::none)
            return status_for(marc::frame::lzss_profile_error_code(error));
        requirements->primary_bytes = needed.frame_encoded_bytes;
        requirements->secondary_bytes = needed.frame_decoded_bytes;
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_lzss_create(
    const marc_lzss_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements needed{};
    const auto query = marc_lzss_workspace_requirements(config, &needed);
    if (query != MARC_STATUS_OK) return query;
    if (!valid_buffer(primary_workspace.data, primary_workspace.size)
        || !valid_buffer(secondary_workspace.data, secondary_workspace.size)
        || primary_workspace.size < needed.primary_bytes
        || secondary_workspace.size < needed.secondary_bytes)
        return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzssParameters parameters{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzssEncoderWorkspaceRequirements ignored{};
        if (marc::frame::make_lzss_profile(
                {config->original_size, config->frame_size, parameters},
                limits, stream, ignored)
            != marc::frame::LzssProfileError::none)
            return MARC_STATUS_INTERNAL_ERROR;
        std::span<std::byte> finder_workspace{};
        std::size_t finder_extent{};
        if (!aligned_buffer_region(
                secondary_workspace, ignored.match_finder_bytes,
                ignored.match_finder_alignment, finder_workspace,
                finder_extent)
            || finder_extent > secondary_workspace.size
            || ignored.frame_encoded_bytes
                > secondary_workspace.size - finder_extent) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        const std::span<std::byte> secondary{
            reinterpret_cast<std::byte*>(secondary_workspace.data),
            needed.secondary_bytes};
        implementation = new (std::nothrow)
            marc::frame::LzssFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                finder_workspace,
                secondary.subspan(
                    finder_extent, ignored.frame_encoded_bytes));
    } else {
        implementation = new (std::nothrow)
            marc::frame::LzssFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes});
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lzss_blocked_huffman_config_init(
    const marc_direction direction,
    marc_lzss_blocked_huffman_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 20;
    config->entropy_block_size = UINT32_C(1) << 16;
    config->window_size = UINT32_C(1) << 16;
    config->min_match_length = 5;
    config->max_match_length = 258;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_block_size = limits.max_block_size;
    config->max_compressed_payload_size =
        limits.max_compressed_payload_size;
    config->max_dictionary_serialized_size =
        limits.max_dictionary_serialized_size;
    config->max_internal_buffered_bytes =
        limits.max_internal_buffered_bytes;
    config->max_blocks_per_frame = limits.max_blocks_per_frame;
    config->max_lz_distance = limits.max_lz_distance;
    config->max_lz_match_length = limits.max_lz_match_length;
    return MARC_STATUS_OK;
}

marc_status marc_lzss_blocked_huffman_workspace_requirements(
    const marc_lzss_blocked_huffman_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::LzssBlockedHuffmanEncoderWorkspaceRequirements needed{};
        const marc::dictionary::internal::LzssParameters parameters{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        const auto error =
            marc::frame::make_lzss_blocked_huffman_profile(
                {config->original_size, config->frame_size,
                 config->entropy_block_size, parameters},
                limits, stream, needed);
        if (error
            != marc::frame::LzssBlockedHuffmanProfileError::none) {
            return status_for(
                marc::frame::lzss_blocked_huffman_profile_error_code(
                    error));
        }
        requirements->primary_bytes = needed.frame_input_bytes;
        if (!marc::core::checked_add(
                needed.dictionary_staging_bytes,
                needed.frame_encoded_bytes,
                requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        requirements->views_bytes = needed.match_finder_bytes;
        requirements->views_alignment = needed.match_finder_alignment;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        using View = marc::entropy::internal::BlockedHuffmanBlockView;
        marc::frame::LzssBlockedHuffmanDecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_lzss_blocked_huffman_decoder_workspace(
                limits, needed);
        if (error
            != marc::frame::LzssBlockedHuffmanProfileError::none) {
            return status_for(
                marc::frame::lzss_blocked_huffman_profile_error_code(
                    error));
        }
        requirements->primary_bytes = needed.frame_encoded_bytes;
        if (!marc::core::checked_add(
                needed.dictionary_staging_bytes,
                needed.frame_decoded_bytes,
                requirements->secondary_bytes)
            || !marc::core::checked_multiply(
                needed.block_view_count, sizeof(View),
                requirements->views_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        requirements->views_alignment = alignof(View);
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_lzss_blocked_huffman_create(
    const marc_lzss_blocked_huffman_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query = marc_lzss_blocked_huffman_workspace_requirements(
        config, &required);
    if (query != MARC_STATUS_OK) return query;
    if (!valid_buffer(primary_workspace.data, primary_workspace.size)
        || !valid_buffer(secondary_workspace.data, secondary_workspace.size)
        || !valid_buffer(views_workspace.data, views_workspace.size)
        || primary_workspace.size < required.primary_bytes
        || secondary_workspace.size < required.secondary_bytes
        || views_workspace.size < required.views_bytes
        || (required.views_bytes != 0
            && reinterpret_cast<std::uintptr_t>(views_workspace.data)
                % required.views_alignment != 0)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    auto* const secondary =
        reinterpret_cast<std::byte*>(secondary_workspace.data);
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::LzssBlockedHuffmanEncoderWorkspaceRequirements needed{};
        const marc::dictionary::internal::LzssParameters parameters{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        if (marc::frame::make_lzss_blocked_huffman_profile(
                {config->original_size, config->frame_size,
                 config->entropy_block_size, parameters},
                limits, stream, needed)
            != marc::frame::LzssBlockedHuffmanProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        auto* const encoded = needed.dictionary_staging_bytes == 0
            ? secondary : secondary + needed.dictionary_staging_bytes;
        implementation = new (std::nothrow)
            marc::frame::LzssBlockedHuffmanFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_input_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {reinterpret_cast<std::byte*>(views_workspace.data),
                 needed.match_finder_bytes},
                {encoded, needed.frame_encoded_bytes});
    } else {
        using View = marc::entropy::internal::BlockedHuffmanBlockView;
        marc::frame::LzssBlockedHuffmanDecoderWorkspaceRequirements needed{};
        if (marc::frame::calculate_lzss_blocked_huffman_decoder_workspace(
                limits, needed)
            != marc::frame::LzssBlockedHuffmanProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        implementation = new (std::nothrow)
            marc::frame::LzssBlockedHuffmanFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_encoded_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {secondary + needed.dictionary_staging_bytes,
                 needed.frame_decoded_bytes},
                {reinterpret_cast<View*>(views_workspace.data),
                 needed.block_view_count});
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lzss_adaptive_huffman_config_init(
    const marc_direction direction,
    marc_lzss_adaptive_huffman_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 16;
    config->window_size = UINT32_C(1) << 16;
    config->min_match_length = 5;
    config->max_match_length = 258;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_compressed_payload_size =
        limits.max_compressed_payload_size;
    config->max_dictionary_serialized_size =
        limits.max_dictionary_serialized_size;
    config->max_internal_buffered_bytes =
        limits.max_internal_buffered_bytes;
    config->max_lz_distance = limits.max_lz_distance;
    config->max_lz_match_length = limits.max_lz_match_length;
    return MARC_STATUS_OK;
}

marc_status marc_lzss_adaptive_huffman_workspace_requirements(
    const marc_lzss_adaptive_huffman_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::LzssAdaptiveHuffmanEncoderWorkspaceRequirements needed{};
        const marc::dictionary::internal::LzssParameters parameters{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        const auto error =
            marc::frame::make_lzss_adaptive_huffman_profile(
                {config->original_size, config->frame_size, parameters},
                limits, stream, needed);
        if (error
            != marc::frame::LzssAdaptiveHuffmanProfileError::none) {
            return status_for(
                marc::frame::lzss_adaptive_huffman_profile_error_code(
                    error));
        }
        requirements->primary_bytes = needed.frame_input_bytes;
        if (!marc::core::checked_add(
                needed.match_finder_bytes,
                needed.match_finder_alignment - 1,
                requirements->secondary_bytes)
            || !marc::core::checked_add(
                requirements->secondary_bytes,
                needed.dictionary_staging_bytes,
                requirements->secondary_bytes)
            || !marc::core::checked_add(
                requirements->secondary_bytes,
                needed.frame_encoded_bytes,
                requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::LzssAdaptiveHuffmanDecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_lzss_adaptive_huffman_decoder_workspace(
                limits, needed);
        if (error
            != marc::frame::LzssAdaptiveHuffmanProfileError::none) {
            return status_for(
                marc::frame::lzss_adaptive_huffman_profile_error_code(
                    error));
        }
        requirements->primary_bytes = needed.frame_encoded_bytes;
        if (!marc::core::checked_add(
                needed.dictionary_staging_bytes,
                needed.frame_decoded_bytes,
                requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_lzss_adaptive_huffman_create(
    const marc_lzss_adaptive_huffman_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query = marc_lzss_adaptive_huffman_workspace_requirements(
        config, &required);
    if (query != MARC_STATUS_OK) return query;
    if (!valid_buffer(primary_workspace.data, primary_workspace.size)
        || !valid_buffer(secondary_workspace.data, secondary_workspace.size)
        || primary_workspace.size < required.primary_bytes
        || secondary_workspace.size < required.secondary_bytes) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    auto* const secondary =
        reinterpret_cast<std::byte*>(secondary_workspace.data);
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::LzssAdaptiveHuffmanEncoderWorkspaceRequirements needed{};
        const marc::dictionary::internal::LzssParameters parameters{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        if (marc::frame::make_lzss_adaptive_huffman_profile(
                {config->original_size, config->frame_size, parameters},
                limits, stream, needed)
            != marc::frame::LzssAdaptiveHuffmanProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        std::span<std::byte> finder_workspace{};
        std::size_t finder_extent{};
        if (!aligned_buffer_region(
                secondary_workspace, needed.match_finder_bytes,
                needed.match_finder_alignment, finder_workspace,
                finder_extent)
            || finder_extent > secondary_workspace.size
            || needed.dictionary_staging_bytes
                > secondary_workspace.size - finder_extent
            || needed.frame_encoded_bytes
                > secondary_workspace.size - finder_extent
                    - needed.dictionary_staging_bytes) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        auto* const dictionary = finder_extent == 0
            ? secondary : secondary + finder_extent;
        auto* const encoded = needed.dictionary_staging_bytes == 0
            ? dictionary : dictionary + needed.dictionary_staging_bytes;
        implementation = new (std::nothrow)
            marc::frame::LzssAdaptiveHuffmanFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_input_bytes},
                {dictionary, needed.dictionary_staging_bytes},
                finder_workspace,
                {encoded, needed.frame_encoded_bytes});
    } else {
        marc::frame::LzssAdaptiveHuffmanDecoderWorkspaceRequirements needed{};
        if (marc::frame::calculate_lzss_adaptive_huffman_decoder_workspace(
                limits, needed)
            != marc::frame::LzssAdaptiveHuffmanProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        implementation = new (std::nothrow)
            marc::frame::LzssAdaptiveHuffmanFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_encoded_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {secondary + needed.dictionary_staging_bytes,
                 needed.frame_decoded_bytes});
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lzss_dynamic_range_config_init(
    const marc_direction direction,
    marc_lzss_dynamic_range_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 16;
    config->window_size = UINT32_C(1) << 16;
    config->min_match_length = 5;
    config->max_match_length = 258;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_compressed_payload_size =
        limits.max_compressed_payload_size;
    config->max_dictionary_serialized_size =
        limits.max_dictionary_serialized_size;
    config->max_internal_buffered_bytes =
        limits.max_internal_buffered_bytes;
    config->max_lz_distance = limits.max_lz_distance;
    config->max_lz_match_length = limits.max_lz_match_length;
    return MARC_STATUS_OK;
}

marc_status marc_lzss_dynamic_range_workspace_requirements(
    const marc_lzss_dynamic_range_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::LzssDynamicRangeEncoderWorkspaceRequirements needed{};
        const marc::dictionary::internal::LzssParameters parameters{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        const auto error = marc::frame::make_lzss_dynamic_range_profile(
            {config->original_size, config->frame_size, parameters},
            limits, stream, needed);
        if (error != marc::frame::LzssDynamicRangeProfileError::none) {
            return status_for(
                marc::frame::lzss_dynamic_range_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_input_bytes;
        if (!marc::core::checked_add(
                needed.match_finder_bytes,
                needed.match_finder_alignment - 1,
                requirements->secondary_bytes)
            || !marc::core::checked_add(
                requirements->secondary_bytes,
                needed.dictionary_staging_bytes,
                requirements->secondary_bytes)
            || !marc::core::checked_add(
                requirements->secondary_bytes,
                needed.frame_encoded_bytes,
                requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::LzssDynamicRangeDecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_lzss_dynamic_range_decoder_workspace(
                limits, needed);
        if (error != marc::frame::LzssDynamicRangeProfileError::none) {
            return status_for(
                marc::frame::lzss_dynamic_range_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_encoded_bytes;
        if (!marc::core::checked_add(
                needed.dictionary_staging_bytes,
                needed.frame_decoded_bytes,
                requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_lzss_dynamic_range_create(
    const marc_lzss_dynamic_range_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query = marc_lzss_dynamic_range_workspace_requirements(
        config, &required);
    if (query != MARC_STATUS_OK) return query;
    if (!valid_buffer(primary_workspace.data, primary_workspace.size)
        || !valid_buffer(secondary_workspace.data, secondary_workspace.size)
        || primary_workspace.size < required.primary_bytes
        || secondary_workspace.size < required.secondary_bytes) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    auto* const secondary =
        reinterpret_cast<std::byte*>(secondary_workspace.data);
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::LzssDynamicRangeEncoderWorkspaceRequirements needed{};
        const marc::dictionary::internal::LzssParameters parameters{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        if (marc::frame::make_lzss_dynamic_range_profile(
                {config->original_size, config->frame_size, parameters},
                limits, stream, needed)
            != marc::frame::LzssDynamicRangeProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        std::span<std::byte> finder_workspace{};
        std::size_t finder_extent{};
        if (!aligned_buffer_region(
                secondary_workspace, needed.match_finder_bytes,
                needed.match_finder_alignment, finder_workspace,
                finder_extent)
            || finder_extent > secondary_workspace.size
            || needed.dictionary_staging_bytes
                > secondary_workspace.size - finder_extent
            || needed.frame_encoded_bytes
                > secondary_workspace.size - finder_extent
                    - needed.dictionary_staging_bytes) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        auto* const dictionary = finder_extent == 0
            ? secondary : secondary + finder_extent;
        auto* const encoded = needed.dictionary_staging_bytes == 0
            ? dictionary : dictionary + needed.dictionary_staging_bytes;
        implementation = new (std::nothrow)
            marc::frame::LzssDynamicRangeFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_input_bytes},
                {dictionary, needed.dictionary_staging_bytes},
                finder_workspace,
                {encoded, needed.frame_encoded_bytes});
    } else {
        marc::frame::LzssDynamicRangeDecoderWorkspaceRequirements needed{};
        if (marc::frame::calculate_lzss_dynamic_range_decoder_workspace(
                limits, needed)
            != marc::frame::LzssDynamicRangeProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        implementation = new (std::nothrow)
            marc::frame::LzssDynamicRangeFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_encoded_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {secondary + needed.dictionary_staging_bytes,
                 needed.frame_decoded_bytes});
    }
    return publish_transform(implementation, transform);
}

} // extern "C"
