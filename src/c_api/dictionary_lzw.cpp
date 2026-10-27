#include "c_api/common.hpp"
#include "frame/lzw_profile.hpp"
#include "frame/lzw_streaming_decoder.hpp"
#include "frame/lzw_streaming_encoder.hpp"
#include "frame/lzw_blocked_huffman_frame_streaming_decoder.hpp"
#include "frame/lzw_blocked_huffman_frame_streaming_encoder.hpp"
#include "frame/lzw_blocked_huffman_profile.hpp"
#include "frame/lzw_adaptive_huffman_frame_streaming_decoder.hpp"
#include "frame/lzw_adaptive_huffman_frame_streaming_encoder.hpp"
#include "frame/lzw_adaptive_huffman_profile.hpp"
#include "frame/lzw_dynamic_range_frame_streaming_decoder.hpp"
#include "frame/lzw_dynamic_range_frame_streaming_encoder.hpp"
#include "frame/lzw_dynamic_range_profile.hpp"
#include "frame/lzw_rans_frame_streaming_decoder.hpp"
#include "frame/lzw_rans_frame_streaming_encoder.hpp"
#include "frame/lzw_rans_profile.hpp"
#include "frame/lzw_tans_frame_streaming_decoder.hpp"
#include "frame/lzw_tans_frame_streaming_encoder.hpp"
#include "frame/lzw_tans_profile.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <span>

