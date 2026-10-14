#include "frame/lzss_position_tans_4m_frame.hpp"
#include "core/buffer_overlap.hpp"
#include <algorithm>

namespace marc::frame::internal {
namespace {
using Error=PositionTans4mFrameError;
using Result=PositionTans4mFrameResult;
using Token=dictionary::internal::LzssTypedToken;
using Entry=entropy::internal::TansDecodeEntry;
struct Region { const void* data;std::size_t bytes; };
template<class T> bool region(std::span<T> s,Region& r,Result& result) noexcept {
    r.data=s.data();
    if (!core::checked_multiply(s.size(),sizeof(T),r.bytes)) {
        result.error=Error::arithmetic_overflow;return false;
    }
    return true;
}
template<std::size_t N> bool prepare(const std::array<Region,N>& regions,
                                    const core::DecoderLimits& limits,Result& r) noexcept {
    // Covers fixed internal work plus retained local control/model objects.
    std::size_t total=131072+sizeof(entropy::internal::PositionTans4mDescriptor)
        +sizeof(context::internal::PositionTans4mTokenContext)
        +sizeof(dictionary::internal::LzssTypedFrameValidationContext);
    for (std::size_t i=0;i<N;++i) {
        if (!core::checked_add(total,regions[i].bytes,total)) { r.error=Error::arithmetic_overflow;return false; }
        for (std::size_t j=0;j<i;++j) {
            const auto e=core::check_buffer_overlap(regions[i].data,regions[i].bytes,regions[j].data,regions[j].bytes);
            if (e!=core::BufferOverlap::disjoint) {
                r.error=e==core::BufferOverlap::arithmetic_overflow ? Error::arithmetic_overflow : Error::overlapping_buffers;
                return false;
            }
        }
    }
    if (core::validate_limits(limits)!=core::LimitError::none || total>limits.max_internal_buffered_bytes) {
        r.error=Error::limit_exceeded;return false;
    }
    return true;
}
Result decode(std::span<const std::byte> input,const PositionTans4mFrameContext& c,
              std::span<Entry> tables,std::span<Token> tokens,std::span<std::byte> scratch,
              std::span<std::byte> output,bool publish) noexcept {
    Result r{};Region bank{},temporary{};
    if (!region(tables,bank,r) || !region(tokens,temporary,r)) return r;
    if (!prepare(std::array{Region{input.data(),input.size()},bank,temporary,
        Region{scratch.data(),scratch.size()},Region{output.data(),output.size()},
        Region{&c,sizeof(c)},Region{&c.stream,sizeof(c.stream)},Region{&c.limits,sizeof(c.limits)}},c.limits,r)) return r;
    PositionTans4mFrameLayout layout{};
    r.format_error=preflight_position_tans_4m_frame(input,c,layout);
    if (r.format_error!=PositionTans4mFormatError::none) { r.error=Error::format_error;return r; }
    r.requirements=layout.requirements;
    const auto& h=layout.header;
    if (tokens.size()<h.token_count || scratch.size()<h.raw_size || (publish && output.size()<h.raw_size)) {
        r.error=Error::output_too_small;return r;
    }
    const context::internal::PositionTans4mTokenContext tc{h.token_count,h.raw_size,h.decision_count,c.raw_committed};
    r.token_result=context::internal::decode_position_tans_4m_token_scratch(
        input.subspan(64,h.descriptor_size),input.subspan(layout.requirements.prefix_size,h.payload_size),
        tc,c.limits,tables,tokens);
    if (r.token_result.error!=context::internal::PositionTans4mTokenError::none) { r.error=Error::token_error;return r; }
    r.reconstruction=dictionary::internal::reconstruct_lzss_typed_frame(tokens.first(h.token_count),c.stream.dictionary,
        {h.token_count,h.raw_size,c.raw_committed},c.limits,scratch,
        dictionary::internal::LzssTypedTokenVariant::field_context_4m_short_length_escape);
    if (r.reconstruction.error!=dictionary::internal::LzssTypedReconstructError::none
        || r.reconstruction.output_size!=h.raw_size) { r.error=Error::reconstruction_error;return r; }
    if (publish) std::copy_n(scratch.begin(),h.raw_size,output.begin());
    r.serialized_size=layout.requirements.serialized_size;r.raw_size=h.raw_size;
    return r;
}
} // namespace
PositionTans4mFrameResult decode_position_tans_4m_frame(
    std::span<const std::byte> input,const PositionTans4mFrameContext& c,
    std::span<Entry> tables,std::span<Token> tokens,std::span<std::byte> scratch,std::span<std::byte> output) noexcept {
    return decode(input,c,tables,tokens,scratch,output,true);
}
PositionTans4mFrameResult decode_position_tans_4m_frame_scratch(
    std::span<const std::byte> input,const PositionTans4mFrameContext& c,
    std::span<Entry> tables,std::span<Token> tokens,std::span<std::byte> scratch) noexcept {
    return decode(input,c,tables,tokens,scratch,{},false);
}
PositionTans4mFrameResult encode_position_tans_4m_frame(std::span<const Token> tokens,
    const PositionTans4mFrameContext& c,std::span<std::uint16_t> tables,std::span<std::byte> payload,
    std::span<std::byte> scratch,std::span<std::byte> output) noexcept {
    Result r{};Region input{},bank{};
    if (!region(tokens,input,r) || !region(tables,bank,r)) return r;
    if (!prepare(std::array{input,bank,Region{payload.data(),payload.size()},Region{scratch.data(),scratch.size()},
        Region{output.data(),output.size()},Region{&c,sizeof(c)},Region{&c.stream,sizeof(c.stream)},
        Region{&c.limits,sizeof(c.limits)}},c.limits,r)) return r;
    r.format_error=validate_position_tans_4m_stream(c.stream,c.limits);
    if (r.format_error!=PositionTans4mFormatError::none) { r.error=Error::format_error;return r; }
    if (c.raw_committed>=c.stream.original_size || c.raw_committed%c.stream.frame_size
        || c.sequence!=c.raw_committed/c.stream.frame_size) {
        r.format_error=PositionTans4mFormatError::invalid_sequence;r.error=Error::format_error;return r;
    }
    if (tokens.size()>UINT32_MAX) { r.error=Error::arithmetic_overflow;return r; }
    const auto raw=static_cast<std::uint32_t>(std::min<std::uint64_t>(c.stream.frame_size,c.stream.original_size-c.raw_committed));
    const dictionary::internal::LzssTypedFrameValidationContext tc{static_cast<std::uint32_t>(tokens.size()),raw,c.raw_committed};
    entropy::internal::PositionTans4mDescriptor model{};
    r.token_result=context::internal::plan_position_tans_4m_tokens(tokens,tc,c.limits,tables,model);
    if (r.token_result.error!=context::internal::PositionTans4mTokenError::none) { r.error=Error::token_error;return r; }
    const auto& tr=r.token_result;
    if (tr.payload_size>UINT32_MAX || tr.descriptor_size>UINT32_MAX) { r.error=Error::arithmetic_overflow;return r; }
    const PositionTans4mFrameHeader h{0,c.sequence,raw,tr.token_count,tr.decision_count,tr.decision_count,
        static_cast<std::uint32_t>(tr.payload_size),static_cast<std::uint32_t>(tr.descriptor_size),0,0};
    r.format_error=validate_position_tans_4m_frame_header(h,c,r.requirements);
    if (r.format_error!=PositionTans4mFormatError::none) { r.error=Error::format_error;return r; }
    if (scratch.size()<r.requirements.serialized_size || output.size()<r.requirements.serialized_size || payload.size()<tr.payload_size) {
        r.error=Error::output_too_small;return r;
    }
    r.token_result=context::internal::encode_position_tans_4m_tokens(tokens,tc,c.limits,tables,payload,
        scratch.subspan(64,h.descriptor_size),scratch.subspan(r.requirements.prefix_size,h.payload_size));
    if (r.token_result.error!=context::internal::PositionTans4mTokenError::none) { r.error=Error::token_error;return r; }
    r.format_error=serialize_position_tans_4m_frame_header(h,c,scratch.first(64));
    if (r.format_error!=PositionTans4mFormatError::none) { r.error=Error::format_error;return r; }
    std::copy_n(scratch.begin(),r.requirements.serialized_size,output.begin());
    r.serialized_size=r.requirements.serialized_size;r.raw_size=raw;return r;
}
} // namespace marc::frame::internal



