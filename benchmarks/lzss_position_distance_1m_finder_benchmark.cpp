#include "position_distance_1m_counter.hpp"
#include "dictionary/lzss_position_distance_1m_candidate.hpp"
#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>

int main(int argc,char** argv) {
    if(argc!=2) return 2;
    std::ifstream file(argv[1],std::ios::binary|std::ios::ate);
    if(!file) return 2;
    const auto size=file.tellg();
    if(size<=0 || size>64*1024*1024) return 2;
    std::vector<std::byte> input(static_cast<std::size_t>(size));file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(input.data()),size)) return 2;
    using namespace marc::dictionary::internal;
    using Clock=std::chrono::steady_clock;
    constexpr std::size_t frame_size=1048576;
    const LzssParameters parameters{frame_size,3,258,0};
    const marc::core::DecoderLimits limits{};
    const auto needed=calculate_lzss_position_distance_1m_match_workspace(frame_size,parameters,limits);
    if(needed.error!=LzssShortPrefixError::none) return 1;
    std::vector<std::max_align_t> storage((needed.workspace_size+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
    const auto workspace=std::as_writable_bytes(std::span{storage}).first(needed.workspace_size);
    std::vector<LzssTypedToken> tokens(frame_size);
    std::array<std::array<double,4>,3> totals{};
    double unclocked{};
    std::uint64_t total_tokens{};
    marc::benchmark::FinderCounts counts;
    for(std::size_t offset=0;offset<input.size();offset+=frame_size) {
        const auto raw=std::span{input}.subspan(offset,std::min(frame_size,input.size()-offset));
        const auto oracle=tokenize_lzss_position_distance_1m_candidate(raw,parameters,limits,3,
            LzssPositionDistance1mSearch::indexed,tokens,workspace);
        if(oracle.error!=LzssShortMatchCandidateError::none) return 1;
        total_tokens+=oracle.token_count;
        const auto selected=std::span{tokens}.first(oracle.token_count);
        auto same=[](const LzssTypedToken& t,LzssMatch m) {
            return t.kind==LzssTypedTokenKind::literal ? m.length<3 : m.length==t.length && m.distance==t.distance;
        };
        for(int iteration=-1;iteration<3;++iteration) {
            const auto start=Clock::now();
            LzssPositionDistance1mMatchFinder finder;
            const auto initialized=initialize_lzss_position_distance_1m_match_finder(raw,parameters,limits,workspace,finder);
            const auto after_init=Clock::now();
            if(initialized!=LzssShortPrefixError::none) return 1;
            double find_time{},advance_time{};std::size_t position{};
            for(const auto& t:selected) {
                Clock::time_point begin;
                if(iteration>=0) begin=Clock::now();
                const auto match=finder.find_match(position);
                if(iteration>=0) find_time+=std::chrono::duration<double>(Clock::now()-begin).count();
                if(!same(t,match) || (t.kind==LzssTypedTokenKind::literal && t.literal!=std::to_integer<std::uint8_t>(raw[position]))) return 1;
                const auto next=position+(t.kind==LzssTypedTokenKind::literal?1:t.length);
                if(iteration>=0) begin=Clock::now();
                finder.advance(position,next);
                if(iteration>=0) advance_time+=std::chrono::duration<double>(Clock::now()-begin).count();
                position=next;
            }
            if(position!=raw.size()) return 1;
            const auto wall=std::chrono::duration<double>(Clock::now()-start).count();
            if(iteration<0) unclocked+=wall;
            else {
                auto& a=totals[iteration];a[0]+=std::chrono::duration<double>(after_init-start).count();
                a[1]+=find_time;a[2]+=advance_time;a[3]+=wall;
            }
        }
        marc::benchmark::CounterFinder counter(raw,counts);
        std::size_t p{};
        for(const auto& t:selected) {
            if(!same(t,counter.find(p))) return 1;
            const auto next=p+(t.kind==LzssTypedTokenKind::literal?1:t.length);
            counter.advance(p,next);p=next;
        }
        if(p!=raw.size()) return 1;
    }
    std::cout<<std::setprecision(12)<<"input_bytes="<<input.size()<<"\ntokens="<<total_tokens
        <<"\nverified_iterations=3\ncounter_verified=1\nunclocked_warmup_seconds="<<unclocked<<'\n';
    const std::array names{"initialize","find","advance","wall"};
    for(std::size_t i=0;i<totals.size();++i) for(std::size_t j=0;j<names.size();++j)
        std::cout<<"iteration_"<<i<<'_'<<names[j]<<"_seconds="<<totals[i][j]<<'\n';
    std::cout<<"short_visits="<<counts.short_visits<<"\nlong_visits="<<counts.long_visits
        <<"\nprobe_rejects="<<counts.probe_rejects<<"\nprefix_rejects="<<counts.prefix_rejects
        <<"\nextension_compares="<<counts.extension_compares<<"\nextension_equal="<<counts.extension_equal
        <<"\nshort_inserts="<<counts.short_inserts<<"\nlong_inserts="<<counts.long_inserts<<'\n';
}
