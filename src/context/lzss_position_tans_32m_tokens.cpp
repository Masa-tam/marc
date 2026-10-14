#include "context/lzss_position_tans_32m_tokens.hpp"
#include "context/lzss_short_length_escape.hpp"
#include "core/buffer_overlap.hpp"
#include <algorithm>
#include <bit>

namespace marc::context::internal {
namespace {
using namespace dictionary::internal;
using namespace entropy::internal;
using Error=PositionTans32mTokenError;
using Result=PositionTans32mTokenResult;
using E=PositionTans32mError;
constexpr LzssParameters parameters{33554432,3,258,0};
constexpr auto variant=LzssTypedTokenVariant::field_context_32m_short_length_escape;
struct Region { const void* data;std::size_t size; };
template<class T> bool region(std::span<T> s,Region& r,Result& result) noexcept {
    r.data=s.data();
    if (!core::checked_multiply(s.size(),sizeof(T),r.size)) {
        result.error=Error::arithmetic_overflow;return false;
    }
    return true;
}
template<std::size_t N> bool separate(const std::array<Region,N>& regions,
                                     Result& result) noexcept {
    for (std::size_t i=0;i<N;++i) for (std::size_t j=0;j<i;++j) {
        const auto e=core::check_buffer_overlap(regions[i].data,regions[i].size,regions[j].data,regions[j].size);
        if (e!=core::BufferOverlap::disjoint) {
            result.error=e==core::BufferOverlap::arithmetic_overflow ? Error::arithmetic_overflow : Error::overlapping_buffers;
            return false;
        }
    }
    return true;
}
template<std::size_t N> bool budget(const std::array<Region,N>& regions,
                                   const core::DecoderLimits& limits,Result& result) noexcept {
    std::size_t sum=131072;
    for (const auto r:regions) if (!core::checked_add(sum,r.size,sum)) {
        result.error=Error::arithmetic_overflow;return false;
    }
    if (sum>limits.max_internal_buffered_bytes) { result.error=Error::limit_exceeded;return false; }
    return true;
}
template<class Emit> E forward(std::span<const LzssTypedToken> tokens,Emit emit) {
    unsigned previous=0;int literal=-1;
    for (const auto& t:tokens) {
        auto e=emit(static_cast<int>(previous),t.kind==LzssTypedTokenKind::literal ? 0 : 1);
        if (e!=E::none) return e;
        if (t.kind==LzssTypedTokenKind::literal) {
            e=emit(literal<0 ? 3 : 4+(literal>>4),t.literal);
            previous=1;literal=t.literal;
        } else {
            const auto length=encode_lzss_short_length_escape(t.length);
            e=emit(static_cast<int>(20+previous),length.length_class);
            for (unsigned b=0;e==E::none && b<length.bit_count;++b) e=emit(-1,(length.extra>>b)&1U);
            const auto dc=std::bit_width(t.distance)-1U;
            if (e==E::none) e=emit(23+length.length_class,dc);
            const auto extra=t.distance-(UINT32_C(1)<<dc);
            for (unsigned b=0;e==E::none && b<dc;++b) e=emit(static_cast<int>(32+b),(extra>>b)&1U);
            previous=2;
        }
        if (e!=E::none) return e;
    }
    return E::none;
}
std::size_t preceding_literal(std::span<const LzssTypedToken> tokens,std::size_t before) noexcept {
    while (before) { --before;if (tokens[before].kind==LzssTypedTokenKind::literal) return before; }
    return tokens.size();
}
E reverse(std::span<const LzssTypedToken> tokens,const PositionTans32mDescriptor& d,
          const core::DecoderLimits& limits,std::span<const std::uint16_t> tables,
          std::span<std::byte> payload,std::size_t& size,std::uint8_t& valid) {
    PositionTans32mReverseWriter writer(d,limits,tables,payload);
    auto before=tokens.empty() ? tokens.size() : preceding_literal(tokens,tokens.size()-1);
    for (std::size_t n=tokens.size();n;--n) {
        const auto i=n-1;const auto& t=tokens[i];
        const int previous=i==0 ? 0 : tokens[i-1].kind==LzssTypedTokenKind::literal ? 1 : 2;
        E e{};
        if (t.kind==LzssTypedTokenKind::literal)
            e=writer.add(before==tokens.size() ? 3 : 4+(tokens[before].literal>>4),t.literal);
        else {
            const auto length=encode_lzss_short_length_escape(t.length);
            const auto dc=std::bit_width(t.distance)-1U;
            const auto extra=t.distance-(UINT32_C(1)<<dc);
            for (auto b=dc;e==E::none && b;--b) e=writer.add(static_cast<int>(31+b),(extra>>(b-1))&1U);
            if (e==E::none) e=writer.add(23+length.length_class,dc);
            for (auto b=length.bit_count;e==E::none && b;--b) e=writer.add(-1,(length.extra>>(b-1))&1U);
            if (e==E::none) e=writer.add(20+previous,length.length_class);
        }
        if (e==E::none) e=writer.add(previous,t.kind==LzssTypedTokenKind::literal ? 0 : 1);
        if (e!=E::none) return e;
        if (i && before==i-1) before=preceding_literal(tokens,i-1);
    }
    return writer.finish(size,valid);
}
bool entropy(E e,Result& r) noexcept {
    r.entropy_error=e;
    if (e==E::none) return true;
    r.error=e==E::limit_exceeded ? Error::limit_exceeded : e==E::output_too_small ? Error::output_too_small
        : e==E::overlapping_buffers ? Error::overlapping_buffers : e==E::arithmetic_overflow ? Error::arithmetic_overflow : Error::entropy_error;
    return false;
}
}
PositionTans32mTokenResult plan_position_tans_32m_tokens(std::span<const LzssTypedToken> tokens,
    const LzssTypedFrameValidationContext& context,const core::DecoderLimits& limits,
    std::span<std::uint16_t> tables,PositionTans32mDescriptor& output) noexcept {
    Result r{};Region input{},bank{};
    if (!region(tokens,input,r) || !region(tables,bank,r)) return r;
    const auto regions=std::array{input,bank,Region{&output,sizeof(output)},Region{&context,sizeof(context)},Region{&limits,sizeof(limits)}};
    if (!separate(regions,r) || !budget(regions,limits,r)) return r;
    if (context.declared_raw_size>33554432) { r.error=Error::invalid_counts;return r; }
    const auto checked=validate_lzss_typed_frame(tokens,parameters,context,limits,variant);
    r.token_error=checked.token_error;
    if (checked.error!=LzssTypedFrameValidationError::none) {
        r.error=checked.error==LzssTypedFrameValidationError::limit_exceeded ? Error::limit_exceeded
            : checked.error==LzssTypedFrameValidationError::arithmetic_overflow ? Error::arithmetic_overflow : Error::invalid_token;
        return r;
    }
    PositionTans32mModelBuilder builder;
    if (!entropy(forward(tokens,[&](int c,std::uint32_t v){return builder.add(c,v);}),r)) return r;
    PositionTans32mDescriptor d{};
    if (!entropy(builder.finish(d),r) || !entropy(build_position_tans_32m_encode_tables(d,limits,tables),r)) return r;
    std::uint8_t valid{};
    if (!entropy(reverse(tokens,d,limits,tables,{},r.payload_size,valid),r)) return r;
    if (r.payload_size>UINT32_MAX) { r.error=Error::arithmetic_overflow;return r; }
    d.payload_size=static_cast<std::uint32_t>(r.payload_size);d.final_valid_bits=valid;
    if (!entropy(validate_position_tans_32m_descriptor(d,limits,r.descriptor_size),r)) return r;
    // Add exact retained descriptor and payload extents to the planning budget.
    Region planned{nullptr,0};
    if (!core::checked_add(r.descriptor_size,r.payload_size,planned.size)) { r.error=Error::arithmetic_overflow;return r; }
    if (!budget(std::array{input,bank,Region{&output,sizeof(output)},planned,Region{&context,sizeof(context)},Region{&limits,sizeof(limits)}},limits,r)) return r;
    r.token_count=context.declared_token_count;r.raw_size=context.declared_raw_size;r.decision_count=d.decision_count;
    output=d;return r;
}
PositionTans32mTokenResult encode_position_tans_32m_tokens(std::span<const LzssTypedToken> tokens,
    const LzssTypedFrameValidationContext& context,const core::DecoderLimits& limits,
    std::span<std::uint16_t> tables,std::span<std::byte> scratch,
    std::span<std::byte> descriptor_output,std::span<std::byte> payload_output) noexcept {
    Result r{};Region input{},bank{};
    if (!region(tokens,input,r) || !region(tables,bank,r)) return r;
    const auto regions=std::array{input,bank,Region{scratch.data(),scratch.size()},
        Region{descriptor_output.data(),descriptor_output.size()},Region{payload_output.data(),payload_output.size()},
        Region{&context,sizeof(context)},Region{&limits,sizeof(limits)}};
    if (!separate(regions,r) || !budget(regions,limits,r)) return r;
    PositionTans32mDescriptor d{};r=plan_position_tans_32m_tokens(tokens,context,limits,tables,d);
    if (r.error!=Error::none) return r;
    if (scratch.size()<r.payload_size || payload_output.size()<r.payload_size || descriptor_output.size()<r.descriptor_size) {
        r.error=Error::output_too_small;return r;
    }
    std::size_t n{};std::uint8_t valid{};
    if (!entropy(reverse(tokens,d,limits,tables,scratch.first(r.payload_size),n,valid),r)) return r;
    std::array<std::byte,9313> descriptor{};std::size_t ds{};
    if (!entropy(serialize_position_tans_32m_descriptor(d,limits,descriptor,ds),r)) return r;
    std::copy_n(descriptor.begin(),ds,descriptor_output.begin());
    std::copy_n(scratch.begin(),n,payload_output.begin());return r;
}
PositionTans32mTokenResult decode_position_tans_32m_token_scratch(std::span<const std::byte> descriptor,
    std::span<const std::byte> payload,const PositionTans32mTokenContext& context,
    const core::DecoderLimits& limits,std::span<TansDecodeEntry> tables,std::span<LzssTypedToken> scratch) noexcept {
    Result r{};Region bank{},target{};
    if (!region(tables,bank,r) || !region(scratch,target,r)) return r;
    const auto regions=std::array{bank,target,Region{descriptor.data(),descriptor.size()},Region{payload.data(),payload.size()},
        Region{&context,sizeof(context)},Region{&limits,sizeof(limits)}};
    if (!separate(regions,r) || !budget(regions,limits,r)) return r;
    r.token_error=validate_lzss_typed_parameters(parameters,limits,variant);
    if (r.token_error!=LzssTypedTokenError::none) {
        r.error=r.token_error==LzssTypedTokenError::limit_exceeded ? Error::limit_exceeded : Error::invalid_token;
        return r;
    }
    const std::uint64_t t=context.token_count,f=context.raw_size,d=context.decision_count;
    if (f>33554432 || t>f || d<2*t || d>34*t || d>10*f || ((f==0)!=(t==0))) { r.error=Error::invalid_counts;return r; }
    std::uint64_t total{};
    if (!core::checked_add(context.output_already_committed,f,total)) { r.error=Error::arithmetic_overflow;return r; }
    if (f>limits.max_frame_size || f>limits.max_block_size || total>limits.max_total_output_size) { r.error=Error::limit_exceeded;return r; }
    if (scratch.size()<t) { r.error=Error::output_too_small;return r; }
    PositionTans32mDescriptor model{};
    if (payload.size()>UINT32_MAX) { r.error=Error::arithmetic_overflow;return r; }
    if (!entropy(parse_position_tans_32m_descriptor(descriptor,context.decision_count,static_cast<std::uint32_t>(payload.size()),limits,model),r)) return r;
    PositionTans32mDecoder decoder;
    if (!entropy(decoder.begin(model,payload,limits,tables),r)) return r;
    unsigned previous=0;int literal=-1;
    const auto read=[&](int c,std::uint32_t& v) { if (!entropy(decoder.read(c,v),r)) return false;++r.decision_count;return true; };
    for (std::size_t i=0;i<t;++i) {
        std::uint32_t kind{};if (!read(static_cast<int>(previous),kind)) return r;
        LzssTypedToken token{};
        if (kind==0) {
            std::uint32_t value{};if (!read(literal<0 ? 3 : 4+(literal>>4),value)) return r;
            token.literal=static_cast<std::uint8_t>(value);literal=static_cast<int>(value);previous=1;
        } else {
            std::uint32_t lc{},extra{},dc{};
            if (!read(static_cast<int>(20+previous),lc)) return r;
            const auto width=static_cast<std::uint8_t>(lc==8 ? 1 : lc);
            for (unsigned b=0;b<width;++b) { std::uint32_t v{};if (!read(-1,v)) return r;extra|=v<<b; }
            const auto length=decode_lzss_short_length_escape(lc,width,extra);
            if (length.error!=LzssShortLengthEscapeError::none) { r.error=Error::invalid_token;return r; }
            if (!read(23+static_cast<int>(lc),dc)) return r;
            extra=0;
            for (unsigned b=0;b<dc;++b) { std::uint32_t v{};if (!read(static_cast<int>(32+b),v)) return r;extra|=v<<b; }
            token.kind=LzssTypedTokenKind::match;token.distance=(UINT32_C(1)<<dc)+extra;token.length=length.length;previous=2;
        }
        std::uint64_t next{};
        r.token_error=validate_lzss_typed_token(token,parameters,{r.raw_size,f},limits,next,variant);
        if (r.token_error!=LzssTypedTokenError::none) { r.error=Error::invalid_token;return r; }
        scratch[i]=token;r.raw_size=static_cast<std::uint32_t>(next);++r.token_count;
    }
    if (r.raw_size!=f || r.decision_count!=d) { r.error=Error::invalid_counts;return r; }
    if (!entropy(decoder.finish(),r)) return r;
    r.descriptor_size=descriptor.size();r.payload_size=payload.size();return r;
}
PositionTans32mTokenResult decode_position_tans_32m_tokens(std::span<const std::byte> descriptor,
    std::span<const std::byte> payload,const PositionTans32mTokenContext& context,
    const core::DecoderLimits& limits,std::span<TansDecodeEntry> tables,
    std::span<LzssTypedToken> scratch,std::span<LzssTypedToken> output) noexcept {
    Result r{};Region bank{},temporary{},destination{};
    if (!region(tables,bank,r) || !region(scratch,temporary,r) || !region(output,destination,r)) return r;
    const auto regions=std::array{bank,temporary,destination,Region{descriptor.data(),descriptor.size()},Region{payload.data(),payload.size()},
        Region{&context,sizeof(context)},Region{&limits,sizeof(limits)}};
    if (!separate(regions,r) || !budget(regions,limits,r)) return r;
    if (output.size()<context.token_count) { r.error=Error::output_too_small;return r; }
    r=decode_position_tans_32m_token_scratch(descriptor,payload,context,limits,tables,scratch);
    if (r.error==Error::none) std::copy_n(scratch.begin(),context.token_count,output.begin());
    return r;
}
} // namespace marc::context::internal



