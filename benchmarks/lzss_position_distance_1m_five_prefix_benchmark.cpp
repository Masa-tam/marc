#include "position_distance_1m_five_prefix.hpp"
#include "position_distance_1m_counter.hpp"
#include "dictionary/lzss_position_distance_1m_candidate.hpp"
#include "frame/lzss_position_distance_1m_frame.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>

using namespace marc::dictionary::internal;
template<class Finder>
std::size_t replay(Finder& finder,std::span<const std::byte> raw,std::span<LzssTypedToken> tokens) {
    std::size_t count{};
    for(std::size_t p=0;p<raw.size();) {
        const auto m=finder.find_match(p);
        const auto next=p+(m.length>=3?m.length:1);
        tokens[count++]=m.length>=3 ? LzssTypedToken{LzssTypedTokenKind::match,0,m.distance,m.length}
            : LzssTypedToken{LzssTypedTokenKind::literal,std::to_integer<std::uint8_t>(raw[p]),0,0};
        finder.advance(p,next);p=next;
    }
    return count;
}
bool same(std::span<const LzssTypedToken> a,std::span<const LzssTypedToken> b) {
    return a.size()==b.size() && std::equal(a.begin(),a.end(),b.begin(),[](auto x,auto y) {
        return x.kind==y.kind && x.literal==y.literal && x.length==y.length && x.distance==y.distance;
    });
}
int main(int argc,char** argv) {
    if(argc!=2) return 2;
    std::ifstream file(argv[1],std::ios::binary|std::ios::ate);if(!file) return 2;
    const auto size=file.tellg();if(size<=0 || size>64*1024*1024) return 2;
    std::vector<std::byte> input(static_cast<std::size_t>(size));file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(input.data()),size)) return 2;
    using Clock=std::chrono::steady_clock;
    constexpr std::size_t frame_size=1048576;
    const LzssParameters parameters{frame_size,3,258,0};
    const marc::core::DecoderLimits limits{};
    const auto needed=calculate_lzss_position_distance_1m_match_workspace(frame_size,parameters,limits);
    if(needed.error!=LzssShortPrefixError::none) return 1;
    std::vector<std::max_align_t> storage((needed.workspace_size+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
    const auto workspace=std::as_writable_bytes(std::span{storage}).first(needed.workspace_size);
    std::vector<LzssTypedToken> oracle(frame_size),baseline(frame_size),prototype(frame_size);
    marc::benchmark::FivePrefixFinder five(frame_size);
    std::vector<marc::context::internal::ModeledOperation> operations(2*frame_size);
    std::vector<std::byte> frame_a(18*frame_size+85),frame_b(frame_a.size());
    std::array<std::array<double,2>,3> times{};
    marc::benchmark::FinderCounts counts;
    std::uint64_t token_count{},frame_bytes{};
    for(std::size_t offset=0;offset<input.size();offset+=frame_size) {
        const auto raw=std::span{input}.subspan(offset,std::min(frame_size,input.size()-offset));
        const auto parsed=tokenize_lzss_position_distance_1m_candidate(raw,parameters,limits,3,
            LzssPositionDistance1mSearch::indexed,oracle,workspace);
        if(parsed.error!=LzssShortMatchCandidateError::none) return 1;
        const auto expected=std::span{oracle}.first(parsed.token_count);token_count+=expected.size();
        for(int iteration=-1;iteration<3;++iteration) {
            for(int order=0;order<2;++order) {
                const auto which=(order+iteration+1+(offset/frame_size)%2)%2;
                std::size_t count{};auto output=which?std::span{prototype}:std::span{baseline};
                const auto begin=Clock::now();
                if(which) {five.reset(raw);count=replay(five,raw,output);}
                else {
                    LzssPositionDistance1mMatchFinder finder;
                    if(initialize_lzss_position_distance_1m_match_finder(raw,parameters,limits,workspace,finder)!=LzssShortPrefixError::none) return 1;
                    count=replay(finder,raw,output);
                }
                const auto elapsed=std::chrono::duration<double>(Clock::now()-begin).count();
                if(!same(expected,output.first(count))) return 1;
                if(iteration>=0) times[iteration][which]+=elapsed;
            }
        }
        using namespace marc::frame::internal;
        const TypedContextStreamHeader stream{frame_size,input.size(),parameters,32768,44,9,1,10};
        const auto a=encode_lzss_position_distance_1m_frame(stream,limits,offset/frame_size,offset,expected,operations,frame_a);
        const auto b=encode_lzss_position_distance_1m_frame(stream,limits,offset/frame_size,offset,std::span{prototype}.first(expected.size()),operations,frame_b);
        if(a.error!=LzssShortMatchFrameEncodeError::none || b.error!=LzssShortMatchFrameEncodeError::none
            || a.serialized_size!=b.serialized_size || !std::equal(frame_a.begin(),frame_a.begin()+a.serialized_size,frame_b.begin())) return 1;
        frame_bytes+=a.serialized_size;
        marc::benchmark::CounterFinder counter(raw,counts);std::size_t p{};
        for(const auto& t:expected) {
            const auto m=counter.find(p);
            if(t.kind==LzssTypedTokenKind::match ? m.length!=t.length || m.distance!=t.distance : m.length>=3) return 1;
            const auto next=p+(t.kind==LzssTypedTokenKind::literal?1:t.length);counter.advance(p,next);p=next;
        }
        if(p!=raw.size()) return 1;
    }
    std::cout<<std::setprecision(12)<<"input_bytes="<<input.size()<<"\ntokens="<<token_count
        <<"\nframe_bytes="<<frame_bytes<<"\nverified_iterations=3\nframe_identity=1\ncounter_verified=1\n"
        <<"additional_array_bytes=4456448\nlong_visits="<<counts.long_visits
        <<"\nexact_long_prefix="<<counts.exact_long_prefix<<"\ncolliding_long_prefix="<<counts.colliding_long_prefix<<'\n';
    for(std::size_t i=0;i<times.size();++i) std::cout<<"iteration_"<<i<<"_baseline_seconds="<<times[i][0]
        <<"\niteration_"<<i<<"_prototype_seconds="<<times[i][1]<<'\n';
}
