#include "c_api/common.hpp"
#include "c_api/configuration.hpp"
#include "frame/lzss_typed_context_frame_streaming_decoder.hpp"
#include "frame/lzss_typed_context_frame_streaming_encoder.hpp"
#include "frame/lzss_typed_context_profile.hpp"
#include "frame/lzss_contextual_rans_frame_streaming_decoder.hpp"
#include "frame/lzss_contextual_rans_frame_streaming_encoder.hpp"
#include "frame/lzss_contextual_rans_profile.hpp"
#include "frame/lzss_contextual_tans_frame_streaming_decoder.hpp"
#include "frame/lzss_contextual_tans_frame_streaming_encoder.hpp"
#include "frame/lzss_contextual_tans_profile.hpp"
#include "frame/lzss_contextual_adaptive_huffman_frame_streaming_decoder.hpp"
#include "frame/lzss_contextual_adaptive_huffman_frame_streaming_encoder.hpp"
#include "frame/lzss_contextual_adaptive_huffman_profile.hpp"
#include "frame/lzss_contextual_blocked_huffman_frame_streaming_decoder.hpp"
#include "frame/lzss_contextual_blocked_huffman_frame_streaming_encoder.hpp"
#include "frame/lzss_contextual_blocked_huffman_profile.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <span>

namespace {
using marc::c_api::status_for;
using namespace marc::c_api::detail;

[[nodiscard]] marc::frame::internal::LzssTypedContextProfileVariant
typed_context_profile_variant(
    const marc_lzss_contextual_profile profile) noexcept {
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_64M) {
        return marc::frame::internal::LzssTypedContextProfileVariant::
            field_context_64m;
    }
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M) {
        return marc::frame::internal::LzssTypedContextProfileVariant::
            field_context_16m;
    }
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_4M) {
        return marc::frame::internal::LzssTypedContextProfileVariant::
            field_context_4m;
    }
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_1M) {
        return marc::frame::internal::LzssTypedContextProfileVariant::
            field_context_1m;
    }
    return marc::frame::internal::LzssTypedContextProfileVariant::
        field_context_64k;
}

[[nodiscard]] marc::frame::internal::LzssTypedContextStreamAdmission
typed_context_stream_admission(
    const marc_lzss_contextual_profile profile) noexcept {
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_64M) {
        return marc::frame::internal::LzssTypedContextStreamAdmission::
            field_context_64m;
    }
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M) {
        return marc::frame::internal::LzssTypedContextStreamAdmission::
            field_context_16m;
    }
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_4M) {
        return marc::frame::internal::LzssTypedContextStreamAdmission::
            field_context_4m;
    }
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_1M) {
        return marc::frame::internal::LzssTypedContextStreamAdmission::
            field_context_1m;
    }
    return marc::frame::internal::LzssTypedContextStreamAdmission::
        field_context_64k;
}

[[nodiscard]] marc::frame::internal::LzssContextualRansProfileVariant
contextual_rans_profile_variant(
    const marc_lzss_contextual_profile profile) noexcept {
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_64M) {
        return marc::frame::internal::LzssContextualRansProfileVariant::
            field_context_64m;
    }
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M) {
        return marc::frame::internal::LzssContextualRansProfileVariant::
            field_context_16m;
    }
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_4M) {
        return marc::frame::internal::LzssContextualRansProfileVariant::
            field_context_4m;
    }
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_1M) {
        return marc::frame::internal::LzssContextualRansProfileVariant::
            field_context_1m;
    }
    return marc::frame::internal::LzssContextualRansProfileVariant::
        field_context_64k;
}

[[nodiscard]] marc::frame::internal::LzssContextualRansStreamAdmission
contextual_rans_stream_admission(
    const marc_lzss_contextual_profile profile) noexcept {
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_64M) {
        return marc::frame::internal::LzssContextualRansStreamAdmission::
            field_context_64m;
    }
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M) {
        return marc::frame::internal::LzssContextualRansStreamAdmission::
            field_context_16m;
    }
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_4M) {
        return marc::frame::internal::LzssContextualRansStreamAdmission::
            field_context_4m;
    }
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_1M) {
        return marc::frame::internal::LzssContextualRansStreamAdmission::
            field_context_1m;
    }
    return marc::frame::internal::LzssContextualRansStreamAdmission::
        field_context_64k;
}

[[nodiscard]] marc::frame::internal::LzssContextualTansProfileVariant
contextual_tans_profile_variant(
    const marc_lzss_contextual_profile profile) noexcept {
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_64M) {
        return marc::frame::internal::LzssContextualTansProfileVariant::
            field_context_64m;
    }
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M) {
        return marc::frame::internal::LzssContextualTansProfileVariant::
            field_context_16m;
    }
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_4M) {
        return marc::frame::internal::LzssContextualTansProfileVariant::
            field_context_4m;
    }
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_1M) {
        return marc::frame::internal::LzssContextualTansProfileVariant::
            field_context_1m;
    }
    return marc::frame::internal::LzssContextualTansProfileVariant::
        field_context_64k;
}

[[nodiscard]] marc::frame::internal::LzssContextualTansStreamAdmission
contextual_tans_stream_admission(
    const marc_lzss_contextual_profile profile) noexcept {
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_64M) {
        return marc::frame::internal::LzssContextualTansStreamAdmission::
            field_context_64m;
    }
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M) {
        return marc::frame::internal::LzssContextualTansStreamAdmission::
            field_context_16m;
    }
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_4M) {
        return marc::frame::internal::LzssContextualTansStreamAdmission::
            field_context_4m;
    }
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_1M) {
        return marc::frame::internal::LzssContextualTansStreamAdmission::
            field_context_1m;
    }
    return marc::frame::internal::LzssContextualTansStreamAdmission::
        field_context_64k;
}

[[nodiscard]] marc::frame::internal::
    LzssContextualBlockedHuffmanProfileVariant
contextual_blocked_huffman_profile_variant(
    const marc_lzss_contextual_profile profile) noexcept {
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_64M) {
        return marc::frame::internal::LzssContextualBlockedHuffmanProfileVariant::field_context_64m;
    }
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M) {
        return marc::frame::internal::
            LzssContextualBlockedHuffmanProfileVariant::field_context_16m;
    }
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_4M) {
        return marc::frame::internal::
            LzssContextualBlockedHuffmanProfileVariant::field_context_4m;
    }
    return profile == MARC_LZSS_CONTEXTUAL_PROFILE_1M
        ? marc::frame::internal::
              LzssContextualBlockedHuffmanProfileVariant::field_context_1m
        : marc::frame::internal::
              LzssContextualBlockedHuffmanProfileVariant::field_context_64k;
}

[[nodiscard]] marc::frame::internal::
    LzssContextualBlockedHuffmanStreamAdmission
contextual_blocked_huffman_stream_admission(
    const marc_lzss_contextual_profile profile) noexcept {
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_64M) {
        return marc::frame::internal::LzssContextualBlockedHuffmanStreamAdmission::field_context_64m;
    }
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M) {
        return marc::frame::internal::
            LzssContextualBlockedHuffmanStreamAdmission::field_context_16m;
    }
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_4M) {
        return marc::frame::internal::
            LzssContextualBlockedHuffmanStreamAdmission::field_context_4m;
    }
    return profile == MARC_LZSS_CONTEXTUAL_PROFILE_1M
        ? marc::frame::internal::
              LzssContextualBlockedHuffmanStreamAdmission::field_context_1m
        : marc::frame::internal::
              LzssContextualBlockedHuffmanStreamAdmission::field_context_64k;
}

