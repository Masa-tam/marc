#include "frame/lzss_position_distance_4m_bucket18_owned_encoder.hpp"
#include "core/buffer_overlap.hpp"
#include <new>

namespace marc::frame::internal {
core::ErrorCode LzssPositionDistance4mBucket18OwnedEncoder::requirements(
    const TypedContextStreamHeader& stream, const core::DecoderLimits& limits,
    LzssPositionDistanceWorkspaceRequirements& result) noexcept {
    const auto error=calculate_lzss_position_distance_4m_bucket18_encode_workspace(stream,limits,
        LzssPositionDistanceWorkspaceDirection::encode,sizeof(LzssPositionDistance4mBucket18OwnedEncoder),result);
    using E=LzssPositionDistanceWorkspaceError;
    if(error==E::none) return core::ErrorCode::none;
    return error==E::limit_exceeded || error==E::arithmetic_overflow
        ? core::ErrorCode::limit_exceeded : core::ErrorCode::invalid_argument;
}

std::unique_ptr<LzssPositionDistance4mBucket18OwnedEncoder> LzssPositionDistance4mBucket18OwnedEncoder::create(
    const TypedContextStreamHeader& stream, const core::DecoderLimits& limits,
    core::ErrorCode& error, std::uint32_t eligibility) noexcept {
    // Snapshot configuration before writing the caller's error output.
    const auto configuration=stream;
    const auto budget=limits;
    if(eligibility<3 || eligibility>5) {error=core::ErrorCode::invalid_argument;return {};}
    LzssPositionDistanceWorkspaceRequirements r{};
    error=requirements(configuration,budget,r);
    if(error!=core::ErrorCode::none) return {};
    std::unique_ptr<LzssPositionDistance4mBucket18OwnedEncoder> owner(new(std::nothrow) LzssPositionDistance4mBucket18OwnedEncoder);
    if(!owner) {error=core::ErrorCode::out_of_memory;return {};}
    owner->raw_.reset(new(std::nothrow) std::byte[r.raw_bytes]);
    if(!owner->raw_) {error=core::ErrorCode::out_of_memory;return {};}
    owner->serialized_.reset(new(std::nothrow) std::byte[r.serialized_bytes]);
    if(!owner->serialized_) {error=core::ErrorCode::out_of_memory;return {};}
    // Ordinary byte-array new supplies alignment for the fundamental-aligned
    // token, operation and uint32 objects constructed in this storage.
    static_assert(alignof(dictionary::internal::LzssTypedToken)<=alignof(std::max_align_t));
    static_assert(alignof(context::internal::ModeledOperation)<=alignof(std::max_align_t));
    owner->aligned_.reset(new(std::nothrow) std::byte[r.views_bytes]);
    if(!owner->aligned_) {error=core::ErrorCode::out_of_memory;return {};}
    owner->encoder_.emplace(configuration,budget,std::span{owner->raw_.get(),r.raw_bytes},
        std::span{owner->serialized_.get(),r.serialized_bytes},std::span{owner->aligned_.get(),r.views_bytes},
        eligibility,
        sizeof(LzssPositionDistance4mBucket18OwnedEncoder)-sizeof(LzssPositionDistance4mBucket18FrameStreamingEncoder));
    error=core::ErrorCode::none;
    return owner;
}

core::ProcessResult LzssPositionDistance4mBucket18OwnedEncoder::process(std::span<const std::byte> input,
    std::span<std::byte> output,std::uint32_t flags) noexcept {
    if(error_.code!=core::ErrorCode::none) return {0,0,core::StreamStatus::error,error_};
    if(ended_) return {0,0,core::StreamStatus::end_of_stream,{}};
    if(core::check_buffer_overlap(this,sizeof(*this),input.data(),input.size())!=core::BufferOverlap::disjoint
        || core::check_buffer_overlap(this,sizeof(*this),output.data(),output.size())!=core::BufferOverlap::disjoint) {
        error_={core::ErrorCode::invalid_argument,0,0};return {0,0,core::StreamStatus::error,error_};
    }
    const auto result=encoder_->process(input,output,flags);
    if(result.status==core::StreamStatus::error) error_=result.error;
    ended_=result.status==core::StreamStatus::end_of_stream;
    return result;
}
}
