#include "marc/marc.h"
#include "frame/lzss_position_distance_4m_five_prefix_frame_streaming_encoder.hpp"
#include "frame/lzss_position_distance_4m_frame_streaming_decoder.hpp"
#include "frame/lzss_position_distance_4m_workspace.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include <algorithm>
#include <array>
#include <memory>
#include <new>

// Keep this definition token-identical to src/marc.cpp for the shared C handle.
struct marc_transform {
    marc::core::Transform* implementation;
};
namespace {
bool valid_buffer(const void* data,std::size_t size) noexcept {return data!=nullptr || size==0;}
class BoundaryGuard final:public marc::core::Transform {
public:
    BoundaryGuard(marc::core::Transform* codec,marc_buffer a,marc_buffer b,marc_buffer c) noexcept
        :codec_(codec),buffers_{a,b,c} {}
    void handle(marc_transform* h) noexcept {handle_=h;}
    marc::core::ProcessResult process(std::span<const std::byte> input,std::span<std::byte> output,std::uint32_t flags) noexcept override {
        using namespace marc::core;
        if(terminal_) return {0,0,last_.status,last_.error};
        struct Region {const void* data;std::size_t size;};
        const std::array regions{Region{input.data(),input.size()},Region{output.data(),output.size()},
            Region{buffers_[0].data,buffers_[0].size},Region{buffers_[1].data,buffers_[1].size},
            Region{buffers_[2].data,buffers_[2].size},Region{this,sizeof(*this)},Region{handle_,sizeof(*handle_)}};
        for(std::size_t i=0;i<regions.size();++i) for(std::size_t j=i+1;j<regions.size();++j)
            if(check_buffer_overlap(regions[i].data,regions[i].size,regions[j].data,regions[j].size)!=BufferOverlap::disjoint) {
                terminal_=true;last_={0,0,StreamStatus::error,{ErrorCode::invalid_argument,0,0}};return last_;
            }
        last_=codec_->process(input,output,flags);
        terminal_=last_.status==StreamStatus::error || last_.status==StreamStatus::end_of_stream;
        return last_;
    }
private:
    std::unique_ptr<marc::core::Transform> codec_;
    std::array<marc_buffer,3> buffers_{};
    marc_transform* handle_{};
    marc::core::ProcessResult last_{};
    bool terminal_{};
};
constexpr auto retained_adapter_bytes=sizeof(marc_transform)+sizeof(BoundaryGuard);
marc_status publish_guarded(marc::core::Transform* codec,marc_buffer a,marc_buffer b,marc_buffer c,marc_transform** output) noexcept {
    if(!codec) return MARC_STATUS_OUT_OF_MEMORY;
    auto* guard=new(std::nothrow) BoundaryGuard(codec,a,b,c);
    if(!guard) {delete codec;return MARC_STATUS_OUT_OF_MEMORY;}
    auto* handle=new(std::nothrow) marc_transform{guard};
    if(!handle) {delete guard;return MARC_STATUS_OUT_OF_MEMORY;}
    guard->handle(handle);*output=handle;return MARC_STATUS_OK;
}
}
marc_status marc_lzss_position_distance_dynamic_range_4m_config_init(
    const marc_direction direction,
    marc_lzss_position_distance_dynamic_range_4m_config* config) noexcept {
    if (config == nullptr || (direction != MARC_DIRECTION_ENCODE
        && direction != MARC_DIRECTION_DECODE)) return MARC_STATUS_INVALID_ARGUMENT;
    marc_lzss_position_distance_dynamic_range_4m_config result{};
    result.struct_size = sizeof(result);
    result.abi_version = MARC_ABI_VERSION;
    result.direction = direction;
    result.frame_size = 4194304;
    result.max_total_output_size = UINT64_C(1) << 40;
    result.max_frame_size = 4194304;
    result.max_block_size = 4194304;
    result.max_compressed_payload_size = 75497477;
    result.max_internal_buffered_bytes = UINT64_C(512) << 20;
    result.max_lz_distance = 4194304;
    result.max_lz_match_length = 258;
    result.max_entropy_table_entries = 2588;
    result.max_range_model_total = 32768;
    result.max_expansion_ratio = 1024;
    result.expansion_slack = UINT64_C(1) << 20;
    *config = result;
    return MARC_STATUS_OK;
}

