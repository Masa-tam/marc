#include "c_api/common.hpp"
#include "c_api/configuration.hpp"
#include "frame/lzmw_profile.hpp"
#include "frame/lzmw_frame_streaming_decoder.hpp"
#include "frame/lzmw_frame_streaming_encoder.hpp"
#include "frame/lzmw_blocked_huffman_frame_streaming_decoder.hpp"
#include "frame/lzmw_blocked_huffman_frame_streaming_encoder.hpp"
#include "frame/lzmw_blocked_huffman_profile.hpp"
#include "frame/lzmw_adaptive_huffman_frame_streaming_decoder.hpp"
#include "frame/lzmw_adaptive_huffman_frame_streaming_encoder.hpp"
#include "frame/lzmw_adaptive_huffman_profile.hpp"
#include "frame/lzmw_dynamic_range_frame_streaming_decoder.hpp"
#include "frame/lzmw_dynamic_range_frame_streaming_encoder.hpp"
#include "frame/lzmw_dynamic_range_profile.hpp"
#include "frame/lzmw_rans_frame_streaming_decoder.hpp"
#include "frame/lzmw_rans_frame_streaming_encoder.hpp"
#include "frame/lzmw_rans_profile.hpp"
#include "frame/lzmw_tans_frame_streaming_decoder.hpp"
#include "frame/lzmw_tans_frame_streaming_encoder.hpp"
#include "frame/lzmw_tans_profile.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <span>

namespace {
using marc::c_api::status_for;
using namespace marc::c_api::detail;

bool load_config(const marc_lzmw_rans_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || config->reserved != 0 || config->reserved2 != 0) {
        return false;
    }
    copy_frame_limits(*config, limits);
    limits.max_block_size = config->max_block_size;
    copy_payload_limits(*config, limits, config->max_dictionary_serialized_size);
    limits.max_dictionary_entries = config->max_dictionary_entries;
    limits.max_blocks_per_frame = config->max_blocks_per_frame;
    return true;
}

bool load_config(const marc_lzmw_tans_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || config->reserved != 0 || config->reserved2 != 0) {
        return false;
    }
    copy_frame_limits(*config, limits);
    limits.max_block_size = config->max_block_size;
    copy_payload_limits(*config, limits, config->max_dictionary_serialized_size);
    limits.max_dictionary_entries = config->max_dictionary_entries;
    limits.max_blocks_per_frame = config->max_blocks_per_frame;
    return true;
}

bool load_config(const marc_lzmw_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || config->reserved != 0 || config->reserved2 != 0) return false;
    copy_frame_limits(*config, limits);
    copy_payload_limits(*config, limits, config->max_dictionary_serialized_size);
    limits.max_dictionary_entries = config->max_dictionary_entries;
    limits.max_block_size = std::min(
        limits.max_block_size, limits.max_internal_buffered_bytes);
    return true;
}

bool load_config(const marc_lzmw_blocked_huffman_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || config->reserved != 0 || config->reserved2 != 0) return false;
    copy_frame_limits(*config, limits);
    limits.max_block_size = config->max_block_size;
    copy_payload_limits(*config, limits, config->max_dictionary_serialized_size);
    limits.max_dictionary_entries = config->max_dictionary_entries;
    limits.max_blocks_per_frame = config->max_blocks_per_frame;
    return true;
}

bool load_config(const marc_lzmw_adaptive_huffman_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || config->reserved != 0 || config->reserved2 != 0) return false;
    copy_frame_limits(*config, limits);
    copy_payload_limits(*config, limits, config->max_dictionary_serialized_size);
    limits.max_dictionary_entries = config->max_dictionary_entries;
    limits.max_block_size = std::min(
        limits.max_block_size, limits.max_internal_buffered_bytes);
    return true;
}

bool load_config(const marc_lzmw_dynamic_range_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || config->reserved != 0 || config->reserved2 != 0) return false;
    copy_frame_limits(*config, limits);
    copy_payload_limits(*config, limits, config->max_dictionary_serialized_size);
    limits.max_dictionary_entries = config->max_dictionary_entries;
    limits.max_block_size = std::min(
        limits.max_block_size, limits.max_internal_buffered_bytes);
    return true;
}

