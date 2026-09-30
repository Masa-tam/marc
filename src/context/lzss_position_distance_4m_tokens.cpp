#include "context/lzss_position_distance_4m_tokens.hpp"
#include "context/lzss_short_length_escape.hpp"
#include "entropy/lzss_position_distance_4m_range_decoder.hpp"
#include "core/buffer_overlap.hpp"
#include <array>
#include <bit>
#include <limits>

namespace marc::context::internal {
namespace {
using namespace dictionary::internal;
using entropy::internal::ContextualDynamicRangeDescriptor;
using entropy::internal::ContextualDynamicRangeDecodeError;
using entropy::internal::LzssPositionDistance4mRangeDecoder;
constexpr auto variant = LzssTypedTokenVariant::field_context_4m_short_length_escape;
using DecodeError = LzssContextualRangeDecodeError;
struct Region { const void* data; std::size_t bytes; };

template<std::size_t N>
core::BufferOverlap overlaps(Region output, const std::array<Region,N>& inputs) noexcept {
    for (const auto& input : inputs) {
        const auto error = core::check_buffer_overlap(output.data,output.bytes,input.data,input.bytes);
        if (error != core::BufferOverlap::disjoint) return error;
    }
    return core::BufferOverlap::disjoint;
}

LzssFieldContextResult map(std::span<const LzssTypedToken> tokens,
    std::span<ModeledOperation> output) noexcept {
    LzssFieldContextResult result{};
    LzssPositionDistance4mFieldCursor cursor;
    const auto emit = [&](std::uint32_t value) {
        auto op=cursor.next().shape;op.value=value;
        if (cursor.accept(op)!=LzssFieldContextError::none) {
            result.error=LzssFieldContextError::invalid_token;return false;
        }
        const auto decisions=op.kind==ModeledOperationKind::symbol ? 1U : op.bit_count;
        if (!core::checked_add(result.decision_count,decisions,result.decision_count)) {
            result.error=LzssFieldContextError::arithmetic_overflow;return false;
        }
        if (!output.empty()) output[result.operation_count]=op;
        ++result.operation_count;return true;
    };
    for (const auto& token : tokens) {
        result.token_index=result.token_count;
        result.operation_index=result.operation_count;
        if (token.kind==LzssTypedTokenKind::literal) {
            if (!emit(0)||!emit(token.literal)) return result;
            ++result.raw_size;
        } else {
            const auto length=encode_lzss_short_length_escape(token.length);
            const auto distance_class=std::bit_width(token.distance)-1U;
            if (!emit(1)||!emit(length.length_class)) return result;
            if (length.bit_count && !emit(length.extra)) return result;
            if (!emit(distance_class)) return result;
            if (distance_class && !emit(token.distance-(UINT32_C(1)<<distance_class))) return result;
            result.raw_size+=token.length;
        }
        ++result.token_count;
    }
    result.operation_index=result.operation_count;result.token_index=result.token_count;
    result.error=cursor.finish();return result;
}

bool read_token(LzssPositionDistance4mRangeDecoder& decoder,LzssTypedToken& token,
    LzssContextualRangeDecodeResult& result) noexcept {
    ModeledOperation op{};
    const auto next=[&] {
        result.entropy=decoder.decode_next(op);
        if (result.entropy.error==ContextualDynamicRangeDecodeError::none) return true;
        result.error=DecodeError::entropy_error;return false;
    };
    // The decoder's cursor enforces each symbol/extra field association.
    if (!next()) return false;
    if (op.value==0) {
        if (!next()) return false;
        token={LzssTypedTokenKind::literal,static_cast<std::uint8_t>(op.value),0,0};
        return true;
    }
    if (!next()) return false;
    const auto length_class=op.value;
    const auto width=static_cast<std::uint8_t>(length_class==8 ? 1 : length_class);
    std::uint32_t extra{};
    if (width) { if (!next()) return false;extra=op.value; }
    const auto length=decode_lzss_short_length_escape(length_class,width,extra);
    if (length.error!=LzssShortLengthEscapeError::none) {result.error=DecodeError::invalid_token;return false;}
    if (!next()) return false;
    const auto distance_class=op.value;
    extra=0;
    if (distance_class) {if (!next()) return false;extra=op.value;}
    token={LzssTypedTokenKind::match,0,(UINT32_C(1)<<distance_class)+extra,length.length};
    return true;
}

LzssContextualRangeDecodeResult run(const ContextualDynamicRangeDescriptor& descriptor,
    std::span<const std::byte> payload,const LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,const core::DecoderLimits& limits,
    std::span<LzssTypedToken> output) noexcept {
    LzssContextualRangeDecodeResult result{};
    result.token_error=validate_lzss_typed_parameters(parameters,limits,variant);
    if (result.token_error!=LzssTypedTokenError::none) {
        result.error=result.token_error==LzssTypedTokenError::limit_exceeded
            ? DecodeError::limit_exceeded : DecodeError::invalid_parameters;return result;
    }
    const std::uint64_t t=context.declared_token_count,e=context.declared_event_count,
        d=context.declared_decision_count,f=context.declared_raw_size;
    if (!f || f>4194304 || !t || t>f || e<2*t || e>5*t || e>2*f
        || d<e || d>33*t || d>9*f || descriptor.decision_count!=d
        || descriptor.payload_size>18*f+5 || descriptor.payload_size>2*d+5) {
        result.error=DecodeError::invalid_counts;return result;
    }
    std::uint64_t total{};
    std::size_t bytes{},aggregate{};
    if (!core::checked_add(context.output_already_committed,f,total)
        || !core::checked_multiply(static_cast<std::size_t>(t),sizeof(LzssTypedToken),bytes)
        || !core::checked_add(bytes,payload.size(),aggregate)
        || !core::checked_add(aggregate,sizeof(LzssPositionDistance4mRangeDecoder),aggregate)) {
        result.error=DecodeError::arithmetic_overflow;return result;
    }
    if (f>limits.max_frame_size || f>limits.max_block_size || total>limits.max_total_output_size
        || aggregate>limits.max_internal_buffered_bytes) {result.error=DecodeError::limit_exceeded;return result;}
    LzssPositionDistance4mRangeDecoder decoder;
    result.entropy=decoder.begin(descriptor,payload,limits);
    if (result.entropy.error!=ContextualDynamicRangeDecodeError::none) {result.error=DecodeError::entropy_error;return result;}
    while (result.token_count<t) {
        result.token_index=result.token_count;
        LzssTypedToken token{};
        if (!read_token(decoder,token,result)) return result;
        std::uint64_t next{};
        result.token_error=validate_lzss_typed_token(token,parameters,{result.raw_size,f},limits,next,variant);
        if (result.token_error!=LzssTypedTokenError::none) {
            result.error=result.token_error==LzssTypedTokenError::limit_exceeded ? DecodeError::limit_exceeded
                : result.token_error==LzssTypedTokenError::arithmetic_overflow ? DecodeError::arithmetic_overflow
                : DecodeError::invalid_token;return result;
        }
        if (!output.empty()) output[result.token_count]=token;
        result.raw_size=next;++result.token_count;
    }
    result.token_index=result.token_count;
    result.entropy=decoder.finish(context.declared_event_count,context.declared_decision_count);
    if (result.entropy.error!=ContextualDynamicRangeDecodeError::none) result.error=DecodeError::entropy_error;
    else if (result.raw_size!=f) result.error=DecodeError::raw_size_mismatch;
    return result;
}

core::BufferOverlap decode_overlap(const ContextualDynamicRangeDescriptor& descriptor,
    std::span<const std::byte> payload,const LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,const core::DecoderLimits& limits,
    std::span<LzssTypedToken> output) noexcept {
    std::size_t bytes{};
    if (!core::checked_multiply(output.size(),sizeof(LzssTypedToken),bytes)) return core::BufferOverlap::arithmetic_overflow;
    return overlaps(Region{output.data(),bytes},std::array{Region{payload.data(),payload.size()},
        Region{&descriptor,sizeof(descriptor)},Region{&parameters,sizeof(parameters)},
        Region{&context,sizeof(context)},Region{&limits,sizeof(limits)}});
}
} // namespace

LzssFieldContextResult plan_lzss_position_distance_4m_operations(std::span<const LzssTypedToken> tokens,
    const LzssParameters& parameters,const LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits) noexcept {
    const auto checked=validate_lzss_typed_frame(tokens,parameters,context,limits,variant);
    LzssFieldContextResult result{};
    result.token_count=checked.token_count;result.token_index=checked.token_index;
    result.raw_size=checked.raw_size;result.token_error=checked.token_error;
    if (checked.error!=LzssTypedFrameValidationError::none) {
        using E=LzssTypedFrameValidationError;
        result.error=checked.error==E::invalid_parameters ? LzssFieldContextError::invalid_parameters
            : checked.error==E::limit_exceeded ? LzssFieldContextError::limit_exceeded
            : checked.error==E::arithmetic_overflow ? LzssFieldContextError::arithmetic_overflow
            : checked.error==E::token_count_mismatch ? LzssFieldContextError::token_count_mismatch
            : checked.error==E::premature_end ? LzssFieldContextError::raw_size_mismatch
            : checked.error==E::trailing_tokens ? LzssFieldContextError::trailing_tokens
            : LzssFieldContextError::invalid_token;
        return result;
    }
    result=map(tokens,{});
    std::size_t token_bytes{},op_bytes{},aggregate{};
    if (!core::checked_multiply(tokens.size(),sizeof(LzssTypedToken),token_bytes)
        || !core::checked_multiply(result.operation_count,sizeof(ModeledOperation),op_bytes)
        || !core::checked_add(token_bytes,op_bytes,aggregate)
        || !core::checked_add(aggregate,sizeof(LzssPositionDistance4mFieldCursor),aggregate))
        result.error=LzssFieldContextError::arithmetic_overflow;
    else if (aggregate>limits.max_internal_buffered_bytes) result.error=LzssFieldContextError::limit_exceeded;
    return result;
}

LzssFieldContextResult model_lzss_position_distance_4m_tokens(std::span<const LzssTypedToken> tokens,
    const LzssParameters& parameters,const LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,std::span<ModeledOperation> operations) noexcept {
    auto result=plan_lzss_position_distance_4m_operations(tokens,parameters,context,limits);
    if (result.error!=LzssFieldContextError::none) return result;
    if (operations.size()<result.operation_count) {result.error=LzssFieldContextError::output_too_small;return result;}
    const auto output=operations.first(result.operation_count);
    const auto overlap=overlaps(Region{output.data(),output.size_bytes()},std::array{
        Region{tokens.data(),tokens.size_bytes()},Region{&parameters,sizeof(parameters)},
        Region{&context,sizeof(context)},Region{&limits,sizeof(limits)}});
    if (overlap!=core::BufferOverlap::disjoint) {
        result.error=overlap==core::BufferOverlap::arithmetic_overflow
            ? LzssFieldContextError::arithmetic_overflow : LzssFieldContextError::overlapping_buffers;
        return result;
    }
    return map(tokens,output);
}

LzssContextualRangeDecodeResult validate_lzss_position_distance_4m_tokens(
    const ContextualDynamicRangeDescriptor& descriptor,std::span<const std::byte> payload,
    const LzssParameters& parameters,const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits) noexcept {return run(descriptor,payload,parameters,context,limits,{});}

LzssContextualRangeDecodeResult decode_lzss_position_distance_4m_tokens(
    const ContextualDynamicRangeDescriptor& descriptor,std::span<const std::byte> payload,
    const LzssParameters& parameters,const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,std::span<LzssTypedToken> tokens) noexcept {
    auto result=run(descriptor,payload,parameters,context,limits,{});
    if (result.error!=DecodeError::none) return result;
    if (tokens.size()<context.declared_token_count) {result.error=DecodeError::output_too_small;return result;}
    const auto output=tokens.first(context.declared_token_count);
    const auto overlap=decode_overlap(descriptor,payload,parameters,context,limits,output);
    if (overlap!=core::BufferOverlap::disjoint) {
        result.error=overlap==core::BufferOverlap::arithmetic_overflow ? DecodeError::arithmetic_overflow : DecodeError::overlapping_buffers;
        return result;
    }
    return run(descriptor,payload,parameters,context,limits,output);
}

LzssContextualRangeDecodeResult decode_lzss_position_distance_4m_token_scratch(
    const ContextualDynamicRangeDescriptor& descriptor,std::span<const std::byte> payload,
    const LzssParameters& parameters,const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,std::span<LzssTypedToken> scratch) noexcept {
    if (scratch.size()<context.declared_token_count
        || decode_overlap(descriptor,payload,parameters,context,limits,scratch.first(context.declared_token_count))!=core::BufferOverlap::disjoint)
        return decode_lzss_position_distance_4m_tokens(descriptor,payload,parameters,context,limits,scratch);
    return run(descriptor,payload,parameters,context,limits,scratch.first(context.declared_token_count));
}
} // namespace marc::context::internal
