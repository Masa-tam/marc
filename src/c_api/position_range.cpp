#include "c_api/common.hpp"
#include "c_api/configuration.hpp"
#include "frame/lzss_position_distance_1m_encode_workspace.hpp"
#include "frame/lzss_position_distance_1m_workspace.hpp"
#include "frame/lzss_position_distance_1m_frame_streaming_encoder.hpp"
#include "frame/lzss_position_distance_1m_prepared_frame_streaming_encoder.hpp"
#include "frame/lzss_position_distance_1m_finder_scratch_frame_streaming_encoder.hpp"
#include "frame/lzss_position_distance_1m_frame_streaming_decoder.hpp"
#include "frame/lzss_position_distance_workspace.hpp"
#include "frame/lzss_position_distance_frame_streaming_encoder.hpp"
#include "frame/lzss_position_distance_frame_streaming_decoder.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <span>

namespace {
using marc::c_api::status_for;
using namespace marc::c_api::detail;



} // namespace

extern "C" {

marc_status marc_lzss_position_distance_dynamic_range_config_init(
    const marc_direction direction,
    marc_lzss_position_distance_dynamic_range_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE)) return MARC_STATUS_INVALID_ARGUMENT;
    marc_lzss_position_distance_dynamic_range_config result{};
    result.struct_size = sizeof(result);
    result.abi_version = MARC_ABI_VERSION;
    result.direction = direction;
    result.frame_size = 65536;
    result.max_total_output_size = UINT64_C(1) << 40;
    result.max_frame_size = 65536;
    result.max_block_size = 65536;
    result.max_compressed_payload_size = 1179653;
    result.max_internal_buffered_bytes = UINT64_C(128) << 20;
    result.max_lz_distance = 65536;
    result.max_lz_match_length = 258;
    result.max_entropy_table_entries = 2522;
    result.max_range_model_total = 32768;
    result.max_expansion_ratio = 1024;
    result.expansion_slack = UINT64_C(1) << 20;
    *config = result;
    return MARC_STATUS_OK;
}

static marc_status prepare_position_distance_config(
    const marc_lzss_position_distance_dynamic_range_config* config,
    marc::core::DecoderLimits& limits,
    marc::frame::internal::TypedContextStreamHeader& stream,
    marc::frame::internal::LzssPositionDistanceWorkspaceRequirements& r) noexcept {
    using namespace marc::frame::internal;
    if (!valid_config_header(config)
        || config->reserved != 0 || config->reserved2 != 0
        || (config->direction != MARC_DIRECTION_ENCODE
            && config->direction != MARC_DIRECTION_DECODE))
        return MARC_STATUS_INVALID_ARGUMENT;
    limits = {};
    copy_frame_limits(*config, limits);
    limits.max_block_size = config->max_block_size;
    limits.max_compressed_payload_size = config->max_compressed_payload_size;
    limits.max_internal_buffered_bytes = config->max_internal_buffered_bytes;
    copy_match_limits(*config, limits);
    limits.max_entropy_table_entries = config->max_entropy_table_entries;
    limits.max_range_model_total = config->max_range_model_total;
    limits.max_expansion_ratio = config->max_expansion_ratio;
    limits.expansion_slack = config->expansion_slack;
    if (marc::core::validate_limits(limits) != marc::core::LimitError::none)
        return MARC_STATUS_INVALID_ARGUMENT;
    const bool encode = config->direction == MARC_DIRECTION_ENCODE;
    if (encode && (config->frame_size == 0 || config->frame_size > 65536))
        return MARC_STATUS_INVALID_ARGUMENT;
    // Reserve the opaque C handle before querying private owner/model storage.
    if (limits.max_internal_buffered_bytes <= sizeof(marc_transform)
        || limits.max_internal_buffered_bytes - sizeof(marc_transform)
            < limits.max_block_size) return MARC_STATUS_LIMIT_EXCEEDED;
    limits.max_internal_buffered_bytes -= sizeof(marc_transform);
    const auto frame_size = encode ? config->frame_size
        : static_cast<std::uint32_t>(std::min(config->max_frame_size, UINT64_C(65536)));
    stream = {frame_size,
        encode ? config->original_size : UINT64_C(0),
        {65536, 3, 258, 0}, 32768, 40, 8, 1, 9};
    const auto error = calculate_lzss_position_distance_workspace(stream, limits,
        encode ? LzssPositionDistanceWorkspaceDirection::encode
               : LzssPositionDistanceWorkspaceDirection::decode,
        encode ? sizeof(LzssPositionDistanceFrameStreamingEncoder)
               : sizeof(LzssPositionDistanceFrameStreamingDecoder), r);
    if (error != LzssPositionDistanceWorkspaceError::none)
        return error == LzssPositionDistanceWorkspaceError::limit_exceeded
            || error == LzssPositionDistanceWorkspaceError::arithmetic_overflow
            ? MARC_STATUS_LIMIT_EXCEEDED : MARC_STATUS_INVALID_ARGUMENT;
    return MARC_STATUS_OK;
}

