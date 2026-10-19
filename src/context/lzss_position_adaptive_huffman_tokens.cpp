#include "context/lzss_position_adaptive_huffman_tokens.hpp"
#include "context/lzss_short_length_escape.hpp"
#include "core/buffer_overlap.hpp"
#include <algorithm>
#include <bit>
#include <initializer_list>

namespace marc::context::internal {
namespace {
using namespace dictionary::internal;
using namespace entropy::internal;
using Error=PositionAdaptiveHuffmanTokenError;
using Result=PositionAdaptiveHuffmanTokenResult;
using E=PositionAdaptiveHuffmanError;
struct Region { const void* data{};std::size_t size{}; };
Result failure(Error e,E entropy=E::none,LzssTypedTokenError token=LzssTypedTokenError::none) noexcept {
    Result r{};r.error=e;r.entropy_error=entropy;r.token_error=token;return r;
}
Result entropy_failure(E e) noexcept {
    const auto mapped=e==E::invalid_profile ? Error::invalid_profile : e==E::limit_exceeded ? Error::limit_exceeded
        : e==E::output_too_small ? Error::output_too_small : e==E::arithmetic_overflow ? Error::arithmetic_overflow
        : e==E::overlapping_buffers ? Error::overlapping_buffers : Error::entropy_error;
    return failure(mapped,e);
}
template<class T> bool region(std::span<T> span,Region& output) noexcept {
    std::size_t bytes{};
    if (!core::checked_multiply(span.size(),sizeof(T),bytes)) return false;
    output={span.data(),bytes};return true;
}
Error admit(std::initializer_list<Region> regions,const core::DecoderLimits& limits,std::size_t extra=0) noexcept {
    std::size_t bytes=131072;
    if (!core::checked_add(bytes,extra,bytes)) return Error::arithmetic_overflow;
    for (auto a=regions.begin();a!=regions.end();++a) {
        if (!core::checked_add(bytes,a->size,bytes)) return Error::arithmetic_overflow;
        for (auto b=regions.begin();b!=a;++b) {
            const auto overlap=core::check_buffer_overlap(a->data,a->size,b->data,b->size);
            if (overlap==core::BufferOverlap::arithmetic_overflow) return Error::arithmetic_overflow;
            if (overlap!=core::BufferOverlap::disjoint) return Error::overlapping_buffers;
        }
    }
    return core::validate_limits(limits)!=core::LimitError::none || bytes>limits.max_internal_buffered_bytes
        ? Error::limit_exceeded : Error::none;
}
LzssParameters parameters(const PositionAdaptiveHuffmanSpec& spec) noexcept { return {spec.window_size,3,258,0}; }
LzssTypedTokenVariant variant(const PositionAdaptiveHuffmanSpec& spec) noexcept {
    return static_cast<LzssTypedTokenVariant>(spec.dictionary_variant);
}
bool counts(const PositionAdaptiveHuffmanSpec& spec,std::uint64_t tokens,std::uint64_t raw,std::uint64_t decisions) noexcept {
    return raw<=spec.window_size && tokens<=raw && (raw==0)==(tokens==0)
        && decisions>=2*tokens && decisions<=(10+static_cast<std::uint64_t>(spec.distance_bits))*tokens
        && decisions<=static_cast<std::uint64_t>(spec.decisions_per_byte)*raw;
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
PositionAdaptiveHuffmanTokenResult plan_position_adaptive_huffman_tokens(
    PositionAdaptiveHuffmanProfile profile,std::span<const LzssTypedToken> tokens,
    const LzssTypedFrameValidationContext& context,const core::DecoderLimits& limits,
    std::span<AdaptiveHuffmanNode> nodes,std::span<std::uint16_t> symbols,
    PositionAdaptiveHuffmanDescriptor& output) noexcept {
    const auto* spec=position_adaptive_huffman_spec(profile);
    if (!spec) return failure(Error::invalid_profile);
    Region input{},node{},symbol{};
    if (!region(tokens,input) || !region(nodes,node) || !region(symbols,symbol)) return failure(Error::arithmetic_overflow);
    const auto regions={input,node,symbol,Region{&output,sizeof(output)},Region{&context,sizeof(context)},Region{&limits,sizeof(limits)}};
    if (const auto e=admit(regions,limits);e!=Error::none) return failure(e);
    if (context.declared_raw_size>spec->window_size) return failure(Error::invalid_counts);
    const auto checked=validate_lzss_typed_frame(tokens,parameters(*spec),context,limits,variant(*spec));
    if (checked.error!=LzssTypedFrameValidationError::none)
        return failure(checked.error==LzssTypedFrameValidationError::limit_exceeded ? Error::limit_exceeded
            : checked.error==LzssTypedFrameValidationError::arithmetic_overflow ? Error::arithmetic_overflow : Error::invalid_token,E::none,checked.token_error);
    PositionAdaptiveHuffmanEncoder encoder;
    if (const auto e=encoder.begin_plan(profile,limits,nodes,symbols);e!=E::none) return entropy_failure(e);
    if (const auto e=forward(tokens,[&](int c,std::uint32_t value){return encoder.emit(c,value);});e!=E::none) return entropy_failure(e);
    PositionAdaptiveHuffmanDescriptor d{};
    if (const auto e=encoder.finish_plan(d);e!=E::none) return entropy_failure(e);
    if (!counts(*spec,context.declared_token_count,context.declared_raw_size,d.decision_count)) return failure(Error::invalid_counts);
    if (const auto e=admit(regions,limits,d.payload_size);e!=Error::none) return failure(e);
    Result r{};r.token_count=context.declared_token_count;r.raw_size=context.declared_raw_size;
    r.decision_count=d.decision_count;r.descriptor_size=16;r.payload_size=d.payload_size;
    output=d;return r;
}
PositionAdaptiveHuffmanTokenResult encode_position_adaptive_huffman_tokens(
    PositionAdaptiveHuffmanProfile profile,std::span<const LzssTypedToken> tokens,
    const LzssTypedFrameValidationContext& context,const core::DecoderLimits& limits,
    std::span<AdaptiveHuffmanNode> nodes,std::span<std::uint16_t> symbols,
    std::span<std::byte> scratch,std::span<std::byte> descriptor_output,std::span<std::byte> payload_output) noexcept {
    Region input{},node{},symbol{};
    if (!region(tokens,input) || !region(nodes,node) || !region(symbols,symbol)) return failure(Error::arithmetic_overflow);
    if (const auto e=admit({input,node,symbol,Region{scratch.data(),scratch.size()},
            Region{descriptor_output.data(),descriptor_output.size()},Region{payload_output.data(),payload_output.size()},
            Region{&context,sizeof(context)},Region{&limits,sizeof(limits)}},limits);e!=Error::none) return failure(e);
    PositionAdaptiveHuffmanDescriptor d{};
    const auto planned=plan_position_adaptive_huffman_tokens(profile,tokens,context,limits,nodes,symbols,d);
    if (planned.error!=Error::none) return planned;
    return write_position_adaptive_huffman_tokens(profile,tokens,context,d,limits,nodes,symbols,scratch,descriptor_output,payload_output);
}
PositionAdaptiveHuffmanTokenResult write_position_adaptive_huffman_tokens(
    PositionAdaptiveHuffmanProfile profile,std::span<const LzssTypedToken> tokens,
    const LzssTypedFrameValidationContext& context,const PositionAdaptiveHuffmanDescriptor& d,
    const core::DecoderLimits& limits,std::span<AdaptiveHuffmanNode> nodes,std::span<std::uint16_t> symbols,
    std::span<std::byte> scratch,std::span<std::byte> descriptor_output,std::span<std::byte> payload_output) noexcept {
    const auto* spec=position_adaptive_huffman_spec(profile);
    if (!spec) return failure(Error::invalid_profile);
    Region input{},node{},symbol{};
    if (!region(tokens,input) || !region(nodes,node) || !region(symbols,symbol)) return failure(Error::arithmetic_overflow);
    if (const auto e=admit({input,node,symbol,Region{scratch.data(),scratch.size()},
            Region{descriptor_output.data(),descriptor_output.size()},Region{payload_output.data(),payload_output.size()},
            Region{&context,sizeof(context)},Region{&d,sizeof(d)},Region{&limits,sizeof(limits)}},limits);e!=Error::none) return failure(e);
    if (!counts(*spec,context.declared_token_count,context.declared_raw_size,d.decision_count)) return failure(Error::invalid_counts);
    const auto checked=validate_lzss_typed_frame(tokens,parameters(*spec),context,limits,variant(*spec));
    if (checked.error!=LzssTypedFrameValidationError::none)
        return failure(checked.error==LzssTypedFrameValidationError::limit_exceeded ? Error::limit_exceeded
            : checked.error==LzssTypedFrameValidationError::arithmetic_overflow ? Error::arithmetic_overflow : Error::invalid_token,E::none,checked.token_error);
    if (const auto e=validate_position_adaptive_huffman_descriptor(profile,d,d.decision_count,d.payload_size,limits);e!=E::none) return entropy_failure(e);
    if (scratch.size()<d.payload_size || descriptor_output.size()<16 || payload_output.size()<d.payload_size) return failure(Error::output_too_small);
    PositionAdaptiveHuffmanEncoder encoder;
    if (const auto e=encoder.begin_write(profile,d,limits,nodes,symbols,scratch.first(d.payload_size));e!=E::none) return entropy_failure(e);
    if (const auto e=forward(tokens,[&](int c,std::uint32_t value){return encoder.emit(c,value);});e!=E::none) return entropy_failure(e);
    if (const auto e=encoder.finish_write();e!=E::none) return entropy_failure(e);
    std::array<std::byte,16> serialized{};
    if (const auto e=serialize_position_adaptive_huffman_descriptor(profile,d,limits,serialized);e!=E::none) return entropy_failure(e);
    std::copy(serialized.begin(),serialized.end(),descriptor_output.begin());
    std::copy_n(scratch.begin(),d.payload_size,payload_output.begin());
    Result written{};written.token_count=context.declared_token_count;written.raw_size=context.declared_raw_size;
    written.decision_count=d.decision_count;written.descriptor_size=16;written.payload_size=d.payload_size;return written;
}
PositionAdaptiveHuffmanTokenResult decode_position_adaptive_huffman_token_scratch(
    PositionAdaptiveHuffmanProfile profile,std::span<const std::byte> descriptor,std::span<const std::byte> payload,
    const PositionAdaptiveHuffmanTokenContext& context,const core::DecoderLimits& limits,
    std::span<AdaptiveHuffmanNode> nodes,std::span<std::uint16_t> symbols,std::span<LzssTypedToken> scratch) noexcept {
    const auto* spec=position_adaptive_huffman_spec(profile);
    if (!spec) return failure(Error::invalid_profile);
    Region node{},symbol{},target{};
    if (!region(nodes,node) || !region(symbols,symbol) || !region(scratch,target)) return failure(Error::arithmetic_overflow);
    if (const auto e=admit({node,symbol,target,Region{descriptor.data(),descriptor.size()},Region{payload.data(),payload.size()},
            Region{&context,sizeof(context)},Region{&limits,sizeof(limits)}},limits);e!=Error::none) return failure(e);
    if (!counts(*spec,context.token_count,context.raw_size,context.decision_count)) return failure(Error::invalid_counts);
    std::uint64_t total{};
    if (!core::checked_add(context.output_already_committed,static_cast<std::uint64_t>(context.raw_size),total)) return failure(Error::arithmetic_overflow);
    if (context.raw_size>limits.max_frame_size || context.raw_size>limits.max_block_size || total>limits.max_total_output_size)
        return failure(Error::limit_exceeded);
    const auto parameter_error=validate_lzss_typed_parameters(parameters(*spec),limits,variant(*spec));
    if (parameter_error!=LzssTypedTokenError::none)
        return failure(parameter_error==LzssTypedTokenError::limit_exceeded ? Error::limit_exceeded : Error::invalid_token,E::none,parameter_error);
    if (scratch.size()<context.token_count) return failure(Error::output_too_small);
    if (payload.size()>UINT32_MAX) return failure(Error::invalid_counts);
    PositionAdaptiveHuffmanDescriptor d{};
    if (const auto e=parse_position_adaptive_huffman_descriptor(profile,descriptor,context.decision_count,
            static_cast<std::uint32_t>(payload.size()),limits,d);e!=E::none) return entropy_failure(e);
    PositionAdaptiveHuffmanDecoder decoder;
    if (const auto e=decoder.begin(profile,d,payload,limits,nodes,symbols);e!=E::none) return entropy_failure(e);
    unsigned previous=0;int literal=-1;std::uint64_t raw{};E error{};
    const auto read=[&](int c,std::uint32_t& value) { error=decoder.read(c,value);return error==E::none; };
    for (std::size_t i=0;i<context.token_count;++i) {
        std::uint32_t kind{};if (!read(static_cast<int>(previous),kind)) return entropy_failure(error);
        LzssTypedToken token{};
        if (kind==0) {
            std::uint32_t value{};if (!read(literal<0 ? 3 : 4+(literal>>4),value)) return entropy_failure(error);
            token.literal=static_cast<std::uint8_t>(value);literal=static_cast<int>(value);previous=1;
        } else {
            std::uint32_t lc{};if (!read(static_cast<int>(20+previous),lc)) return entropy_failure(error);
            const auto width=static_cast<std::uint8_t>(lc==8 ? 1 : lc);std::uint32_t extra{};
            for (unsigned b=0;b<width;++b) {
                std::uint32_t bit{};if (!read(-1,bit)) return entropy_failure(error);extra|=bit<<b;
            }
            const auto length=decode_lzss_short_length_escape(lc,width,extra);
            if (length.error!=LzssShortLengthEscapeError::none) return failure(Error::invalid_token,E::none,LzssTypedTokenError::invalid_length);
            std::uint32_t dc{};if (!read(static_cast<int>(23+lc),dc)) return entropy_failure(error);
            std::uint32_t de{};
            for (unsigned b=0;b<dc;++b) {
                std::uint32_t bit{};if (!read(static_cast<int>(32+b),bit)) return entropy_failure(error);de|=bit<<b;
            }
            token.kind=LzssTypedTokenKind::match;token.length=length.length;token.distance=(UINT32_C(1)<<dc)+de;previous=2;
        }
        std::uint64_t next{};
        const auto token_error=validate_lzss_typed_token(token,parameters(*spec),{raw,context.raw_size},limits,next,variant(*spec));
        if (token_error!=LzssTypedTokenError::none)
            return failure(token_error==LzssTypedTokenError::limit_exceeded ? Error::limit_exceeded
                : token_error==LzssTypedTokenError::arithmetic_overflow ? Error::arithmetic_overflow : Error::invalid_token,E::none,token_error);
        scratch[i]=token;raw=next;
    }
    if (const auto e=decoder.finish();e!=E::none) return entropy_failure(e);
    if (raw!=context.raw_size) return failure(Error::invalid_counts);
    Result result{};result.token_count=context.token_count;result.raw_size=context.raw_size;
    result.decision_count=context.decision_count;result.descriptor_size=16;result.payload_size=payload.size();return result;
}
PositionAdaptiveHuffmanTokenResult decode_position_adaptive_huffman_tokens(
    PositionAdaptiveHuffmanProfile profile,std::span<const std::byte> descriptor,std::span<const std::byte> payload,
    const PositionAdaptiveHuffmanTokenContext& context,const core::DecoderLimits& limits,
    std::span<AdaptiveHuffmanNode> nodes,std::span<std::uint16_t> symbols,std::span<LzssTypedToken> scratch,
    std::span<LzssTypedToken> output) noexcept {
    Region node{},symbol{},staging{},target{};
    if (!region(nodes,node) || !region(symbols,symbol) || !region(scratch,staging) || !region(output,target)) return failure(Error::arithmetic_overflow);
    if (const auto e=admit({node,symbol,staging,target,Region{descriptor.data(),descriptor.size()},Region{payload.data(),payload.size()},
            Region{&context,sizeof(context)},Region{&limits,sizeof(limits)}},limits);e!=Error::none) return failure(e);
    if (output.size()<context.token_count) return failure(Error::output_too_small);
    const auto decoded=decode_position_adaptive_huffman_token_scratch(profile,descriptor,payload,context,limits,nodes,symbols,scratch);
    if (decoded.error==Error::none) std::copy_n(scratch.begin(),context.token_count,output.begin());
    return decoded;
}
} // namespace marc::context::internal
