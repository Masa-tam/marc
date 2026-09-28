#include "frame/lzss_position_distance_1m_owned_decoder.hpp"
#include "core/buffer_overlap.hpp"
#include <algorithm>
#include <new>

namespace marc::frame::internal {
core::ErrorCode LzssPositionDistance1mOwnedDecoder::requirements(std::uint32_t capacity,
    const core::DecoderLimits& limits,LzssPositionDistance1mDecodeWorkspace& result) noexcept {
    return calculate_lzss_position_distance_1m_decode_workspace(capacity,limits,sizeof(LzssPositionDistance1mOwnedDecoder),result);
}

std::unique_ptr<LzssPositionDistance1mOwnedDecoder> LzssPositionDistance1mOwnedDecoder::create(
    std::uint32_t capacity,const core::DecoderLimits& limits,core::ErrorCode& error) noexcept {
    // No borrowed configuration survives creation.
    auto effective=limits;
    LzssPositionDistance1mDecodeWorkspace r{};
    error=requirements(capacity,effective,r);
    if (error!=core::ErrorCode::none) return {};
    effective.max_frame_size=std::min(effective.max_frame_size,static_cast<std::uint64_t>(capacity));
    std::unique_ptr<LzssPositionDistance1mOwnedDecoder> owner(new(std::nothrow) LzssPositionDistance1mOwnedDecoder);
    if (!owner) {error=core::ErrorCode::out_of_memory;return {};}
    owner->serialized_.reset(new(std::nothrow) std::byte[r.serialized_bytes]);
    if (!owner->serialized_) {error=core::ErrorCode::out_of_memory;return {};}
    owner->raw_.reset(new(std::nothrow) std::byte[r.raw_bytes]);
    if (!owner->raw_) {error=core::ErrorCode::out_of_memory;return {};}
    owner->tokens_.reset(new(std::nothrow) dictionary::internal::LzssTypedToken[r.token_count]);
    if (!owner->tokens_) {error=core::ErrorCode::out_of_memory;return {};}
    static_assert(sizeof(LzssPositionDistance1mOwnedDecoder)>=sizeof(LzssPositionDistance1mFrameStreamingDecoder));
    owner->decoder_.emplace(effective,std::span{owner->serialized_.get(),r.serialized_bytes},
        std::span{owner->tokens_.get(),r.token_count},std::span{owner->raw_.get(),r.raw_bytes},
        sizeof(LzssPositionDistance1mOwnedDecoder)-sizeof(LzssPositionDistance1mFrameStreamingDecoder));
    error=core::ErrorCode::none;
    return owner;
}

core::ProcessResult LzssPositionDistance1mOwnedDecoder::process(std::span<const std::byte> input,
    std::span<std::byte> output,std::uint32_t flags) noexcept {
    if (error_.code!=core::ErrorCode::none) return {0,0,core::StreamStatus::error,error_};
    if (ended_) return {0,0,core::StreamStatus::end_of_stream,{}};
    if (core::check_buffer_overlap(this,sizeof(*this),input.data(),input.size())!=core::BufferOverlap::disjoint
        || core::check_buffer_overlap(this,sizeof(*this),output.data(),output.size())!=core::BufferOverlap::disjoint) {
        error_={core::ErrorCode::invalid_argument,0,0};
        return {0,0,core::StreamStatus::error,error_};
    }
    const auto result=decoder_->process(input,output,flags);
    if (result.status==core::StreamStatus::error) error_=result.error;
    ended_=result.status==core::StreamStatus::end_of_stream;
    return result;
}
}