marc_status marc_lzss_position_distance_dynamic_range_workspace_requirements(
    const marc_lzss_position_distance_dynamic_range_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (config == nullptr || requirements == nullptr
        || marc::core::check_buffer_overlap(config, sizeof(*config), requirements,
            sizeof(*requirements)) != marc::core::BufferOverlap::disjoint)
        return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::DecoderLimits limits{};
    marc::frame::internal::TypedContextStreamHeader stream{};
    marc::frame::internal::LzssPositionDistanceWorkspaceRequirements r{};
    const auto status = prepare_position_distance_config(config, limits, stream, r);
    if (status != MARC_STATUS_OK) return status;
    const bool encode = config->direction == MARC_DIRECTION_ENCODE;
    *requirements = {sizeof(*requirements), MARC_ABI_VERSION,
        encode ? r.raw_bytes : r.serialized_bytes,
        encode ? r.serialized_bytes : r.raw_bytes, r.views_bytes, r.views_alignment};
    return MARC_STATUS_OK;
}

marc_status marc_lzss_position_distance_dynamic_range_create(
    const marc_lzss_position_distance_dynamic_range_config* config,
    const marc_buffer primary, const marc_buffer secondary,
    const marc_buffer views, marc_transform** transform) noexcept {
    using namespace marc::frame::internal;
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    const auto disjoint = [](const void* a, std::size_t an,
                             const void* b, std::size_t bn) noexcept {
        return marc::core::check_buffer_overlap(a, an, b, bn)
            == marc::core::BufferOverlap::disjoint;
    };
    if (config != nullptr && !disjoint(config, sizeof(*config), transform, sizeof(*transform)))
        return MARC_STATUS_INVALID_ARGUMENT;
    for (const auto storage : {primary, secondary, views}) {
        if (!disjoint(storage.data, storage.size, transform, sizeof(*transform)))
            return MARC_STATUS_INVALID_ARGUMENT;
    }
    *transform = nullptr;
    if (!valid_buffer(primary.data, primary.size)
        || !valid_buffer(secondary.data, secondary.size)
        || !valid_buffer(views.data, views.size)) return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::DecoderLimits limits{};
    TypedContextStreamHeader stream{};
    LzssPositionDistanceWorkspaceRequirements r{};
    const auto status = prepare_position_distance_config(config, limits, stream, r);
    if (status != MARC_STATUS_OK) return status;
    const bool encode = config->direction == MARC_DIRECTION_ENCODE;
    const auto raw = encode ? primary : secondary;
    const auto serialized = encode ? secondary : primary;
    if (raw.size < r.raw_bytes || serialized.size < r.serialized_bytes
        || views.size < r.views_bytes
        || reinterpret_cast<std::uintptr_t>(views.data) % r.views_alignment != 0)
        return MARC_STATUS_INVALID_ARGUMENT;
    const marc_buffer prefixes[]{{raw.data, r.raw_bytes},
        {serialized.data, r.serialized_bytes}, {views.data, r.views_bytes}};
    for (std::size_t i = 0; i < 3; ++i) {
        if (!disjoint(prefixes[i].data, prefixes[i].size, config, sizeof(*config)))
            return MARC_STATUS_INVALID_ARGUMENT;
        for (std::size_t j = 0; j < i; ++j)
            if (!disjoint(prefixes[i].data, prefixes[i].size, prefixes[j].data, prefixes[j].size))
                return MARC_STATUS_INVALID_ARGUMENT;
    }
    const std::span raw_span{reinterpret_cast<std::byte*>(raw.data), r.raw_bytes};
    const std::span serialized_span{reinterpret_cast<std::byte*>(serialized.data), r.serialized_bytes};
    const std::span views_span{reinterpret_cast<std::byte*>(views.data), r.views_bytes};
    marc::core::Transform* implementation{};
    if (encode) {
        implementation = new (std::nothrow) LzssPositionDistanceFrameStreamingEncoder(
            stream, limits, raw_span, serialized_span, views_span);
    } else {
        LzssPositionDistanceWorkspaceViews partition{};
        const auto error = partition_lzss_position_distance_workspace(stream, limits,
            LzssPositionDistanceWorkspaceDirection::decode,
            sizeof(LzssPositionDistanceFrameStreamingDecoder), raw_span,
            serialized_span, views_span, partition);
        if (error != LzssPositionDistanceWorkspaceError::none)
            return MARC_STATUS_INTERNAL_ERROR; // All inputs were checked above.
        implementation = new (std::nothrow) LzssPositionDistanceFrameStreamingDecoder(
            limits, serialized_span, partition.tokens, raw_span);
    }
    return publish_transform(implementation, transform);
}