[[nodiscard]] marc::frame::internal::
    LzssContextualAdaptiveHuffmanProfileVariant
contextual_adaptive_huffman_profile_variant(
    const marc_lzss_contextual_profile profile) noexcept {
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_64M) {
        return marc::frame::internal::
            LzssContextualAdaptiveHuffmanProfileVariant::field_context_64m;
    }
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M) {
        return marc::frame::internal::
            LzssContextualAdaptiveHuffmanProfileVariant::field_context_16m;
    }
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_4M) {
        return marc::frame::internal::
            LzssContextualAdaptiveHuffmanProfileVariant::field_context_4m;
    }
    return profile == MARC_LZSS_CONTEXTUAL_PROFILE_1M
        ? marc::frame::internal::
              LzssContextualAdaptiveHuffmanProfileVariant::field_context_1m
        : marc::frame::internal::
              LzssContextualAdaptiveHuffmanProfileVariant::field_context_64k;
}

[[nodiscard]] marc::frame::internal::
    LzssContextualAdaptiveHuffmanStreamAdmission
contextual_adaptive_huffman_stream_admission(
    const marc_lzss_contextual_profile profile) noexcept {
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_64M) {
        return marc::frame::internal::
            LzssContextualAdaptiveHuffmanStreamAdmission::field_context_64m;
    }
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M) {
        return marc::frame::internal::
            LzssContextualAdaptiveHuffmanStreamAdmission::field_context_16m;
    }
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_4M) {
        return marc::frame::internal::
            LzssContextualAdaptiveHuffmanStreamAdmission::field_context_4m;
    }
    return profile == MARC_LZSS_CONTEXTUAL_PROFILE_1M
        ? marc::frame::internal::
              LzssContextualAdaptiveHuffmanStreamAdmission::field_context_1m
        : marc::frame::internal::
              LzssContextualAdaptiveHuffmanStreamAdmission::field_context_64k;
}

[[nodiscard]] bool known_lzss_match_finder_strategy(
    const marc_lzss_match_finder_strategy strategy) noexcept {
    return strategy == MARC_LZSS_MATCH_FINDER_HASH_CHAIN_EXACT
        || strategy == MARC_LZSS_MATCH_FINDER_BINARY_TREE_EXACT;
}

[[nodiscard]] marc::dictionary::internal::LzssMatchFinderStrategy
internal_lzss_match_finder_strategy(
    const marc_lzss_match_finder_strategy strategy) noexcept {
    return strategy == MARC_LZSS_MATCH_FINDER_BINARY_TREE_EXACT
        ? marc::dictionary::internal::LzssMatchFinderStrategy::binary_tree_exact
        : marc::dictionary::internal::LzssMatchFinderStrategy::hash_chain_exact;
}

bool load_config(
    const marc_lzss_contextual_dynamic_range_config* config,
    marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || !known_lzss_match_finder_strategy(
            config->match_finder_strategy)
        || config->reserved2 != 0
        || (config->profile != MARC_LZSS_CONTEXTUAL_PROFILE_64K
            && config->profile != MARC_LZSS_CONTEXTUAL_PROFILE_1M
            && config->profile != MARC_LZSS_CONTEXTUAL_PROFILE_4M
            && config->profile != MARC_LZSS_CONTEXTUAL_PROFILE_16M
            && config->profile != MARC_LZSS_CONTEXTUAL_PROFILE_64M)) {
        return false;
    }
    copy_frame_limits(*config, limits);
    limits.max_block_size = config->max_block_size;
    limits.max_compressed_payload_size =
        config->max_compressed_payload_size;
    limits.max_internal_buffered_bytes =
        config->max_internal_buffered_bytes;
    copy_match_limits(*config, limits);
    limits.max_entropy_table_entries =
        config->max_entropy_table_entries;
    limits.max_range_model_total = config->max_range_model_total;
    return true;
}

bool load_config(
    const marc_lzss_contextual_rans_config* config,
    marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || !known_lzss_match_finder_strategy(
            config->match_finder_strategy)
        || config->reserved2 != 0
        || (config->profile != MARC_LZSS_CONTEXTUAL_PROFILE_64K
            && config->profile != MARC_LZSS_CONTEXTUAL_PROFILE_1M
            && config->profile != MARC_LZSS_CONTEXTUAL_PROFILE_4M
            && config->profile != MARC_LZSS_CONTEXTUAL_PROFILE_16M
            && config->profile != MARC_LZSS_CONTEXTUAL_PROFILE_64M)) {
        return false;
    }
    copy_frame_limits(*config, limits);
    limits.max_block_size = config->max_block_size;
    limits.max_compressed_payload_size =
        config->max_compressed_payload_size;
    limits.max_internal_buffered_bytes =
        config->max_internal_buffered_bytes;
    copy_match_limits(*config, limits);
    limits.max_entropy_table_entries =
        config->max_entropy_table_entries;
    return true;
}

bool load_config(
    const marc_lzss_contextual_tans_config* config,
    marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || !known_lzss_match_finder_strategy(
            config->match_finder_strategy)
        || config->reserved2 != 0
        || (config->profile != MARC_LZSS_CONTEXTUAL_PROFILE_64K
            && config->profile != MARC_LZSS_CONTEXTUAL_PROFILE_1M
            && config->profile != MARC_LZSS_CONTEXTUAL_PROFILE_4M
            && config->profile != MARC_LZSS_CONTEXTUAL_PROFILE_16M
            && config->profile != MARC_LZSS_CONTEXTUAL_PROFILE_64M)) {
        return false;
    }
    copy_frame_limits(*config, limits);
    limits.max_block_size = config->max_block_size;
    limits.max_compressed_payload_size =
        config->max_compressed_payload_size;
    limits.max_internal_buffered_bytes =
        config->max_internal_buffered_bytes;
    copy_match_limits(*config, limits);
    limits.max_entropy_table_entries =
        config->max_entropy_table_entries;
    return true;
}

bool load_config(
    const marc_lzss_contextual_adaptive_huffman_config* config,
    marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || !known_lzss_match_finder_strategy(
            config->match_finder_strategy)
        || config->reserved2 != 0
        || (config->profile != MARC_LZSS_CONTEXTUAL_PROFILE_64K
            && config->profile != MARC_LZSS_CONTEXTUAL_PROFILE_1M
            && config->profile != MARC_LZSS_CONTEXTUAL_PROFILE_4M
            && config->profile != MARC_LZSS_CONTEXTUAL_PROFILE_16M
            && config->profile != MARC_LZSS_CONTEXTUAL_PROFILE_64M)) {
        return false;
    }
    copy_frame_limits(*config, limits);
    limits.max_block_size = config->max_block_size;
    limits.max_compressed_payload_size =
        config->max_compressed_payload_size;
    limits.max_internal_buffered_bytes =
        config->max_internal_buffered_bytes;
    copy_match_limits(*config, limits);
    limits.max_entropy_table_entries =
        config->max_entropy_table_entries;
    return true;
}

