#include "position_distance_1m_lazy_six_checks.hpp"
#include "position_distance_1m_online_six.hpp"
#include "dictionary/lzss_position_distance_1m_five_prefix_finder.hpp"
#include "dictionary/lzss_position_distance_1m_candidate.hpp"
#include "frame/lzss_position_distance_1m_frame.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string_view>
using namespace marc::dictionary::internal;
using namespace marc::benchmark;
using Clock=std::chrono::steady_clock;
constexpr std::array names{"bounded_five","unobserved","monitor", "s1k1","s1k4","s1k16","s2k1","s2k4","s2k16","s3k1","s3k4","s3k16"};
static_assert(sizeof(LazySixPrefixFinder)==sizeof(ObservedLazySixPrefixFinder));

template<class Finder>
std::size_t replay(Finder& finder,std::span<const std::byte> raw,std::span<LzssTypedToken> tokens) {
    std::size_t count{};
    for(std::size_t p=0;p<raw.size();) {
        const auto m=finder.find_match(p);const auto step=m.length>=3?m.length:1;
        if(step>raw.size()-p || count>=tokens.size()) return tokens.size()+1;
        tokens[count++]=m.length>=3?LzssTypedToken{LzssTypedTokenKind::match,0,m.distance,m.length}
            :LzssTypedToken{LzssTypedTokenKind::literal,std::to_integer<std::uint8_t>(raw[p]),0,0};
        finder.advance(p,p+step);p+=step;
    }
    return count;
}
bool same(std::span<const LzssTypedToken> a,std::span<const LzssTypedToken> b) {
    return a.size()==b.size() && std::equal(a.begin(),a.end(),b.begin(),[](auto x,auto y) {
        return x.kind==y.kind && x.literal==y.literal && x.length==y.length && x.distance==y.distance;
    });
}
bool hand_checks() {
    std::array<std::byte,7> raw{};OnlineSixFinder finder(raw.size());
    if(!finder.reset(raw,2,1) || finder.find_match(0)!=LzssMatch{} || !finder.advance(0,1)
        || finder.find_match(1)!=LzssMatch{1,6} || !finder.advance(1,7)) return false;
    const auto before=finder.trace();std::array<std::byte,8> oversized{};
    if(finder.reset(oversized,2,1) || finder.trace()!=before || finder.reset(raw,0,1)
        || finder.trace()!=before || finder.reset(raw,2,2) || finder.trace()!=before) return false;
    return finder.reset({},2,1) && finder.trace().count==0 && finder.trace().activation==0 && finder.trace().ok;
}
int main(int argc,char** argv) {
    const bool verify_only=argc==3 && std::string_view(argv[2])=="--verify-only";
    if(argc!=2 && !verify_only) return 2;
    std::ifstream file(argv[1],std::ios::binary|std::ios::ate);if(!file) return 2;
    const auto size=file.tellg();if(size<=0 || size>64*1024*1024) return 2;
    std::vector<std::byte> input(static_cast<std::size_t>(size));file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(input.data()),size)) return 2;
    if(!check_lazy_six_boundaries() || !hand_checks()) return 1;
    constexpr std::size_t capacity=1048576;
    const LzssParameters parameters{capacity,3,258,0};const marc::core::DecoderLimits limits{};
    const auto q=calculate_lzss_position_distance_1m_five_prefix_workspace(capacity,parameters,limits);
    if(q.error!=LzssShortPrefixError::none) return 1;
    std::vector<std::uint32_t> storage(q.workspace_size/4);const auto workspace=std::as_writable_bytes(std::span{storage});
    std::vector<LzssTypedToken> tokens(capacity),actual(capacity);
    std::vector<marc::context::internal::ModeledOperation> operations(2*capacity);
    std::vector<std::byte> frame_a(18*capacity+85),frame_b(frame_a.size()),restored(capacity);
    LazySixPrefixFinder plain(capacity);OnlineSixFinder online(capacity);
    std::array<std::array<double,names.size()>,3> times{};
    std::array<std::uint64_t,names.size()> transitions{},checks{};
    std::uint64_t total_tokens{},frame_bytes{};
    for(std::size_t offset=0;offset<input.size();offset+=capacity) {
        const auto raw=std::span{input}.subspan(offset,std::min(capacity,input.size()-offset));
        const auto oracle=tokenize_lzss_position_distance_1m_candidate(raw,parameters,limits,3,
            LzssPositionDistance1mSearch::indexed,tokens,workspace);
        if(oracle.error!=LzssShortMatchCandidateError::none) return 1;
        const auto expected=std::span{tokens}.first(oracle.token_count);total_tokens+=expected.size();
        std::array<OnlineDecisionTrace,names.size()> expected_traces{};
        for(int iteration=-1;iteration<(verify_only?0:3);++iteration) for(std::size_t order=0;order<names.size();++order) {
            const auto mode=(order+static_cast<std::size_t>(iteration+1)+offset/capacity)%names.size();
            const auto start=Clock::now();std::size_t count{};
            if(mode==0) {
                LzssPositionDistance1mFivePrefixFinder finder;
                if(initialize_lzss_position_distance_1m_five_prefix_finder(raw,parameters,limits,workspace,finder)!=LzssShortPrefixError::none) return 1;
                count=replay(finder,raw,actual);
            } else if(mode==1) {
                if(!plain.reset(raw)) return 1;count=replay(plain,raw,actual);
            } else {
                constexpr std::array<unsigned,3> scales{1,4,16};
                const auto index=mode>=3?mode-3:3;
                if(!online.reset(raw,static_cast<unsigned>(index/3+1),scales[index%3],mode==2)) return 1;
                count=replay(online,raw,actual);
            }
            const auto elapsed=std::chrono::duration<double>(Clock::now()-start).count();
            if(count>actual.size() || !same(expected,std::span{actual}.first(count))) return 1;
            if(mode>=2) {
                if(!online.trace().ok) return 1;
                if(iteration<0) expected_traces[mode]=online.trace();
                else if(online.trace()!=expected_traces[mode]) return 1;
            }
            if(iteration>=0) times[iteration][mode]+=elapsed;
        }
        for(std::size_t mode=2;mode<names.size();++mode) {
            const auto& trace=expected_traces[mode];checks[mode]+=trace.count;
            if(trace.activation<raw.size()) ++transitions[mode];
            std::cout<<"frame_"<<offset/capacity<<'_'<<names[mode]<<"_activation="<<trace.activation<<'\n';
            std::cout<<"frame_"<<offset/capacity<<'_'<<names[mode]<<"_events=";
            for(std::size_t j=0;j<trace.count;++j) {
                const auto& e=trace.events[j];
                if(j) std::cout<<';';
                std::cout<<e.position<<','<<e.width<<','<<e.visits<<','<<e.queries<<','<<e.benefit<<','<<e.cost<<','<<e.selected;
            }
            std::cout<<'\n';
        }
        // All paths produced the same tokens. Serialize the last actual replay
        // against the retained oracle outside the timing region.
        using namespace marc::frame::internal;
        const TypedContextStreamHeader stream{capacity,input.size(),parameters,32768,44,9,1,10};
        const auto a=encode_lzss_position_distance_1m_frame(stream,limits,offset/capacity,offset,expected,operations,frame_a);
        const auto b=encode_lzss_position_distance_1m_frame(stream,limits,offset/capacity,offset,std::span{actual}.first(expected.size()),operations,frame_b);
        if(a.error!=LzssShortMatchFrameEncodeError::none || b.error!=LzssShortMatchFrameEncodeError::none || a.serialized_size!=b.serialized_size
            || !std::equal(frame_a.begin(),frame_a.begin()+a.serialized_size,frame_b.begin())) return 1;
        const auto decoded=decode_lzss_position_distance_1m_frame_scratch(std::span{frame_b}.first(b.serialized_size),
            {stream,limits,offset/capacity,offset},actual,restored);
        if(decoded.error!=LzssShortMatchFrameDecodeError::none || !std::equal(raw.begin(),raw.end(),restored.begin())) return 1;
        frame_bytes+=a.serialized_size;
    }
    std::cout<<std::setprecision(12)<<"input_bytes="<<input.size()<<"\ntokens="<<total_tokens<<"\nframe_bytes="<<frame_bytes
        <<"\nverified_iterations="<<(verify_only?0:3)<<"\nframe_identity=1\nonline_verified=1\n";
    for(std::size_t mode=0;mode<names.size();++mode) {
        std::cout<<names[mode]<<"_transitions="<<transitions[mode]<<'\n'<<names[mode]<<"_checks="<<checks[mode]<<'\n';
        for(std::size_t i=0;i<3;++i) std::cout<<"iteration_"<<i<<'_'<<names[mode]<<"_seconds="<<times[i][mode]<<'\n';
    }
}
