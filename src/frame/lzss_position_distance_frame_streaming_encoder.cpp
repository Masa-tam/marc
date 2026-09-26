#include "frame/lzss_position_distance_frame_streaming_encoder.hpp"
#include "frame/lzss_position_distance_stream_encoder.hpp"
#include "core/buffer_overlap.hpp"
#include <algorithm>
#include <cstring>

namespace marc::frame::internal {
namespace {
core::ErrorCode workspace_error(LzssPositionDistanceWorkspaceError error) noexcept {
    using E = LzssPositionDistanceWorkspaceError;
    if (error == E::limit_exceeded || error == E::arithmetic_overflow)
        return core::ErrorCode::limit_exceeded;
    if (error == E::too_small) return core::ErrorCode::out_of_memory;
    return core::ErrorCode::invalid_argument;
}
}

bool LzssPositionDistanceFrameStreamingEncoder::disjoint(
    std::span<const std::byte> input, std::span<std::byte> output) const noexcept {
    struct Region { const void* data; std::size_t size; };
    const std::array regions{Region{input.data(),input.size()}, Region{output.data(),output.size()},
        Region{raw_storage_.data(),raw_storage_.size()},
        Region{serialized_storage_.data(),serialized_storage_.size()},
        Region{aligned_storage_.data(),aligned_storage_.size()}, Region{this,sizeof(*this)}};
    for (std::size_t i=0;i<regions.size();++i) for (std::size_t j=i+1;j<regions.size();++j)
        if (core::check_buffer_overlap(regions[i].data,regions[i].size,regions[j].data,regions[j].size)
            != core::BufferOverlap::disjoint) return false;
    return true;
}

core::ProcessResult LzssPositionDistanceFrameStreamingEncoder::fail(core::ErrorCode code,
    std::uint64_t position, std::size_t consumed, std::size_t produced) noexcept {
    state_=State::error; error_={code,position,0};
    return {consumed,produced,core::StreamStatus::error,error_};
}

LzssPositionDistanceFrameStreamingEncoder::LzssPositionDistanceFrameStreamingEncoder(
    TypedContextStreamHeader stream, core::DecoderLimits limits, std::span<std::byte> raw,
    std::span<std::byte> serialized, std::span<std::byte> aligned,
    std::uint32_t eligibility, LzssPositionDistanceSearch search) noexcept
    : stream_(stream), limits_(limits), raw_storage_(raw), serialized_storage_(serialized),
      aligned_storage_(aligned), eligibility_(eligibility), search_(search) {
    if (eligibility<3 || eligibility>5
        || (search!=LzssPositionDistanceSearch::reference && search!=LzssPositionDistanceSearch::indexed)
        || !disjoint({},{})) {
        static_cast<void>(fail(core::ErrorCode::invalid_argument,0)); return;
    }
    const auto error=partition_lzss_position_distance_workspace(stream_,limits_,
        LzssPositionDistanceWorkspaceDirection::encode,sizeof(*this),raw,serialized,aligned,views_);
    if (error!=LzssPositionDistanceWorkspaceError::none) {
        static_cast<void>(fail(workspace_error(error),0)); return;
    }
    if (!serialize_lzss_position_distance_stream_header(stream_,limits_,header_)) {
        static_cast<void>(fail(core::ErrorCode::internal_error,0)); return;
    }
    pending_=header_.size();
}

core::ProcessResult LzssPositionDistanceFrameStreamingEncoder::process(
    std::span<const std::byte> input, std::span<std::byte> output, std::uint32_t flags) noexcept {
    using Status=core::StreamStatus;
    using Code=core::ErrorCode;
    if (state_==State::error) return {0,0,Status::error,error_};
    if (state_==State::ended) return {0,0,Status::end_of_stream,{}};
    constexpr auto end_flag=core::flag_value(core::ProcessFlags::end_input);
    constexpr auto allowed=end_flag|core::flag_value(core::ProcessFlags::flush);
    if ((flags & ~allowed)!=0) return fail(Code::unsupported,received_);
    if (!disjoint(input,output)) return fail(Code::invalid_argument,received_);
    const bool final=(flags & end_flag)!=0;
    const auto remaining=stream_.original_size-received_;
    if (input.size()>remaining || (final && input.size()!=remaining) || (end_seen_ && !input.empty()))
        return fail(Code::invalid_argument,received_);
    std::size_t consumed{},produced{};
    while (true) {
        if (final && consumed==input.size()) end_seen_=true;
        if (state_==State::header || state_==State::draining) {
            const auto source=state_==State::header ? std::span<const std::byte>{header_}
                : std::span<const std::byte>{views_.serialized}.first(pending_);
            const auto count=std::min(pending_-drained_,output.size()-produced);
            if (count!=0) std::memcpy(output.data()+produced,source.data()+drained_,count);
            drained_+=count; produced+=count;
            if (drained_!=pending_) return {consumed,produced,Status::need_output,{}};
            drained_=0; pending_=0; collected_=0;
            state_=committed_==stream_.original_size ? State::awaiting_end : State::collecting;
        }
        if (state_==State::awaiting_end) {
            if (end_seen_) { state_=State::ended; return {consumed,produced,Status::end_of_stream,{}}; }
            return {consumed,produced,Status::need_input,{}};
        }
        if (state_==State::collecting) {
            const auto target=static_cast<std::size_t>(std::min<std::uint64_t>(
                stream_.frame_size,stream_.original_size-committed_));
            const auto count=std::min(target-collected_,input.size()-consumed);
            if (count!=0) std::memcpy(views_.raw.data()+collected_,input.data()+consumed,count);
            consumed+=count; received_+=count; collected_+=count;
            if (collected_!=target) return {consumed,produced,Status::need_input,{}};
            // A single raw-frame call tokenizes once and retains those tokens
            // through entropy planning/writing. Never run the raw-frame planner.
            ++preparations_;
            const auto result=encode_lzss_position_distance_raw_frame(stream_,limits_,
                preparations_-1,committed_,views_.raw.first(target),eligibility_,search_,
                views_.tokens,views_.operations,views_.finder,views_.serialized);
            if (result.error!=LzssPositionDistanceRawFrameError::none) {
                const bool limited=result.error==LzssPositionDistanceRawFrameError::workspace_limit
                    || result.frame.error==LzssShortMatchFrameEncodeError::workspace_limit
                    || result.frame.preflight_error==LzssShortMatchPreflightError::limit_exceeded;
                return fail(limited ? Code::limit_exceeded : Code::internal_error,committed_,consumed,produced);
            }
            committed_+=target; pending_=result.frame.serialized_size; state_=State::draining;
        }
    }
}
} // namespace marc::frame::internal
