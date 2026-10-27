#include "c_api/common.hpp"
#include "frame/lz78_profile.hpp"
#include "frame/lz78_streaming_decoder.hpp"
#include "frame/lz78_streaming_encoder.hpp"
#include "frame/lz78_blocked_huffman_frame_streaming_decoder.hpp"
#include "frame/lz78_blocked_huffman_frame_streaming_encoder.hpp"
#include "frame/lz78_blocked_huffman_profile.hpp"
#include "frame/lz78_adaptive_huffman_frame_streaming_decoder.hpp"
#include "frame/lz78_adaptive_huffman_frame_streaming_encoder.hpp"
#include "frame/lz78_adaptive_huffman_profile.hpp"
#include "frame/lz78_dynamic_range_frame_streaming_decoder.hpp"
#include "frame/lz78_dynamic_range_frame_streaming_encoder.hpp"
#include "frame/lz78_dynamic_range_profile.hpp"
#include "frame/lz78_rans_frame_streaming_decoder.hpp"
#include "frame/lz78_rans_frame_streaming_encoder.hpp"
#include "frame/lz78_rans_profile.hpp"
#include "frame/lz78_tans_frame_streaming_decoder.hpp"
#include "frame/lz78_tans_frame_streaming_encoder.hpp"
#include "frame/lz78_tans_profile.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <span>

namespace {
using marc::c_api::status_for;
using namespace marc::c_api::detail;

bool load_config(const marc_lz78_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (config == nullptr
        || config->struct_size != sizeof(marc_lz78_config)
        || config->abi_version != MARC_ABI_VERSION
        || config->reserved != 0 || config->reserved2 != 0) return false;
    limits.max_total_output_size = config->max_total_output_size;
    limits.max_frame_size = config->max_frame_size;
    limits.max_compressed_payload_size = config->max_compressed_payload_size;
    limits.max_dictionary_serialized_size =
        config->max_dictionary_serialized_size;
    limits.max_internal_buffered_bytes = config->max_internal_buffered_bytes;
    limits.max_dictionary_entries = config->max_dictionary_entries;
    limits.max_block_size = std::min(
        limits.max_block_size, limits.max_internal_buffered_bytes);
    return true;
}

bool load_config(const marc_lz78_blocked_huffman_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (config == nullptr
        || config->struct_size
            != sizeof(marc_lz78_blocked_huffman_config)
        || config->abi_version != MARC_ABI_VERSION
        || config->reserved != 0 || config->reserved2 != 0) return false;
    limits.max_total_output_size = config->max_total_output_size;
    limits.max_frame_size = config->max_frame_size;
    limits.max_block_size = config->max_block_size;
    limits.max_compressed_payload_size =
        config->max_compressed_payload_size;
    limits.max_dictionary_serialized_size =
        config->max_dictionary_serialized_size;
    limits.max_internal_buffered_bytes =
        config->max_internal_buffered_bytes;
    limits.max_dictionary_entries = config->max_dictionary_entries;
    limits.max_blocks_per_frame = config->max_blocks_per_frame;
    return true;
}

bool load_config(const marc_lz78_adaptive_huffman_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (config == nullptr
        || config->struct_size
            != sizeof(marc_lz78_adaptive_huffman_config)
        || config->abi_version != MARC_ABI_VERSION
        || config->reserved != 0 || config->reserved2 != 0) return false;
    limits.max_total_output_size = config->max_total_output_size;
    limits.max_frame_size = config->max_frame_size;
    limits.max_compressed_payload_size =
        config->max_compressed_payload_size;
    limits.max_dictionary_serialized_size =
        config->max_dictionary_serialized_size;
    limits.max_internal_buffered_bytes =
        config->max_internal_buffered_bytes;
    limits.max_dictionary_entries = config->max_dictionary_entries;
    limits.max_block_size = std::min(
        limits.max_block_size, limits.max_internal_buffered_bytes);
    return true;
}

bool load_config(const marc_lz78_dynamic_range_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (config == nullptr
        || config->struct_size
            != sizeof(marc_lz78_dynamic_range_config)
        || config->abi_version != MARC_ABI_VERSION
        || config->reserved != 0 || config->reserved2 != 0) return false;
    limits.max_total_output_size = config->max_total_output_size;
    limits.max_frame_size = config->max_frame_size;
    limits.max_compressed_payload_size =
        config->max_compressed_payload_size;
    limits.max_dictionary_serialized_size =
        config->max_dictionary_serialized_size;
    limits.max_internal_buffered_bytes =
        config->max_internal_buffered_bytes;
    limits.max_dictionary_entries = config->max_dictionary_entries;
    limits.max_block_size = std::min(
        limits.max_block_size, limits.max_internal_buffered_bytes);
    return true;
}

bool load_config(const marc_lz78_rans_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (config == nullptr
        || config->struct_size != sizeof(marc_lz78_rans_config)
        || config->abi_version != MARC_ABI_VERSION
        || config->reserved != 0 || config->reserved2 != 0) {
        return false;
    }
    limits.max_total_output_size = config->max_total_output_size;
    limits.max_frame_size = config->max_frame_size;
    limits.max_block_size = config->max_block_size;
    limits.max_compressed_payload_size =
        config->max_compressed_payload_size;
    limits.max_dictionary_serialized_size =
        config->max_dictionary_serialized_size;
    limits.max_internal_buffered_bytes =
        config->max_internal_buffered_bytes;
    limits.max_dictionary_entries = config->max_dictionary_entries;
    limits.max_blocks_per_frame = config->max_blocks_per_frame;
    return true;
}

bool load_config(const marc_lz78_tans_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (config == nullptr
        || config->struct_size != sizeof(marc_lz78_tans_config)
        || config->abi_version != MARC_ABI_VERSION
        || config->reserved != 0 || config->reserved2 != 0) {
        return false;
    }
    limits.max_total_output_size = config->max_total_output_size;
    limits.max_frame_size = config->max_frame_size;
    limits.max_block_size = config->max_block_size;
    limits.max_compressed_payload_size =
        config->max_compressed_payload_size;
    limits.max_dictionary_serialized_size =
        config->max_dictionary_serialized_size;
    limits.max_internal_buffered_bytes =
        config->max_internal_buffered_bytes;
    limits.max_dictionary_entries = config->max_dictionary_entries;
    limits.max_blocks_per_frame = config->max_blocks_per_frame;
    return true;
}

} // namespace

