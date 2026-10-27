#include "c_api/common.hpp"
#include "c_api/configuration.hpp"
#include "entropy/blocked_huffman_controller.hpp"
#include "frame/blocked_huffman_frame_streaming_decoder.hpp"
#include "frame/blocked_huffman_frame_streaming_encoder.hpp"
#include "frame/blocked_huffman_profile.hpp"
#include "frame/checksum_raw_profile.hpp"
#include "frame/checksum_raw_streaming_decoder.hpp"
#include "frame/checksum_raw_streaming_encoder.hpp"
#include "frame/adaptive_huffman_frame_streaming_decoder.hpp"
#include "frame/adaptive_huffman_frame_streaming_encoder.hpp"
#include "frame/adaptive_huffman_profile.hpp"
#include "frame/dynamic_range_frame_streaming_decoder.hpp"
#include "frame/dynamic_range_frame_streaming_encoder.hpp"
#include "frame/dynamic_range_profile.hpp"
#include "frame/rans_frame_streaming_decoder.hpp"
#include "frame/rans_frame_streaming_encoder.hpp"
#include "frame/rans_profile.hpp"
#include "frame/tans_frame_streaming_decoder.hpp"
#include "frame/tans_frame_streaming_encoder.hpp"
#include "frame/tans_profile.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <span>

namespace {
using marc::c_api::status_for;
using namespace marc::c_api::detail;

bool load_config(const marc_checksum_raw_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || config->reserved != 0 || config->reserved2 != 0
        || config->reserved3 != 0) {
        return false;
    }
    copy_frame_limits(*config, limits);
    copy_payload_limits(*config, limits, config->max_dictionary_serialized_size);
    limits.max_block_size = std::min(
        limits.max_block_size, limits.max_internal_buffered_bytes);
    return true;
}

bool load_config(const marc_blocked_huffman_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || config->reserved != 0 || config->reserved2 != 0) {
        return false;
    }
    copy_frame_limits(*config, limits);
    limits.max_block_size = config->max_block_size;
    copy_payload_limits(*config, limits, config->max_frame_size);
    limits.max_blocks_per_frame = config->max_blocks_per_frame;
    return true;
}

bool load_config(const marc_adaptive_huffman_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || config->reserved != 0 || config->reserved2 != 0) return false;
    copy_frame_limits(*config, limits);
    copy_payload_limits(*config, limits, config->max_frame_size);
    limits.max_block_size = std::min(
        limits.max_block_size, limits.max_internal_buffered_bytes);
    return true;
}

bool load_config(const marc_dynamic_range_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || config->reserved != 0 || config->reserved2 != 0) return false;
    copy_frame_limits(*config, limits);
    copy_payload_limits(*config, limits, config->max_frame_size);
    limits.max_range_model_total = config->max_range_model_total;
    limits.max_block_size = std::min(
        limits.max_block_size, limits.max_internal_buffered_bytes);
    return true;
}

bool load_config(const marc_rans_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || config->reserved != 0 || config->reserved2 != 0) return false;
    copy_frame_limits(*config, limits);
    limits.max_block_size = config->max_block_size;
    copy_payload_limits(*config, limits, config->max_frame_size);
    limits.max_blocks_per_frame = config->max_blocks_per_frame;
    return true;
}

bool load_config(const marc_tans_config* config,
                 marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || config->reserved != 0 || config->reserved2 != 0) return false;
    copy_frame_limits(*config, limits);
    limits.max_block_size = config->max_block_size;
    copy_payload_limits(*config, limits, config->max_frame_size);
    limits.max_blocks_per_frame = config->max_blocks_per_frame;
    return true;
}

} // namespace

