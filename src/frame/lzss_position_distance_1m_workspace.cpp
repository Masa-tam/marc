#include "frame/lzss_position_distance_1m_workspace.hpp"
#include "frame/lzss_position_distance_1m_preflight.hpp"
#include "entropy/lzss_position_distance_1m_range_decoder.hpp"
#include "dictionary/lzss_typed_token.hpp"
#include "core/checked_math.hpp"

namespace marc::frame::internal {
core::ErrorCode charge_lzss_position_distance_1m_decode_workspace(
    const core::DecoderLimits& limits, std::size_t retained,
    std::size_t raw, std::size_t serialized, std::size_t tokens,
    std::size_t& aggregate) noexcept {
    if (core::validate_limits(limits)!=core::LimitError::none) return core::ErrorCode::invalid_argument;
    std::size_t total=sizeof(entropy::internal::LzssPositionDistance1mRangeDecoder);
    for (const auto bytes : {retained,raw,serialized,tokens})
        if (!core::checked_add(total,bytes,total)) return core::ErrorCode::limit_exceeded;
    if (total>limits.max_internal_buffered_bytes) return core::ErrorCode::limit_exceeded;
    aggregate=total;return core::ErrorCode::none;
}

core::ErrorCode calculate_lzss_position_distance_1m_decode_workspace(
    std::uint32_t frame_capacity,const core::DecoderLimits& limits,
    std::size_t retained,LzssPositionDistance1mDecodeWorkspace& result) noexcept {
    if (!frame_capacity || frame_capacity>1048576 || core::validate_limits(limits)!=core::LimitError::none)
        return core::ErrorCode::invalid_argument;
    const TypedContextStreamHeader stream{frame_capacity,0,{1048576,3,258,0},32768,44,9,1,10};
    const auto valid=validate_lzss_position_distance_1m_stream_semantics(stream,limits);
    if (valid!=LzssShortMatchPreflightError::none || frame_capacity>limits.max_block_size)
        return core::ErrorCode::limit_exceeded;
    LzssPositionDistance1mDecodeWorkspace r{};
    r.raw_bytes=r.token_count=frame_capacity;r.retained_state_bytes=retained;
    r.model_bytes=sizeof(entropy::internal::LzssPositionDistance1mRangeDecoder);
    std::size_t payload{};
    if (!core::checked_multiply(r.raw_bytes,std::size_t{18},payload)
        || !core::checked_add(payload,std::size_t{5},payload)
        || !core::checked_add(payload,std::size_t{80},r.serialized_bytes)
        || !core::checked_multiply(r.token_count,sizeof(dictionary::internal::LzssTypedToken),r.token_bytes)
        || payload>limits.max_compressed_payload_size) return core::ErrorCode::limit_exceeded;
    const auto error=charge_lzss_position_distance_1m_decode_workspace(limits,retained,
        r.raw_bytes,r.serialized_bytes,r.token_bytes,r.aggregate_bytes);
    if (error!=core::ErrorCode::none) return error;
    result=r;return core::ErrorCode::none;
}
}