bool load_config(
    const marc_lzss_contextual_blocked_huffman_config* config,
    marc::core::DecoderLimits& limits) noexcept {
    if (!valid_config_header(config)
        || !known_lzss_match_finder_strategy(
            config->match_finder_strategy)
        || config->reserved2 != 0
        || (config->profile != MARC_LZSS_CONTEXTUAL_PROFILE_64K
            && config->profile != MARC_LZSS_CONTEXTUAL_PROFILE_1M
            && config->profile != MARC_LZSS_CONTEXTUAL_PROFILE_4M
            && config->profile != MARC_LZSS_CONTEXTUAL_PROFILE_16M
            && config->profile != MARC_LZSS_CONTEXTUAL_PROFILE_64M)) {
        return false;
    }
    copy_frame_limits(*config, limits);
    limits.max_block_size = config->max_block_size;
    limits.max_compressed_payload_size =
        config->max_compressed_payload_size;
    limits.max_internal_buffered_bytes =
        config->max_internal_buffered_bytes;
    copy_match_limits(*config, limits);
    limits.max_entropy_table_entries =
        config->max_entropy_table_entries;
    return true;
}

template <typename Config>
marc_status initialize_contextual_rans_config(
    const marc_direction direction, Config* const config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->match_finder_strategy =
        MARC_LZSS_MATCH_FINDER_HASH_CHAIN_EXACT;
    config->frame_size = UINT32_C(1) << 16;
    config->window_size = UINT32_C(1) << 16;
    config->min_match_length = 5;
    config->max_match_length = 258;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_block_size = limits.max_block_size;
    config->max_compressed_payload_size =
        limits.max_compressed_payload_size;
    config->max_internal_buffered_bytes =
        limits.max_internal_buffered_bytes;
    config->max_lz_distance = limits.max_lz_distance;
    config->max_lz_match_length = limits.max_lz_match_length;
    config->max_entropy_table_entries =
        limits.max_entropy_table_entries;
    return MARC_STATUS_OK;
}