extern "C" {

marc_status marc_checksum_raw_config_init(
    const marc_direction direction,
    marc_checksum_raw_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 20;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_compressed_payload_size =
        limits.max_compressed_payload_size;
    config->max_dictionary_serialized_size =
        limits.max_dictionary_serialized_size;
    config->max_internal_buffered_bytes =
        limits.max_internal_buffered_bytes;
    return MARC_STATUS_OK;
}

marc_status marc_checksum_raw_workspace_requirements(
    const marc_checksum_raw_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;

    marc::frame::ChecksumRawWorkspaceRequirements needed{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::HashDescriptor descriptor{};
        const auto error = marc::frame::make_checksum_raw_profile_v1_1(
            {config->original_size, config->frame_size}, limits, stream,
            descriptor, needed);
        if (error != marc::frame::ChecksumRawProfileError::none) {
            return status_for(
                marc::frame::checksum_raw_profile_error_code(error));
        }
    } else if (config->direction == MARC_DIRECTION_DECODE) {
        const auto error =
            marc::frame::calculate_checksum_raw_decoder_workspace_v1_1(
                limits, needed);
        if (error != marc::frame::ChecksumRawProfileError::none) {
            return status_for(
                marc::frame::checksum_raw_profile_error_code(error));
        }
    } else {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    requirements->primary_bytes = needed.serialized_frame_bytes;
    return MARC_STATUS_OK;
}

marc_status marc_checksum_raw_create(
    const marc_checksum_raw_config* config,
    const marc_buffer primary_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements needed{};
    const auto query = marc_checksum_raw_workspace_requirements(
        config, &needed);
    if (query != MARC_STATUS_OK) return query;
    if (!valid_buffer(primary_workspace.data, primary_workspace.size)
        || primary_workspace.size < needed.primary_bytes) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    const std::span<std::byte> workspace{
        reinterpret_cast<std::byte*>(primary_workspace.data),
        needed.primary_bytes};
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::HashDescriptor descriptor{};
        marc::frame::ChecksumRawWorkspaceRequirements ignored{};
        if (marc::frame::make_checksum_raw_profile_v1_1(
                {config->original_size, config->frame_size}, limits, stream,
                descriptor, ignored)
            != marc::frame::ChecksumRawProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        implementation = new (std::nothrow)
            marc::frame::ChecksumRawStreamingEncoder(
                stream, descriptor, limits, workspace);
    } else {
        implementation = new (std::nothrow)
            marc::frame::ChecksumRawStreamingDecoder(limits, workspace);
    }
    return publish_transform(implementation, transform);
}

marc_status marc_blocked_huffman_config_init(
    const marc_direction direction,
    marc_blocked_huffman_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 20;
    config->block_size = UINT32_C(1) << 16;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_block_size = limits.max_block_size;
    config->max_compressed_payload_size =
        limits.max_compressed_payload_size;
    config->max_internal_buffered_bytes = limits.max_internal_buffered_bytes;
    config->max_blocks_per_frame = limits.max_blocks_per_frame;
    return MARC_STATUS_OK;
}

marc_status marc_blocked_huffman_workspace_requirements(
    const marc_blocked_huffman_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::EncoderWorkspaceRequirements needed{};
        const auto error = marc::frame::make_blocked_huffman_profile(
            {config->original_size, config->frame_size, config->block_size},
            limits, stream, needed);
        if (error != marc::frame::ProfileError::none) {
            return status_for(marc::frame::profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_input_bytes;
        requirements->secondary_bytes = needed.frame_encoded_bytes;
        requirements->views_alignment = 1;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::DecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_blocked_huffman_decoder_workspace(
                limits, needed);
        if (error != marc::frame::ProfileError::none) {
            return status_for(marc::frame::profile_error_code(error));
        }
        using View = marc::entropy::internal::BlockedHuffmanBlockView;
        if (needed.block_view_count
            > std::numeric_limits<std::size_t>::max() / sizeof(View)) {
            return MARC_STATUS_LIMIT_EXCEEDED;
        }
        requirements->primary_bytes = needed.frame_encoded_bytes;
        requirements->secondary_bytes = needed.frame_decoded_bytes;
        requirements->views_bytes = needed.block_view_count * sizeof(View);
        requirements->views_alignment = alignof(View);
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_blocked_huffman_create(
    const marc_blocked_huffman_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements needed{};
    const auto query = marc_blocked_huffman_workspace_requirements(
        config, &needed);
    if (query != MARC_STATUS_OK) return query;
    if (!valid_buffer(primary_workspace.data, primary_workspace.size)
        || !valid_buffer(secondary_workspace.data, secondary_workspace.size)
        || !valid_buffer(views_workspace.data, views_workspace.size)
        || primary_workspace.size < needed.primary_bytes
        || secondary_workspace.size < needed.secondary_bytes
        || views_workspace.size < needed.views_bytes
        || (needed.views_bytes != 0
            && reinterpret_cast<std::uintptr_t>(views_workspace.data)
                % needed.views_alignment != 0)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::EncoderWorkspaceRequirements ignored{};
        if (marc::frame::make_blocked_huffman_profile(
                {config->original_size, config->frame_size,
                 config->block_size}, limits, stream, ignored)
            != marc::frame::ProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        implementation = new (std::nothrow)
            marc::frame::BlockedHuffmanFrameStreamingEncoder(
                stream, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes});
    } else {
        using View = marc::entropy::internal::BlockedHuffmanBlockView;
        implementation = new (std::nothrow)
            marc::frame::BlockedHuffmanFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes},
                {reinterpret_cast<View*>(views_workspace.data),
                 needed.views_bytes / sizeof(View)});
    }
    if (implementation == nullptr) return MARC_STATUS_OUT_OF_MEMORY;
    auto* handle = new (std::nothrow) marc_transform{implementation};
    if (handle == nullptr) {
        delete implementation;
        return MARC_STATUS_OUT_OF_MEMORY;
    }
    *transform = handle;
    return MARC_STATUS_OK;
}

marc_status marc_adaptive_huffman_config_init(
    const marc_direction direction,
    marc_adaptive_huffman_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE))
        return MARC_STATUS_INVALID_ARGUMENT;
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 20;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_compressed_payload_size = limits.max_compressed_payload_size;
    config->max_internal_buffered_bytes = limits.max_internal_buffered_bytes;
    return MARC_STATUS_OK;
}

marc_status marc_adaptive_huffman_workspace_requirements(
    const marc_adaptive_huffman_config* config,
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
        marc::frame::AdaptiveHuffmanEncoderWorkspaceRequirements needed{};
        const auto error = marc::frame::make_adaptive_huffman_profile(
            {config->original_size, config->frame_size}, limits,
            stream, needed);
        if (error != marc::frame::AdaptiveHuffmanProfileError::none)
            return status_for(
                marc::frame::adaptive_huffman_profile_error_code(error));
        requirements->primary_bytes = needed.frame_input_bytes;
        requirements->secondary_bytes = needed.frame_encoded_bytes;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::AdaptiveHuffmanDecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_adaptive_huffman_decoder_workspace(
                limits, needed);
        if (error != marc::frame::AdaptiveHuffmanProfileError::none)
            return status_for(
                marc::frame::adaptive_huffman_profile_error_code(error));
        requirements->primary_bytes = needed.frame_encoded_bytes;
        requirements->secondary_bytes = needed.frame_decoded_bytes;
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_adaptive_huffman_create(
    const marc_adaptive_huffman_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements needed{};
    const auto query = marc_adaptive_huffman_workspace_requirements(
        config, &needed);
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
        marc::frame::StreamHeader stream{};
        marc::frame::AdaptiveHuffmanEncoderWorkspaceRequirements ignored{};
        if (marc::frame::make_adaptive_huffman_profile(
                {config->original_size, config->frame_size}, limits,
                stream, ignored)
            != marc::frame::AdaptiveHuffmanProfileError::none)
            return MARC_STATUS_INTERNAL_ERROR;
        implementation = new (std::nothrow)
            marc::frame::AdaptiveHuffmanFrameStreamingEncoder(
                stream, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes});
    } else {
        implementation = new (std::nothrow)
            marc::frame::AdaptiveHuffmanFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes});
    }
    return publish_transform(implementation, transform);
}

