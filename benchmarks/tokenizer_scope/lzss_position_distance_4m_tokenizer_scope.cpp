#include "lzss_position_distance_4m_tokenizer_scope.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include <array>

namespace marc::dictionary::internal {
namespace {
using Error=LzssShortMatchCandidateError;
using Clock=std::chrono::steady_clock;
template<class Finder>
LzssShortMatchCandidateResult parse(std::span<const std::byte> input,
    std::uint32_t eligibility,std::span<LzssTypedToken> tokens,Finder& finder,TokenizerScopeSample& sample) noexcept {
    LzssShortMatchCandidateResult r{};r.input_size=input.size();
    for(std::size_t position=0;position<input.size();) {
        auto before=sample.timed?Clock::now():Clock::time_point{};
        const auto match=finder.find_match(position);
        if(sample.timed) sample.find_seconds+=std::chrono::duration<double>(Clock::now()-before).count();
        ++sample.find_calls;
        const bool use=match.length>=eligibility;
        const std::size_t step=use?match.length:1;
        if(!tokens.empty()) tokens[r.token_count]=use ? LzssTypedToken{LzssTypedTokenKind::match,0,match.distance,match.length}
            : LzssTypedToken{LzssTypedTokenKind::literal,std::to_integer<std::uint8_t>(input[position]),0,0};
        ++r.token_count;before=sample.timed?Clock::now():Clock::time_point{};
        finder.advance(position,position+step);
        if(sample.timed) sample.advance_seconds+=std::chrono::duration<double>(Clock::now()-before).count();
        ++sample.advance_calls;sample.advanced_positions+=step;position+=step;
    }
    r.token_storage_size=r.token_count*sizeof(LzssTypedToken);return r;
}

}

LzssShortMatchCandidateResult tokenize_lzss_position_distance_4m_tokenizer_scope(
    std::span<const std::byte> input,const LzssParameters& parameters,
    const core::DecoderLimits& limits,std::uint32_t eligibility,
    std::span<LzssTypedToken> tokens,
    std::span<std::byte> workspace,TokenizerScopeSample& report,bool timed) noexcept {
    LzssShortMatchCandidateResult r{};r.input_size=input.size();
    if(eligibility<3 || eligibility>5) {r.error=Error::invalid_eligibility;return r;}
    r.token_error=validate_lzss_typed_parameters(parameters,limits,LzssTypedTokenVariant::field_context_4m_short_length_escape);
    if(r.token_error!=LzssTypedTokenError::none) {
        r.error=r.token_error==LzssTypedTokenError::limit_exceeded?Error::input_limit_exceeded:Error::invalid_parameters;return r;
    }
    if(input.size()>4194304 || input.size()>limits.max_frame_size || input.size()>limits.max_block_size
        || input.size()>limits.max_total_output_size) {r.error=Error::input_limit_exceeded;return r;}
    std::size_t token_bytes{},aggregate{};
    if(!core::checked_multiply(tokens.size(),sizeof(LzssTypedToken),token_bytes)
        || !core::checked_add(input.size(),token_bytes,aggregate)
        || !core::checked_add(aggregate,workspace.size(),aggregate)
        || !core::checked_add(aggregate,sizeof(LzssPositionDistance4mFivePrefixFinder),aggregate)
        || !core::checked_add(aggregate,tokenizer_scope_transient_state_bytes,aggregate)) {
        r.error=Error::arithmetic_overflow;return r;
    }
    if(aggregate>limits.max_internal_buffered_bytes) {r.error=Error::token_storage_limit_exceeded;return r;}
    struct Region {const void* data;std::size_t size;};
    const std::array regions{Region{input.data(),input.size()},Region{tokens.data(),token_bytes},
        Region{workspace.data(),workspace.size()},Region{&parameters,sizeof(parameters)},Region{&limits,sizeof(limits)},Region{&report,sizeof(report)}};
    for(std::size_t i=0;i<regions.size();++i) for(std::size_t j=i+1;j<regions.size();++j) {
        const auto overlap=core::check_buffer_overlap(regions[i].data,regions[i].size,regions[j].data,regions[j].size);
        if(overlap!=core::BufferOverlap::disjoint) {
            r.error=overlap==core::BufferOverlap::arithmetic_overflow?Error::arithmetic_overflow:Error::overlapping_buffers;return r;
        }
    }
    TokenizerScopeSample prepared{};prepared.raw_bytes=input.size();prepared.timed=timed;
    const auto run=[&](std::span<LzssTypedToken> output) {
        LzssPositionDistance4mFivePrefixFinder finder;
        LzssShortMatchCandidateResult result{};result.input_size=input.size();
        ++prepared.parse_passes;
        {
            auto before=timed?Clock::now():Clock::time_point{};
            result.finder_error=initialize_lzss_position_distance_4m_five_prefix_finder(input,parameters,limits,workspace,finder);
            if(timed) prepared.initialize_seconds+=std::chrono::duration<double>(Clock::now()-before).count();
        }
        if(result.finder_error!=LzssShortPrefixError::none) {
            result.error=result.finder_error==LzssShortPrefixError::workspace_too_small?Error::workspace_too_small
                :result.finder_error==LzssShortPrefixError::misaligned_workspace?Error::misaligned_workspace
                :result.finder_error==LzssShortPrefixError::workspace_limit_exceeded?Error::token_storage_limit_exceeded
                :result.finder_error==LzssShortPrefixError::arithmetic_overflow?Error::arithmetic_overflow
                :result.finder_error==LzssShortPrefixError::overlapping_buffers?Error::overlapping_buffers
                :Error::invalid_parameters;
            return result;
        }
        return parse(input,eligibility,output,finder,prepared);
    };
    const auto publish=[&](LzssShortMatchCandidateResult result) {
        if(result.error==Error::none) {prepared.token_count=result.token_count;report=prepared;}
        return result;
    };
    if(tokens.size()>=input.size()) return publish(run(tokens.first(input.size())));
    r=run({});
    if(r.error!=Error::none) return r;
    if(tokens.size()<r.token_count) {r.error=Error::output_too_small;return r;}
    return publish(run(tokens.first(r.token_count)));
}
}
