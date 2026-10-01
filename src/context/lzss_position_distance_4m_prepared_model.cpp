#include "context/lzss_position_distance_4m_prepared_model.hpp"
#include "context/lzss_short_length_escape.hpp"
#include "context/lzss_position_distance_4m_field_cursor.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include <array>
#include <algorithm>
#include <bit>
namespace marc::context::internal {
namespace {
using namespace dictionary::internal;
using Error=LzssFieldContextError;
struct Region {const void* data;std::size_t bytes;};
template<std::size_t N>
Error overlap(Region output,const std::array<Region,N>& inputs) noexcept {
    for(auto in:inputs) {
        const auto r=core::check_buffer_overlap(output.data,output.bytes,in.data,in.bytes);
        if(r!=core::BufferOverlap::disjoint)
            return r==core::BufferOverlap::arithmetic_overflow?Error::arithmetic_overflow:Error::overlapping_buffers;
    }
    return Error::none;
}
LzssFieldContextResult fail(LzssFieldContextResult r,Error e) noexcept {r.error=e;return r;}
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

}
LzssFieldContextResult PreparedLzssPositionDistance4mModel::prepare(
    std::span<const LzssTypedToken> tokens,const LzssParameters& parameters,
    const LzssTypedFrameValidationContext& context,const core::DecoderLimits& limits) noexcept {
    std::size_t token_bytes{};
    if(!core::checked_multiply(tokens.size(),sizeof(LzssTypedToken),token_bytes))
        return fail({},Error::arithmetic_overflow);
    const auto alias=overlap(Region{this,sizeof(*this)},std::array{
        Region{tokens.data(),token_bytes},Region{&parameters,sizeof(parameters)},
        Region{&context,sizeof(context)},Region{&limits,sizeof(limits)}});
    if(alias!=Error::none) return fail({},alias);
    ready_=false;tokens_={};parameters_=nullptr;context_=nullptr;limits_=nullptr;plan_={};
    auto plan=plan_lzss_position_distance_4m_operations(tokens,parameters,context,limits);
    if(plan.error!=Error::none) return plan;
    std::size_t bytes{},total{};
    if(!core::checked_multiply(plan.operation_count,sizeof(ModeledOperation),bytes)
        || !core::checked_add(token_bytes,bytes,total)
        || !core::checked_add(total,sizeof(LzssPositionDistance4mFieldCursor),total)
        || !core::checked_add(total,sizeof(*this),total)) return fail(plan,Error::arithmetic_overflow);
    if(total>limits.max_internal_buffered_bytes) return fail(plan,Error::limit_exceeded);
    tokens_=tokens;parameters_=&parameters;context_=&context;limits_=&limits;plan_=plan;ready_=true;
    return plan;
}
LzssFieldContextResult PreparedLzssPositionDistance4mModel::write(std::span<ModeledOperation> operations) noexcept {
    if(!ready_) return fail({},Error::invalid_parameters);
    const auto count=std::min(operations.size(),plan_.operation_count);
    std::size_t bytes{};
    if(!core::checked_multiply(count,sizeof(ModeledOperation),bytes)) return fail(plan_,Error::arithmetic_overflow);
    // Do not mutate readiness through an overlapping output buffer on failure.
    const auto self=overlap(Region{operations.data(),bytes},std::array{Region{this,sizeof(*this)}});
    if(self!=Error::none) return fail(plan_,self);
    ready_=false;
    if(operations.size()<plan_.operation_count) return fail(plan_,Error::output_too_small);
    const auto output=operations.first(plan_.operation_count);
    const auto alias=overlap(Region{output.data(),bytes},std::array{
        Region{tokens_.data(),tokens_.size_bytes()},Region{parameters_,sizeof(*parameters_)},
        Region{context_,sizeof(*context_)},Region{limits_,sizeof(*limits_)}});
    if(alias!=Error::none) return fail(plan_,alias);
    return map(tokens_,output);
}
}
