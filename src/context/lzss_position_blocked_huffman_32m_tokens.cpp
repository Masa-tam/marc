#include "context/lzss_position_blocked_huffman_32m_tokens.hpp"
#include "context/lzss_short_length_escape.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include <algorithm>
#include <bit>

namespace marc::context::internal {
namespace {
using namespace dictionary::internal;
using namespace entropy::internal;
using Error=PositionBlockedHuffman32mTokenError;
using Result=PositionBlockedHuffman32mTokenResult;
using E=PositionBlockedHuffmanError;
using Descriptor=PositionBlockedHuffman32mDescriptor;
constexpr LzssParameters parameters{33554432,3,258,0};
constexpr auto variant=LzssTypedTokenVariant::field_context_32m_short_length_escape;
// Conservative simultaneous workspace bounds, not physical process peaks or
// public profile grants. All caller-provided span capacity is charged below.
constexpr auto plan_workspace=position_blocked_huffman_32m_plan_workspace;
constexpr auto encode_workspace=position_blocked_huffman_32m_encode_workspace;
constexpr auto decode_workspace=position_blocked_huffman_32m_decode_workspace;
struct Region { const void* data{};std::size_t size{}; };
template<class T> bool region(std::span<T> span,Region& out,Result& r) noexcept {
    out.data=span.data();
    if (core::checked_multiply(span.size(),sizeof(T),out.size)) return true;
    r.error=Error::arithmetic_overflow;return false;
}
template<std::size_t N> bool separate(const std::array<Region,N>& regions,Result& r) noexcept {
    for (std::size_t i=0;i<N;++i) for (std::size_t j=0;j<i;++j) {
        const auto e=core::check_buffer_overlap(regions[i].data,regions[i].size,regions[j].data,regions[j].size);
        if (e!=core::BufferOverlap::disjoint) {
            r.error=e==core::BufferOverlap::arithmetic_overflow ? Error::arithmetic_overflow : Error::overlapping_buffers;
            return false;
        }
    }
    return true;
}
template<std::size_t N> bool budget(const std::array<Region,N>& regions,std::size_t sum,
                                   const core::DecoderLimits& limits,Result& r) noexcept {
    for (const auto item:regions) if (!core::checked_add(sum,item.size,sum)) {
        r.error=Error::arithmetic_overflow;return false;
    }
    if (sum>limits.max_internal_buffered_bytes) { r.error=Error::limit_exceeded;return false; }
    return true;
}
bool entropy(E e,Result& r) noexcept {
    r.entropy_error=e;
    if (e==E::none) return true;
    r.error=e==E::limit_exceeded ? Error::limit_exceeded : e==E::output_too_small ? Error::output_too_small
        : e==E::overlapping_buffers ? Error::overlapping_buffers : e==E::arithmetic_overflow ? Error::arithmetic_overflow : Error::entropy_error;
    return false;
}
template<class Emit> E forward(std::span<const LzssTypedToken> tokens,Emit emit) noexcept {
    unsigned previous=0;int literal=-1;
    for (const auto& token:tokens) {
        auto e=emit(static_cast<int>(previous),token.kind==LzssTypedTokenKind::literal ? 0 : 1);
        if (e!=E::none) return e;
        if (token.kind==LzssTypedTokenKind::literal) {
            e=emit(literal<0 ? 3 : 4+(literal>>4),token.literal);
            previous=1;literal=token.literal;
        } else {
            const auto length=encode_lzss_short_length_escape(token.length);
            if (length.error!=LzssShortLengthEscapeError::none || token.distance==0) return E::invalid_symbol;
            e=emit(static_cast<int>(20+previous),length.length_class);
            for (unsigned b=0;e==E::none && b<length.bit_count;++b) e=emit(-1,(length.extra>>b)&1U);
            const auto dc=std::bit_width(token.distance)-1U;
            if (e==E::none) e=emit(23+length.length_class,dc);
            const auto extra=token.distance-(UINT32_C(1)<<dc);
            for (unsigned b=0;e==E::none && b<dc;++b) e=emit(static_cast<int>(32+b),(extra>>b)&1U);
            previous=2;
        }
        if (e!=E::none) return e;
    }
    return E::none;
}
}
PositionBlockedHuffman32mTokenResult plan_position_blocked_huffman_32m_tokens(
    std::span<const LzssTypedToken> tokens,const LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,Descriptor& output) noexcept {
    Result r{};Region input{};
    if (!region(tokens,input,r)) return r;
    const auto regions=std::array{input,Region{&output,sizeof(output)},Region{&context,sizeof(context)},Region{&limits,sizeof(limits)}};
    if (!separate(regions,r) || !budget(regions,plan_workspace,limits,r)) return r;
    if (context.declared_raw_size>33554432) { r.error=Error::invalid_counts;return r; }
    const auto checked=validate_lzss_typed_frame(tokens,parameters,context,limits,variant);
    r.token_error=checked.token_error;
    if (checked.error!=LzssTypedFrameValidationError::none) {
        r.error=checked.error==LzssTypedFrameValidationError::limit_exceeded ? Error::limit_exceeded
            : checked.error==LzssTypedFrameValidationError::arithmetic_overflow ? Error::arithmetic_overflow : Error::invalid_token;
        return r;
    }
    PositionBlockedHuffman32mModelBuilder builder;
    if (!entropy(forward(tokens,[&](int c,std::uint32_t s){return builder.add(c,s);}),r)) return r;
    Descriptor model{};
    if (!entropy(builder.finish(limits,model),r)
        || !entropy(validate_position_blocked_huffman_32m_descriptor(model,limits,r.descriptor_size),r)) return r;
    r.payload_size=model.payload_size;
    std::size_t retained{};
    if (!core::checked_add(r.descriptor_size,r.payload_size,retained)) { r.error=Error::arithmetic_overflow;return r; }
    if (!budget(std::array{regions[0],regions[1],regions[2],regions[3],Region{nullptr,retained}},plan_workspace,limits,r)) return r;
    r.token_count=context.declared_token_count;r.raw_size=context.declared_raw_size;r.decision_count=model.decision_count;
    output=model;return r;
}
PositionBlockedHuffman32mTokenResult encode_position_blocked_huffman_32m_tokens(
    std::span<const LzssTypedToken> tokens,const LzssTypedFrameValidationContext& context,const core::DecoderLimits& limits,
    std::span<CanonicalHuffmanTable> tables,std::span<std::byte> scratch,
    std::span<std::byte> descriptor_output,std::span<std::byte> payload_output) noexcept {
    Result r{};Region input{},bank{};
    if (!region(tokens,input,r) || !region(tables,bank,r)) return r;
    const auto regions=std::array{input,bank,Region{scratch.data(),scratch.size()},Region{descriptor_output.data(),descriptor_output.size()},
        Region{payload_output.data(),payload_output.size()},Region{&context,sizeof(context)},Region{&limits,sizeof(limits)}};
    if (!separate(regions,r) || !budget(regions,encode_workspace,limits,r)) return r;
    Descriptor model{};r=plan_position_blocked_huffman_32m_tokens(tokens,context,limits,model);
    if (r.error!=Error::none) return r;
    if (scratch.size()<r.payload_size || descriptor_output.size()<r.descriptor_size || payload_output.size()<r.payload_size) {
        r.error=Error::output_too_small;return r;
    }
    PositionBlockedHuffman32mPrivateWriter writer;
    if (!entropy(writer.begin(model,limits,tables,scratch),r)
        || !entropy(forward(tokens,[&](int c,std::uint32_t s){return writer.add(c,s);}),r)) return r;
    std::size_t size{};
    if (!entropy(writer.finish(size),r)) return r;
    std::array<std::byte,2578> descriptor{};std::size_t ds{};
    if (!entropy(serialize_position_blocked_huffman_32m_descriptor(model,limits,descriptor,ds),r)) return r;
    // Every fallible operation precedes the first ordinary destination write.
    std::copy_n(descriptor.begin(),ds,descriptor_output.begin());
    std::copy_n(scratch.begin(),size,payload_output.begin());return r;
}
PositionBlockedHuffman32mTokenResult decode_position_blocked_huffman_32m_token_scratch(
    std::span<const std::byte> descriptor,std::span<const std::byte> payload,const PositionBlockedHuffman32mTokenContext& context,
    const core::DecoderLimits& limits,std::span<HuffmanDecodeTable> tables,std::span<LzssTypedToken> scratch) noexcept {
    Result r{};Region bank{},target{};
    if (!region(tables,bank,r) || !region(scratch,target,r)) return r;
    const auto regions=std::array{bank,target,Region{descriptor.data(),descriptor.size()},Region{payload.data(),payload.size()},
        Region{&context,sizeof(context)},Region{&limits,sizeof(limits)}};
    if (!separate(regions,r) || !budget(regions,decode_workspace,limits,r)) return r;
    r.token_error=validate_lzss_typed_parameters(parameters,limits,variant);
    if (r.token_error!=LzssTypedTokenError::none) {
        r.error=r.token_error==LzssTypedTokenError::limit_exceeded ? Error::limit_exceeded : Error::invalid_token;return r;
    }
    const std::uint64_t t=context.token_count,f=context.raw_size,d=context.decision_count;
    if (f>33554432 || t>f || d<2*t || d>35*t || d>10*f || ((f==0)!=(t==0))) { r.error=Error::invalid_counts;return r; }
    std::uint64_t total{};
    if (!core::checked_add(context.output_already_committed,f,total)) { r.error=Error::arithmetic_overflow;return r; }
    if (f>limits.max_frame_size || f>limits.max_block_size || total>limits.max_total_output_size) { r.error=Error::limit_exceeded;return r; }
    if (scratch.size()<t) { r.error=Error::output_too_small;return r; }
    if (payload.size()>UINT32_MAX) { r.error=Error::arithmetic_overflow;return r; }
    Descriptor model{};
    if (!entropy(parse_position_blocked_huffman_32m_descriptor(descriptor,context.decision_count,
        static_cast<std::uint32_t>(payload.size()),limits,model),r)) return r;
    PositionBlockedHuffman32mDecoder decoder;
    if (!entropy(decoder.begin(model,payload,limits,tables),r)) return r;
    unsigned previous=0;int literal=-1;
    const auto read=[&](int c,std::uint32_t& value) {
        if (!entropy(decoder.read(c,value),r)) return false;
        ++r.decision_count;return true;
    };
    for (std::size_t i=0;i<t;++i) {
        std::uint32_t kind{};if (!read(static_cast<int>(previous),kind)) return r;
        LzssTypedToken token{};
        if (kind==0) {
            std::uint32_t value{};if (!read(literal<0 ? 3 : 4+(literal>>4),value)) return r;
            if (value>255) { r.error=Error::invalid_token;return r; }
            token.literal=static_cast<std::uint8_t>(value);literal=static_cast<int>(value);previous=1;
        } else if (kind==1) {
            std::uint32_t lc{},extra{},dc{};
            if (!read(static_cast<int>(20+previous),lc)) return r;
            if (lc>8) { r.error=Error::invalid_token;return r; }
            const auto width=static_cast<std::uint8_t>(lc==8 ? 1 : lc);
            for (unsigned b=0;b<width;++b) { std::uint32_t value{};if (!read(-1,value)) return r;extra|=value<<b; }
            const auto length=decode_lzss_short_length_escape(lc,width,extra);
            if (length.error!=LzssShortLengthEscapeError::none) { r.error=Error::invalid_token;return r; }
            if (!read(23+static_cast<int>(lc),dc)) return r;
            if (dc>25) { r.error=Error::invalid_token;return r; }
            extra=0;
            for (unsigned b=0;b<dc;++b) { std::uint32_t value{};if (!read(static_cast<int>(32+b),value)) return r;extra|=value<<b; }
            token.kind=LzssTypedTokenKind::match;token.distance=(UINT32_C(1)<<dc)+extra;token.length=length.length;previous=2;
        } else { r.error=Error::invalid_token;return r; }
        std::uint64_t next{};
        r.token_error=validate_lzss_typed_token(token,parameters,{r.raw_size,f},limits,next,variant);
        if (r.token_error!=LzssTypedTokenError::none) {
            r.error=r.token_error==LzssTypedTokenError::limit_exceeded ? Error::limit_exceeded
                : r.token_error==LzssTypedTokenError::arithmetic_overflow ? Error::arithmetic_overflow : Error::invalid_token;
            return r;
        }
        scratch[i]=token;r.raw_size=static_cast<std::uint32_t>(next);++r.token_count;
    }
    if (r.raw_size!=f || r.decision_count!=d) { r.error=Error::invalid_counts;return r; }
    if (!entropy(decoder.finish(),r)) return r;
    r.descriptor_size=descriptor.size();r.payload_size=payload.size();return r;
}
PositionBlockedHuffman32mTokenResult decode_position_blocked_huffman_32m_tokens(
    std::span<const std::byte> descriptor,std::span<const std::byte> payload,const PositionBlockedHuffman32mTokenContext& context,
    const core::DecoderLimits& limits,std::span<HuffmanDecodeTable> tables,std::span<LzssTypedToken> scratch,
    std::span<LzssTypedToken> output) noexcept {
    Result r{};Region bank{},temporary{},destination{};
    if (!region(tables,bank,r) || !region(scratch,temporary,r) || !region(output,destination,r)) return r;
    const auto regions=std::array{bank,temporary,destination,Region{descriptor.data(),descriptor.size()},Region{payload.data(),payload.size()},
        Region{&context,sizeof(context)},Region{&limits,sizeof(limits)}};
    if (!separate(regions,r) || !budget(regions,decode_workspace,limits,r)) return r;
    if (output.size()<context.token_count) { r.error=Error::output_too_small;return r; }
    r=decode_position_blocked_huffman_32m_token_scratch(descriptor,payload,context,limits,tables,scratch);
    if (r.error==Error::none) std::copy_n(scratch.begin(),context.token_count,output.begin());
    return r;
}
} // namespace marc::context::internal
