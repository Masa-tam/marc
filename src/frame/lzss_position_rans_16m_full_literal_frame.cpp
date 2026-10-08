#include "frame/lzss_position_rans_16m_full_literal_frame.hpp"
#include "core/buffer_overlap.hpp"
#include <algorithm>

namespace marc::frame::internal {
namespace {
using Error=PositionRans16mFullLiteralFrameError;
using Token=dictionary::internal::LzssTypedToken;
struct Region { const void* data; std::size_t size; };
template<std::size_t N> Error disjoint(const std::array<Region,N>& regions) noexcept {
    for (std::size_t i=0;i<N;++i) for (std::size_t j=i+1;j<N;++j) {
        const auto result=core::check_buffer_overlap(regions[i].data,regions[i].size,regions[j].data,regions[j].size);
        if (result!=core::BufferOverlap::disjoint) return result==core::BufferOverlap::arithmetic_overflow
            ? Error::arithmetic_overflow : Error::overlapping_buffers;
    }
    return Error::none;
}
PositionRans16mFullLiteralFrameResult decode(std::span<const std::byte> input,const PositionRans16mFullLiteralFrameContext& context,
    std::span<Token> tokens,std::span<std::byte> raw,bool scratch) noexcept {
    PositionRans16mFullLiteralFrameResult r{};
    PositionRans16mFullLiteralFrameLayout layout{};
    r.format_error=preflight_position_rans_16m_full_literal_frame(input,context,layout);
    if (r.format_error!=PositionRans16mFullLiteralFormatError::none) { r.error=Error::format_error; return r; }
    r.requirements=layout.requirements;
    if (tokens.size()<r.requirements.token_count) { r.error=Error::token_output_too_small; return r; }
    if (raw.size()<r.requirements.raw_size) { r.error=Error::raw_output_too_small; return r; }
    input=input.first(r.requirements.serialized_size);
    tokens=tokens.first(r.requirements.token_count); raw=raw.first(r.requirements.raw_size);
    r.error=disjoint(std::array{Region{input.data(),input.size()},Region{tokens.data(),tokens.size_bytes()},
        Region{raw.data(),raw.size()},Region{&context,sizeof(context)},Region{&context.stream,sizeof(context.stream)},
        Region{&context.limits,sizeof(context.limits)}});
    if (r.error!=Error::none) return r;
    const auto& h=layout.header;
    const auto descriptor=input.subspan(64,h.descriptor_size);
    const auto payload=input.subspan(r.requirements.prefix_size,h.payload_size);
    const context::internal::LzssFieldContextValidationContext tc{h.token_count,h.event_count,h.decision_count,h.raw_size,context.raw_committed};
    const auto reader=scratch ? context::internal::decode_lzss_position_rans_16m_full_literal_token_scratch
        : context::internal::decode_lzss_position_rans_16m_full_literal_tokens;
    r.token_result=reader(descriptor,payload,context.stream.dictionary,tc,context.limits,tokens);
    if (r.token_result.error!=context::internal::LzssPositionRans16mFullLiteralTokenError::none) { r.error=Error::token_error; return r; }
    const auto reconstructed=dictionary::internal::reconstruct_lzss_typed_frame(tokens,context.stream.dictionary,
        {h.token_count,h.raw_size,context.raw_committed},context.limits,raw,
        dictionary::internal::LzssTypedTokenVariant::field_context_16m_short_length_escape);
    if (reconstructed.error!=dictionary::internal::LzssTypedReconstructError::none) { r.error=Error::reconstruction_error; return r; }
    r.serialized_size=r.requirements.serialized_size;
    return r;
}
} // namespace
PositionRans16mFullLiteralFrameResult encode_position_rans_16m_full_literal_frame(
    const std::span<const Token> tokens,const PositionRans16mFullLiteralFrameContext& context,std::span<std::byte> output) noexcept {
    PositionRans16mFullLiteralFrameResult r{};
    r.format_error=validate_position_rans_16m_full_literal_stream(context.stream,context.limits);
    if (r.format_error!=PositionRans16mFullLiteralFormatError::none) { r.error=Error::format_error; return r; }
    if (context.raw_committed>=context.stream.original_size || context.raw_committed%context.stream.frame_size
        || context.sequence!=context.raw_committed/context.stream.frame_size) {
        r.format_error=PositionRans16mFullLiteralFormatError::invalid_sequence; r.error=Error::format_error; return r;
    }
    if (tokens.size()>UINT32_MAX) { r.error=Error::arithmetic_overflow; return r; }
    const auto size=static_cast<std::uint32_t>(std::min<std::uint64_t>(context.stream.frame_size,
        context.stream.original_size-context.raw_committed));
    const dictionary::internal::LzssTypedFrameValidationContext tc{static_cast<std::uint32_t>(tokens.size()),size,context.raw_committed};
    entropy::internal::PositionRans16mFullLiteralDescriptor model{};
    r.token_result=context::internal::plan_lzss_position_rans_16m_full_literal_tokens(tokens,context.stream.dictionary,tc,context.limits,model);
    if (r.token_result.error!=context::internal::LzssPositionRans16mFullLiteralTokenError::none) { r.error=Error::token_error; return r; }
    const auto& tr=r.token_result;
    const PositionRans16mFullLiteralFrameHeader h{0,context.sequence,size,tr.token_count,tr.event_count,tr.decision_count,
        static_cast<std::uint32_t>(tr.payload_size),static_cast<std::uint32_t>(tr.descriptor_size),0,0};
    r.format_error=validate_position_rans_16m_full_literal_frame_header(h,context,r.requirements);
    if (r.format_error!=PositionRans16mFullLiteralFormatError::none) { r.error=Error::format_error; return r; }
    if (output.size()<r.requirements.serialized_size) { r.error=Error::serialized_output_too_small; return r; }
    output=output.first(r.requirements.serialized_size);
    r.error=disjoint(std::array{Region{tokens.data(),tokens.size_bytes()},Region{output.data(),output.size()},
        Region{&context,sizeof(context)},Region{&context.stream,sizeof(context.stream)},Region{&context.limits,sizeof(context.limits)}});
    if (r.error!=Error::none) return r;
    std::array<std::byte,64> header{};
    r.format_error=serialize_position_rans_16m_full_literal_frame_header(h,context,header);
    if (r.format_error!=PositionRans16mFullLiteralFormatError::none) { r.error=Error::format_error; return r; }
    r.token_result=context::internal::encode_lzss_position_rans_16m_full_literal_tokens(tokens,context.stream.dictionary,tc,context.limits,
        output.subspan(64,h.descriptor_size),output.subspan(r.requirements.prefix_size,h.payload_size));
    if (r.token_result.error!=context::internal::LzssPositionRans16mFullLiteralTokenError::none) { r.error=Error::token_error; return r; }
    std::copy(header.begin(),header.end(),output.begin());
    r.serialized_size=r.requirements.serialized_size;
    return r;
}
PositionRans16mFullLiteralFrameResult decode_position_rans_16m_full_literal_frame(std::span<const std::byte> input,
    const PositionRans16mFullLiteralFrameContext& context,std::span<Token> tokens,std::span<std::byte> raw) noexcept {
    return decode(input,context,tokens,raw,false);
}
PositionRans16mFullLiteralFrameResult decode_position_rans_16m_full_literal_frame_scratch(std::span<const std::byte> input,
    const PositionRans16mFullLiteralFrameContext& context,std::span<Token> tokens,std::span<std::byte> raw) noexcept {
    return decode(input,context,tokens,raw,true);
}
} // namespace marc::frame::internal
