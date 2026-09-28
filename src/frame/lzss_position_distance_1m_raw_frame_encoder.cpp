#include "frame/lzss_position_distance_1m_raw_frame_encoder.hpp"
#include "entropy/lzss_position_distance_1m_range_encoder.hpp"
#include "entropy/lzss_position_distance_1m_range_decoder.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include <algorithm>
#include <array>

namespace marc::frame::internal {
LzssPositionDistanceRawFrameResult encode_lzss_position_distance_1m_raw_frame(
    const TypedContextStreamHeader& stream,const core::DecoderLimits& limits,
    std::uint64_t sequence,std::uint64_t committed,std::span<const std::byte> raw,
    std::uint32_t eligibility,dictionary::internal::LzssPositionDistance1mSearch search,
    std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<context::internal::ModeledOperation> operations,
    std::span<std::byte> finder,std::span<std::byte> output) noexcept {
    using Error=LzssPositionDistanceRawFrameError;
    using Search=dictionary::internal::LzssPositionDistance1mSearch;
    LzssPositionDistanceRawFrameResult r{};
    if(validate_lzss_position_distance_1m_stream_semantics(stream,limits)!=LzssShortMatchPreflightError::none) {
        r.error=Error::invalid_stream;return r;
    }
    if(committed>=stream.original_size || committed%stream.frame_size || sequence!=committed/stream.frame_size) {
        r.error=Error::invalid_position;return r;
    }
    if(raw.size()!=std::min<std::uint64_t>(stream.frame_size,stream.original_size-committed)) {
        r.error=Error::raw_size_mismatch;return r;
    }
    if(search!=Search::exhaustive && search!=Search::indexed_reference && search!=Search::indexed) {
        r.error=Error::invalid_search;return r;
    }
    std::size_t token_bytes{},operation_bytes{};
    if(!core::checked_multiply(tokens.size(),sizeof(dictionary::internal::LzssTypedToken),token_bytes)
        || !core::checked_multiply(operations.size(),sizeof(context::internal::ModeledOperation),operation_bytes)) {
        r.error=Error::arithmetic_overflow;return r;
    }
    struct Region {const void* data;std::size_t size;};
    const std::array regions{Region{raw.data(),raw.size()},Region{tokens.data(),token_bytes},
        Region{operations.data(),operation_bytes},Region{finder.data(),finder.size()},Region{output.data(),output.size()},
        Region{&stream,sizeof(stream)},Region{&limits,sizeof(limits)}};
    for(std::size_t i=0;i<regions.size();++i) for(std::size_t j=i+1;j<regions.size();++j) {
        const auto overlap=core::check_buffer_overlap(regions[i].data,regions[i].size,regions[j].data,regions[j].size);
        if(overlap!=core::BufferOverlap::disjoint) {
            r.error=overlap==core::BufferOverlap::arithmetic_overflow?Error::arithmetic_overflow:Error::overlapping_buffers;return r;
        }
    }
    auto aggregate=std::max(entropy::internal::lzss_position_distance_1m_range_encoder_state_bytes(),
        sizeof(entropy::internal::LzssPositionDistance1mRangeDecoder));
    for(const auto bytes:{raw.size(),token_bytes,operation_bytes,finder.size(),output.size()})
        if(!core::checked_add(aggregate,bytes,aggregate)) {r.error=Error::arithmetic_overflow;return r;}
    if(aggregate>limits.max_internal_buffered_bytes) {r.error=Error::workspace_limit;return r;}
    r.candidate=dictionary::internal::tokenize_lzss_position_distance_1m_candidate(
        raw,stream.dictionary,limits,eligibility,search,tokens,finder);
    if(r.candidate.error!=dictionary::internal::LzssShortMatchCandidateError::none) {r.error=Error::candidate_error;return r;}
    r.frame=encode_lzss_position_distance_1m_frame(stream,limits,sequence,committed,
        tokens.first(r.candidate.token_count),operations,output);
    if(r.frame.error!=LzssShortMatchFrameEncodeError::none) r.error=Error::frame_error;
    return r;
}
}