template <typename Config>
marc_status contextual_rans_workspace_requirements(
    const Config* const config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzssParameters dictionary{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        marc::frame::internal::LzssContextualRansStreamHeader stream{};
        marc::frame::internal::
            LzssContextualRansEncoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::internal::make_lzss_contextual_rans_profile(
                {config->original_size, config->frame_size, dictionary,
                 contextual_rans_profile_variant(config->profile),
                 internal_lzss_match_finder_strategy(
                     config->match_finder_strategy)},
                limits, stream, needed);
        if (error != marc::frame::internal::
                         LzssContextualRansProfileError::none) {
            return status_for(
                marc::frame::internal::
                    lzss_contextual_rans_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_input_bytes;
        requirements->secondary_bytes = needed.frame_encoded_bytes;
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::internal::
            LzssContextualRansDecoderWorkspaceRequirements needed{};
        const auto error = marc::frame::internal::
            calculate_lzss_contextual_rans_decoder_workspace(
                limits, needed,
                contextual_rans_profile_variant(config->profile));
        if (error != marc::frame::internal::
                         LzssContextualRansProfileError::none) {
            return status_for(
                marc::frame::internal::
                    lzss_contextual_rans_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_encoded_bytes;
        requirements->secondary_bytes = needed.frame_decoded_bytes;
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

template <typename Config>
marc_status create_contextual_rans(
    const Config* const config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** const transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query =
        contextual_rans_workspace_requirements(config, &required);
    if (query != MARC_STATUS_OK) return query;
    if (!valid_buffer(primary_workspace.data, primary_workspace.size)
        || !valid_buffer(secondary_workspace.data, secondary_workspace.size)
        || !valid_buffer(views_workspace.data, views_workspace.size)
        || primary_workspace.size < required.primary_bytes
        || secondary_workspace.size < required.secondary_bytes
        || views_workspace.size < required.views_bytes
        || (required.views_bytes != 0
            && reinterpret_cast<std::uintptr_t>(views_workspace.data)
                % required.views_alignment != 0)
        || !disjoint_buffer_prefixes(
            primary_workspace, required.primary_bytes,
            secondary_workspace, required.secondary_bytes)
        || !disjoint_buffer_prefixes(
            primary_workspace, required.primary_bytes,
            views_workspace, required.views_bytes)
        || !disjoint_buffer_prefixes(
            secondary_workspace, required.secondary_bytes,
            views_workspace, required.views_bytes)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    const auto primary = std::span<std::byte>{
        reinterpret_cast<std::byte*>(primary_workspace.data),
        required.primary_bytes};
    const auto secondary = std::span<std::byte>{
        reinterpret_cast<std::byte*>(secondary_workspace.data),
        required.secondary_bytes};
    const auto views_storage = std::span<std::byte>{
        reinterpret_cast<std::byte*>(views_workspace.data),
        required.views_bytes};
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzssParameters dictionary{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        marc::frame::internal::LzssContextualRansStreamHeader stream{};
        marc::frame::internal::
            LzssContextualRansEncoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::internal::make_lzss_contextual_rans_profile(
                {config->original_size, config->frame_size, dictionary,
                 contextual_rans_profile_variant(config->profile),
                 internal_lzss_match_finder_strategy(
                     config->match_finder_strategy)},
                limits, stream, needed);
        if (error != marc::frame::internal::
                         LzssContextualRansProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::internal::LzssContextualRansEncoderViews views{};
        if (marc::frame::internal::
                partition_lzss_contextual_rans_encoder_views(
                    needed, views_storage, views)
            != marc::frame::internal::
                   LzssContextualRansWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow)
            marc::frame::internal::
                LzssContextualRansFrameStreamingEncoder(
                    stream, limits, primary, views.tokens,
                    views.match_finder, secondary,
                    needed.match_finder_strategy);
    } else {
        marc::frame::internal::
            LzssContextualRansDecoderWorkspaceRequirements needed{};
        const auto error = marc::frame::internal::
            calculate_lzss_contextual_rans_decoder_workspace(
                limits, needed,
                contextual_rans_profile_variant(config->profile));
        if (error != marc::frame::internal::
                         LzssContextualRansProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::internal::LzssContextualRansDecoderViews views{};
        if (marc::frame::internal::
                partition_lzss_contextual_rans_decoder_views(
                    needed, views_storage, views)
            != marc::frame::internal::
                   LzssContextualRansWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow)
            marc::frame::internal::
                LzssContextualRansFrameStreamingDecoder(
                    limits, primary, views.tables, views.tokens, secondary,
                    contextual_rans_stream_admission(
                        config->profile));
    }
    return publish_transform(implementation, transform);
}

marc_status contextual_tans_workspace_requirements(
    const marc_lzss_contextual_tans_config* const config,
    marc_workspace_requirements* const requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzssParameters dictionary{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        marc::frame::internal::LzssContextualTansStreamHeader stream{};
        marc::frame::internal::
            LzssContextualTansEncoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::internal::make_lzss_contextual_tans_profile(
                {config->original_size, config->frame_size, dictionary,
                 contextual_tans_profile_variant(config->profile),
                 internal_lzss_match_finder_strategy(
                     config->match_finder_strategy)},
                limits, stream, needed);
        if (error != marc::frame::internal::
                         LzssContextualTansProfileError::none) {
            return status_for(
                marc::frame::internal::
                    lzss_contextual_tans_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_input_bytes;
        requirements->secondary_bytes = needed.frame_encoded_bytes;
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::internal::
            LzssContextualTansDecoderWorkspaceRequirements needed{};
        const auto error = marc::frame::internal::
            calculate_lzss_contextual_tans_decoder_workspace(
                limits, needed,
                contextual_tans_profile_variant(config->profile));
        if (error != marc::frame::internal::
                         LzssContextualTansProfileError::none) {
            return status_for(
                marc::frame::internal::
                    lzss_contextual_tans_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_encoded_bytes;
        requirements->secondary_bytes = needed.frame_decoded_bytes;
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status create_contextual_tans(
    const marc_lzss_contextual_tans_config* const config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** const transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query = contextual_tans_workspace_requirements(
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
                % required.views_alignment != 0)
        || !disjoint_buffer_prefixes(
            primary_workspace, required.primary_bytes,
            secondary_workspace, required.secondary_bytes)
        || !disjoint_buffer_prefixes(
            primary_workspace, required.primary_bytes,
            views_workspace, required.views_bytes)
        || !disjoint_buffer_prefixes(
            secondary_workspace, required.secondary_bytes,
            views_workspace, required.views_bytes)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    const auto primary = std::span<std::byte>{
        reinterpret_cast<std::byte*>(primary_workspace.data),
        required.primary_bytes};
    const auto secondary = std::span<std::byte>{
        reinterpret_cast<std::byte*>(secondary_workspace.data),
        required.secondary_bytes};
    const auto views_storage = std::span<std::byte>{
        reinterpret_cast<std::byte*>(views_workspace.data),
        required.views_bytes};
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzssParameters dictionary{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        marc::frame::internal::LzssContextualTansStreamHeader stream{};
        marc::frame::internal::
            LzssContextualTansEncoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::internal::make_lzss_contextual_tans_profile(
                {config->original_size, config->frame_size, dictionary,
                 contextual_tans_profile_variant(config->profile),
                 internal_lzss_match_finder_strategy(
                     config->match_finder_strategy)},
                limits, stream, needed);
        if (error != marc::frame::internal::
                         LzssContextualTansProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::internal::LzssContextualTansEncoderViews views{};
        if (marc::frame::internal::
                partition_lzss_contextual_tans_encoder_views(
                    needed, views_storage, views)
            != marc::frame::internal::
                   LzssContextualTansWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow) marc::frame::internal::
            LzssContextualTansFrameStreamingEncoder(
                stream, limits, primary, views.tokens, views.tables,
                views.match_finder, secondary,
                needed.match_finder_strategy);
    } else {
        marc::frame::internal::
            LzssContextualTansDecoderWorkspaceRequirements needed{};
        const auto error = marc::frame::internal::
            calculate_lzss_contextual_tans_decoder_workspace(
                limits, needed,
                contextual_tans_profile_variant(config->profile));
        if (error != marc::frame::internal::
                         LzssContextualTansProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::internal::LzssContextualTansDecoderViews views{};
        if (marc::frame::internal::
                partition_lzss_contextual_tans_decoder_views(
                    needed, views_storage, views)
            != marc::frame::internal::
                   LzssContextualTansWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow) marc::frame::internal::
            LzssContextualTansFrameStreamingDecoder(
                limits, primary, views.tables, views.tokens, secondary,
                contextual_tans_stream_admission(config->profile));
    }
    return publish_transform(implementation, transform);
}

marc_status contextual_adaptive_huffman_workspace_requirements(
    const marc_lzss_contextual_adaptive_huffman_config* const config,
    marc_workspace_requirements* const requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzssParameters dictionary{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        marc::frame::internal::LzssContextualAdaptiveHuffmanStreamHeader
            stream{};
        marc::frame::internal::
        LzssContextualAdaptiveHuffmanEncoderWorkspaceRequirements
                needed{};
        const auto error = marc::frame::internal::
            make_lzss_contextual_adaptive_huffman_profile(
                {config->original_size, config->frame_size, dictionary,
                 contextual_adaptive_huffman_profile_variant(
                     config->profile),
                 internal_lzss_match_finder_strategy(
                     config->match_finder_strategy)},
                limits, stream, needed);
        if (error != marc::frame::internal::
                         LzssContextualAdaptiveHuffmanProfileError::none) {
            return status_for(marc::frame::internal::
                lzss_contextual_adaptive_huffman_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_input_bytes;
        requirements->secondary_bytes = needed.frame_encoded_bytes;
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::internal::
            LzssContextualAdaptiveHuffmanDecoderWorkspaceRequirements
                needed{};
        const auto error = marc::frame::internal::
            calculate_lzss_contextual_adaptive_huffman_decoder_workspace(
                limits, needed,
                contextual_adaptive_huffman_profile_variant(
                    config->profile));
        if (error != marc::frame::internal::
                         LzssContextualAdaptiveHuffmanProfileError::none) {
            return status_for(marc::frame::internal::
                lzss_contextual_adaptive_huffman_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_encoded_bytes;
        requirements->secondary_bytes = needed.frame_decoded_bytes;
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status create_contextual_adaptive_huffman(
    const marc_lzss_contextual_adaptive_huffman_config* const config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** const transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query = contextual_adaptive_huffman_workspace_requirements(
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
                % required.views_alignment != 0)
        || !disjoint_buffer_prefixes(
            primary_workspace, required.primary_bytes,
            secondary_workspace, required.secondary_bytes)
        || !disjoint_buffer_prefixes(
            primary_workspace, required.primary_bytes,
            views_workspace, required.views_bytes)
        || !disjoint_buffer_prefixes(
            secondary_workspace, required.secondary_bytes,
            views_workspace, required.views_bytes)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    const auto primary = std::span<std::byte>{
        reinterpret_cast<std::byte*>(primary_workspace.data),
        required.primary_bytes};
    const auto secondary = std::span<std::byte>{
        reinterpret_cast<std::byte*>(secondary_workspace.data),
        required.secondary_bytes};
    const auto views_storage = std::span<std::byte>{
        reinterpret_cast<std::byte*>(views_workspace.data),
        required.views_bytes};
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzssParameters dictionary{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        marc::frame::internal::LzssContextualAdaptiveHuffmanStreamHeader
            stream{};
        marc::frame::internal::
            LzssContextualAdaptiveHuffmanEncoderWorkspaceRequirements
                needed{};
        const auto error = marc::frame::internal::
            make_lzss_contextual_adaptive_huffman_profile(
                    {config->original_size, config->frame_size, dictionary,
                     contextual_adaptive_huffman_profile_variant(
                         config->profile),
                     internal_lzss_match_finder_strategy(
                         config->match_finder_strategy)},
                limits, stream, needed);
        if (error != marc::frame::internal::
                         LzssContextualAdaptiveHuffmanProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::internal::LzssContextualAdaptiveHuffmanEncoderViews
            views{};
        if (marc::frame::internal::
                partition_lzss_contextual_adaptive_huffman_encoder_views(
                    needed, views_storage, views)
            != marc::frame::internal::
                   LzssContextualAdaptiveHuffmanWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow) marc::frame::internal::
            LzssContextualAdaptiveHuffmanFrameStreamingEncoder(
                stream, limits, primary, views.tokens, views.nodes,
                views.symbols, views.match_finder, secondary,
                needed.match_finder_strategy);
    } else {
        marc::frame::internal::
            LzssContextualAdaptiveHuffmanDecoderWorkspaceRequirements
                needed{};
        const auto error = marc::frame::internal::
            calculate_lzss_contextual_adaptive_huffman_decoder_workspace(
                limits, needed,
                contextual_adaptive_huffman_profile_variant(
                    config->profile));
        if (error != marc::frame::internal::
                         LzssContextualAdaptiveHuffmanProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::internal::LzssContextualAdaptiveHuffmanDecoderViews
            views{};
        if (marc::frame::internal::
                partition_lzss_contextual_adaptive_huffman_decoder_views(
                    needed, views_storage, views)
            != marc::frame::internal::
                   LzssContextualAdaptiveHuffmanWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow) marc::frame::internal::
            LzssContextualAdaptiveHuffmanFrameStreamingDecoder(
                limits, primary, views.nodes, views.symbols, views.tokens,
                secondary, contextual_adaptive_huffman_stream_admission(
                               config->profile));
    }
    return publish_transform(implementation, transform);
}

marc_status contextual_blocked_huffman_workspace_requirements(
    const marc_lzss_contextual_blocked_huffman_config* const config,
    marc_workspace_requirements* const requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzssParameters dictionary{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        marc::frame::internal::LzssContextualBlockedHuffmanStreamHeader
            stream{};
        marc::frame::internal::
            LzssContextualBlockedHuffmanEncoderWorkspaceRequirements needed{};
        const auto error = marc::frame::internal::
            make_lzss_contextual_blocked_huffman_profile(
                {config->original_size, config->frame_size, dictionary,
                 contextual_blocked_huffman_profile_variant(
                     config->profile),
                 internal_lzss_match_finder_strategy(
                     config->match_finder_strategy)},
                limits, stream, needed);
        if (error != marc::frame::internal::
                         LzssContextualBlockedHuffmanProfileError::none) {
            return status_for(marc::frame::internal::
                lzss_contextual_blocked_huffman_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_input_bytes;
        requirements->secondary_bytes = needed.frame_encoded_bytes;
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::internal::
            LzssContextualBlockedHuffmanDecoderWorkspaceRequirements needed{};
        const auto error = marc::frame::internal::
            calculate_lzss_contextual_blocked_huffman_decoder_workspace(
                limits, needed,
                contextual_blocked_huffman_profile_variant(
                    config->profile));
        if (error != marc::frame::internal::
                         LzssContextualBlockedHuffmanProfileError::none) {
            return status_for(marc::frame::internal::
                lzss_contextual_blocked_huffman_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_encoded_bytes;
        requirements->secondary_bytes = needed.frame_decoded_bytes;
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status create_contextual_blocked_huffman(
    const marc_lzss_contextual_blocked_huffman_config* const config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** const transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query = contextual_blocked_huffman_workspace_requirements(
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
                % required.views_alignment != 0)
        || !disjoint_buffer_prefixes(
            primary_workspace, required.primary_bytes,
            secondary_workspace, required.secondary_bytes)
        || !disjoint_buffer_prefixes(
            primary_workspace, required.primary_bytes,
            views_workspace, required.views_bytes)
        || !disjoint_buffer_prefixes(
            secondary_workspace, required.secondary_bytes,
            views_workspace, required.views_bytes)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    const auto primary = std::span<std::byte>{
        reinterpret_cast<std::byte*>(primary_workspace.data),
        required.primary_bytes};
    const auto secondary = std::span<std::byte>{
        reinterpret_cast<std::byte*>(secondary_workspace.data),
        required.secondary_bytes};
    const auto views_storage = std::span<std::byte>{
        reinterpret_cast<std::byte*>(views_workspace.data),
        required.views_bytes};
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzssParameters dictionary{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        marc::frame::internal::LzssContextualBlockedHuffmanStreamHeader
            stream{};
        marc::frame::internal::
            LzssContextualBlockedHuffmanEncoderWorkspaceRequirements needed{};
        if (marc::frame::internal::
                make_lzss_contextual_blocked_huffman_profile(
                    {config->original_size, config->frame_size, dictionary,
                     contextual_blocked_huffman_profile_variant(
                         config->profile),
                     internal_lzss_match_finder_strategy(
                         config->match_finder_strategy)},
                    limits, stream, needed)
            != marc::frame::internal::
                   LzssContextualBlockedHuffmanProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::internal::LzssContextualBlockedHuffmanEncoderViews views{};
        if (marc::frame::internal::
                partition_lzss_contextual_blocked_huffman_encoder_views(
                    needed, views_storage, views)
            != marc::frame::internal::
                   LzssContextualBlockedHuffmanWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow) marc::frame::internal::
            LzssContextualBlockedHuffmanFrameStreamingEncoder(
                stream, limits, primary, views.tokens, views.match_finder,
                secondary, needed.match_finder_strategy);
    } else {
        marc::frame::internal::
            LzssContextualBlockedHuffmanDecoderWorkspaceRequirements needed{};
        if (marc::frame::internal::
                calculate_lzss_contextual_blocked_huffman_decoder_workspace(
                    limits, needed,
                    contextual_blocked_huffman_profile_variant(
                        config->profile))
            != marc::frame::internal::
                   LzssContextualBlockedHuffmanProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::internal::LzssContextualBlockedHuffmanDecoderViews views{};
        if (marc::frame::internal::
                partition_lzss_contextual_blocked_huffman_decoder_views(
                    needed, views_storage, views)
            != marc::frame::internal::
                   LzssContextualBlockedHuffmanWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow) marc::frame::internal::
            LzssContextualBlockedHuffmanFrameStreamingDecoder(
                limits, primary, views.tables, views.tokens, secondary,
                contextual_blocked_huffman_stream_admission(
                    config->profile));
    }
    return publish_transform(implementation, transform);
}

} // namespace

extern "C" {

marc_status marc_lzss_contextual_dynamic_range_config_init(
    const marc_direction direction,
    marc_lzss_contextual_dynamic_range_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    *config = {};
    config->struct_size = sizeof(*config);
    config->abi_version = MARC_ABI_VERSION;
    config->direction = direction;
    config->match_finder_strategy =
        MARC_LZSS_MATCH_FINDER_HASH_CHAIN_EXACT;
    config->frame_size = UINT32_C(1) << 16;
    config->window_size = UINT32_C(1) << 16;
    config->min_match_length = 5;
    config->max_match_length = 258;
    const marc::core::DecoderLimits limits{};
    config->max_total_output_size = limits.max_total_output_size;
    config->max_frame_size = limits.max_frame_size;
    config->max_block_size = limits.max_block_size;
    config->max_compressed_payload_size =
        limits.max_compressed_payload_size;
    config->max_internal_buffered_bytes =
        limits.max_internal_buffered_bytes;
    config->max_lz_distance = limits.max_lz_distance;
    config->max_lz_match_length = limits.max_lz_match_length;
    config->max_entropy_table_entries =
        limits.max_entropy_table_entries;
    config->max_range_model_total = limits.max_range_model_total;
    config->profile = MARC_LZSS_CONTEXTUAL_PROFILE_64K;
    return MARC_STATUS_OK;
}

marc_status marc_lzss_contextual_dynamic_range_config_apply_profile(
    marc_lzss_contextual_dynamic_range_config* const config,
    const marc_lzss_contextual_profile profile) noexcept {
    if (!valid_config_header(config)
        || (config->direction != MARC_DIRECTION_ENCODE
            && config->direction != MARC_DIRECTION_DECODE)
        || !known_lzss_match_finder_strategy(
            config->match_finder_strategy)
        || config->reserved2 != 0
        || (profile != MARC_LZSS_CONTEXTUAL_PROFILE_64K
            && profile != MARC_LZSS_CONTEXTUAL_PROFILE_1M
            && profile != MARC_LZSS_CONTEXTUAL_PROFILE_4M
            && profile != MARC_LZSS_CONTEXTUAL_PROFILE_16M
            && profile != MARC_LZSS_CONTEXTUAL_PROFILE_64M)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }

    auto applied = *config;
    std::uint32_t extent = UINT32_C(1) << 16;
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_1M) {
        extent = UINT32_C(1) << 20;
    } else if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_4M) {
        extent = UINT32_C(1) << 22;
    } else if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M) {
        extent = UINT32_C(1) << 24;
    } else if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_64M) {
        extent = UINT32_C(1) << 26;
    }
    std::uint64_t decisions_per_byte = 12;
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_4M
        || profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M) {
        decisions_per_byte = 14;
    } else if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_64M) {
        decisions_per_byte = 16;
    }
    const std::uint64_t payload =
        static_cast<std::uint64_t>(extent) * decisions_per_byte + 5;
    applied.frame_size = extent;
    applied.window_size = extent;
    applied.min_match_length = 5;
    applied.max_match_length = 258;
    applied.max_frame_size = extent;
    applied.max_block_size = extent;
    applied.max_compressed_payload_size = payload;
    applied.max_internal_buffered_bytes = UINT64_C(8) << 20;
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_1M) {
        applied.max_internal_buffered_bytes = UINT64_C(128) << 20;
    } else if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_4M) {
        applied.max_internal_buffered_bytes = UINT64_C(256) << 20;
    } else if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M) {
        applied.max_internal_buffered_bytes = UINT64_C(1) << 30;
    } else if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_64M) {
        applied.max_internal_buffered_bytes = UINT64_C(8) << 30;
    }
    applied.max_lz_distance = extent;
    applied.max_lz_match_length = 258;
    applied.max_entropy_table_entries = 4518;
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_1M) {
        applied.max_entropy_table_entries = 4550;
    } else if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_4M) {
        applied.max_entropy_table_entries = 4566;
    } else if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M) {
        applied.max_entropy_table_entries = 4582;
    } else if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_64M) {
        applied.max_entropy_table_entries = 4598;
    }
    applied.max_range_model_total = UINT64_C(1) << 15;
    applied.profile = profile;
    *config = applied;
    return MARC_STATUS_OK;
}

marc_status marc_lzss_contextual_dynamic_range_workspace_requirements(
    const marc_lzss_contextual_dynamic_range_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (requirements == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *requirements = {};
    requirements->struct_size = sizeof(*requirements);
    requirements->abi_version = MARC_ABI_VERSION;
    requirements->views_alignment = 1;
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzssParameters dictionary{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        marc::frame::internal::TypedContextStreamHeader stream{};
        marc::frame::internal::
            LzssTypedContextEncoderWorkspaceRequirements needed{};
        const auto error =
            marc::frame::internal::make_lzss_typed_context_profile(
                {config->original_size, config->frame_size, dictionary,
                 typed_context_profile_variant(config->profile),
                 internal_lzss_match_finder_strategy(
                     config->match_finder_strategy)},
                limits, stream, needed);
        if (error != marc::frame::internal::
                         LzssTypedContextProfileError::none) {
            return status_for(
                marc::frame::internal::
                    lzss_typed_context_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_input_bytes;
        requirements->secondary_bytes = needed.frame_encoded_bytes;
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    if (config->direction == MARC_DIRECTION_DECODE) {
        marc::frame::internal::
            LzssTypedContextDecoderWorkspaceRequirements needed{};
        const auto error = marc::frame::internal::
            calculate_lzss_typed_context_decoder_workspace(
                limits, needed,
                typed_context_profile_variant(config->profile));
        if (error != marc::frame::internal::
                         LzssTypedContextProfileError::none) {
            return status_for(
                marc::frame::internal::
                    lzss_typed_context_profile_error_code(error));
        }
        requirements->primary_bytes = needed.frame_encoded_bytes;
        requirements->secondary_bytes = needed.frame_decoded_bytes;
        requirements->views_bytes = needed.views_bytes;
        requirements->views_alignment = needed.views_alignment;
        return MARC_STATUS_OK;
    }
    return MARC_STATUS_INVALID_ARGUMENT;
}

marc_status marc_lzss_contextual_dynamic_range_create(
    const marc_lzss_contextual_dynamic_range_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    *transform = nullptr;
    marc_workspace_requirements required{};
    const auto query =
        marc_lzss_contextual_dynamic_range_workspace_requirements(
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
                % required.views_alignment != 0)
        || !disjoint_buffer_prefixes(
            primary_workspace, required.primary_bytes,
            secondary_workspace, required.secondary_bytes)
        || !disjoint_buffer_prefixes(
            primary_workspace, required.primary_bytes,
            views_workspace, required.views_bytes)
        || !disjoint_buffer_prefixes(
            secondary_workspace, required.secondary_bytes,
            views_workspace, required.views_bytes)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }
    marc::core::DecoderLimits limits{};
    if (!load_config(config, limits)) return MARC_STATUS_INVALID_ARGUMENT;
    const auto primary = std::span<std::byte>{
        reinterpret_cast<std::byte*>(primary_workspace.data),
        required.primary_bytes};
    const auto secondary = std::span<std::byte>{
        reinterpret_cast<std::byte*>(secondary_workspace.data),
        required.secondary_bytes};
    const auto views_storage = std::span<std::byte>{
        reinterpret_cast<std::byte*>(views_workspace.data),
        required.views_bytes};
    marc::core::Transform* implementation{};
    if (config->direction == MARC_DIRECTION_ENCODE) {
        const marc::dictionary::internal::LzssParameters dictionary{
            config->window_size, config->min_match_length,
            config->max_match_length, 0};
        marc::frame::internal::TypedContextStreamHeader stream{};
        marc::frame::internal::
            LzssTypedContextEncoderWorkspaceRequirements needed{};
        if (marc::frame::internal::make_lzss_typed_context_profile(
                {config->original_size, config->frame_size, dictionary,
                 typed_context_profile_variant(config->profile),
                 internal_lzss_match_finder_strategy(
                     config->match_finder_strategy)},
                limits, stream, needed)
            != marc::frame::internal::
                   LzssTypedContextProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::internal::LzssTypedContextEncoderViews views{};
        if (marc::frame::internal::partition_lzss_typed_context_encoder_views(
                needed, views_storage, views)
            != marc::frame::internal::
                   LzssTypedContextWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow)
            marc::frame::internal::LzssTypedContextFrameStreamingEncoder(
                stream, limits, primary, views.tokens, views.operations,
                views.match_finder, secondary,
                needed.match_finder_strategy);
    } else {
        marc::frame::internal::
            LzssTypedContextDecoderWorkspaceRequirements needed{};
        if (marc::frame::internal::
                calculate_lzss_typed_context_decoder_workspace(
                    limits, needed,
                    typed_context_profile_variant(config->profile))
            != marc::frame::internal::
                   LzssTypedContextProfileError::none) {
            return MARC_STATUS_INTERNAL_ERROR;
        }
        marc::frame::internal::LzssTypedContextDecoderViews views{};
        if (marc::frame::internal::partition_lzss_typed_context_decoder_views(
                needed, views_storage, views)
            != marc::frame::internal::
                   LzssTypedContextWorkspaceError::none) {
            return MARC_STATUS_INVALID_ARGUMENT;
        }
        implementation = new (std::nothrow)
            marc::frame::internal::LzssTypedContextFrameStreamingDecoder(
                limits, primary, views.tokens, secondary,
                typed_context_stream_admission(config->profile));
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lzss_contextual_rans_config_init(
    const marc_direction direction,
    marc_lzss_contextual_rans_config* config) noexcept {
    const auto status = initialize_contextual_rans_config(direction, config);
    if (status == MARC_STATUS_OK) {
        config->profile = MARC_LZSS_CONTEXTUAL_PROFILE_64K;
    }
    return status;
}

marc_status marc_lzss_contextual_rans_config_apply_profile(
    marc_lzss_contextual_rans_config* const config,
    const marc_lzss_contextual_profile profile) noexcept {
    if (!valid_config_header(config)
        || (config->direction != MARC_DIRECTION_ENCODE
            && config->direction != MARC_DIRECTION_DECODE)
        || !known_lzss_match_finder_strategy(
            config->match_finder_strategy)
        || config->reserved2 != 0
        || (profile != MARC_LZSS_CONTEXTUAL_PROFILE_64K
            && profile != MARC_LZSS_CONTEXTUAL_PROFILE_1M
            && profile != MARC_LZSS_CONTEXTUAL_PROFILE_4M
            && profile != MARC_LZSS_CONTEXTUAL_PROFILE_16M
            && profile != MARC_LZSS_CONTEXTUAL_PROFILE_64M)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }

    auto applied = *config;
    std::uint32_t extent = UINT32_C(1) << 16;
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_1M) {
        extent = UINT32_C(1) << 20;
    } else if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_4M) {
        extent = UINT32_C(1) << 22;
    } else if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M) {
        extent = UINT32_C(1) << 24;
    } else if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_64M) {
        extent = UINT32_C(1) << 26;
    }
    std::uint64_t decisions_per_byte = 6;
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_4M
        || profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M) {
        decisions_per_byte = 7;
    } else if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_64M) {
        decisions_per_byte = 8;
    }
    const std::uint64_t payload_bytes_per_raw = decisions_per_byte * 2;
    applied.frame_size = extent;
    applied.window_size = extent;
    applied.min_match_length = 5;
    applied.max_match_length = 258;
    applied.max_frame_size = extent;
    applied.max_block_size =
        static_cast<std::uint64_t>(extent) * decisions_per_byte;
    applied.max_compressed_payload_size =
        static_cast<std::uint64_t>(extent) * payload_bytes_per_raw + 8;
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_64K) {
        applied.max_internal_buffered_bytes = UINT64_C(8) << 20;
    } else if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M) {
        applied.max_internal_buffered_bytes = UINT64_C(512) << 20;
    } else if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_64M) {
        applied.max_internal_buffered_bytes = UINT64_C(4) << 30;
    } else {
        applied.max_internal_buffered_bytes = UINT64_C(128) << 20;
    }
    applied.max_lz_distance = extent;
    applied.max_lz_match_length = 258;
    applied.max_entropy_table_entries = 126976;
    applied.profile = profile;
    *config = applied;
    return MARC_STATUS_OK;
}

marc_status marc_lzss_contextual_rans_workspace_requirements(
    const marc_lzss_contextual_rans_config* config,
    marc_workspace_requirements* requirements) noexcept {
    return contextual_rans_workspace_requirements(config, requirements);
}

marc_status marc_lzss_contextual_rans_create(
    const marc_lzss_contextual_rans_config* config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** transform) noexcept {
    return create_contextual_rans(
        config, primary_workspace, secondary_workspace, views_workspace,
        transform);
}

marc_status marc_lzss_contextual_tans_config_init(
    const marc_direction direction,
    marc_lzss_contextual_tans_config* const config) noexcept {
    const auto status = initialize_contextual_rans_config(direction, config);
    if (status == MARC_STATUS_OK) {
        config->profile = MARC_LZSS_CONTEXTUAL_PROFILE_64K;
    }
    return status;
}

marc_status marc_lzss_contextual_tans_config_apply_profile(
    marc_lzss_contextual_tans_config* const config,
    const marc_lzss_contextual_profile profile) noexcept {
    if (!valid_config_header(config)
        || (config->direction != MARC_DIRECTION_ENCODE
            && config->direction != MARC_DIRECTION_DECODE)
        || !known_lzss_match_finder_strategy(
            config->match_finder_strategy)
        || config->reserved2 != 0
        || (profile != MARC_LZSS_CONTEXTUAL_PROFILE_64K
            && profile != MARC_LZSS_CONTEXTUAL_PROFILE_1M
            && profile != MARC_LZSS_CONTEXTUAL_PROFILE_4M
            && profile != MARC_LZSS_CONTEXTUAL_PROFILE_16M
            && profile != MARC_LZSS_CONTEXTUAL_PROFILE_64M)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }

    auto applied = *config;
    std::uint32_t extent = UINT32_C(1) << 16;
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_1M) {
        extent = UINT32_C(1) << 20;
    } else if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_4M) {
        extent = UINT32_C(1) << 22;
    } else if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M) {
        extent = UINT32_C(1) << 24;
    } else if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_64M) {
        extent = UINT32_C(1) << 26;
    }
    std::uint64_t decisions_per_byte = 6;
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_4M
        || profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M) {
        decisions_per_byte = 7;
    } else if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_64M) {
        decisions_per_byte = 8;
    }
    const std::uint64_t payload =
        static_cast<std::uint64_t>(extent) * decisions_per_byte * 3 / 2 + 2;
    applied.frame_size = extent;
    applied.window_size = extent;
    applied.min_match_length = 5;
    applied.max_match_length = 258;
    applied.max_frame_size = extent;
    applied.max_block_size =
        static_cast<std::uint64_t>(extent) * decisions_per_byte;
    applied.max_compressed_payload_size = payload;
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_64K) {
        applied.max_internal_buffered_bytes = UINT64_C(8) << 20;
    } else if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_64M) {
        applied.max_internal_buffered_bytes = UINT64_C(4) << 30;
    } else if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M) {
        applied.max_internal_buffered_bytes = UINT64_C(512) << 20;
    } else {
        applied.max_internal_buffered_bytes = UINT64_C(128) << 20;
    }
    applied.max_lz_distance = extent;
    applied.max_lz_match_length = 258;
    applied.max_entropy_table_entries = 131072;
    applied.profile = profile;
    *config = applied;
    return MARC_STATUS_OK;
}

marc_status marc_lzss_contextual_tans_workspace_requirements(
    const marc_lzss_contextual_tans_config* const config,
    marc_workspace_requirements* const requirements) noexcept {
    return contextual_tans_workspace_requirements(config, requirements);
}

marc_status marc_lzss_contextual_tans_create(
    const marc_lzss_contextual_tans_config* const config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** const transform) noexcept {
    return create_contextual_tans(
        config, primary_workspace, secondary_workspace, views_workspace,
        transform);
}

marc_status marc_lzss_contextual_adaptive_huffman_config_init(
    const marc_direction direction,
    marc_lzss_contextual_adaptive_huffman_config* const config) noexcept {
    const auto status = initialize_contextual_rans_config(direction, config);
    if (status == MARC_STATUS_OK) {
        config->profile = MARC_LZSS_CONTEXTUAL_PROFILE_64K;
    }
    return status;
}

marc_status
marc_lzss_contextual_adaptive_huffman_config_apply_profile(
    marc_lzss_contextual_adaptive_huffman_config* const config,
    const marc_lzss_contextual_profile profile) noexcept {
    if (!valid_config_header(config)
        || (config->direction != MARC_DIRECTION_ENCODE
            && config->direction != MARC_DIRECTION_DECODE)
        || !known_lzss_match_finder_strategy(
            config->match_finder_strategy)
        || config->reserved2 != 0
        || (profile != MARC_LZSS_CONTEXTUAL_PROFILE_64K
            && profile != MARC_LZSS_CONTEXTUAL_PROFILE_1M
            && profile != MARC_LZSS_CONTEXTUAL_PROFILE_4M
            && profile != MARC_LZSS_CONTEXTUAL_PROFILE_16M
            && profile != MARC_LZSS_CONTEXTUAL_PROFILE_64M)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }

    auto applied = *config;
    std::uint32_t extent = UINT32_C(1) << 16;
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_1M) {
        extent = UINT32_C(1) << 20;
    } else if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_4M) {
        extent = UINT32_C(1) << 22;
    } else if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M) {
        extent = UINT32_C(1) << 24;
    } else if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_64M) {
        extent = UINT32_C(1) << 26;
    }
    const std::uint64_t payload =
        (static_cast<std::uint64_t>(extent) * UINT64_C(267)
         + UINT64_C(7)) / UINT64_C(8);
    applied.frame_size = extent;
    applied.window_size = extent;
    applied.min_match_length = 5;
    applied.max_match_length = 258;
    applied.max_frame_size = extent;
    applied.max_block_size = extent;
    applied.max_compressed_payload_size = payload;
    applied.max_internal_buffered_bytes =
        profile == MARC_LZSS_CONTEXTUAL_PROFILE_64M
        ? UINT64_C(8) << 30
        : profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M
        ? UINT64_C(1) << 30
        : profile == MARC_LZSS_CONTEXTUAL_PROFILE_4M
            ? UINT64_C(256) << 20
            : profile == MARC_LZSS_CONTEXTUAL_PROFILE_1M
                ? UINT64_C(128) << 20
                : UINT64_C(8) << 20;
    applied.max_lz_distance = extent;
    applied.max_lz_match_length = 258;
    applied.max_entropy_table_entries =
        profile == MARC_LZSS_CONTEXTUAL_PROFILE_64M
        ? UINT64_C(13825)
        : profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M
        ? UINT64_C(13777)
        : profile == MARC_LZSS_CONTEXTUAL_PROFILE_4M
            ? UINT64_C(13729)
            : profile == MARC_LZSS_CONTEXTUAL_PROFILE_1M
                ? UINT64_C(13681)
                : UINT64_C(13585);
    applied.profile = profile;
    *config = applied;
    return MARC_STATUS_OK;
}

marc_status
marc_lzss_contextual_adaptive_huffman_workspace_requirements(
    const marc_lzss_contextual_adaptive_huffman_config* const config,
    marc_workspace_requirements* const requirements) noexcept {
    return contextual_adaptive_huffman_workspace_requirements(
        config, requirements);
}

marc_status marc_lzss_contextual_adaptive_huffman_create(
    const marc_lzss_contextual_adaptive_huffman_config* const config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** const transform) noexcept {
    return create_contextual_adaptive_huffman(
        config, primary_workspace, secondary_workspace, views_workspace,
        transform);
}

marc_status marc_lzss_contextual_blocked_huffman_config_init(
    const marc_direction direction,
    marc_lzss_contextual_blocked_huffman_config* const config) noexcept {
    const auto status = initialize_contextual_rans_config(direction, config);
    if (status == MARC_STATUS_OK) {
        config->profile = MARC_LZSS_CONTEXTUAL_PROFILE_64K;
    }
    return status;
}

marc_status marc_lzss_contextual_blocked_huffman_config_apply_profile(
    marc_lzss_contextual_blocked_huffman_config* const config,
    const marc_lzss_contextual_profile profile) noexcept {
    if (!valid_config_header(config)
        || (config->direction != MARC_DIRECTION_ENCODE
            && config->direction != MARC_DIRECTION_DECODE)
        || !known_lzss_match_finder_strategy(
            config->match_finder_strategy)
        || config->reserved2 != 0
        || (profile != MARC_LZSS_CONTEXTUAL_PROFILE_64K
            && profile != MARC_LZSS_CONTEXTUAL_PROFILE_1M
            && profile != MARC_LZSS_CONTEXTUAL_PROFILE_4M
            && profile != MARC_LZSS_CONTEXTUAL_PROFILE_16M
            && profile != MARC_LZSS_CONTEXTUAL_PROFILE_64M)) {
        return MARC_STATUS_INVALID_ARGUMENT;
    }

    auto applied = *config;
    std::uint32_t extent = UINT32_C(1) << 16;
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_1M) {
        extent = UINT32_C(1) << 20;
    } else if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_4M) {
        extent = UINT32_C(1) << 22;
    } else if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M) {
        extent = UINT32_C(1) << 24;
    }
    if (profile == MARC_LZSS_CONTEXTUAL_PROFILE_64M) extent = UINT32_C(1) << 26;
    const std::uint64_t decisions_per_byte =
        profile == MARC_LZSS_CONTEXTUAL_PROFILE_64M ? 8 :
        profile == MARC_LZSS_CONTEXTUAL_PROFILE_4M
                || profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M
            ? 7
            : 6;
    const std::uint64_t payload =
        profile == MARC_LZSS_CONTEXTUAL_PROFILE_64M
        ? static_cast<std::uint64_t>(extent) * 15 :
        profile == MARC_LZSS_CONTEXTUAL_PROFILE_4M
                || profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M
        ? static_cast<std::uint64_t>(extent) * 105 / 8
        : static_cast<std::uint64_t>(extent) * 12;
    applied.frame_size = extent;
    applied.window_size = extent;
    applied.min_match_length = 5;
    applied.max_match_length = 258;
    applied.max_frame_size = extent;
    applied.max_block_size =
        static_cast<std::uint64_t>(extent) * decisions_per_byte;
    applied.max_compressed_payload_size = payload;
    applied.max_internal_buffered_bytes =
        profile == MARC_LZSS_CONTEXTUAL_PROFILE_64M ? UINT64_C(4) << 30 :
        profile == MARC_LZSS_CONTEXTUAL_PROFILE_64K
        ? UINT64_C(8) << 20
        : profile == MARC_LZSS_CONTEXTUAL_PROFILE_16M
            ? UINT64_C(512) << 20
            : UINT64_C(128) << 20;
    applied.max_lz_distance = extent;
    applied.max_lz_match_length = 258;
    applied.max_entropy_table_entries = 17885;
    applied.profile = profile;
    *config = applied;
    return MARC_STATUS_OK;
}

marc_status marc_lzss_contextual_blocked_huffman_workspace_requirements(
    const marc_lzss_contextual_blocked_huffman_config* const config,
    marc_workspace_requirements* const requirements) noexcept {
    return contextual_blocked_huffman_workspace_requirements(
        config, requirements);
}

marc_status marc_lzss_contextual_blocked_huffman_create(
    const marc_lzss_contextual_blocked_huffman_config* const config,
    const marc_buffer primary_workspace,
    const marc_buffer secondary_workspace,
    const marc_buffer views_workspace,
    marc_transform** const transform) noexcept {
    return create_contextual_blocked_huffman(
        config, primary_workspace, secondary_workspace, views_workspace,
        transform);
}

} // extern "C"
