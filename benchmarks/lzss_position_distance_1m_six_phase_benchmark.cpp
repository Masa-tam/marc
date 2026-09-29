#include "position_distance_1m_five_prefix.hpp"
#include "position_distance_1m_six_prefix.hpp"
#include "dictionary/lzss_position_distance_1m_five_prefix_finder.hpp"
#include "dictionary/lzss_position_distance_1m_candidate.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
using namespace marc::dictionary::internal;
using Clock=std::chrono::steady_clock;
using Times=std::array<double,5>;
double seconds(Clock::time_point begin) {return std::chrono::duration<double>(Clock::now()-begin).count();}
bool same(const LzssTypedToken& t,LzssMatch m) {
    return t.kind==LzssTypedTokenKind::literal ? m.length<3 : m.length==t.length && m.distance==t.distance;
}
template<bool Detailed,class Finder,class Reset>
bool replay(Finder& finder,Reset reset,std::span<const std::byte> raw,
    std::span<const LzssTypedToken> expected,std::span<LzssTypedToken> actual,Times& times) {
    const auto start=Clock::now();
    if(!reset()) return false;
    times[0]=seconds(start);
    std::size_t count{};
    for(std::size_t p=0;p<raw.size();) {
        Clock::time_point begin;
        if constexpr(Detailed) begin=Clock::now();
        const auto m=finder.find_match(p);
        if constexpr(Detailed) times[1]+=seconds(begin);
        const auto step=m.length>=3?m.length:1;
        if(step>raw.size()-p || count>=actual.size()) return false;
        actual[count++]=m.length>=3?LzssTypedToken{LzssTypedTokenKind::match,0,m.distance,m.length}
            :LzssTypedToken{LzssTypedTokenKind::literal,std::to_integer<std::uint8_t>(raw[p]),0,0};
        if constexpr(Detailed) begin=Clock::now();
        finder.advance(p,p+step);
        if constexpr(Detailed) times[2]+=seconds(begin);
        p+=step;
    }
    times[3]=seconds(start);
    if(count!=expected.size()) return false;
    for(std::size_t i=0;i<count;++i) {
        const auto a=actual[i],b=expected[i];
        if(a.kind!=b.kind || a.literal!=b.literal || a.length!=b.length || a.distance!=b.distance) return false;
    }
    // Separate batch replay without searches. Reset/validation excluded.
    // Leave the last token unadvanced, then query it to observe the built index.
    if(!reset()) return false;
    std::size_t p{};
    const auto begin=Clock::now();
    for(std::size_t i=0;i+1<expected.size();++i) {
        const auto step=expected[i].kind==LzssTypedTokenKind::literal?1:expected[i].length;
        finder.advance(p,p+step);p+=step;
    }
    times[4]=seconds(begin);
    return !expected.empty() && same(expected.back(),finder.find_match(p));
}
int main(int argc,char** argv) {
    if(argc!=2) return 2;
    std::ifstream file(argv[1],std::ios::binary|std::ios::ate);if(!file) return 2;
    const auto size=file.tellg();if(size<=0 || size>64*1024*1024) return 2;
    std::vector<std::byte> input(static_cast<std::size_t>(size));file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(input.data()),size)) return 2;
    constexpr std::size_t capacity=1048576;
    const LzssParameters parameters{capacity,3,258,0};const marc::core::DecoderLimits limits{};
    const auto q=calculate_lzss_position_distance_1m_five_prefix_workspace(capacity,parameters,limits);
    if(q.error!=LzssShortPrefixError::none) return 1;
    std::vector<std::uint32_t> storage(q.workspace_size/4);
    const auto workspace=std::as_writable_bytes(std::span{storage});
    std::vector<LzssTypedToken> expected(capacity),actual(capacity);
    LzssPositionDistance1mFivePrefixFinder bounded;
    marc::benchmark::SharedFivePrefixFinder five(capacity);
    marc::benchmark::SixPrefixFinder six(capacity);
    std::array<std::array<std::array<Times,3>,2>,3> totals{};
    std::uint64_t tokens{},advance_bytes{};
    for(std::size_t offset=0;offset<input.size();offset+=capacity) {
        const auto raw=std::span{input}.subspan(offset,std::min(capacity,input.size()-offset));
        const auto oracle=tokenize_lzss_position_distance_1m_candidate(raw,parameters,limits,3,
            LzssPositionDistance1mSearch::indexed,expected,workspace);
        if(oracle.error!=LzssShortMatchCandidateError::none || !oracle.token_count) return 1;
        const auto selected=std::span{expected}.first(oracle.token_count);tokens+=selected.size();
        const auto last=selected.back();advance_bytes+=raw.size()-(last.kind==LzssTypedTokenKind::literal?1:last.length);
        for(int iteration=-1;iteration<3;++iteration) for(int order=0;order<3;++order) {
            const auto which=(order+iteration+1+offset/capacity)%3;
            for(int mode_order=0;mode_order<2;++mode_order) {
                const auto mode=(mode_order+iteration+1)%2;Times t{};
                auto run=[&](auto& finder,auto reset) {
                    return mode ? replay<true>(finder,reset,raw,selected,actual,t)
                        : replay<false>(finder,reset,raw,selected,actual,t);
                };
                bool ok{};
                if(which==0) ok=run(bounded,[&]{return initialize_lzss_position_distance_1m_five_prefix_finder(raw,parameters,limits,workspace,bounded)==LzssShortPrefixError::none;});
                else if(which==1) ok=run(five,[&]{five.reset(raw);return true;});
                else ok=run(six,[&]{six.reset(raw);return true;});
                if(!ok) return 1;
                if(iteration>=0) for(std::size_t j=0;j<t.size();++j) totals[iteration][mode][which][j]+=t[j];
            }
        }
    }
    std::cout<<std::setprecision(12)<<"input_bytes="<<input.size()<<"\ntokens="<<tokens
        <<"\nadvance_only_bytes="<<advance_bytes<<"\nverified_iterations=3\nphase_verified=1\n";
    const std::array paths{"bounded_five","vector_five","six"};
    const std::array modes{"coarse","detailed"};
    const std::array phases{"initialize","find","advance","wall","advance_only"};
    for(std::size_t i=0;i<3;++i) for(std::size_t m=0;m<2;++m)
        for(std::size_t p=0;p<3;++p) for(std::size_t f=0;f<5;++f)
            std::cout<<"iteration_"<<i<<'_'<<modes[m]<<'_'<<paths[p]<<'_'<<phases[f]<<"_seconds="<<totals[i][m][p][f]<<'\n';
}
