#include "frame/lzss_position_distance_raw_frame_encoder.hpp"
#include "frame/lzss_position_distance_preflight.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include <algorithm>
#include <array>

namespace marc::frame::internal {
namespace {
LzssPositionDistanceRawFrameResult process_frame(
    const TypedContextStreamHeader& stream, const core::DecoderLimits& limits,
    std::uint64_t sequence, std::uint64_t committed, std::span<const std::byte> raw,
    std::uint32_t eligibility, LzssPositionDistanceSearch search,
    std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<context::internal::ModeledOperation> operations,
    std::span<std::byte> finder, std::span<std::byte> output, bool write) noexcept {
    using Error=LzssPositionDistanceRawFrameError;
    namespace dictionary=marc::dictionary::internal;
    LzssPositionDistanceRawFrameResult result{};
    if (validate_lzss_position_distance_stream_semantics(stream,limits)
        != LzssShortMatchPreflightError::none) {
        result.error=Error::invalid_stream; return result;
    }
    if (committed>=stream.original_size || committed%stream.frame_size!=0
        || sequence!=committed/stream.frame_size) {
        result.error=Error::invalid_position; return result;
    }
    if (raw.size()!=std::min<std::uint64_t>(stream.frame_size,stream.original_size-committed)) {
        result.error=Error::raw_size_mismatch; return result;
    }
    if (search!=LzssPositionDistanceSearch::reference && search!=LzssPositionDistanceSearch::indexed) {
        result.error=Error::invalid_search; return result;
    }
    std::size_t token_bytes{},operation_bytes{};
    if (!core::checked_multiply(tokens.size(),sizeof(dictionary::LzssTypedToken),token_bytes)
        || !core::checked_multiply(operations.size(),sizeof(context::internal::ModeledOperation),operation_bytes)) {
        result.error=Error::arithmetic_overflow; return result;
    }
    struct Region { const void* data; std::size_t size; };
    const std::array regions{Region{raw.data(),raw.size()},Region{tokens.data(),token_bytes},
        Region{operations.data(),operation_bytes},Region{finder.data(),finder.size()},Region{output.data(),output.size()}};
    for(std::size_t i=0;i<regions.size();++i) for(std::size_t j=i+1;j<regions.size();++j) {
        const auto overlap=core::check_buffer_overlap(regions[i].data,regions[i].size,regions[j].data,regions[j].size);
        if(overlap!=core::BufferOverlap::disjoint) {
            result.error=overlap==core::BufferOverlap::arithmetic_overflow ? Error::arithmetic_overflow : Error::overlapping_buffers;
            return result;
        }
    }
    auto frame_limits=limits;
    if(search==LzssPositionDistanceSearch::indexed) {
        const auto needed=dictionary::calculate_lzss_short_prefix_workspace(raw.size(),stream.dictionary,limits,
            dictionary::LzssTypedTokenVariant::field_context_64k_short_length_escape);
        if(needed.error!=dictionary::LzssShortPrefixError::none
            || needed.workspace_size>limits.max_internal_buffered_bytes
            || limits.max_internal_buffered_bytes-needed.workspace_size<limits.max_block_size) {
            result.error=Error::workspace_limit; result.candidate.finder_error=needed.error; return result;
        }
        frame_limits.max_internal_buffered_bytes-=needed.workspace_size;
        result.candidate=dictionary::tokenize_lzss_short_length_escape_candidate_indexed(
            raw,stream.dictionary,limits,eligibility,tokens,finder);
    } else {
        result.candidate=dictionary::tokenize_lzss_short_length_escape_candidate(
            raw,stream.dictionary,limits,eligibility,tokens);
    }
    if(result.candidate.error!=dictionary::LzssShortMatchCandidateError::none) {
        result.error=Error::candidate_error; return result;
    }
    // Retain the materialized tokens through planning/writing; no second search.
    result.frame=write
        ? encode_lzss_position_distance_frame(stream,frame_limits,sequence,committed,
            tokens.first(result.candidate.token_count),operations,output)
        : plan_lzss_position_distance_frame(stream,frame_limits,sequence,committed,
            tokens.first(result.candidate.token_count),operations);
    if(result.frame.error!=LzssShortMatchFrameEncodeError::none) result.error=Error::frame_error;
    return result;
}
} // namespace

LzssPositionDistanceRawFrameResult plan_lzss_position_distance_raw_frame(
    const TypedContextStreamHeader& stream, const core::DecoderLimits& limits,
    std::uint64_t sequence, std::uint64_t committed, std::span<const std::byte> raw,
    std::uint32_t eligibility, LzssPositionDistanceSearch search,
    std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<context::internal::ModeledOperation> operations,
    std::span<std::byte> finder) noexcept {
    return process_frame(stream,limits,sequence,committed,raw,eligibility,search,
                         tokens,operations,finder,{},false);
}
LzssPositionDistanceRawFrameResult encode_lzss_position_distance_raw_frame(
    const TypedContextStreamHeader& stream, const core::DecoderLimits& limits,
    std::uint64_t sequence, std::uint64_t committed, std::span<const std::byte> raw,
    std::uint32_t eligibility, LzssPositionDistanceSearch search,
    std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<context::internal::ModeledOperation> operations,
    std::span<std::byte> finder, std::span<std::byte> output) noexcept {
    return process_frame(stream,limits,sequence,committed,raw,eligibility,search,
                         tokens,operations,finder,output,true);
}
}
