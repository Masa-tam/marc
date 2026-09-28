#include "dictionary/lzss_position_distance_1m_candidate.hpp"
#include "dictionary/lzss_position_distance_1m_five_prefix_candidate.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include <array>

namespace marc::dictionary::internal {
namespace {
using Error=LzssShortMatchCandidateError;
template<class Finder>
LzssShortMatchCandidateResult parse(std::span<const std::byte> input,
    std::uint32_t eligibility,std::span<LzssTypedToken> tokens,Finder& finder) noexcept {
    LzssShortMatchCandidateResult r{};r.input_size=input.size();
    for(std::size_t position=0;position<input.size();) {
        const auto match=finder.find_match(position);
        const bool use=match.length>=eligibility;
        const std::size_t step=use?match.length:1;
        if(!tokens.empty()) tokens[r.token_count]=use ? LzssTypedToken{LzssTypedTokenKind::match,0,match.distance,match.length}
            : LzssTypedToken{LzssTypedTokenKind::literal,std::to_integer<std::uint8_t>(input[position]),0,0};
        ++r.token_count;finder.advance(position,position+step);position+=step;
    }
    r.token_storage_size=r.token_count*sizeof(LzssTypedToken);return r;
}
struct ReferenceIndex {
    LzssPositionDistance1mMatchFinder& finder;
    LzssMatch find_match(std::size_t p) noexcept {return finder.find_match_reference(p);}
    void advance(std::size_t p,std::size_t n) noexcept {finder.advance(p,n);}
};
}

LzssShortMatchCandidateResult tokenize_lzss_position_distance_1m_candidate(
    std::span<const std::byte> input,const LzssParameters& parameters,
    const core::DecoderLimits& limits,std::uint32_t eligibility,
    LzssPositionDistance1mSearch search,std::span<LzssTypedToken> tokens,
    std::span<std::byte> workspace) noexcept {
    if(search==LzssPositionDistance1mSearch::indexed_five_prefix)
        return tokenize_lzss_position_distance_1m_five_prefix_candidate(input,parameters,limits,eligibility,tokens,workspace);
    LzssShortMatchCandidateResult r{};r.input_size=input.size();
    if(eligibility<3 || eligibility>5) {r.error=Error::invalid_eligibility;return r;}
    if(search!=LzssPositionDistance1mSearch::exhaustive && search!=LzssPositionDistance1mSearch::indexed_reference
        && search!=LzssPositionDistance1mSearch::indexed) {r.error=Error::invalid_parameters;return r;}
    r.token_error=validate_lzss_typed_parameters(parameters,limits,LzssTypedTokenVariant::field_context_1m_short_length_escape);
    if(r.token_error!=LzssTypedTokenError::none) {
        r.error=r.token_error==LzssTypedTokenError::limit_exceeded?Error::input_limit_exceeded:Error::invalid_parameters;return r;
    }
    if(input.size()>1048576 || input.size()>limits.max_frame_size || input.size()>limits.max_block_size
        || input.size()>limits.max_total_output_size) {r.error=Error::input_limit_exceeded;return r;}
    std::size_t token_bytes{},aggregate{};
    if(!core::checked_multiply(tokens.size(),sizeof(LzssTypedToken),token_bytes)
        || !core::checked_add(input.size(),token_bytes,aggregate)
        || !core::checked_add(aggregate,workspace.size(),aggregate)
        || !core::checked_add(aggregate,sizeof(LzssPositionDistance1mMatchFinder),aggregate)) {
        r.error=Error::arithmetic_overflow;return r;
    }
    if(aggregate>limits.max_internal_buffered_bytes) {r.error=Error::token_storage_limit_exceeded;return r;}
    struct Region {const void* data;std::size_t size;};
    const std::array regions{Region{input.data(),input.size()},Region{tokens.data(),token_bytes},
        Region{workspace.data(),workspace.size()},Region{&parameters,sizeof(parameters)},Region{&limits,sizeof(limits)}};
    for(std::size_t i=0;i<regions.size();++i) for(std::size_t j=i+1;j<regions.size();++j) {
        const auto overlap=core::check_buffer_overlap(regions[i].data,regions[i].size,regions[j].data,regions[j].size);
        if(overlap!=core::BufferOverlap::disjoint) {
            r.error=overlap==core::BufferOverlap::arithmetic_overflow?Error::arithmetic_overflow:Error::overlapping_buffers;return r;
        }
    }
    const auto run=[&](std::span<LzssTypedToken> output) {
        if(search==LzssPositionDistance1mSearch::exhaustive) {
            LzssExhaustiveMatchFinder finder{input,parameters};return parse(input,eligibility,output,finder);
        }
        LzssPositionDistance1mMatchFinder finder;
        LzssShortMatchCandidateResult result{};result.input_size=input.size();
        result.finder_error=initialize_lzss_position_distance_1m_match_finder(input,parameters,limits,workspace,finder);
        if(result.finder_error!=LzssShortPrefixError::none) {
            result.error=result.finder_error==LzssShortPrefixError::workspace_too_small?Error::workspace_too_small
                :result.finder_error==LzssShortPrefixError::misaligned_workspace?Error::misaligned_workspace
                :result.finder_error==LzssShortPrefixError::workspace_limit_exceeded?Error::token_storage_limit_exceeded
                :result.finder_error==LzssShortPrefixError::arithmetic_overflow?Error::arithmetic_overflow
                :result.finder_error==LzssShortPrefixError::overlapping_buffers?Error::overlapping_buffers
                :Error::invalid_parameters;
            return result;
        }
        if(search==LzssPositionDistance1mSearch::indexed_reference) {
            ReferenceIndex reference{finder};return parse(input,eligibility,output,reference);
        }
        return parse(input,eligibility,output,finder);
    };
    if(tokens.size()>=input.size()) return run(tokens.first(input.size()));
    r=run({});
    if(r.error!=Error::none) return r;
    if(tokens.size()<r.token_count) {r.error=Error::output_too_small;return r;}
    return run(tokens.first(r.token_count));
}
}