marc_status marc_dynamic_range_config_init(
    const marc_direction direction,
    marc_dynamic_range_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE))
        return MARC_STATUS_INVALID_ARGUMENT;
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 20;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_compressed_payload_size = limits.max_compressed_payload_size;
    config->max_internal_buffered_bytes = limits.max_internal_buffered_bytes;
    config->max_range_model_total = limits.max_range_model_total;
    return MARC_STATUS_OK;
}

marc_status marc_dynamic_range_workspace_requirements(
    const marc_dynamic_range_config* config,
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
        marc::frame::DynamicRangeEncoderWorkspaceRequirements needed{};
        const auto error = marc::frame::make_dynamic_range_profile(
            {config->original_size, config->frame_size}, limits,
            stream, needed);
        if (error != marc::frame::DynamicRangeProfileError::none)
            return status_for(
                marc::frame::dynamic_range_profile_error_code(error));
        requirements->primary_bytes = needed.frame_input_bytes;
        requirements->secondary_bytes = needed.frame_encoded_bytes;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::DynamicRangeDecoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::calculate_dynamic_range_decoder_workspace(
                limits, needed);
        if (error != marc::frame::DynamicRangeProfileError::none)
            return status_for(
                marc::frame::dynamic_range_profile_error_code(error));
        requirements->primary_bytes = needed.frame_encoded_bytes;
        requirements->secondary_bytes = needed.frame_decoded_bytes;
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_dynamic_range_create(
    const marc_dynamic_range_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements needed{};
    const auto query = marc_dynamic_range_workspace_requirements(
        config, &needed);
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
        marc::frame::StreamHeader stream{};
        marc::frame::DynamicRangeEncoderWorkspaceRequirements ignored{};
        if (marc::frame::make_dynamic_range_profile(
                {config->original_size, config->frame_size}, limits,
                stream, ignored)
            != marc::frame::DynamicRangeProfileError::none)
            return MARC_STATUS_INTERNAL_ERROR;
        implementation = new (std::nothrow)
            marc::frame::DynamicRangeFrameStreamingEncoder(
                stream, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes});
    } else {
        implementation = new (std::nothrow)
            marc::frame::DynamicRangeFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes});
    }
    return publish_transform(implementation, transform);
}