namespace {
using marc::c_api::status_for;
using namespace marc::c_api::detail;

bool load_config(const marc_lzw_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (config == nullptr
        || config->struct_size != sizeof(marc_lzw_config)
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

bool load_config(const marc_lzw_blocked_huffman_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (config == nullptr
        || config->struct_size != sizeof(marc_lzw_blocked_huffman_config)
        || config->abi_version != MARC_ABI_VERSION
        || config->reserved != 0 || config->reserved2 != 0) return false;
    limits.max_total_output_size = config->max_total_output_size;
    limits.max_frame_size = config->max_frame_size;
    limits.max_block_size = config->max_block_size;
    limits.max_compressed_payload_size = config->max_compressed_payload_size;
    limits.max_dictionary_serialized_size =
        config->max_dictionary_serialized_size;
    limits.max_internal_buffered_bytes =
        config->max_internal_buffered_bytes;
    limits.max_dictionary_entries = config->max_dictionary_entries;
    limits.max_blocks_per_frame = config->max_blocks_per_frame;
    return true;
}

bool load_config(const marc_lzw_adaptive_huffman_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (config == nullptr
        || config->struct_size
            != sizeof(marc_lzw_adaptive_huffman_config)
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

bool load_config(const marc_lzw_dynamic_range_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (config == nullptr
        || config->struct_size != sizeof(marc_lzw_dynamic_range_config)
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

bool load_config(const marc_lzw_rans_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (config == nullptr
        || config->struct_size != sizeof(marc_lzw_rans_config)
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

bool load_config(const marc_lzw_tans_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (config == nullptr
        || config->struct_size != sizeof(marc_lzw_tans_config)
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

marc_status marc_lzw_config_init(
    const marc_direction direction, marc_lzw_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE))
        return MARC_STATUS_INVALID_ARGUMENT;
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 20;
    config->maximum_code_width = 16;
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

marc_status marc_lzw_workspace_requirements(
    const marc_lzw_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        using Entry = marc::dictionary::internal::LzwEncoderEntry;
        marc::frame::StreamHeader stream{};
        marc::frame::LzwEncoderWorkspaceRequirements needed{};
        const marc::dictionary::internal::LzwParameters parameters{
            config->maximum_code_width, 0, 0};
        const auto error = marc::frame::make_lzw_profile(
            {config->original_size, config->frame_size, parameters}, limits,
            stream, needed);
        if (error != marc::frame::LzwProfileError::none)
            return status_for(marc::frame::lzw_profile_error_code(error));
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
        using Entry = marc::dictionary::internal::LzwPhraseEntry;
        marc::frame::LzwDecoderWorkspaceRequirements needed{};
        const auto error = marc::frame::calculate_lzw_decoder_workspace(
            limits, needed);
        if (error != marc::frame::LzwProfileError::none)
            return status_for(marc::frame::lzw_profile_error_code(error));
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

marc_status marc_lzw_create(
    const marc_lzw_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements needed{};
    const auto query = marc_lzw_workspace_requirements(config, &needed);
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
        using Entry = marc::dictionary::internal::LzwEncoderEntry;
        const marc::dictionary::internal::LzwParameters parameters{
            config->maximum_code_width, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzwEncoderWorkspaceRequirements ignored{};
        if (marc::frame::make_lzw_profile(
                {config->original_size, config->frame_size, parameters},
                limits, stream, ignored)
            != marc::frame::LzwProfileError::none)
            return MARC_STATUS_INTERNAL_ERROR;
        implementation = new (std::nothrow)
            marc::frame::LzwFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes},
                {reinterpret_cast<Entry*>(views_workspace.data),
                 needed.views_bytes / sizeof(Entry)});
    } else {
        using Entry = marc::dictionary::internal::LzwPhraseEntry;
        implementation = new (std::nothrow)
            marc::frame::LzwFrameStreamingDecoder(
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

marc_status marc_lzw_blocked_huffman_config_init(
    const marc_direction direction,
    marc_lzw_blocked_huffman_config* config) noexcept {
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
    config->maximum_code_width = 16;
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

marc_status marc_lzw_blocked_huffman_workspace_requirements(
    const marc_lzw_blocked_huffman_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr)
        return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits))
        return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzwParameters parameters{
            config->maximum_code_width, 0, 0
        };
        marc::frame::StreamHeader stream{};
        marc::frame::LzwBlockedHuffmanEncoderWorkspaceRequirements needed{};
        const auto error = marc::frame::make_lzw_blocked_huffman_profile(
            { config->original_size, config->frame_size,
                config->entropy_block_size, parameters },
            limits, stream, needed);
        if (error != marc::frame::LzwBlockedHuffmanProfileError::none) {
            return status_for(
                marc::frame::lzw_blocked_huffman_profile_error_code(error));
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
        marc::frame::LzwBlockedHuffmanDecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_lzw_blocked_huffman_decoder_workspace(
                limits, needed);
        if (error != marc::frame::LzwBlockedHuffmanProfileError::none) {
            return status_for(
                marc::frame::lzw_blocked_huffman_profile_error_code(error));
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

marc_status marc_lzw_blocked_huffman_create(
    const marc_lzw_blocked_huffman_config* config,
    const marc_buffer primary_workspace, const marc_buffer secondary_workspace,
    const marc_buffer views_workspace, marc_transform** transform) noexcept {
    if (transform == nullptr)
        return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query =
        marc_lzw_blocked_huffman_workspace_requirements(config, &required);
    if (query != MARC_STATUS_OK)
        return query;
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
    if (!load_config(config, limits))
        return MARC_STATUS_INVALID_ARGUMENT;
    auto* const secondary =
        reinterpret_cast<std::byte*>(secondary_workspace.data);
    const auto views_storage = std::span<std::byte>{
        reinterpret_cast<std::byte*>(views_workspace.data), required.views_bytes
    };
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzwParameters parameters{
            config->maximum_code_width, 0, 0
        };
        marc::frame::StreamHeader stream{};
        marc::frame::LzwBlockedHuffmanEncoderWorkspaceRequirements needed{};
        if (marc::frame::make_lzw_blocked_huffman_profile(
                { config->original_size, config->frame_size,
                    config->entropy_block_size, parameters },
                limits, stream, needed)
            != marc::frame::LzwBlockedHuffmanProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzwBlockedHuffmanEncoderViews views{};
        if (marc::frame::partition_lzw_blocked_huffman_encoder_views(
                needed, views_storage, views)
            != marc::frame::LzwBlockedHuffmanWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        auto* const encoded = needed.dictionary_staging_bytes == 0
            ? secondary
            : secondary + needed.dictionary_staging_bytes;
        implementation = new (std::nothrow)
            marc::frame::LzwBlockedHuffmanFrameStreamingEncoder(stream,
                parameters, limits,
                { reinterpret_cast<std::byte*>(primary_workspace.data),
                    needed.frame_input_bytes },
                { secondary, needed.dictionary_staging_bytes },
                { encoded, needed.frame_encoded_bytes }, views.entries);
    } else {
        marc::frame::LzwBlockedHuffmanDecoderWorkspaceRequirements needed{};
        if (marc::frame::calculate_lzw_blocked_huffman_decoder_workspace(
                limits, needed)
            != marc::frame::LzwBlockedHuffmanProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzwBlockedHuffmanDecoderViews views{};
        if (marc::frame::partition_lzw_blocked_huffman_decoder_views(
                needed, views_storage, views)
            != marc::frame::LzwBlockedHuffmanWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow)
            marc::frame::LzwBlockedHuffmanFrameStreamingDecoder(limits,
                { reinterpret_cast<std::byte*>(primary_workspace.data),
                    needed.frame_encoded_bytes },
                { secondary, needed.dictionary_staging_bytes },
                { secondary + needed.dictionary_staging_bytes,
                    needed.frame_decoded_bytes },
                views.blocks, views.phrases);
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lzw_adaptive_huffman_config_init(
    const marc_direction direction,
    marc_lzw_adaptive_huffman_config* config) noexcept {
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
    config->maximum_code_width = 16;
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

marc_status marc_lzw_adaptive_huffman_workspace_requirements(
    const marc_lzw_adaptive_huffman_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzwParameters parameters{
            config->maximum_code_width, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzwAdaptiveHuffmanEncoderWorkspaceRequirements
            needed{};
        const auto error =
            marc::frame::make_lzw_adaptive_huffman_profile(
                {config->original_size, config->frame_size, parameters},
                limits, stream, needed);
        if (error
            != marc::frame::LzwAdaptiveHuffmanProfileError::none) {
            return status_for(
                marc::frame::lzw_adaptive_huffman_profile_error_code(
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
        marc::frame::LzwAdaptiveHuffmanDecoderWorkspaceRequirements
            needed{};
        const auto error =
            marc::frame::calculate_lzw_adaptive_huffman_decoder_workspace(
                limits, needed);
        if (error
            != marc::frame::LzwAdaptiveHuffmanProfileError::none) {
            return status_for(
                marc::frame::lzw_adaptive_huffman_profile_error_code(
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

marc_status marc_lzw_adaptive_huffman_create(
    const marc_lzw_adaptive_huffman_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query = marc_lzw_adaptive_huffman_workspace_requirements(
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
        const marc::dictionary::internal::LzwParameters parameters{
            config->maximum_code_width, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzwAdaptiveHuffmanEncoderWorkspaceRequirements
            needed{};
        if (marc::frame::make_lzw_adaptive_huffman_profile(
                {config->original_size, config->frame_size, parameters},
                limits, stream, needed)
            != marc::frame::LzwAdaptiveHuffmanProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzwAdaptiveHuffmanEncoderViews views{};
        if (marc::frame::partition_lzw_adaptive_huffman_encoder_views(
                needed, views_storage, views)
            != marc::frame::LzwAdaptiveHuffmanWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        auto* const encoded = needed.dictionary_staging_bytes == 0
            ? secondary : secondary + needed.dictionary_staging_bytes;
        implementation = new (std::nothrow)
            marc::frame::LzwAdaptiveHuffmanFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_input_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {encoded, needed.frame_encoded_bytes}, views.entries);
    } else {
        marc::frame::LzwAdaptiveHuffmanDecoderWorkspaceRequirements
            needed{};
        if (marc::frame::calculate_lzw_adaptive_huffman_decoder_workspace(
                limits, needed)
            != marc::frame::LzwAdaptiveHuffmanProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzwAdaptiveHuffmanDecoderViews views{};
        if (marc::frame::partition_lzw_adaptive_huffman_decoder_views(
                needed, views_storage, views)
            != marc::frame::LzwAdaptiveHuffmanWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow)
            marc::frame::LzwAdaptiveHuffmanFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_encoded_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {secondary + needed.dictionary_staging_bytes,
                 needed.frame_decoded_bytes}, views.phrases);
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lzw_dynamic_range_config_init(
    const marc_direction direction,
    marc_lzw_dynamic_range_config* config) noexcept {
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
    config->maximum_code_width = 16;
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

marc_status marc_lzw_dynamic_range_workspace_requirements(
    const marc_lzw_dynamic_range_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzwParameters parameters{
            config->maximum_code_width, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzwDynamicRangeEncoderWorkspaceRequirements needed{};
        const auto error = marc::frame::make_lzw_dynamic_range_profile(
            {config->original_size, config->frame_size, parameters},
            limits, stream, needed);
        if (error != marc::frame::LzwDynamicRangeProfileError::none) {
            return status_for(
                marc::frame::lzw_dynamic_range_profile_error_code(error));
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
        marc::frame::LzwDynamicRangeDecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_lzw_dynamic_range_decoder_workspace(
                limits, needed);
        if (error != marc::frame::LzwDynamicRangeProfileError::none) {
            return status_for(
                marc::frame::lzw_dynamic_range_profile_error_code(error));
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

marc_status marc_lzw_dynamic_range_create(
    const marc_lzw_dynamic_range_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query = marc_lzw_dynamic_range_workspace_requirements(
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
        const marc::dictionary::internal::LzwParameters parameters{
            config->maximum_code_width, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzwDynamicRangeEncoderWorkspaceRequirements needed{};
        if (marc::frame::make_lzw_dynamic_range_profile(
                {config->original_size, config->frame_size, parameters},
                limits, stream, needed)
            != marc::frame::LzwDynamicRangeProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzwDynamicRangeEncoderViews views{};
        if (marc::frame::partition_lzw_dynamic_range_encoder_views(
                needed, views_storage, views)
            != marc::frame::LzwDynamicRangeWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        auto* const encoded = needed.dictionary_staging_bytes == 0
            ? secondary : secondary + needed.dictionary_staging_bytes;
        implementation = new (std::nothrow)
            marc::frame::LzwDynamicRangeFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_input_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {encoded, needed.frame_encoded_bytes}, views.entries);
    } else {
        marc::frame::LzwDynamicRangeDecoderWorkspaceRequirements needed{};
        if (marc::frame::calculate_lzw_dynamic_range_decoder_workspace(
                limits, needed)
            != marc::frame::LzwDynamicRangeProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzwDynamicRangeDecoderViews views{};
        if (marc::frame::partition_lzw_dynamic_range_decoder_views(
                needed, views_storage, views)
            != marc::frame::LzwDynamicRangeWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow)
            marc::frame::LzwDynamicRangeFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_encoded_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {secondary + needed.dictionary_staging_bytes,
                 needed.frame_decoded_bytes}, views.phrases);
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lzw_rans_config_init(
    const marc_direction direction, marc_lzw_rans_config* config) noexcept {
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
    config->maximum_code_width = 16;
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

marc_status marc_lzw_rans_workspace_requirements(
    const marc_lzw_rans_config* config,
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
        const marc::dictionary::internal::LzwParameters parameters{
            config->maximum_code_width, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzwRansEncoderWorkspaceRequirements needed{};
        const auto error = marc::frame::make_lzw_rans_profile(
            {config->original_size, config->frame_size,
             config->entropy_block_size, parameters},
            limits, stream, needed);
        if (error != marc::frame::LzwRansProfileError::none) {
            return status_for(
                marc::frame::lzw_rans_profile_error_code(error));
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
        marc::frame::LzwRansDecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_lzw_rans_decoder_workspace(
                limits, needed);
        if (error != marc::frame::LzwRansProfileError::none) {
            return status_for(
                marc::frame::lzw_rans_profile_error_code(error));
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

marc_status marc_lzw_rans_create(
    const marc_lzw_rans_config* config,
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
        marc_lzw_rans_workspace_requirements(config, &required);
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
        const marc::dictionary::internal::LzwParameters parameters{
            config->maximum_code_width, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzwRansEncoderWorkspaceRequirements needed{};
        if (marc::frame::make_lzw_rans_profile(
                {config->original_size, config->frame_size,
                 config->entropy_block_size, parameters},
                limits, stream, needed)
            != marc::frame::LzwRansProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzwRansEncoderViews views{};
        if (marc::frame::partition_lzw_rans_encoder_views(
                needed, views_storage, views)
            != marc::frame::LzwRansWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        auto* const encoded = needed.dictionary_staging_bytes == 0
            ? secondary : secondary + needed.dictionary_staging_bytes;
        implementation = new (std::nothrow)
            marc::frame::LzwRansFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_input_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {encoded, needed.frame_encoded_bytes}, views.entries);
    } else {
        marc::frame::LzwRansDecoderWorkspaceRequirements needed{};
        if (marc::frame::calculate_lzw_rans_decoder_workspace(
                limits, needed)
            != marc::frame::LzwRansProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzwRansDecoderViews views{};
        if (marc::frame::partition_lzw_rans_decoder_views(
                needed, views_storage, views)
            != marc::frame::LzwRansWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow)
            marc::frame::LzwRansFrameStreamingDecoder(
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

marc_status marc_lzw_tans_config_init(
    const marc_direction direction, marc_lzw_tans_config* config) noexcept {
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
    config->maximum_code_width = 16;
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

marc_status marc_lzw_tans_workspace_requirements(
    const marc_lzw_tans_config* config,
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
        const marc::dictionary::internal::LzwParameters parameters{
            config->maximum_code_width, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzwTansEncoderWorkspaceRequirements needed{};
        const auto error = marc::frame::make_lzw_tans_profile(
            {config->original_size, config->frame_size,
             config->entropy_block_size, parameters},
            limits, stream, needed);
        if (error != marc::frame::LzwTansProfileError::none) {
            return status_for(
                marc::frame::lzw_tans_profile_error_code(error));
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
        marc::frame::LzwTansDecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_lzw_tans_decoder_workspace(
                limits, needed);
        if (error != marc::frame::LzwTansProfileError::none) {
            return status_for(
                marc::frame::lzw_tans_profile_error_code(error));
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

marc_status marc_lzw_tans_create(
    const marc_lzw_tans_config* config,
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
        marc_lzw_tans_workspace_requirements(config, &required);
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
        const marc::dictionary::internal::LzwParameters parameters{
            config->maximum_code_width, 0, 0};
        marc::frame::StreamHeader stream{};
        marc::frame::LzwTansEncoderWorkspaceRequirements needed{};
        if (marc::frame::make_lzw_tans_profile(
                {config->original_size, config->frame_size,
                 config->entropy_block_size, parameters},
                limits, stream, needed)
            != marc::frame::LzwTansProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzwTansEncoderViews views{};
        if (marc::frame::partition_lzw_tans_encoder_views(
                needed, views_storage, views)
            != marc::frame::LzwTansWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        auto* const encoded = needed.dictionary_staging_bytes == 0
            ? secondary : secondary + needed.dictionary_staging_bytes;
        implementation = new (std::nothrow)
            marc::frame::LzwTansFrameStreamingEncoder(
                stream, parameters, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.frame_input_bytes},
                {secondary, needed.dictionary_staging_bytes},
                {encoded, needed.frame_encoded_bytes}, views.entries);
    } else {
        marc::frame::LzwTansDecoderWorkspaceRequirements needed{};
        if (marc::frame::calculate_lzw_tans_decoder_workspace(
                limits, needed)
            != marc::frame::LzwTansProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::LzwTansDecoderViews views{};
        if (marc::frame::partition_lzw_tans_decoder_views(
                needed, views_storage, views)
            != marc::frame::LzwTansWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow)
            marc::frame::LzwTansFrameStreamingDecoder(
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