bool lzmw_decoder_views_layout(
    const marc::frame::LzmwDecoderWorkspaceRequirements& needed,
    std::size_t& expansion_offset, std::size_t& total_bytes) noexcept {
    using Phrase = marc::dictionary::internal::LzmwPhraseEntry;
    std::size_t phrase_bytes{};
    std::size_t expansion_bytes{};
    if (!marc::core::checked_multiply(
            needed.phrase_entries, sizeof(Phrase), phrase_bytes)
        || !marc::core::checked_multiply(
            needed.expansion_entries, sizeof(std::uint32_t), expansion_bytes))
        return false;
    const auto remainder = phrase_bytes % alignof(std::uint32_t);
    const auto padding = remainder == 0 ? std::size_t{0}
                                        : alignof(std::uint32_t) - remainder;
    return marc::core::checked_add(
               phrase_bytes, padding, expansion_offset)
        && marc::core::checked_add(
               expansion_offset, expansion_bytes, total_bytes);
}

} // namespace

extern "C" {

marc_status marc_lzmw_rans_config_init(
    const marc_direction direction, marc_lzmw_rans_config* config) noexcept {
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
    config->maximum_entries =
        marc::dictionary::internal::lzmw_default_maximum_entries;
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
    config->max_dictionary_entries = limits.max_dictionary_entries;
    config->max_blocks_per_frame = limits.max_blocks_per_frame;
    return MARC_STATUS_OK;
}

marc_status marc_lzmw_rans_workspace_requirements(
    const marc_lzmw_rans_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzmwParameters parameters{
            config->maximum_entries, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzmwRansEncoderWorkspaceRequirements needed{};
        const auto error = marc::frame::make_lzmw_rans_profile(
            {config->original_size, config->frame_size,
             config->entropy_block_size, parameters},
            limits, stream, needed);
        if (error != marc::frame::LzmwRansProfileError::none) {
            return status_for(
                marc::frame::lzmw_rans_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_input_bytes;
        if (!marc::core::checked_add(
                needed.dictionary_staging_bytes,
                needed.frame_encoded_bytes,
                requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::LzmwRansDecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_lzmw_rans_decoder_workspace(
                limits, needed);
        if (error != marc::frame::LzmwRansProfileError::none) {
            return status_for(
                marc::frame::lzmw_rans_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_encoded_bytes;
        if (!marc::core::checked_add(
                needed.dictionary_staging_bytes,
                needed.frame_decoded_bytes,
                requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_lzmw_rans_create(
    const marc_lzmw_rans_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query =
        marc_lzmw_rans_workspace_requirements(config, &required);
    if (query != MARC_STATUS_OK) {
        return query;
    }
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
    if (!load_config(config, limits)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    auto* const secondary =
        reinterpret_cast<std::byte*>(secondary_workspace.data);
    const auto views_storage = std::span<std::byte>{
        reinterpret_cast<std::byte*>(views_workspace.data),
        required.views_bytes};
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzmwParameters parameters{
            config->maximum_entries, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzmwRansEncoderWorkspaceRequirements needed{};
        if (marc::frame::make_lzmw_rans_profile(
                {config->original_size, config->frame_size,
                 config->entropy_block_size, parameters},
                limits, stream, needed)
            != marc::frame::LzmwRansProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzmwRansEncoderViews views{};
        if (marc::frame::partition_lzmw_rans_encoder_views(
                needed, views_storage, views)
            != marc::frame::LzmwRansWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        auto* const encoded = needed.dictionary_staging_bytes == 0
            ? secondary : secondary + needed.dictionary_staging_bytes;
        implementation = new (std::nothrow)
            marc::frame::LzmwRansFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_input_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {encoded, needed.frame_encoded_bytes}, views.entries);
    } else {
        marc::frame::LzmwRansDecoderWorkspaceRequirements needed{};
        if (marc::frame::calculate_lzmw_rans_decoder_workspace(
                limits, needed)
            != marc::frame::LzmwRansProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzmwRansDecoderViews views{};
        if (marc::frame::partition_lzmw_rans_decoder_views(
                needed, views_storage, views)
            != marc::frame::LzmwRansWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow)
            marc::frame::LzmwRansFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_encoded_bytes},
                views.blocks,
                {secondary, needed.dictionary_staging_bytes},
                {secondary + needed.dictionary_staging_bytes,
                 needed.frame_decoded_bytes},
                views.phrases, views.expansion);
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lzmw_tans_config_init(
    const marc_direction direction, marc_lzmw_tans_config* config) noexcept {
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
    config->maximum_entries =
        marc::dictionary::internal::lzmw_default_maximum_entries;
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
    config->max_dictionary_entries = limits.max_dictionary_entries;
    config->max_blocks_per_frame = limits.max_blocks_per_frame;
    return MARC_STATUS_OK;
}

marc_status marc_lzmw_tans_workspace_requirements(
    const marc_lzmw_tans_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzmwParameters parameters{
            config->maximum_entries, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzmwTansEncoderWorkspaceRequirements needed{};
        const auto error = marc::frame::make_lzmw_tans_profile(
            {config->original_size, config->frame_size,
             config->entropy_block_size, parameters},
            limits, stream, needed);
        if (error != marc::frame::LzmwTansProfileError::none) {
            return status_for(
                marc::frame::lzmw_tans_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_input_bytes;
        if (!marc::core::checked_add(
                needed.dictionary_staging_bytes,
                needed.frame_encoded_bytes,
                requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::LzmwTansDecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_lzmw_tans_decoder_workspace(
                limits, needed);
        if (error != marc::frame::LzmwTansProfileError::none) {
            return status_for(
                marc::frame::lzmw_tans_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_encoded_bytes;
        if (!marc::core::checked_add(
                needed.dictionary_staging_bytes,
                needed.frame_decoded_bytes,
                requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_lzmw_tans_create(
    const marc_lzmw_tans_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query =
        marc_lzmw_tans_workspace_requirements(config, &required);
    if (query != MARC_STATUS_OK) {
        return query;
    }
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
    if (!load_config(config, limits)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    auto* const secondary =
        reinterpret_cast<std::byte*>(secondary_workspace.data);
    const auto views_storage = std::span<std::byte>{
        reinterpret_cast<std::byte*>(views_workspace.data),
        required.views_bytes};
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzmwParameters parameters{
            config->maximum_entries, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzmwTansEncoderWorkspaceRequirements needed{};
        if (marc::frame::make_lzmw_tans_profile(
                {config->original_size, config->frame_size,
                 config->entropy_block_size, parameters},
                limits, stream, needed)
            != marc::frame::LzmwTansProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzmwTansEncoderViews views{};
        if (marc::frame::partition_lzmw_tans_encoder_views(
                needed, views_storage, views)
            != marc::frame::LzmwTansWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        auto* const encoded = needed.dictionary_staging_bytes == 0
            ? secondary : secondary + needed.dictionary_staging_bytes;
        implementation = new (std::nothrow)
            marc::frame::LzmwTansFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_input_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {encoded, needed.frame_encoded_bytes}, views.entries);
    } else {
        marc::frame::LzmwTansDecoderWorkspaceRequirements needed{};
        if (marc::frame::calculate_lzmw_tans_decoder_workspace(
                limits, needed)
            != marc::frame::LzmwTansProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzmwTansDecoderViews views{};
        if (marc::frame::partition_lzmw_tans_decoder_views(
                needed, views_storage, views)
            != marc::frame::LzmwTansWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow)
            marc::frame::LzmwTansFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_encoded_bytes},
                views.blocks,
                {secondary, needed.dictionary_staging_bytes},
                {secondary + needed.dictionary_staging_bytes,
                 needed.frame_decoded_bytes},
                views.phrases, views.expansion);
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lzmw_config_init(
    const marc_direction direction, marc_lzmw_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE))
        return MARC_STATUS_INVALID_ARGUMENT;
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 20;
    config->maximum_entries =
        marc::dictionary::internal::lzmw_default_maximum_entries;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_compressed_payload_size =
        limits.max_compressed_payload_size;
    config->max_dictionary_serialized_size =
        limits.max_dictionary_serialized_size;
    config->max_internal_buffered_bytes = limits.max_internal_buffered_bytes;
    config->max_dictionary_entries = limits.max_dictionary_entries;
    return MARC_STATUS_OK;
}

marc_status marc_lzmw_workspace_requirements(
    const marc_lzmw_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        using Entry = marc::dictionary::internal::LzmwEncoderEntry;
        marc::frame::StreamHeader stream{};
        marc::frame::LzmwEncoderWorkspaceRequirements needed{};
        const marc::dictionary::internal::LzmwParameters parameters{
            config->maximum_entries, 0, 0};
        const auto error = marc::frame::make_lzmw_profile(
            {config->original_size, config->frame_size, parameters}, limits,
            stream, needed);
        if (error != marc::frame::LzmwProfileError::none)
            return status_for(marc::frame::lzmw_profile_error_code(error));
        if (!marc::core::checked_multiply(
                needed.dictionary_entries, sizeof(Entry),
                requirements->views_bytes))
            return MARC_STATUS_LIMIT_EXCEEDED;
        requirements->primary_bytes = needed.frame_input_bytes;
        requirements->secondary_bytes = needed.frame_encoded_bytes;
        requirements->views_alignment = alignof(Entry);
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::LzmwDecoderWorkspaceRequirements needed{};
        const auto error = marc::frame::calculate_lzmw_decoder_workspace(
            limits, needed);
        if (error != marc::frame::LzmwProfileError::none)
            return status_for(marc::frame::lzmw_profile_error_code(error));
        std::size_t expansion_offset{};
        if (!lzmw_decoder_views_layout(
                needed, expansion_offset, requirements->views_bytes))
            return MARC_STATUS_LIMIT_EXCEEDED;
        requirements->primary_bytes = needed.frame_encoded_bytes;
        requirements->secondary_bytes = needed.frame_decoded_bytes;
        requirements->views_alignment = std::max(
            alignof(marc::dictionary::internal::LzmwPhraseEntry),
            alignof(std::uint32_t));
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_lzmw_create(
    const marc_lzmw_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query = marc_lzmw_workspace_requirements(config, &required);
    if (query != MARC_STATUS_OK) return query;
    if (!valid_buffer(primary_workspace.data, primary_workspace.size)
        || !valid_buffer(secondary_workspace.data, secondary_workspace.size)
        || !valid_buffer(views_workspace.data, views_workspace.size)
        || primary_workspace.size < required.primary_bytes
        || secondary_workspace.size < required.secondary_bytes
        || views_workspace.size < required.views_bytes
        || (required.views_bytes != 0
            && reinterpret_cast<std::uintptr_t>(views_workspace.data)
                % required.views_alignment != 0))
        return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        using Entry = marc::dictionary::internal::LzmwEncoderEntry;
        const marc::dictionary::internal::LzmwParameters parameters{
            config->maximum_entries, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzmwEncoderWorkspaceRequirements ignored{};
        if (marc::frame::make_lzmw_profile(
                {config->original_size, config->frame_size, parameters},
                limits, stream, ignored)
            != marc::frame::LzmwProfileError::none)
            return MARC_STATUS_INTERNAL_ERROR;
        implementation = new (std::nothrow)
            marc::frame::LzmwFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 required.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 required.secondary_bytes},
                {reinterpret_cast<Entry*>(views_workspace.data),
                 required.views_bytes / sizeof(Entry)});
    } else {
        using Phrase = marc::dictionary::internal::LzmwPhraseEntry;
        marc::frame::LzmwDecoderWorkspaceRequirements needed{};
        if (marc::frame::calculate_lzmw_decoder_workspace(limits, needed)
            != marc::frame::LzmwProfileError::none)
            return MARC_STATUS_INTERNAL_ERROR;
        std::size_t expansion_offset{};
        std::size_t ignored{};
        if (!lzmw_decoder_views_layout(needed, expansion_offset, ignored))
            return MARC_STATUS_INTERNAL_ERROR;
        auto* const views = reinterpret_cast<std::byte*>(views_workspace.data);
        implementation = new (std::nothrow)
            marc::frame::LzmwFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 required.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 required.secondary_bytes},
                {reinterpret_cast<Phrase*>(views), needed.phrase_entries},
                {reinterpret_cast<std::uint32_t*>(views + expansion_offset),
                 needed.expansion_entries});
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lzmw_blocked_huffman_config_init(
    const marc_direction direction,
    marc_lzmw_blocked_huffman_config* config) noexcept {
    if (config == nullptr
        || (direction != MARC_DIRECTION_ENCODE
            && direction != MARC_DIRECTION_DECODE)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 20;
    config->entropy_block_size = UINT32_C(1) << 16;
    config->maximum_entries =
        marc::dictionary::internal::lzmw_default_maximum_entries;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_block_size = limits.max_block_size;
    config->max_compressed_payload_size = limits.max_compressed_payload_size;
    config->max_dictionary_serialized_size =
        limits.max_dictionary_serialized_size;
    config->max_internal_buffered_bytes = limits.max_internal_buffered_bytes;
    config->max_dictionary_entries = limits.max_dictionary_entries;
    config->max_blocks_per_frame = limits.max_blocks_per_frame;
    return MARC_STATUS_OK;
}

marc_status marc_lzmw_blocked_huffman_workspace_requirements(
    const marc_lzmw_blocked_huffman_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzmwParameters parameters{
            config->maximum_entries, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzmwBlockedHuffmanEncoderWorkspaceRequirements needed{};
        const auto error = marc::frame::make_lzmw_blocked_huffman_profile(
            {config->original_size, config->frame_size,
             config->entropy_block_size, parameters},
            limits, stream, needed);
        if (error != marc::frame::LzmwBlockedHuffmanProfileError::none) {
            return status_for(
                marc::frame::lzmw_blocked_huffman_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_input_bytes;
        if (!marc::core::checked_add(needed.dictionary_staging_bytes,
                needed.frame_encoded_bytes, requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::LzmwBlockedHuffmanDecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_lzmw_blocked_huffman_decoder_workspace(
                limits, needed);
        if (error != marc::frame::LzmwBlockedHuffmanProfileError::none) {
            return status_for(
                marc::frame::lzmw_blocked_huffman_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_encoded_bytes;
        if (!marc::core::checked_add(needed.dictionary_staging_bytes,
                needed.frame_decoded_bytes, requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_lzmw_blocked_huffman_create(
    const marc_lzmw_blocked_huffman_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query = marc_lzmw_blocked_huffman_workspace_requirements(
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
                    % required.views_alignment
                != 0)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    auto* const secondary =
        reinterpret_cast<std::byte*>(secondary_workspace.data);
    const auto views_storage = std::span<std::byte>{
        reinterpret_cast<std::byte*>(views_workspace.data),
        required.views_bytes};
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzmwParameters parameters{
            config->maximum_entries, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzmwBlockedHuffmanEncoderWorkspaceRequirements needed{};
        if (marc::frame::make_lzmw_blocked_huffman_profile(
                {config->original_size, config->frame_size,
                 config->entropy_block_size, parameters},
                limits, stream, needed)
            != marc::frame::LzmwBlockedHuffmanProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzmwBlockedHuffmanEncoderViews views{};
        if (marc::frame::partition_lzmw_blocked_huffman_encoder_views(
                needed, views_storage, views)
            != marc::frame::LzmwBlockedHuffmanWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        auto* const encoded = needed.dictionary_staging_bytes == 0
            ? secondary
            : secondary + needed.dictionary_staging_bytes;
        implementation = new (std::nothrow)
            marc::frame::LzmwBlockedHuffmanFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_input_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {encoded, needed.frame_encoded_bytes}, views.entries);
    } else {
        marc::frame::LzmwBlockedHuffmanDecoderWorkspaceRequirements needed{};
        if (marc::frame::calculate_lzmw_blocked_huffman_decoder_workspace(
                limits, needed)
            != marc::frame::LzmwBlockedHuffmanProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzmwBlockedHuffmanDecoderViews views{};
        if (marc::frame::partition_lzmw_blocked_huffman_decoder_views(
                needed, views_storage, views)
            != marc::frame::LzmwBlockedHuffmanWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow)
            marc::frame::LzmwBlockedHuffmanFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_encoded_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {secondary + needed.dictionary_staging_bytes,
                 needed.frame_decoded_bytes},
                views.blocks, views.phrases, views.expansion);
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lzmw_adaptive_huffman_config_init(
    const marc_direction direction,
    marc_lzmw_adaptive_huffman_config* config) noexcept {
    if (config == nullptr
        || (direction != MARC_DIRECTION_ENCODE
            && direction != MARC_DIRECTION_DECODE)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 16;
    config->maximum_entries =
        marc::dictionary::internal::lzmw_default_maximum_entries;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_compressed_payload_size =
        limits.max_compressed_payload_size;
    config->max_dictionary_serialized_size =
        limits.max_dictionary_serialized_size;
    config->max_internal_buffered_bytes =
        limits.max_internal_buffered_bytes;
    config->max_dictionary_entries = limits.max_dictionary_entries;
    return MARC_STATUS_OK;
}

marc_status marc_lzmw_adaptive_huffman_workspace_requirements(
    const marc_lzmw_adaptive_huffman_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzmwParameters parameters{
            config->maximum_entries, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzmwAdaptiveHuffmanEncoderWorkspaceRequirements
            needed{};
        const auto error =
            marc::frame::make_lzmw_adaptive_huffman_profile(
                {config->original_size, config->frame_size, parameters},
                limits, stream, needed);
        if (error != marc::frame::LzmwAdaptiveHuffmanProfileError::none) {
            return status_for(
                marc::frame::lzmw_adaptive_huffman_profile_error_code(
                    error));
        }
        requirements->primary_bytes = needed.frame_input_bytes;
        if (!marc::core::checked_add(needed.dictionary_staging_bytes,
                needed.frame_encoded_bytes, requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::LzmwAdaptiveHuffmanDecoderWorkspaceRequirements
            needed{};
        const auto error =
            marc::frame::calculate_lzmw_adaptive_huffman_decoder_workspace(
                limits, needed);
        if (error != marc::frame::LzmwAdaptiveHuffmanProfileError::none) {
            return status_for(
                marc::frame::lzmw_adaptive_huffman_profile_error_code(
                    error));
        }
        requirements->primary_bytes = needed.frame_encoded_bytes;
        if (!marc::core::checked_add(needed.dictionary_staging_bytes,
                needed.frame_decoded_bytes, requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_lzmw_adaptive_huffman_create(
    const marc_lzmw_adaptive_huffman_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query = marc_lzmw_adaptive_huffman_workspace_requirements(
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
                    % required.views_alignment
                != 0)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    auto* const secondary =
        reinterpret_cast<std::byte*>(secondary_workspace.data);
    const auto views_storage = std::span<std::byte>{
        reinterpret_cast<std::byte*>(views_workspace.data),
        required.views_bytes};
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzmwParameters parameters{
            config->maximum_entries, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzmwAdaptiveHuffmanEncoderWorkspaceRequirements
            needed{};
        if (marc::frame::make_lzmw_adaptive_huffman_profile(
                {config->original_size, config->frame_size, parameters},
                limits, stream, needed)
            != marc::frame::LzmwAdaptiveHuffmanProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzmwAdaptiveHuffmanEncoderViews views{};
        if (marc::frame::partition_lzmw_adaptive_huffman_encoder_views(
                needed, views_storage, views)
            != marc::frame::LzmwAdaptiveHuffmanWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        auto* const encoded = needed.dictionary_staging_bytes == 0
            ? secondary : secondary + needed.dictionary_staging_bytes;
        implementation = new (std::nothrow)
            marc::frame::LzmwAdaptiveHuffmanFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_input_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {encoded, needed.frame_encoded_bytes}, views.entries);
    } else {
        marc::frame::LzmwAdaptiveHuffmanDecoderWorkspaceRequirements
            needed{};
        if (marc::frame::calculate_lzmw_adaptive_huffman_decoder_workspace(
                limits, needed)
            != marc::frame::LzmwAdaptiveHuffmanProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzmwAdaptiveHuffmanDecoderViews views{};
        if (marc::frame::partition_lzmw_adaptive_huffman_decoder_views(
                needed, views_storage, views)
            != marc::frame::LzmwAdaptiveHuffmanWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow)
            marc::frame::LzmwAdaptiveHuffmanFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_encoded_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {secondary + needed.dictionary_staging_bytes,
                 needed.frame_decoded_bytes},
                views.phrases, views.expansion);
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lzmw_dynamic_range_config_init(
    const marc_direction direction,
    marc_lzmw_dynamic_range_config* config) noexcept {
    if (config == nullptr
        || (direction != MARC_DIRECTION_ENCODE
            && direction != MARC_DIRECTION_DECODE)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 16;
    config->maximum_entries =
        marc::dictionary::internal::lzmw_default_maximum_entries;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_compressed_payload_size =
        limits.max_compressed_payload_size;
    config->max_dictionary_serialized_size =
        limits.max_dictionary_serialized_size;
    config->max_internal_buffered_bytes =
        limits.max_internal_buffered_bytes;
    config->max_dictionary_entries = limits.max_dictionary_entries;
    return MARC_STATUS_OK;
}

marc_status marc_lzmw_dynamic_range_workspace_requirements(
    const marc_lzmw_dynamic_range_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzmwParameters parameters{
            config->maximum_entries, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzmwDynamicRangeEncoderWorkspaceRequirements needed{};
        const auto error = marc::frame::make_lzmw_dynamic_range_profile(
            {config->original_size, config->frame_size, parameters},
            limits, stream, needed);
        if (error != marc::frame::LzmwDynamicRangeProfileError::none) {
            return status_for(
                marc::frame::lzmw_dynamic_range_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_input_bytes;
        if (!marc::core::checked_add(needed.dictionary_staging_bytes,
                needed.frame_encoded_bytes, requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::LzmwDynamicRangeDecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_lzmw_dynamic_range_decoder_workspace(
                limits, needed);
        if (error != marc::frame::LzmwDynamicRangeProfileError::none) {
            return status_for(
                marc::frame::lzmw_dynamic_range_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_encoded_bytes;
        if (!marc::core::checked_add(needed.dictionary_staging_bytes,
                needed.frame_decoded_bytes, requirements->secondary_bytes)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_lzmw_dynamic_range_create(
    const marc_lzmw_dynamic_range_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query = marc_lzmw_dynamic_range_workspace_requirements(
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
                    % required.views_alignment
                != 0)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    auto* const secondary =
        reinterpret_cast<std::byte*>(secondary_workspace.data);
    const auto views_storage = std::span<std::byte>{
        reinterpret_cast<std::byte*>(views_workspace.data),
        required.views_bytes};
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzmwParameters parameters{
            config->maximum_entries, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzmwDynamicRangeEncoderWorkspaceRequirements needed{};
        if (marc::frame::make_lzmw_dynamic_range_profile(
                {config->original_size, config->frame_size, parameters},
                limits, stream, needed)
            != marc::frame::LzmwDynamicRangeProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzmwDynamicRangeEncoderViews views{};
        if (marc::frame::partition_lzmw_dynamic_range_encoder_views(
                needed, views_storage, views)
            != marc::frame::LzmwDynamicRangeWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        auto* const encoded = needed.dictionary_staging_bytes == 0
            ? secondary : secondary + needed.dictionary_staging_bytes;
        implementation = new (std::nothrow)
            marc::frame::LzmwDynamicRangeFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_input_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {encoded, needed.frame_encoded_bytes}, views.entries);
    } else {
        marc::frame::LzmwDynamicRangeDecoderWorkspaceRequirements needed{};
        if (marc::frame::calculate_lzmw_dynamic_range_decoder_workspace(
                limits, needed)
            != marc::frame::LzmwDynamicRangeProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzmwDynamicRangeDecoderViews views{};
        if (marc::frame::partition_lzmw_dynamic_range_decoder_views(
                needed, views_storage, views)
            != marc::frame::LzmwDynamicRangeWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow)
            marc::frame::LzmwDynamicRangeFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_encoded_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {secondary + needed.dictionary_staging_bytes,
                 needed.frame_decoded_bytes},
                views.phrases, views.expansion);
    }
    return publish_transform(implementation, transform);
}

} // extern "C"