marc_status marc_rans_config_init(
    const marc_direction direction, marc_rans_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE))
        return MARC_STATUS_INVALID_ARGUMENT;
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 20;
    config->block_size = UINT32_C(1) << 16;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_block_size = limits.max_block_size;
    config->max_compressed_payload_size = limits.max_compressed_payload_size;
    config->max_internal_buffered_bytes = limits.max_internal_buffered_bytes;
    config->max_blocks_per_frame = limits.max_blocks_per_frame;
    return MARC_STATUS_OK;
}

marc_status marc_rans_workspace_requirements(
    const marc_rans_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::RansEncoderWorkspaceRequirements needed{};
        const auto error = marc::frame::make_rans_profile(
            {config->original_size, config->frame_size, config->block_size},
            limits, stream, needed);
        if (error != marc::frame::RansProfileError::none)
            return status_for(marc::frame::rans_profile_error_code(error));
        requirements->primary_bytes = needed.frame_input_bytes;
        requirements->secondary_bytes = needed.frame_encoded_bytes;
        requirements->views_alignment = 1;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::RansDecoderWorkspaceRequirements needed{};
        const auto error = marc::frame::calculate_rans_decoder_workspace(
            limits, needed);
        if (error != marc::frame::RansProfileError::none)
            return status_for(marc::frame::rans_profile_error_code(error));
        using View = marc::entropy::internal::RansBlockView;
        if (needed.block_view_count
            > std::numeric_limits<std::size_t>::max() / sizeof(View))
            return MARC_STATUS_LIMIT_EXCEEDED;
        requirements->primary_bytes = needed.frame_encoded_bytes;
        requirements->secondary_bytes = needed.frame_decoded_bytes;
        requirements->views_bytes = needed.block_view_count * sizeof(View);
        requirements->views_alignment = alignof(View);
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_rans_create(
    const marc_rans_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements needed{};
    const auto query = marc_rans_workspace_requirements(config, &needed);
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
        marc::frame::StreamHeader stream{};
        marc::frame::RansEncoderWorkspaceRequirements ignored{};
        if (marc::frame::make_rans_profile(
                {config->original_size, config->frame_size,
                 config->block_size}, limits, stream, ignored)
            != marc::frame::RansProfileError::none)
            return MARC_STATUS_INTERNAL_ERROR;
        implementation = new (std::nothrow)
            marc::frame::RansFrameStreamingEncoder(
                stream, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes});
    } else {
        using View = marc::entropy::internal::RansBlockView;
        implementation = new (std::nothrow)
            marc::frame::RansFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes},
                {reinterpret_cast<View*>(views_workspace.data),
                 needed.views_bytes / sizeof(View)});
    }
    return publish_transform(implementation, transform);
}

