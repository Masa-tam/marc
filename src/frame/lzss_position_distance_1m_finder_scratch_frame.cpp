#include "frame/lzss_position_distance_1m_finder_scratch_frame.hpp"
#include "context/lzss_position_distance_1m_context_layout.hpp"
#include "context/lzss_position_distance_1m_prepared_model.hpp"
#include "context/lzss_position_distance_1m_field_cursor.hpp"
#include "entropy/lzss_position_distance_1m_range_encoder.hpp"
#include "entropy/lzss_position_distance_1m_range_decoder.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include "core/endian.hpp"
#include <algorithm>
#include <array>
#include <utility>
namespace marc::frame::internal {
namespace {
using Token=dictionary::internal::LzssTypedToken;
using Operation=context::internal::ModeledOperation;
using EncodeError=LzssShortMatchFrameEncodeError;
constexpr auto prefix_size=typed_context_frame_header_size+typed_context_range_descriptor_size;
struct Region {const void* data;std::size_t bytes;};
static_assert(sizeof(context::internal::PreparedLzssPositionDistance1mModel)
    + sizeof(context::internal::LzssPositionDistance1mFieldCursor)
    <= sizeof(entropy::internal::LzssPositionDistance1mRangeDecoder));
template<std::size_t N>
core::BufferOverlap disjoint(const std::array<Region,N>& regions) noexcept {
    for (std::size_t i=0; i<N; ++i) for (std::size_t j=i+1; j<N; ++j) {
        const auto result=core::check_buffer_overlap(regions[i].data,regions[i].bytes,
            regions[j].data,regions[j].bytes);
        if (result!=core::BufferOverlap::disjoint) return result;
    }
    return core::BufferOverlap::disjoint;
}

}
LzssPositionDistance1mFinderScratchFrameResult encode_lzss_position_distance_1m_finder_scratch_frame(
    const TypedContextStreamHeader& stream,const core::DecoderLimits& limits,
    std::uint64_t sequence,std::uint64_t committed,std::span<const Token> tokens,
    std::span<Operation> operations,std::span<std::byte> output,std::span<std::byte> scratch) noexcept {
    const auto operation_storage=operations;
    LzssPositionDistance1mFinderScratchFrameResult result{};
    result.preflight_error=validate_lzss_position_distance_1m_stream_semantics(stream,limits);
    if (result.preflight_error!=LzssShortMatchPreflightError::none) {
        result.error=EncodeError::invalid_stream;return result;
    }
    if (committed>=stream.original_size || committed%stream.frame_size!=0 || sequence!=committed/stream.frame_size) {
        result.error=EncodeError::invalid_frame_position;return result;
    }
    const auto raw=static_cast<std::uint32_t>(std::min<std::uint64_t>(stream.frame_size,stream.original_size-committed));
    if (!std::in_range<std::uint32_t>(tokens.size())) {result.error=EncodeError::token_count_unsupported;return result;}
    const dictionary::internal::LzssTypedFrameValidationContext tc{static_cast<std::uint32_t>(tokens.size()),raw,committed};
    bool retry_reference=false;
    { // Mapping state dies before either fallback or range preparation.
        context::internal::PreparedLzssPositionDistance1mModel model;
        result.context=model.prepare(tokens,stream.dictionary,tc,limits);
        if(result.context.error!=context::internal::LzssFieldContextError::none) {
            retry_reference=true;
        } else {
            result.raw_size=raw;result.token_count=tokens.size();
            result.operation_count=result.context.operation_count;result.decision_count=result.context.decision_count;
            if (operations.size()<result.operation_count) {result.error=EncodeError::operation_workspace_too_small;return result;}
            operations=operations.first(result.operation_count);
            const auto overlap=disjoint(std::array{Region{tokens.data(),tokens.size_bytes()},
                Region{operations.data(),operations.size_bytes()},Region{output.data(),output.size()},
                Region{&stream,sizeof(stream)},Region{&limits,sizeof(limits)}});
            if (overlap!=core::BufferOverlap::disjoint) {
                result.error=overlap==core::BufferOverlap::arithmetic_overflow
                    ? EncodeError::arithmetic_overflow : EncodeError::overlapping_workspaces;return result;
            }
            result.context=model.write(operations);
            if (result.context.error!=context::internal::LzssFieldContextError::none) {result.error=EncodeError::context_error;return result;}
        }
    }
    if(retry_reference)
        return encode_lzss_position_distance_1m_prepared_frame(stream,limits,sequence,committed,tokens,operations,output);
    // Only reuse storage after mapping state has expired, without touching any
    // live token, operation, configuration or serialized-output region.
    const auto fallback=[&]() -> LzssPositionDistance1mFinderScratchFrameResult {
        return encode_lzss_position_distance_1m_prepared_frame(
            stream,limits,sequence,committed,tokens,operation_storage,output);
    };
    std::size_t bound{};
    if (!core::checked_multiply(static_cast<std::size_t>(result.decision_count),std::size_t{2},bound)
        || !core::checked_add(bound,std::size_t{5},bound)
        || bound>UINT32_MAX || bound>scratch.size() || bound>limits.max_compressed_payload_size
        || core::validate_limits(limits)!=core::LimitError::none
        || context::internal::lzss_position_distance_1m_frequency_entries>limits.max_entropy_table_entries
        || entropy::internal::contextual_dynamic_range_model_total_limit>limits.max_range_model_total)
        return fallback();
    // Conservatively account for all simultaneously used supplied capacities.
    // The raw-frame wrapper already charges its finder span once, not once per
    // role. An oversized optional buffer selects the reference, never a new error.
    auto scratch_aggregate=std::max(entropy::internal::lzss_position_distance_1m_range_encoder_state_bytes(),
        sizeof(entropy::internal::LzssPositionDistance1mRangeDecoder));
    for (const auto bytes:{static_cast<std::size_t>(raw),tokens.size_bytes(),
            operation_storage.size_bytes(),scratch.size(),output.size()})
        if (!core::checked_add(scratch_aggregate,bytes,scratch_aggregate)) return fallback();
    if (scratch_aggregate>limits.max_internal_buffered_bytes
        || disjoint(std::array{Region{scratch.data(),scratch.size()},
            Region{tokens.data(),tokens.size_bytes()},Region{operation_storage.data(),operation_storage.size_bytes()},
            Region{output.data(),output.size()},Region{&stream,sizeof(stream)},Region{&limits,sizeof(limits)}})
            !=core::BufferOverlap::disjoint) return fallback();
    TypedContextRangeDescriptor descriptor{};
    result.entropy=entropy::internal::encode_lzss_position_distance_1m_range_operations_scratch(
        operations,limits,scratch.first(bound),descriptor);
    if (result.entropy.error!=entropy::internal::ContextualDynamicRangeEncodeError::none) return fallback();
    result.used_finder_scratch=true;
    result.payload_size=result.entropy.payload_size;
    const TypedContextFrameHeader header{0,sequence,raw,static_cast<std::uint32_t>(tokens.size()),
        static_cast<std::uint32_t>(operations.size()),result.decision_count,
        static_cast<std::uint32_t>(result.payload_size),typed_context_range_descriptor_size,0,0};
    LzssShortMatchFrameRequirements requirements{};
    result.preflight_error=preflight_lzss_position_distance_1m_frame_semantics(header,descriptor,
        {stream,limits,sequence,committed},requirements);
    if (result.preflight_error!=LzssShortMatchPreflightError::none) {result.error=EncodeError::preflight_error;return result;}
    result.serialized_size=requirements.serialized_frame_bytes;
    const auto decoder_state=sizeof(entropy::internal::LzssPositionDistance1mRangeDecoder);
    const auto encoder_state=entropy::internal::lzss_position_distance_1m_range_encoder_state_bytes();
    const auto extra=encoder_state>decoder_state ? encoder_state-decoder_state : 0;
    std::size_t aggregate{};
    if (!core::checked_add(requirements.aggregate_working_bytes,operations.size_bytes(),aggregate)
        || !core::checked_add(aggregate,extra,aggregate)) {result.error=EncodeError::arithmetic_overflow;return result;}
    if (aggregate>limits.max_internal_buffered_bytes) {result.error=EncodeError::workspace_limit;return result;}
    if (output.size()<result.serialized_size) {result.error=EncodeError::serialized_output_too_small;return result;}
    std::array<std::byte,prefix_size> prefix{};
    prefix[0]=std::byte{0x4d};prefix[1]=std::byte{0x52};prefix[2]=std::byte{0x46};prefix[3]=std::byte{0x32};
    const std::span<std::byte> target{prefix};
    const bool stored=core::store_le(target,4,std::uint16_t{64})
        && core::store_le(target,8,sequence) && core::store_le(target,16,raw)
        && core::store_le(target,20,header.token_count) && core::store_le(target,24,header.event_count)
        && core::store_le(target,28,header.decision_count) && core::store_le(target,32,header.payload_size)
        && core::store_le(target,36,header.descriptor_size)
        && core::store_le(target,64,descriptor.decision_count) && core::store_le(target,68,descriptor.payload_size)
        && core::store_le(target,72,descriptor.context_count);
    if (!stored) {result.error=EncodeError::internal_error;return result;}
    std::copy_n(scratch.begin(),result.payload_size,output.begin()+prefix_size);
    std::copy(prefix.begin(),prefix.end(),output.begin());
    return result;
}
} // namespace marc::frame::internal
