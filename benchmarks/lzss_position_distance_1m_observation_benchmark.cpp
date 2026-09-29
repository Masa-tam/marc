#include "position_distance_1m_lazy_six_checks.hpp"
#include "position_distance_1m_five_counter.hpp"
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
constexpr std::array names{"bounded_five","unobserved","observed"};
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
    std::array<std::byte,7> raw{};ObservedLazySixPrefixFinder finder(raw.size());
    if(!finder.reset(raw) || finder.observations()!=LazySearchObservations{}) return false;
    if(finder.find_match(0)!=LzssMatch{} || !finder.advance(0,1) || finder.find_match(1)!=LzssMatch{1,6}
        || finder.observations()!=LazySearchObservations{2,1,1,1}) return false;
    if(!finder.advance(1,7) || finder.activate(7)) return false;
    const auto before=finder.observations();std::array<std::byte,8> oversized{};
    if(finder.reset(oversized) || finder.observations()!=before) return false;
    if(!finder.reset(raw) || finder.observations()!=LazySearchObservations{} || !finder.activate(0)) return false;
    if(finder.find_match(0)!=LzssMatch{} || !finder.advance(0,1) || finder.find_match(1)!=LzssMatch{1,6}
        || finder.observations()!=LazySearchObservations{}) return false;
    return finder.reset({}) && finder.observations()==LazySearchObservations{};
}
void print_counts(const std::string& prefix,const LazySearchObservations& c) {
    std::cout<<prefix<<"queries="<<c.queries<<'\n'<<prefix<<"long_visits="<<c.long_visits<<'\n'
        <<prefix<<"probe_passes="<<c.probe_passes<<'\n'<<prefix<<"prefix_passes="<<c.prefix_passes<<'\n';
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
    LazySixPrefixFinder plain(capacity);ObservedLazySixPrefixFinder observed(capacity);
    std::array<std::array<double,3>,3> times{};LazySearchObservations totals;
    std::uint64_t total_tokens{},frame_bytes{};
    for(std::size_t offset=0;offset<input.size();offset+=capacity) {
        const auto raw=std::span{input}.subspan(offset,std::min(capacity,input.size()-offset));
        const auto oracle=tokenize_lzss_position_distance_1m_candidate(raw,parameters,limits,3,
            LzssPositionDistance1mSearch::indexed,tokens,workspace);
        if(oracle.error!=LzssShortMatchCandidateError::none) return 1;
        const auto expected=std::span{tokens}.first(oracle.token_count);total_tokens+=expected.size();
        LazySearchObservations expected_counts;
        for(int iteration=-1;iteration<(verify_only?0:3);++iteration) for(int order=0;order<3;++order) {
            const auto mode=(order+iteration+1+offset/capacity)%3;
            const auto start=Clock::now();std::size_t count{};
            if(mode==0) {
                LzssPositionDistance1mFivePrefixFinder finder;
                if(initialize_lzss_position_distance_1m_five_prefix_finder(raw,parameters,limits,workspace,finder)!=LzssShortPrefixError::none) return 1;
                count=replay(finder,raw,actual);
            } else if(mode==1) {
                if(!plain.reset(raw)) return 1;count=replay(plain,raw,actual);
            } else {
                if(!observed.reset(raw)) return 1;count=replay(observed,raw,actual);
            }
            const auto elapsed=std::chrono::duration<double>(Clock::now()-start).count();
            if(count>actual.size() || !same(expected,std::span{actual}.first(count))) return 1;
            if(mode==2) {
                if(observed.active()) return 1;
                if(iteration<0) expected_counts=observed.observations();
                else if(observed.observations()!=expected_counts) return 1;
            }
            if(iteration>=0) times[iteration][mode]+=elapsed;
        }
        // Separate untimed replay validates counters and samples cumulative
        // observations after the first token end reaching each raw-byte quarter.
        FiveFinderCounts reference_counts;FiveCounterFinder reference(raw,reference_counts);
        if(!observed.reset(raw)) return 1;
        std::size_t p{},sample=1;
        for(const auto& token:expected) {
            const auto m=observed.find_match(p);
            if(m!=reference.find(p)) return 1;
            const auto next=p+(token.kind==LzssTypedTokenKind::literal?1:token.length);
            if(!observed.advance(p,next)) return 1;reference.advance(p,next);p=next;
            while(sample<=4 && p>=(raw.size()*sample+3)/4) {
                const auto prefix="frame_"+std::to_string(offset/capacity)+"_sample_"+std::to_string(sample)+"_";
                std::cout<<prefix<<"position="<<p<<'\n';print_counts(prefix,observed.observations());++sample;
            }
        }
        const auto c=observed.observations();
        if(p!=raw.size() || sample!=5 || c!=expected_counts || c.queries!=expected.size()
            || c.long_visits!=reference_counts.long_visits
            || c.probe_passes!=reference_counts.long_visits-reference_counts.probe_rejects
            || c.prefix_passes!=c.probe_passes-reference_counts.prefix_rejects
            || c.long_visits>std::uint64_t(raw.size())*raw.size()) return 1;
        totals.queries+=c.queries;totals.long_visits+=c.long_visits;
        totals.probe_passes+=c.probe_passes;totals.prefix_passes+=c.prefix_passes;
        // Both actual replays already matched all tokens; serialize the observed one.
        if(!observed.reset(raw) || replay(observed,raw,actual)!=expected.size()
            || !same(expected,std::span{actual}.first(expected.size()))) return 1;
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
        <<"\nverified_iterations="<<(verify_only?0:3)<<"\nframe_identity=1\nobservations_verified=1\n"
        <<"observation_payload_bytes="<<sizeof(LazySearchObservations)<<"\n";
    print_counts("total_",totals);
    for(std::size_t i=0;i<3;++i) for(std::size_t mode=0;mode<3;++mode)
        std::cout<<"iteration_"<<i<<'_'<<names[mode]<<"_seconds="<<times[i][mode]<<'\n';
}
