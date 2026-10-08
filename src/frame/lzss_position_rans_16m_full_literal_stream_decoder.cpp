#include "frame/lzss_position_rans_16m_full_literal_stream_decoder.hpp"
#include "core/buffer_overlap.hpp"
#include <algorithm>
#include <cstring>

namespace marc::frame::internal {
namespace {
core::ErrorCode category(PositionRans16mFullLiteralFormatError error) noexcept {
    using E=PositionRans16mFullLiteralFormatError;
    if (error==E::limit_exceeded || error==E::arithmetic_overflow) return core::ErrorCode::limit_exceeded;
    if (error==E::unsupported_identity) return core::ErrorCode::unsupported;
    return core::ErrorCode::malformed_stream;
}
}
bool PositionRans16mFullLiteralStreamDecoder::disjoint(std::span<const std::byte> input,std::span<std::byte> output) const noexcept {
    struct Region { const void* data; std::size_t size; };
    const std::array regions{Region{input.data(),input.size()},Region{output.data(),output.size()},
        Region{serialized_.data(),serialized_.size()},Region{tokens_.data(),token_bytes_},
        Region{raw_.data(),raw_.size()},Region{this,sizeof(*this)}};
    for (std::size_t i=0;i<regions.size();++i) for (std::size_t j=i+1;j<regions.size();++j)
        if (core::check_buffer_overlap(regions[i].data,regions[i].size,regions[j].data,regions[j].size)
            !=core::BufferOverlap::disjoint) return false;
    return true;
}
PositionRans16mFullLiteralStreamDecoder::PositionRans16mFullLiteralStreamDecoder(core::DecoderLimits limits,
    std::span<std::byte> serialized,std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<std::byte> raw,std::size_t additional_owner_bytes) noexcept
    : limits_(limits),serialized_(serialized),raw_(raw),tokens_(tokens) {
    if (core::validate_limits(limits_)!=core::LimitError::none
        || !core::checked_multiply(tokens_.size(),sizeof(dictionary::internal::LzssTypedToken),token_bytes_)
        || !disjoint({},{})) {
        state_=State::error; error_={core::ErrorCode::invalid_argument,0,0}; return;
    }
    std::size_t aggregate{};
    if (!core::checked_add(sizeof(*this),additional_owner_bytes,aggregate)
        || !core::checked_add(aggregate,serialized_.size(),aggregate)
        || !core::checked_add(aggregate,raw_.size(),aggregate)
        || !core::checked_add(aggregate,token_bytes_,aggregate)
        || !core::checked_add(aggregate,context::internal::lzss_position_rans_16m_full_literal_fixed_working_bytes,aggregate)
        || aggregate>limits_.max_internal_buffered_bytes) {
        state_=State::error; error_={core::ErrorCode::limit_exceeded,0,0};
    }
}
core::ProcessResult PositionRans16mFullLiteralStreamDecoder::fail(core::ErrorCode code,std::uint64_t position,
    std::size_t consumed,std::size_t produced) noexcept {
    state_=State::error; error_={code,position,0};
    return {consumed,produced,core::StreamStatus::error,error_};
}
core::ProcessResult PositionRans16mFullLiteralStreamDecoder::process(std::span<const std::byte> input,
    std::span<std::byte> output,std::uint32_t flags) noexcept {
    using Status=core::StreamStatus;
    using Code=core::ErrorCode;
    if (state_==State::error) return {0,0,Status::error,error_};
    if (state_==State::ended) return {0,0,Status::end_of_stream,{}};
    constexpr auto end=core::flag_value(core::ProcessFlags::end_input);
    constexpr auto allowed=end | core::flag_value(core::ProcessFlags::flush);
    if (flags & ~allowed) return fail(Code::unsupported,position_,0,0);
    if (!disjoint(input,output)) return fail(Code::invalid_argument,position_,0,0);
    if (end_seen_ && !input.empty()) return fail(Code::malformed_stream,position_,0,0);
    const bool final=(flags & end)!=0;
    std::size_t consumed{},produced{};
    while (true) {
        if (final && consumed==input.size()) end_seen_=true;
        if (state_==State::draining) {
            const auto count=std::min(layout_.requirements.raw_size-drained_,output.size()-produced);
            if (count) std::memcpy(output.data()+produced,raw_.data()+drained_,count);
            produced+=count; drained_+=count;
            if (drained_!=layout_.requirements.raw_size) return {consumed,produced,Status::need_output,{}};
            collected_=0; frame_start_=position_;
            state_=raw_validated_==stream_.original_size ? State::awaiting_end : State::frame_header;
            continue;
        }
        if (state_==State::awaiting_end) {
            if (consumed!=input.size()) return fail(Code::malformed_stream,position_,consumed,produced);
            if (end_seen_) { state_=State::ended; return {consumed,produced,Status::end_of_stream,{}}; }
            return {consumed,produced,consumed || produced ? Status::progress : Status::need_input,{}};
        }
        if (state_==State::frame_header && serialized_.size()<64)
            return fail(Code::limit_exceeded,frame_start_,consumed,produced);
        const auto destination=state_==State::stream_header ? std::span<std::byte>(header_)
            : state_==State::frame_header ? serialized_.first(64)
            : state_==State::descriptor ? serialized_.first(layout_.requirements.prefix_size)
            : serialized_.first(layout_.requirements.serialized_size);
        const auto count=std::min(destination.size()-collected_,input.size()-consumed);
        std::uint64_t next{};
        if (!core::checked_add(position_,static_cast<std::uint64_t>(count),next))
            return fail(Code::limit_exceeded,position_,consumed,produced);
        if (count) std::memcpy(destination.data()+collected_,input.data()+consumed,count);
        consumed+=count; collected_+=count; position_=next;
        if (final && consumed==input.size()) end_seen_=true;
        if (collected_!=destination.size()) {
            if (end_seen_) return fail(Code::malformed_stream,position_,consumed,produced);
            return {consumed,produced,consumed || produced ? Status::progress : Status::need_input,{}};
        }
        if (state_==State::stream_header) {
            std::size_t parsed{};
            const auto error=parse_position_rans_16m_full_literal_stream(header_,limits_,stream_,parsed);
            if (error!=PositionRans16mFullLiteralFormatError::none) return fail(category(error),0,consumed,produced);
            collected_=0; frame_start_=position_;
            state_=stream_.original_size==0 ? State::awaiting_end : State::frame_header;
        } else if (state_==State::frame_header) {
            const auto error=parse_position_rans_16m_full_literal_frame_header(serialized_.first(64),
                {stream_,limits_,sequence_,raw_validated_},layout_);
            if (error!=PositionRans16mFullLiteralFormatError::none) return fail(category(error),frame_start_,consumed,produced);
            if (layout_.requirements.serialized_size>serialized_.size()
                || layout_.requirements.token_count>tokens_.size() || layout_.requirements.raw_size>raw_.size())
                return fail(Code::limit_exceeded,frame_start_,consumed,produced);
            state_=State::descriptor;
        } else if (state_==State::descriptor) {
            const auto error=preflight_position_rans_16m_full_literal_frame_prefix(serialized_.first(layout_.requirements.prefix_size),
                {stream_,limits_,sequence_,raw_validated_},layout_);
            if (error!=PositionRans16mFullLiteralFormatError::none) return fail(category(error),frame_start_+64,consumed,produced);
            state_=State::payload;
        } else {
            const auto decoded=decode_position_rans_16m_full_literal_frame_scratch(serialized_.first(layout_.requirements.serialized_size),
                {stream_,limits_,sequence_,raw_validated_},tokens_,raw_);
            if (decoded.error!=PositionRans16mFullLiteralFrameError::none)
                return fail(Code::malformed_stream,frame_start_+layout_.requirements.prefix_size,consumed,produced);
            raw_validated_+=layout_.requirements.raw_size; ++sequence_;
            drained_=0; state_=State::draining;
        }
    }
}
} // namespace marc::frame::internal