marc_status marc_lzss_position_distance_dynamic_range_1m_config_init(
    const marc_direction direction,
    marc_lzss_position_distance_dynamic_range_1m_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE)) return MARC_STATUS_INVALID_ARGUMENT;
    marc_lzss_position_distance_dynamic_range_1m_config result{};
    result.struct_size = sizeof(result);
    result.abi_version = MARC_ABI_VERSION;
    result.direction = direction;
    result.frame_size = 1048576;
    result.max_total_output_size = UINT64_C(1) << 40;
    result.max_frame_size = 1048576;
    result.max_block_size = 1048576;
    result.max_compressed_payload_size = 18874373;
    result.max_internal_buffered_bytes = UINT64_C(128) << 20;
    result.max_lz_distance = 1048576;
    result.max_lz_match_length = 258;
    result.max_entropy_table_entries = 2566;
    result.max_range_model_total = 32768;
    result.max_expansion_ratio = 1024;
    result.expansion_slack = UINT64_C(1) << 20;
    *config = result;
    return MARC_STATUS_OK;
}

static marc_status prepare_position_distance_1m_config(
    const marc_lzss_position_distance_dynamic_range_1m_config* config,
    marc::core::DecoderLimits& limits,
    marc::frame::internal::TypedContextStreamHeader& stream,
    marc::frame::internal::LzssPositionDistanceWorkspaceRequirements& r) noexcept {
    using namespace marc::frame::internal;
    if (!valid_config_header(config)
        || config->reserved != 0 || config->reserved2 != 0
        || (config->direction != MARC_DIRECTION_ENCODE
            && config->direction != MARC_DIRECTION_DECODE))
        return MARC_STATUS_INVALID_ARGUMENT;
    limits = {};
    copy_frame_limits(*config, limits);
    limits.max_block_size = config->max_block_size;
    limits.max_compressed_payload_size = config->max_compressed_payload_size;
    limits.max_internal_buffered_bytes = config->max_internal_buffered_bytes;
    copy_match_limits(*config, limits);
    limits.max_entropy_table_entries = config->max_entropy_table_entries;
    limits.max_range_model_total = config->max_range_model_total;
    limits.max_expansion_ratio = config->max_expansion_ratio;
    limits.expansion_slack = config->expansion_slack;
    if (marc::core::validate_limits(limits) != marc::core::LimitError::none)
        return MARC_STATUS_INVALID_ARGUMENT;
    const bool encode = config->direction == MARC_DIRECTION_ENCODE;
    if (encode && (config->frame_size == 0 || config->frame_size > 1048576))
        return MARC_STATUS_INVALID_ARGUMENT;
    // Reserve the opaque C handle before querying private owner/model storage.
    if (limits.max_internal_buffered_bytes <= sizeof(marc_transform)
        || limits.max_internal_buffered_bytes - sizeof(marc_transform)
            < limits.max_block_size) return MARC_STATUS_LIMIT_EXCEEDED;
    limits.max_internal_buffered_bytes -= sizeof(marc_transform);
    const auto frame_size = encode ? config->frame_size
        : static_cast<std::uint32_t>(std::min(config->max_frame_size, UINT64_C(1048576)));
    stream = {frame_size,
        encode ? config->original_size : UINT64_C(0),
        {1048576, 3, 258, 0}, 32768, 44, 9, 1, 10};
    if (encode) {
        const auto error = calculate_lzss_position_distance_1m_encode_workspace(stream,limits,
            LzssPositionDistanceWorkspaceDirection::encode,sizeof(LzssPositionDistance1mFinderScratchFrameStreamingEncoder),r,
            marc::dictionary::internal::LzssPositionDistance1mSearch::indexed_five_prefix);
        if(error!=LzssPositionDistanceWorkspaceError::none)
            return error==LzssPositionDistanceWorkspaceError::limit_exceeded
                || error==LzssPositionDistanceWorkspaceError::arithmetic_overflow
                ? MARC_STATUS_LIMIT_EXCEEDED : MARC_STATUS_INVALID_ARGUMENT;
    } else {
        LzssPositionDistance1mDecodeWorkspace decoded{};
        const auto error=calculate_lzss_position_distance_1m_decode_workspace(frame_size,limits,
            sizeof(LzssPositionDistance1mFrameStreamingDecoder),decoded);
        if(error!=marc::core::ErrorCode::none)
            return error==marc::core::ErrorCode::limit_exceeded?MARC_STATUS_LIMIT_EXCEEDED:MARC_STATUS_INVALID_ARGUMENT;
        r.raw_bytes=decoded.raw_bytes;r.serialized_bytes=decoded.serialized_bytes;
        r.token_count=decoded.token_count;r.views_bytes=decoded.token_bytes;
        r.views_alignment=alignof(marc::dictionary::internal::LzssTypedToken);
        r.aggregate_bytes=decoded.aggregate_bytes;
        limits.max_frame_size=frame_size;
    }
    return MARC_STATUS_OK;
}