extern "C" {

marc_status marc_lz78_config_init(
    const marc_direction direction, marc_lz78_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE))
        return MARC_STATUS_INVALID_ARGUMENT;
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 20;
    config->maximum_entries = UINT32_C(1) << 16;
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

marc_status marc_lz78_workspace_requirements(
    const marc_lz78_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        using Entry = marc::dictionary::internal::Lz78EncoderEntry;
        marc::frame::StreamHeader stream{};
        marc::frame::Lz78EncoderWorkspaceRequirements needed{};
        const marc::dictionary::internal::Lz78Parameters parameters{
            config->maximum_entries, 0};
        const auto error = marc::frame::make_lz78_profile(
            {config->original_size, config->frame_size, parameters}, limits,
            stream, needed);
        if (error != marc::frame::Lz78ProfileError::none)
            return status_for(marc::frame::lz78_profile_error_code(error));
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
        using Entry = marc::dictionary::internal::Lz78PhraseEntry;
        marc::frame::Lz78DecoderWorkspaceRequirements needed{};
        const auto error = marc::frame::calculate_lz78_decoder_workspace(
            limits, needed);
        if (error != marc::frame::Lz78ProfileError::none)
            return status_for(marc::frame::lz78_profile_error_code(error));
        if (!marc::core::checked_multiply(
                needed.dictionary_entries, sizeof(Entry),
                requirements->views_bytes))
            return MARC_STATUS_LIMIT_EXCEEDED;
        requirements->primary_bytes = needed.frame_encoded_bytes;
        requirements->secondary_bytes = needed.frame_decoded_bytes;
        requirements->views_alignment = alignof(Entry);
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_lz78_create(
    const marc_lz78_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements needed{};
    const auto query = marc_lz78_workspace_requirements(config, &needed);
    if (query != MARC_STATUS_OK) return query;
    if (!valid_buffer(primary_workspace.data, primary_workspace.size)
        || !valid_buffer(secondary_workspace.data, secondary_workspace.size)
        || !valid_buffer(views_workspace.data, views_workspace.size)
        || primary_workspace.size < needed.primary_bytes
        || secondary_workspace.size < needed.secondary_bytes
        || views_workspace.size < needed.views_bytes
        || (needed.views_bytes != 0
            && reinterpret_cast<std::uintptr_t>(views_workspace.data)
                % needed.views_alignment != 0))
        return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        using Entry = marc::dictionary::internal::Lz78EncoderEntry;
        const marc::dictionary::internal::Lz78Parameters parameters{
            config->maximum_entries, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::Lz78EncoderWorkspaceRequirements ignored{};
        if (marc::frame::make_lz78_profile(
                {config->original_size, config->frame_size, parameters},
                limits, stream, ignored)
            != marc::frame::Lz78ProfileError::none)
            return MARC_STATUS_INTERNAL_ERROR;
        implementation = new (std::nothrow)
            marc::frame::Lz78FrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes},
                {reinterpret_cast<Entry*>(views_workspace.data),
                 needed.views_bytes / sizeof(Entry)});
    } else {
        using Entry = marc::dictionary::internal::Lz78PhraseEntry;
        implementation = new (std::nothrow)
            marc::frame::Lz78FrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes},
                {reinterpret_cast<Entry*>(views_workspace.data),
                 needed.views_bytes / sizeof(Entry)});
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lz78_blocked_huffman_config_init(
    const marc_direction direction,
    marc_lz78_blocked_huffman_config* config) noexcept {
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
    config->maximum_entries = UINT32_C(1) << 16;
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

marc_status marc_lz78_blocked_huffman_workspace_requirements(
    const marc_lz78_blocked_huffman_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::Lz78Parameters parameters{
            config->maximum_entries, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::Lz78BlockedHuffmanEncoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::make_lz78_blocked_huffman_profile(
                {config->original_size, config->frame_size,
                 config->entropy_block_size, parameters},
                limits, stream, needed);
        if (error != marc::frame::Lz78BlockedHuffmanProfileError::none) {
            return status_for(
                marc::frame::lz78_blocked_huffman_profile_error_code(
                    error));
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
        marc::frame::Lz78BlockedHuffmanDecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_lz78_blocked_huffman_decoder_workspace(
                limits, needed);
        if (error != marc::frame::Lz78BlockedHuffmanProfileError::none) {
            return status_for(
                marc::frame::lz78_blocked_huffman_profile_error_code(
                    error));
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

marc_status marc_lz78_blocked_huffman_create(
    const marc_lz78_blocked_huffman_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query = marc_lz78_blocked_huffman_workspace_requirements(
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
    const auto views_storage = std::span<std::byte>{
        reinterpret_cast<std::byte*>(views_workspace.data),
        required.views_bytes};
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::Lz78Parameters parameters{
            config->maximum_entries, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::Lz78BlockedHuffmanEncoderWorkspaceRequirements needed{};
        if (marc::frame::make_lz78_blocked_huffman_profile(
                {config->original_size, config->frame_size,
                 config->entropy_block_size, parameters},
                limits, stream, needed)
            != marc::frame::Lz78BlockedHuffmanProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::Lz78BlockedHuffmanEncoderViews views{};
        if (marc::frame::partition_lz78_blocked_huffman_encoder_views(
                needed, views_storage, views)
            != marc::frame::Lz78BlockedHuffmanWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        auto* const encoded = needed.dictionary_staging_bytes == 0
            ? secondary : secondary + needed.dictionary_staging_bytes;
        implementation = new (std::nothrow)
            marc::frame::Lz78BlockedHuffmanFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_input_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {encoded, needed.frame_encoded_bytes}, views.entries);
    } else {
        marc::frame::Lz78BlockedHuffmanDecoderWorkspaceRequirements needed{};
        if (marc::frame::calculate_lz78_blocked_huffman_decoder_workspace(
                limits, needed)
            != marc::frame::Lz78BlockedHuffmanProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::Lz78BlockedHuffmanDecoderViews views{};
        if (marc::frame::partition_lz78_blocked_huffman_decoder_views(
                needed, views_storage, views)
            != marc::frame::Lz78BlockedHuffmanWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow)
            marc::frame::Lz78BlockedHuffmanFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_encoded_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {secondary + needed.dictionary_staging_bytes,
                 needed.frame_decoded_bytes},
                views.blocks, views.phrases);
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lz78_adaptive_huffman_config_init(
    const marc_direction direction,
    marc_lz78_adaptive_huffman_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 16;
    config->maximum_entries = UINT32_C(1) << 16;
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

marc_status marc_lz78_adaptive_huffman_workspace_requirements(
    const marc_lz78_adaptive_huffman_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::Lz78Parameters parameters{
            config->maximum_entries, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::Lz78AdaptiveHuffmanEncoderWorkspaceRequirements
            needed{};
        const auto error =
            marc::frame::make_lz78_adaptive_huffman_profile(
                {config->original_size, config->frame_size, parameters},
                limits, stream, needed);
        if (error
            != marc::frame::Lz78AdaptiveHuffmanProfileError::none) {
            return status_for(
                marc::frame::lz78_adaptive_huffman_profile_error_code(
                    error));
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
        marc::frame::Lz78AdaptiveHuffmanDecoderWorkspaceRequirements
            needed{};
        const auto error =
            marc::frame::calculate_lz78_adaptive_huffman_decoder_workspace(
                limits, needed);
        if (error
            != marc::frame::Lz78AdaptiveHuffmanProfileError::none) {
            return status_for(
                marc::frame::lz78_adaptive_huffman_profile_error_code(
                    error));
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

marc_status marc_lz78_adaptive_huffman_create(
    const marc_lz78_adaptive_huffman_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query = marc_lz78_adaptive_huffman_workspace_requirements(
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
    const auto views_storage = std::span<std::byte>{
        reinterpret_cast<std::byte*>(views_workspace.data),
        required.views_bytes};
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::Lz78Parameters parameters{
            config->maximum_entries, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::Lz78AdaptiveHuffmanEncoderWorkspaceRequirements
            needed{};
        if (marc::frame::make_lz78_adaptive_huffman_profile(
                {config->original_size, config->frame_size, parameters},
                limits, stream, needed)
            != marc::frame::Lz78AdaptiveHuffmanProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::Lz78AdaptiveHuffmanEncoderViews views{};
        if (marc::frame::partition_lz78_adaptive_huffman_encoder_views(
                needed, views_storage, views)
            != marc::frame::Lz78AdaptiveHuffmanWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        auto* const encoded = needed.dictionary_staging_bytes == 0
            ? secondary : secondary + needed.dictionary_staging_bytes;
        implementation = new (std::nothrow)
            marc::frame::Lz78AdaptiveHuffmanFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_input_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {encoded, needed.frame_encoded_bytes}, views.entries);
    } else {
        marc::frame::Lz78AdaptiveHuffmanDecoderWorkspaceRequirements
            needed{};
        if (marc::frame::calculate_lz78_adaptive_huffman_decoder_workspace(
                limits, needed)
            != marc::frame::Lz78AdaptiveHuffmanProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::Lz78AdaptiveHuffmanDecoderViews views{};
        if (marc::frame::partition_lz78_adaptive_huffman_decoder_views(
                needed, views_storage, views)
            != marc::frame::Lz78AdaptiveHuffmanWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow)
            marc::frame::Lz78AdaptiveHuffmanFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_encoded_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {secondary + needed.dictionary_staging_bytes,
                 needed.frame_decoded_bytes}, views.phrases);
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lz78_dynamic_range_config_init(
    const marc_direction direction,
    marc_lz78_dynamic_range_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 16;
    config->maximum_entries = UINT32_C(1) << 16;
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

marc_status marc_lz78_dynamic_range_workspace_requirements(
    const marc_lz78_dynamic_range_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::Lz78Parameters parameters{
            config->maximum_entries, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::Lz78DynamicRangeEncoderWorkspaceRequirements
            needed{};
        const auto error =
            marc::frame::make_lz78_dynamic_range_profile(
                {config->original_size, config->frame_size, parameters},
                limits, stream, needed);
        if (error != marc::frame::Lz78DynamicRangeProfileError::none) {
            return status_for(
                marc::frame::lz78_dynamic_range_profile_error_code(
                    error));
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
        marc::frame::Lz78DynamicRangeDecoderWorkspaceRequirements
            needed{};
        const auto error =
            marc::frame::calculate_lz78_dynamic_range_decoder_workspace(
                limits, needed);
        if (error != marc::frame::Lz78DynamicRangeProfileError::none) {
            return status_for(
                marc::frame::lz78_dynamic_range_profile_error_code(
                    error));
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

marc_status marc_lz78_dynamic_range_create(
    const marc_lz78_dynamic_range_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query = marc_lz78_dynamic_range_workspace_requirements(
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
    const auto views_storage = std::span<std::byte>{
        reinterpret_cast<std::byte*>(views_workspace.data),
        required.views_bytes};
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::Lz78Parameters parameters{
            config->maximum_entries, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::Lz78DynamicRangeEncoderWorkspaceRequirements
            needed{};
        if (marc::frame::make_lz78_dynamic_range_profile(
                {config->original_size, config->frame_size, parameters},
                limits, stream, needed)
            != marc::frame::Lz78DynamicRangeProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::Lz78DynamicRangeEncoderViews views{};
        if (marc::frame::partition_lz78_dynamic_range_encoder_views(
                needed, views_storage, views)
            != marc::frame::Lz78DynamicRangeWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        auto* const encoded = needed.dictionary_staging_bytes == 0
            ? secondary : secondary + needed.dictionary_staging_bytes;
        implementation = new (std::nothrow)
            marc::frame::Lz78DynamicRangeFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_input_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {encoded, needed.frame_encoded_bytes}, views.entries);
    } else {
        marc::frame::Lz78DynamicRangeDecoderWorkspaceRequirements
            needed{};
        if (marc::frame::calculate_lz78_dynamic_range_decoder_workspace(
                limits, needed)
            != marc::frame::Lz78DynamicRangeProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::Lz78DynamicRangeDecoderViews views{};
        if (marc::frame::partition_lz78_dynamic_range_decoder_views(
                needed, views_storage, views)
            != marc::frame::Lz78DynamicRangeWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow)
            marc::frame::Lz78DynamicRangeFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_encoded_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {secondary + needed.dictionary_staging_bytes,
                 needed.frame_decoded_bytes}, views.phrases);
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lz78_rans_config_init(
    const marc_direction direction,
    marc_lz78_rans_config* config) noexcept {
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
    config->maximum_entries = UINT32_C(1) << 16;
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

marc_status marc_lz78_rans_workspace_requirements(
    const marc_lz78_rans_config* config,
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
        const marc::dictionary::internal::Lz78Parameters parameters{
            config->maximum_entries, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::Lz78RansEncoderWorkspaceRequirements needed{};
        const auto error = marc::frame::make_lz78_rans_profile(
            {config->original_size, config->frame_size,
             config->entropy_block_size, parameters},
            limits, stream, needed);
        if (error != marc::frame::Lz78RansProfileError::none) {
            return status_for(
                marc::frame::lz78_rans_profile_error_code(error));
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
        marc::frame::Lz78RansDecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_lz78_rans_decoder_workspace(
                limits, needed);
        if (error != marc::frame::Lz78RansProfileError::none) {
            return status_for(
                marc::frame::lz78_rans_profile_error_code(error));
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

marc_status marc_lz78_rans_create(
    const marc_lz78_rans_config* config,
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
        marc_lz78_rans_workspace_requirements(config, &required);
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
        const marc::dictionary::internal::Lz78Parameters parameters{
            config->maximum_entries, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::Lz78RansEncoderWorkspaceRequirements needed{};
        if (marc::frame::make_lz78_rans_profile(
                {config->original_size, config->frame_size,
                 config->entropy_block_size, parameters},
                limits, stream, needed)
            != marc::frame::Lz78RansProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::Lz78RansEncoderViews views{};
        if (marc::frame::partition_lz78_rans_encoder_views(
                needed, views_storage, views)
            != marc::frame::Lz78RansWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        auto* const encoded = needed.dictionary_staging_bytes == 0
            ? secondary : secondary + needed.dictionary_staging_bytes;
        implementation = new (std::nothrow)
            marc::frame::Lz78RansFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_input_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {encoded, needed.frame_encoded_bytes}, views.entries);
    } else {
        marc::frame::Lz78RansDecoderWorkspaceRequirements needed{};
        if (marc::frame::calculate_lz78_rans_decoder_workspace(
                limits, needed)
            != marc::frame::Lz78RansProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::Lz78RansDecoderViews views{};
        if (marc::frame::partition_lz78_rans_decoder_views(
                needed, views_storage, views)
            != marc::frame::Lz78RansWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow)
            marc::frame::Lz78RansFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_encoded_bytes},
                views.blocks,
                {secondary, needed.dictionary_staging_bytes},
                {secondary + needed.dictionary_staging_bytes,
                 needed.frame_decoded_bytes},
                views.phrases);
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lz78_tans_config_init(
    const marc_direction direction,
    marc_lz78_tans_config* config) noexcept {
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
    config->maximum_entries = UINT32_C(1) << 16;
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

marc_status marc_lz78_tans_workspace_requirements(
    const marc_lz78_tans_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::Lz78Parameters parameters{
            config->maximum_entries, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::Lz78TansEncoderWorkspaceRequirements needed{};
        const auto error = marc::frame::make_lz78_tans_profile(
            {config->original_size, config->frame_size,
             config->entropy_block_size, parameters},
            limits, stream, needed);
        if (error != marc::frame::Lz78TansProfileError::none) {
            return status_for(
                marc::frame::lz78_tans_profile_error_code(error));
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
        marc::frame::Lz78TansDecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_lz78_tans_decoder_workspace(
                limits, needed);
        if (error != marc::frame::Lz78TansProfileError::none) {
            return status_for(
                marc::frame::lz78_tans_profile_error_code(error));
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

marc_status marc_lz78_tans_create(
    const marc_lz78_tans_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query =
        marc_lz78_tans_workspace_requirements(config, &required);
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
    const auto views_storage = std::span<std::byte>{
        reinterpret_cast<std::byte*>(views_workspace.data),
        required.views_bytes};
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::Lz78Parameters parameters{
            config->maximum_entries, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::Lz78TansEncoderWorkspaceRequirements needed{};
        if (marc::frame::make_lz78_tans_profile(
                {config->original_size, config->frame_size,
                 config->entropy_block_size, parameters},
                limits, stream, needed)
            != marc::frame::Lz78TansProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::Lz78TansEncoderViews views{};
        if (marc::frame::partition_lz78_tans_encoder_views(
                needed, views_storage, views)
            != marc::frame::Lz78TansWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        auto* const encoded = needed.dictionary_staging_bytes == 0
            ? secondary : secondary + needed.dictionary_staging_bytes;
        implementation = new (std::nothrow)
            marc::frame::Lz78TansFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_input_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {encoded, needed.frame_encoded_bytes}, views.entries);
    } else {
        marc::frame::Lz78TansDecoderWorkspaceRequirements needed{};
        if (marc::frame::calculate_lz78_tans_decoder_workspace(
                limits, needed)
            != marc::frame::Lz78TansProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::Lz78TansDecoderViews views{};
        if (marc::frame::partition_lz78_tans_decoder_views(
                needed, views_storage, views)
            != marc::frame::Lz78TansWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow)
            marc::frame::Lz78TansFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_encoded_bytes},
                views.blocks,
                {secondary, needed.dictionary_staging_bytes},
                {secondary + needed.dictionary_staging_bytes,
                 needed.frame_decoded_bytes},
                views.phrases);
    }
    return publish_transform(implementation, transform);
}

} // extern "C"