static marc_status prepare_position_distance_4m_config(
    const marc_lzss_position_distance_dynamic_range_4m_config* config,
    marc::core::DecoderLimits& limits,
    marc::frame::internal::TypedContextStreamHeader& stream,
    marc::frame::internal::LzssPositionDistanceWorkspaceRequirements& r) noexcept {
    using namespace marc::frame::internal;
    if (config == nullptr || config->struct_size != sizeof(*config)
        || config->abi_version != MARC_ABI_VERSION
        || config->reserved != 0 || config->reserved2 != 0
        || (config->direction != MARC_DIRECTION_ENCODE
            && config->direction != MARC_DIRECTION_DECODE))
        return MARC_STATUS_INVALID_ARGUMENT;
    limits = {};
    limits.max_total_output_size = config->max_total_output_size;
    limits.max_frame_size = config->max_frame_size;
    limits.max_block_size = config->max_block_size;
    limits.max_compressed_payload_size = config->max_compressed_payload_size;
    limits.max_internal_buffered_bytes = config->max_internal_buffered_bytes;
    limits.max_lz_distance = config->max_lz_distance;
    limits.max_lz_match_length = config->max_lz_match_length;
    limits.max_entropy_table_entries = config->max_entropy_table_entries;
    limits.max_range_model_total = config->max_range_model_total;
    limits.max_expansion_ratio = config->max_expansion_ratio;
    limits.expansion_slack = config->expansion_slack;
    if (marc::core::validate_limits(limits) != marc::core::LimitError::none)
        return MARC_STATUS_INVALID_ARGUMENT;
    const bool encode = config->direction == MARC_DIRECTION_ENCODE;
    if (encode && (config->frame_size == 0 || config->frame_size > 4194304))
        return MARC_STATUS_INVALID_ARGUMENT;
    // Reserve the opaque C handle before querying private owner/model storage.
    if (limits.max_internal_buffered_bytes <= retained_adapter_bytes
        || limits.max_internal_buffered_bytes - retained_adapter_bytes
            < limits.max_block_size) return MARC_STATUS_LIMIT_EXCEEDED;
    limits.max_internal_buffered_bytes -= retained_adapter_bytes;
    const auto frame_size = encode ? config->frame_size
        : static_cast<std::uint32_t>(std::min(config->max_frame_size, UINT64_C(4194304)));
    stream = {frame_size,
        encode ? config->original_size : UINT64_C(0),
        {4194304, 3, 258, 0}, 32768, 46, 10, 1, 11};
    if (encode) {
        const auto error = calculate_lzss_position_distance_4m_five_prefix_encode_workspace(stream,limits,
            LzssPositionDistanceWorkspaceDirection::encode,sizeof(LzssPositionDistance4mFivePrefixFrameStreamingEncoder),r);
        if(error!=LzssPositionDistanceWorkspaceError::none)
            return error==LzssPositionDistanceWorkspaceError::limit_exceeded
                || error==LzssPositionDistanceWorkspaceError::arithmetic_overflow
                ? MARC_STATUS_LIMIT_EXCEEDED : MARC_STATUS_INVALID_ARGUMENT;
    } else {
        LzssPositionDistance4mDecodeWorkspace decoded{};
        const auto error=calculate_lzss_position_distance_4m_decode_workspace(frame_size,limits,
            sizeof(LzssPositionDistance4mFrameStreamingDecoder),decoded);
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

marc_status marc_lzss_position_distance_dynamic_range_4m_workspace_requirements(
    const marc_lzss_position_distance_dynamic_range_4m_config* config,
    marc_workspace_requirements* requirements) noexcept {
    if (config == nullptr || requirements == nullptr
        || marc::core::check_buffer_overlap(config, sizeof(*config), requirements,
            sizeof(*requirements)) != marc::core::BufferOverlap::disjoint)
        return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::DecoderLimits limits{};
    marc::frame::internal::TypedContextStreamHeader stream{};
    marc::frame::internal::LzssPositionDistanceWorkspaceRequirements r{};
    const auto status = prepare_position_distance_4m_config(config, limits, stream, r);
    if (status != MARC_STATUS_OK) return status;
    const bool encode = config->direction == MARC_DIRECTION_ENCODE;
    *requirements = {sizeof(*requirements), MARC_ABI_VERSION,
        encode ? r.raw_bytes : r.serialized_bytes,
        encode ? r.serialized_bytes : r.raw_bytes, r.views_bytes, r.views_alignment};
    return MARC_STATUS_OK;
}

marc_status marc_lzss_position_distance_dynamic_range_4m_create(
    const marc_lzss_position_distance_dynamic_range_4m_config* config,
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
    const auto status = prepare_position_distance_4m_config(config, limits, stream, r);
    if (status != MARC_STATUS_OK) return status;
    const bool encode = config->direction == MARC_DIRECTION_ENCODE;
    const auto raw = encode ? primary : secondary;
    const auto serialized = encode ? secondary : primary;
    if (raw.size < r.raw_bytes || serialized.size < r.serialized_bytes
        || views.size < r.views_bytes
        || reinterpret_cast<std::uintptr_t>(views.data) % r.views_alignment != 0)
        return MARC_STATUS_INVALID_ARGUMENT;
    const marc_buffer prefixes[]{raw,serialized,views};
    for (std::size_t i = 0; i < 3; ++i) {
        if (!disjoint(prefixes[i].data, prefixes[i].size, config, sizeof(*config)))
            return MARC_STATUS_INVALID_ARGUMENT;
        for (std::size_t j = 0; j < i; ++j)
            if (!disjoint(prefixes[i].data, prefixes[i].size, prefixes[j].data, prefixes[j].size))
                return MARC_STATUS_INVALID_ARGUMENT;
    }
    std::size_t aggregate=r.aggregate_bytes;
    for(const auto bytes:{raw.size-r.raw_bytes,serialized.size-r.serialized_bytes,views.size-r.views_bytes})
        if(!marc::core::checked_add(aggregate,bytes,aggregate)) return MARC_STATUS_LIMIT_EXCEEDED;
    if(aggregate>limits.max_internal_buffered_bytes) return MARC_STATUS_LIMIT_EXCEEDED;
    const std::span raw_span{reinterpret_cast<std::byte*>(raw.data), raw.size};
    const std::span serialized_span{reinterpret_cast<std::byte*>(serialized.data), serialized.size};
    const std::span views_span{reinterpret_cast<std::byte*>(views.data), r.views_bytes};
    marc::core::Transform* implementation{};
    if (encode) {
        implementation = new (std::nothrow) LzssPositionDistance4mFivePrefixFrameStreamingEncoder(
            stream, limits, raw_span, serialized_span, views_span, 3);
    } else {
        using Token=marc::dictionary::internal::LzssTypedToken;
        const std::span tokens{reinterpret_cast<Token*>(views_span.data()),r.token_count};
        for(auto& token:tokens) std::construct_at(&token);
        implementation = new (std::nothrow) LzssPositionDistance4mFrameStreamingDecoder(
            limits, serialized_span, tokens, raw_span);
    }
    return publish_guarded(implementation, primary, secondary, views, transform);
}