marc_status marc_lzss_position_distance_dynamic_range_1m_workspace_requirements(
    const marc_lzss_position_distance_dynamic_range_1m_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (config == nullptr || requirements == nullptr
        || marc::core::check_buffer_overlap(config, sizeof(*config), requirements,
            sizeof(*requirements)) != marc::core::BufferOverlap::disjoint)
        return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::DecoderLimits limits{};
    marc::frame::internal::TypedContextStreamHeader stream{};
    marc::frame::internal::LzssPositionDistanceWorkspaceRequirements r{};
    const auto status = prepare_position_distance_1m_config(config, limits, stream, r);
    if (status != MARC_STATUS_OK) return status;
    const bool encode = config->direction == MARC_DIRECTION_ENCODE;
    *requirements = {sizeof(*requirements), MARC_ABI_VERSION,
        encode ? r.raw_bytes : r.serialized_bytes,
        encode ? r.serialized_bytes : r.raw_bytes, r.views_bytes, r.views_alignment};
    return MARC_STATUS_OK;
}

marc_status marc_lzss_position_distance_dynamic_range_1m_create(
    const marc_lzss_position_distance_dynamic_range_1m_config* config,
    const marc_buffer primary, const marc_buffer secondary,
    const marc_buffer views, marc_transform** transform) noexcept {
    using namespace marc::frame::internal;
    if (transform == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
    const auto disjoint = [](const void* a, std::size_t an,
                             const void* b, std::size_t bn) noexcept {
        return marc::core::check_buffer_overlap(a, an, b, bn)
            == marc::core::BufferOverlap::disjoint;
    };
    if (config != nullptr && !disjoint(config, sizeof(*config), transform, sizeof(*transform)))
        return MARC_STATUS_INVALID_ARGUMENT;
    for (const auto storage : {primary, secondary, views}) {
        if (!disjoint(storage.data, storage.size, transform, sizeof(*transform)))
            return MARC_STATUS_INVALID_ARGUMENT;
    }
    *transform = nullptr;
    if (!valid_buffer(primary.data, primary.size)
        || !valid_buffer(secondary.data, secondary.size)
        || !valid_buffer(views.data, views.size)) return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::DecoderLimits limits{};
    TypedContextStreamHeader stream{};
    LzssPositionDistanceWorkspaceRequirements r{};
    const auto status = prepare_position_distance_1m_config(config, limits, stream, r);
    if (status != MARC_STATUS_OK) return status;
    const bool encode = config->direction == MARC_DIRECTION_ENCODE;
    const auto raw = encode ? primary : secondary;
    const auto serialized = encode ? secondary : primary;
    if (raw.size < r.raw_bytes || serialized.size < r.serialized_bytes
        || views.size < r.views_bytes
        || reinterpret_cast<std::uintptr_t>(views.data) % r.views_alignment != 0)
        return MARC_STATUS_INVALID_ARGUMENT;
    const marc_buffer prefixes[]{{raw.data, r.raw_bytes},
        {serialized.data, r.serialized_bytes}, {views.data, r.views_bytes}};
    for (std::size_t i = 0; i < 3; ++i) {
        if (!disjoint(prefixes[i].data, prefixes[i].size, config, sizeof(*config)))
            return MARC_STATUS_INVALID_ARGUMENT;
        for (std::size_t j = 0; j < i; ++j)
            if (!disjoint(prefixes[i].data, prefixes[i].size, prefixes[j].data, prefixes[j].size))
                return MARC_STATUS_INVALID_ARGUMENT;
    }
    const std::span raw_span{reinterpret_cast<std::byte*>(raw.data), r.raw_bytes};
    const std::span serialized_span{reinterpret_cast<std::byte*>(serialized.data), r.serialized_bytes};
    const std::span views_span{reinterpret_cast<std::byte*>(views.data), r.views_bytes};
    marc::core::Transform* implementation{};
    if (encode) {
        static_assert(sizeof(LzssPositionDistance1mPreparedFrameStreamingEncoder)
            == sizeof(LzssPositionDistance1mFrameStreamingEncoder));
        static_assert(alignof(LzssPositionDistance1mPreparedFrameStreamingEncoder)
            == alignof(LzssPositionDistance1mFrameStreamingEncoder));
        static_assert(sizeof(LzssPositionDistance1mFinderScratchFrameStreamingEncoder)
            == sizeof(LzssPositionDistance1mPreparedFrameStreamingEncoder));
        static_assert(alignof(LzssPositionDistance1mFinderScratchFrameStreamingEncoder)
            == alignof(LzssPositionDistance1mPreparedFrameStreamingEncoder));
        implementation = new (std::nothrow) LzssPositionDistance1mFinderScratchFrameStreamingEncoder(
            stream, limits, raw_span, serialized_span, views_span, 3,
            marc::dictionary::internal::LzssPositionDistance1mSearch::indexed_five_prefix);
    } else {
        using Token=marc::dictionary::internal::LzssTypedToken;
        const std::span tokens{reinterpret_cast<Token*>(views_span.data()),r.token_count};
        for(auto& token:tokens) std::construct_at(&token);
        implementation = new (std::nothrow) LzssPositionDistance1mFrameStreamingDecoder(
            limits, serialized_span, tokens, raw_span);
    }
    return publish_transform(implementation, transform);
}

} // extern "C"
