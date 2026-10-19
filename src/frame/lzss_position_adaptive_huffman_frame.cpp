#include "frame/lzss_position_adaptive_huffman_frame.hpp"
#include "core/buffer_overlap.hpp"
#include <algorithm>
#include <initializer_list>

namespace marc::frame::internal {
namespace {
using namespace entropy::internal;
using Token=dictionary::internal::LzssTypedToken;
using Error=PositionAdaptiveHuffmanFrameError;
using Result=PositionAdaptiveHuffmanFrameResult;
using Format=PositionAdaptiveHuffmanFormatError;
using TokenResult=context::internal::PositionAdaptiveHuffmanTokenResult;
using TokenError=context::internal::PositionAdaptiveHuffmanTokenError;
struct Region { const void* data{};std::size_t size{}; };
Result failure(Error error,Format format=Format::none) noexcept {
    Result result{};result.error=error;result.format_error=format;return result;
}
Result format_failure(Format error) noexcept {
    return failure(error==Format::limit_exceeded ? Error::limit_exceeded
        : error==Format::arithmetic_overflow ? Error::arithmetic_overflow
        : error==Format::output_too_small ? Error::output_too_small : Error::format_error,error);
}
Result token_failure(const TokenResult& token) noexcept {
    auto result=failure(token.error==TokenError::limit_exceeded ? Error::limit_exceeded
        : token.error==TokenError::arithmetic_overflow ? Error::arithmetic_overflow
        : token.error==TokenError::output_too_small ? Error::output_too_small
        : token.error==TokenError::overlapping_buffers ? Error::overlapping_buffers : Error::token_error);
    result.token_result.error=token.error;result.token_result.token_error=token.token_error;
    result.token_result.entropy_error=token.entropy_error;return result;
}
template<class T> bool region(std::span<T> input,Region& result) noexcept {
    std::size_t bytes{};
    if (!core::checked_multiply(input.size(),sizeof(T),bytes)) return false;
    result={input.data(),bytes};return true;
}
Error prepare(std::initializer_list<Region> regions,const core::DecoderLimits& limits) noexcept {
    std::size_t total=131072;
    for (auto a=regions.begin();a!=regions.end();++a) {
        if (!core::checked_add(total,a->size,total)) return Error::arithmetic_overflow;
        for (auto b=regions.begin();b!=a;++b) {
            const auto overlap=core::check_buffer_overlap(a->data,a->size,b->data,b->size);
            if (overlap==core::BufferOverlap::arithmetic_overflow) return Error::arithmetic_overflow;
            if (overlap!=core::BufferOverlap::disjoint) return Error::overlapping_buffers;
        }
    }
    return core::validate_limits(limits)!=core::LimitError::none || total>limits.max_internal_buffered_bytes
        ? Error::limit_exceeded : Error::none;
}
Result decode(std::span<const std::byte> input,const PositionAdaptiveHuffmanFrameContext& context,
    std::span<AdaptiveHuffmanNode> nodes,std::span<std::uint16_t> symbols,std::span<Token> tokens,
    std::span<std::byte> raw,std::span<std::byte> output,bool publish) noexcept {
    PositionAdaptiveHuffmanFrameLayout layout{};
    if (const auto e=preflight_position_adaptive_huffman_frame(input,context,layout);e!=Format::none)
        return format_failure(e);
    const auto& req=layout.requirements;const auto& h=layout.header;
    Region node{},symbol{},token{};
    if (!region(nodes,node) || !region(symbols,symbol) || !region(tokens,token)) return failure(Error::arithmetic_overflow);
    if (const auto e=prepare({node,symbol,token,Region{input.data(),input.size()},Region{raw.data(),raw.size()},
            Region{output.data(),output.size()},Region{&context,sizeof(context)},Region{&context.stream,sizeof(context.stream)},
            Region{&context.limits,sizeof(context.limits)}},context.limits);e!=Error::none) return failure(e);
    if (nodes.size()<req.node_entries || symbols.size()<req.symbol_entries || tokens.size()<req.token_count
        || raw.size()<req.raw_size || (publish && output.size()<req.raw_size)) return failure(Error::output_too_small);
    const context::internal::PositionAdaptiveHuffmanTokenContext tc{h.token_count,h.raw_size,h.decision_count,context.raw_committed};
    const auto decoded=context::internal::decode_position_adaptive_huffman_token_scratch(context.stream.profile,input.subspan(64,16),
        input.subspan(80,h.payload_size),tc,context.limits,nodes,symbols,tokens);
    if (decoded.error!=TokenError::none) return token_failure(decoded);
    const dictionary::internal::LzssTypedFrameValidationContext rc{h.token_count,h.raw_size,context.raw_committed};
    const auto* spec=position_adaptive_huffman_spec(context.stream.profile);
    const auto restored=dictionary::internal::reconstruct_lzss_typed_frame(tokens.first(h.token_count),context.stream.dictionary,
        rc,context.limits,raw.first(h.raw_size),static_cast<dictionary::internal::LzssTypedTokenVariant>(spec->dictionary_variant));
    if (restored.error!=dictionary::internal::LzssTypedReconstructError::none) {
        auto result=failure(Error::reconstruction_error);result.reconstruction_error=restored.error;return result;
    }
    if (publish) std::copy_n(raw.begin(),h.raw_size,output.begin());
    Result result{};result.serialized_size=req.serialized_size;result.raw_size=h.raw_size;
    result.token_result=decoded;result.requirements=req;return result;
}
}
PositionAdaptiveHuffmanFrameResult decode_position_adaptive_huffman_frame(
    std::span<const std::byte> input,const PositionAdaptiveHuffmanFrameContext& context,
    std::span<AdaptiveHuffmanNode> nodes,std::span<std::uint16_t> symbols,std::span<Token> tokens,
    std::span<std::byte> raw,std::span<std::byte> output) noexcept {
    return decode(input,context,nodes,symbols,tokens,raw,output,true);
}
PositionAdaptiveHuffmanFrameResult decode_position_adaptive_huffman_frame_scratch(
    std::span<const std::byte> input,const PositionAdaptiveHuffmanFrameContext& context,
    std::span<AdaptiveHuffmanNode> nodes,std::span<std::uint16_t> symbols,std::span<Token> tokens,
    std::span<std::byte> raw) noexcept {
    return decode(input,context,nodes,symbols,tokens,raw,{},false);
}
PositionAdaptiveHuffmanFrameResult encode_position_adaptive_huffman_frame(std::span<const Token> tokens,
    const PositionAdaptiveHuffmanFrameContext& context,std::span<AdaptiveHuffmanNode> nodes,
    std::span<std::uint16_t> symbols,std::span<std::byte> payload,std::span<std::byte> scratch,std::span<std::byte> output) noexcept {
    Region input{},node{},symbol{};
    if (!region(tokens,input) || !region(nodes,node) || !region(symbols,symbol)) return failure(Error::arithmetic_overflow);
    if (const auto e=prepare({input,node,symbol,Region{payload.data(),payload.size()},Region{scratch.data(),scratch.size()},
            Region{output.data(),output.size()},Region{&context,sizeof(context)},Region{&context.stream,sizeof(context.stream)},
            Region{&context.limits,sizeof(context.limits)}},context.limits);e!=Error::none) return failure(e);
    if (const auto e=validate_position_adaptive_huffman_stream(context.stream,context.limits);e!=Format::none)
        return format_failure(e);
    if (context.raw_committed>=context.stream.original_size || context.raw_committed%context.stream.frame_size
        || context.sequence!=context.raw_committed/context.stream.frame_size) return failure(Error::format_error,Format::invalid_sequence);
    if (tokens.size()>UINT32_MAX) return failure(Error::arithmetic_overflow);
    const auto raw=static_cast<std::uint32_t>(std::min<std::uint64_t>(context.stream.frame_size,context.stream.original_size-context.raw_committed));
    const dictionary::internal::LzssTypedFrameValidationContext tc{static_cast<std::uint32_t>(tokens.size()),raw,context.raw_committed};
    PositionAdaptiveHuffmanDescriptor model{};
    const auto planned=context::internal::plan_position_adaptive_huffman_tokens(context.stream.profile,tokens,tc,context.limits,nodes,symbols,model);
    if (planned.error!=TokenError::none) return token_failure(planned);
    PositionAdaptiveHuffmanFrameHeader h{};h.sequence=context.sequence;h.raw_size=raw;h.token_count=planned.token_count;
    h.event_count=h.decision_count=planned.decision_count;h.payload_size=model.payload_size;
    PositionAdaptiveHuffmanFrameRequirements req{};
    if (const auto e=validate_position_adaptive_huffman_frame_header(h,context,req);e!=Format::none) return format_failure(e);
    if (scratch.size()<req.serialized_size || output.size()<req.serialized_size || payload.size()<model.payload_size)
        return failure(Error::output_too_small);
    const auto written=context::internal::write_position_adaptive_huffman_tokens(context.stream.profile,tokens,tc,model,
        context.limits,nodes,symbols,payload,scratch.subspan(64,16),scratch.subspan(80,model.payload_size));
    if (written.error!=TokenError::none) return token_failure(written);
    if (const auto e=serialize_position_adaptive_huffman_frame_header(h,context,scratch.first(64));e!=Format::none)
        return format_failure(e);
    std::copy_n(scratch.begin(),req.serialized_size,output.begin());
    Result result{};result.serialized_size=req.serialized_size;result.raw_size=raw;
    result.token_result=written;result.requirements=req;return result;
}
PositionAdaptiveHuffmanFrameResult encode_position_adaptive_huffman_frame_scratch(std::span<const Token> tokens,
    const PositionAdaptiveHuffmanFrameContext& context,std::span<AdaptiveHuffmanNode> nodes,
    std::span<std::uint16_t> symbols,std::span<std::byte> scratch) noexcept {
    Region input{},node{},symbol{};
    if (!region(tokens,input) || !region(nodes,node) || !region(symbols,symbol)) return failure(Error::arithmetic_overflow);
    if (const auto e=prepare({input,node,symbol,Region{scratch.data(),scratch.size()},
            Region{&context,sizeof(context)},Region{&context.stream,sizeof(context.stream)},
            Region{&context.limits,sizeof(context.limits)}},context.limits);e!=Error::none) return failure(e);
    if (const auto e=validate_position_adaptive_huffman_stream(context.stream,context.limits);e!=Format::none)
        return format_failure(e);
    if (context.raw_committed>=context.stream.original_size || context.raw_committed%context.stream.frame_size
        || context.sequence!=context.raw_committed/context.stream.frame_size) return failure(Error::format_error,Format::invalid_sequence);
    if (tokens.size()>UINT32_MAX) return failure(Error::arithmetic_overflow);
    const auto raw=static_cast<std::uint32_t>(std::min<std::uint64_t>(context.stream.frame_size,context.stream.original_size-context.raw_committed));
    const dictionary::internal::LzssTypedFrameValidationContext tc{static_cast<std::uint32_t>(tokens.size()),raw,context.raw_committed};
    PositionAdaptiveHuffmanDescriptor model{};
    const auto planned=context::internal::plan_position_adaptive_huffman_tokens(context.stream.profile,tokens,tc,context.limits,nodes,symbols,model);
    if (planned.error!=TokenError::none) return token_failure(planned);
    PositionAdaptiveHuffmanFrameHeader h{};h.sequence=context.sequence;h.raw_size=raw;h.token_count=planned.token_count;
    h.event_count=h.decision_count=planned.decision_count;h.payload_size=model.payload_size;
    PositionAdaptiveHuffmanFrameRequirements req{};
    if (const auto e=validate_position_adaptive_huffman_frame_header(h,context,req);e!=Format::none) return format_failure(e);
    if (scratch.size()<req.serialized_size) return failure(Error::output_too_small);
    const auto written=context::internal::write_position_adaptive_huffman_token_scratch(context.stream.profile,tokens,tc,model,
        context.limits,nodes,symbols,scratch.subspan(80,model.payload_size));
    if (written.error!=TokenError::none) return token_failure(written);
    if (const auto e=serialize_position_adaptive_huffman_descriptor(context.stream.profile,model,context.limits,scratch.subspan(64,16));e!=PositionAdaptiveHuffmanError::none)
        return token_failure(TokenResult{.entropy_error=e,.error=TokenError::entropy_error});
    if (const auto e=serialize_position_adaptive_huffman_frame_header(h,context,scratch.first(64));e!=Format::none)
        return format_failure(e);
    Result result{};result.serialized_size=req.serialized_size;result.raw_size=raw;
    result.token_result=written;result.requirements=req;return result;
}
} // namespace marc::frame::internal
