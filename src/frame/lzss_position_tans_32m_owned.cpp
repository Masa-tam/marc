#include "frame/lzss_position_tans_32m_owned.hpp"
#include "dictionary/lzss_position_distance_32m_five_prefix_candidate.hpp"
#include "core/buffer_overlap.hpp"
#include <algorithm>
#include <cstring>
#include <new>

namespace marc::frame::internal {
namespace {
using Token=dictionary::internal::LzssTypedToken;
using Code=core::ErrorCode;
Code format_error(PositionTans32mFormatError e) noexcept {
    return e==PositionTans32mFormatError::limit_exceeded || e==PositionTans32mFormatError::arithmetic_overflow
        ? Code::limit_exceeded : Code::invalid_argument;
}
Code requirements(const PositionTans32mStreamHeader& stream,const core::DecoderLimits& limits,
    bool encode,std::size_t owner,PositionTans32mWorkspace& result) noexcept {
    if (const auto e=validate_position_tans_32m_stream(stream,limits);e!=PositionTans32mFormatError::none) return format_error(e);
    PositionTans32mWorkspace r{};
    r.raw_bytes=stream.frame_size; r.token_count=stream.frame_size;
    r.owner_bytes=owner;
    r.fixed_working_bytes=131072+sizeof(entropy::internal::PositionTans32mDescriptor)
        +sizeof(context::internal::PositionTans32mTokenContext)
        +sizeof(dictionary::internal::LzssTypedFrameValidationContext);
    std::size_t bits{};
    if (!core::checked_multiply(r.raw_bytes,std::size_t{96},bits)
        || !core::checked_add(bits,std::size_t{7},bits)
        || !core::checked_add(bits/8,std::size_t{2},r.payload_bytes)
        || !core::checked_add(r.payload_bytes,std::size_t{64+9313},r.serialized_bytes)
        || !core::checked_multiply(r.token_count,sizeof(Token),r.token_bytes)
        || !core::checked_multiply(entropy::internal::position_tans_32m_table_entries,
            encode ? sizeof(std::uint16_t) : sizeof(entropy::internal::TansDecodeEntry),r.table_bytes))
        return Code::limit_exceeded;
    if (encode) {
        r.private_serialized_bytes=r.serialized_bytes;
        const auto f=dictionary::internal::calculate_lzss_position_distance_32m_five_prefix_workspace(
            r.raw_bytes,stream.dictionary,limits);
        if (f.error!=dictionary::internal::LzssShortPrefixError::none) return Code::limit_exceeded;
        r.finder_bytes=f.workspace_size;
    } else r.payload_bytes=0; // Payload is retained inside serialized_ on decode.
    for (const auto bytes : {r.raw_bytes,r.token_bytes,r.serialized_bytes,r.finder_bytes,
                            r.table_bytes,r.payload_bytes,r.private_serialized_bytes})
        if (!core::checked_add(r.owned_buffer_bytes,bytes,r.owned_buffer_bytes)) return Code::limit_exceeded;
    if (!core::checked_add(r.owner_bytes,r.owned_buffer_bytes,r.aggregate_bytes)
        || !core::checked_add(r.aggregate_bytes,r.fixed_working_bytes,r.aggregate_bytes))
        return Code::limit_exceeded;
    // Shared header admission also charges a full decode bank. Expose this
    // conservative nonallocated floor separately from actual owned buffers.
    r.metadata_floor_bytes=131072;
    for (const auto bytes : {r.raw_bytes,r.token_bytes,r.serialized_bytes,
        entropy::internal::position_tans_32m_table_entries*sizeof(entropy::internal::TansDecodeEntry)})
        if (!core::checked_add(r.metadata_floor_bytes,bytes,r.metadata_floor_bytes)) return Code::limit_exceeded;
    r.aggregate_bytes=std::max(r.aggregate_bytes,r.metadata_floor_bytes);
    if (r.aggregate_bytes>limits.max_internal_buffered_bytes) return Code::limit_exceeded;
    result=r; return Code::none;
}
}
Code PositionTans32mOwnedEncoder::requirements(const PositionTans32mStreamHeader& s,
    const core::DecoderLimits& l,PositionTans32mWorkspace& r) noexcept {
    return internal::requirements(s,l,true,sizeof(PositionTans32mOwnedEncoder),r);
}
std::unique_ptr<PositionTans32mOwnedEncoder> PositionTans32mOwnedEncoder::create(
    const PositionTans32mStreamHeader& stream,const core::DecoderLimits& limits,Code& error) noexcept {
    const auto s=stream; const auto l=limits;
    PositionTans32mWorkspace r{};
    error=requirements(s,l,r); if (error!=Code::none) return {};
    std::unique_ptr<PositionTans32mOwnedEncoder> p(new(std::nothrow) PositionTans32mOwnedEncoder);
    if (!p) { error=Code::out_of_memory; return {}; }
    p->stream_=s; p->limits_=l; p->workspace_=r;
    p->raw_.reset(new(std::nothrow) std::byte[r.raw_bytes]);
    if (!p->raw_) { error=Code::out_of_memory; return {}; }
    p->serialized_.reset(new(std::nothrow) std::byte[r.serialized_bytes]);
    if (!p->serialized_) { error=Code::out_of_memory; return {}; }
    p->tokens_.reset(new(std::nothrow) Token[r.token_count]);
    if (!p->tokens_) { error=Code::out_of_memory; return {}; }
    static_assert(alignof(std::uint32_t)<=alignof(std::max_align_t));
    if (r.finder_bytes) p->finder_.reset(new(std::nothrow) std::byte[r.finder_bytes]);
    if (!p->raw_ || !p->serialized_ || !p->tokens_ || (r.finder_bytes && !p->finder_)) {
        error=Code::out_of_memory; return {};
    }
    p->tables_.reset(new(std::nothrow) std::uint16_t[entropy::internal::position_tans_32m_table_entries]);
    if (!p->tables_) { error=Code::out_of_memory; return {}; }
    p->payload_.reset(new(std::nothrow) std::byte[r.payload_bytes]);
    if (!p->payload_) { error=Code::out_of_memory; return {}; }
    p->private_serialized_.reset(new(std::nothrow) std::byte[r.private_serialized_bytes]);
    if (!p->private_serialized_) { error=Code::out_of_memory; return {}; }
    if (const auto e=serialize_position_tans_32m_stream(s,l,p->header_);e!=PositionTans32mFormatError::none) {
        error=format_error(e); return {};
    }
    error=Code::none; return p;
}
bool PositionTans32mOwnedEncoder::disjoint(std::span<const std::byte> input,std::span<std::byte> output) const noexcept {
    struct Region { const void* data; std::size_t bytes; };
    const std::array regions{Region{this,sizeof(*this)},Region{input.data(),input.size()},Region{output.data(),output.size()},
        Region{raw_.get(),workspace_.raw_bytes},Region{serialized_.get(),workspace_.serialized_bytes},
        Region{tokens_.get(),workspace_.token_bytes},Region{finder_.get(),workspace_.finder_bytes},
        Region{tables_.get(),workspace_.table_bytes},Region{payload_.get(),workspace_.payload_bytes},
        Region{private_serialized_.get(),workspace_.private_serialized_bytes}};
    for (std::size_t i=0;i<regions.size();++i) for (std::size_t j=i+1;j<regions.size();++j)
        if (core::check_buffer_overlap(regions[i].data,regions[i].bytes,regions[j].data,regions[j].bytes)!=core::BufferOverlap::disjoint) return false;
    return true;
}
core::ProcessResult PositionTans32mOwnedEncoder::fail(Code code,std::size_t consumed,std::size_t produced) noexcept {
    state_=State::error; error_={code,received_,0};
    return {consumed,produced,core::StreamStatus::error,error_};
}
core::ProcessResult PositionTans32mOwnedEncoder::process(std::span<const std::byte> input,
    std::span<std::byte> output,std::uint32_t flags) noexcept {
    using Status=core::StreamStatus;
    if (state_==State::error) return {0,0,Status::error,error_};
    if (state_==State::ended) return {0,0,Status::end_of_stream,{}};
    constexpr auto end=core::flag_value(core::ProcessFlags::end_input);
    constexpr auto allowed=end | core::flag_value(core::ProcessFlags::flush);
    if (flags & ~allowed) return fail(Code::unsupported);
    if (!disjoint(input,output)) return fail(Code::invalid_argument);
    const bool final=(flags & end)!=0;
    const auto remaining=stream_.original_size-received_;
    if (input.size()>remaining || (final && input.size()!=remaining) || (end_seen_ && !input.empty()))
        return fail(Code::invalid_argument);
    std::size_t consumed{},produced{};
    while (true) {
        if (final && consumed==input.size()) end_seen_=true;
        if (state_==State::header || state_==State::draining) {
            const auto source=state_==State::header ? std::span<const std::byte>(header_)
                : std::span<const std::byte>(serialized_.get(),pending_);
            const auto count=std::min(pending_-drained_,output.size()-produced);
            if (count) std::memcpy(output.data()+produced,source.data()+drained_,count);
            produced+=count; drained_+=count;
            if (drained_!=pending_) return {consumed,produced,Status::need_output,{}};
            collected_=0; drained_=0; pending_=0;
            state_=committed_==stream_.original_size ? State::awaiting_end : State::collecting;
            continue;
        }
        if (state_==State::awaiting_end) {
            if (end_seen_) { state_=State::ended; return {consumed,produced,Status::end_of_stream,{}}; }
            return {consumed,produced,consumed || produced ? Status::progress : Status::need_input,{}};
        }
        const auto target=static_cast<std::size_t>(std::min<std::uint64_t>(stream_.frame_size,stream_.original_size-committed_));
        const auto count=std::min(target-collected_,input.size()-consumed);
        if (count) std::memcpy(raw_.get()+collected_,input.data()+consumed,count);
        consumed+=count; received_+=count; collected_+=count;
        if (collected_!=target) return {consumed,produced,consumed || produced ? Status::progress : Status::need_input,{}};
        const auto candidate=dictionary::internal::tokenize_lzss_position_distance_32m_five_prefix_candidate(
            {raw_.get(),target},stream_.dictionary,limits_,5,{tokens_.get(),workspace_.token_count},
            {finder_.get(),workspace_.finder_bytes});
        if (candidate.error!=dictionary::internal::LzssShortMatchCandidateError::none) return fail(Code::internal_error,consumed,produced);
        const auto encoded=encode_position_tans_32m_frame({tokens_.get(),candidate.token_count},
            {stream_,limits_,sequence_,committed_},
            {tables_.get(),entropy::internal::position_tans_32m_table_entries},
            {payload_.get(),workspace_.payload_bytes},{private_serialized_.get(),workspace_.private_serialized_bytes},
            {serialized_.get(),workspace_.serialized_bytes});
        if (encoded.error!=PositionTans32mFrameError::none) {
            const bool limited=encoded.error==PositionTans32mFrameError::limit_exceeded
                || encoded.error==PositionTans32mFrameError::arithmetic_overflow || encoded.format_error==PositionTans32mFormatError::limit_exceeded
                || encoded.format_error==PositionTans32mFormatError::arithmetic_overflow
                || encoded.token_result.error==context::internal::PositionTans32mTokenError::limit_exceeded;
            return fail(limited ? Code::limit_exceeded : Code::internal_error,consumed,produced);
        }
        committed_+=target; ++sequence_; pending_=encoded.serialized_size; state_=State::draining;
    }
}
Code PositionTans32mOwnedDecoder::requirements(std::uint32_t capacity,const core::DecoderLimits& l,PositionTans32mWorkspace& r) noexcept {
    return internal::requirements(PositionTans32mStreamHeader{capacity,0},l,false,sizeof(PositionTans32mOwnedDecoder),r);
}
std::unique_ptr<PositionTans32mOwnedDecoder> PositionTans32mOwnedDecoder::create(
    std::uint32_t capacity,const core::DecoderLimits& limits,Code& error) noexcept {
    auto l=limits;
    PositionTans32mWorkspace r{};
    error=requirements(capacity,l,r); if (error!=Code::none) return {};
    l.max_frame_size=std::min<std::uint64_t>(l.max_frame_size,capacity);
    std::unique_ptr<PositionTans32mOwnedDecoder> p(new(std::nothrow) PositionTans32mOwnedDecoder);
    if (!p) { error=Code::out_of_memory; return {}; }
    p->raw_.reset(new(std::nothrow) std::byte[r.raw_bytes]);
    if (!p->raw_) { error=Code::out_of_memory; return {}; }
    p->serialized_.reset(new(std::nothrow) std::byte[r.serialized_bytes]);
    if (!p->serialized_) { error=Code::out_of_memory; return {}; }
    p->tokens_.reset(new(std::nothrow) Token[r.token_count]);
    if (!p->raw_ || !p->serialized_ || !p->tokens_) { error=Code::out_of_memory; return {}; }
    p->tables_.reset(new(std::nothrow) entropy::internal::TansDecodeEntry[entropy::internal::position_tans_32m_table_entries]);
    if (!p->tables_) { error=Code::out_of_memory; return {}; }
    p->decoder_.emplace(l,std::span(p->serialized_.get(),r.serialized_bytes),std::span(p->tokens_.get(),r.token_count),
        std::span(p->raw_.get(),r.raw_bytes),std::span(p->tables_.get(),entropy::internal::position_tans_32m_table_entries),sizeof(PositionTans32mOwnedDecoder)-sizeof(PositionTans32mStreamDecoder));
    const auto initial=p->decoder_->process({},{},0);
    if (initial.status==core::StreamStatus::error) { error=initial.error.code; return {}; }
    error=Code::none; return p;
}
core::ProcessResult PositionTans32mOwnedDecoder::process(std::span<const std::byte> input,std::span<std::byte> output,std::uint32_t flags) noexcept {
    if (error_.code!=Code::none) return {0,0,core::StreamStatus::error,error_};
    if (ended_) return {0,0,core::StreamStatus::end_of_stream,{}};
    if (core::check_buffer_overlap(this,sizeof(*this),input.data(),input.size())!=core::BufferOverlap::disjoint
        || core::check_buffer_overlap(this,sizeof(*this),output.data(),output.size())!=core::BufferOverlap::disjoint) {
        error_={Code::invalid_argument,0,0}; return {0,0,core::StreamStatus::error,error_};
    }
    const auto r=decoder_->process(input,output,flags);
    if (r.status==core::StreamStatus::error) error_=r.error;
    ended_=r.status==core::StreamStatus::end_of_stream;
    return r;
}
} // namespace marc::frame::internal