marc_status marc_tans_config_init(
    const marc_direction direction, marc_tans_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE))
        return MARC_STATUS_INVALID_ARGUMENT;
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->frame_size = UINT32_C(1) << 20;
    config->block_size = UINT32_C(1) << 16;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_block_size = limits.max_block_size;
    config->max_compressed_payload_size = limits.max_compressed_payload_size;
    config->max_internal_buffered_bytes = limits.max_internal_buffered_bytes;
    config->max_blocks_per_frame = limits.max_blocks_per_frame;
    return MARC_STATUS_OK;
}

marc_status marc_tans_workspace_requirements(
    const marc_tans_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        marc::frame::StreamHeader stream{};
        marc::frame::TansEncoderWorkspaceRequirements needed{};
        const auto error = marc::frame::make_tans_profile(
            {config->original_size, config->frame_size, config->block_size},
            limits, stream, needed);
        if (error != marc::frame::TansProfileError::none)
            return status_for(marc::frame::tans_profile_error_code(error));
        requirements->primary_bytes = needed.frame_input_bytes;
        requirements->secondary_bytes = needed.frame_encoded_bytes;
        requirements->views_alignment = 1;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::TansDecoderWorkspaceRequirements needed{};
        const auto error = marc::frame::calculate_tans_decoder_workspace(
            limits, needed);
        if (error != marc::frame::TansProfileError::none)
            return status_for(marc::frame::tans_profile_error_code(error));
        using View = marc::entropy::internal::TansBlockView;
        if (needed.block_view_count
            > std::numeric_limits<std::size_t>::max() / sizeof(View))
            return MARC_STATUS_LIMIT_EXCEEDED;
        requirements->primary_bytes = needed.frame_encoded_bytes;
        requirements->secondary_bytes = needed.frame_decoded_bytes;
        requirements->views_bytes = needed.block_view_count * sizeof(View);
        requirements->views_alignment = alignof(View);
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_tans_create(
    const marc_tans_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements needed{};
    const auto query = marc_tans_workspace_requirements(config, &needed);
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
        marc::frame::StreamHeader stream{};
        marc::frame::TansEncoderWorkspaceRequirements ignored{};
        if (marc::frame::make_tans_profile(
                {config->original_size, config->frame_size,
                 config->block_size}, limits, stream, ignored)
            != marc::frame::TansProfileError::none)
            return MARC_STATUS_INTERNAL_ERROR;
        implementation = new (std::nothrow)
            marc::frame::TansFrameStreamingEncoder(
                stream, limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes});
    } else {
        using View = marc::entropy::internal::TansBlockView;
        implementation = new (std::nothrow)
            marc::frame::TansFrameStreamingDecoder(
                limits,
                {reinterpret_cast<std::byte*>(primary_workspace.data),
                 needed.primary_bytes},
                {reinterpret_cast<std::byte*>(secondary_workspace.data),
                 needed.secondary_bytes},
                {reinterpret_cast<View*>(views_workspace.data),
                 needed.views_bytes / sizeof(View)});
    }
    return publish_transform(implementation, transform);
}

} // extern "C"
