#include "position_distance_1m_lazy_six_checks.hpp"
#include "position_distance_1m_six_prefix.hpp"
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
constexpr std::array names{"bounded_five","original_six","never","zero","quarter","half","late"};
struct ForcedFinder {
    LazySixPrefixFinder& finder;
    std::size_t checkpoint;
    bool ok{true};
    LzssMatch find_match(std::size_t p) {
        if(p==checkpoint && !finder.activate(p)) ok=false;
        return finder.find_match(p);
    }
    void advance(std::size_t p,std::size_t next) {if(!finder.advance(p,next)) ok=false;}
};
template<class Finder>
std::size_t replay(Finder& finder,std::span<const std::byte> raw,std::span<LzssTypedToken> tokens) {
    std::size_t count{};
    for(std::size_t p=0;p<raw.size();) {
        const auto m=finder.find_match(p);
        const auto step=m.length>=3?m.length:1;
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
int main(int argc,char** argv) {
    const bool verify_only=argc==3 && std::string_view(argv[2])=="--verify-only";
    if(argc!=2 && !verify_only) return 2;
    std::ifstream file(argv[1],std::ios::binary|std::ios::ate);if(!file) return 2;
    const auto size=file.tellg();if(size<=0 || size>64*1024*1024) return 2;
    std::vector<std::byte> input(static_cast<std::size_t>(size));file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(input.data()),size)) return 2;
    if(!check_lazy_six_boundaries()) return 1;
    constexpr std::size_t capacity=1048576;
    const LzssParameters parameters{capacity,3,258,0};const marc::core::DecoderLimits limits{};
    const auto q=calculate_lzss_position_distance_1m_five_prefix_workspace(capacity,parameters,limits);
    if(q.error!=LzssShortPrefixError::none) return 1;
    std::vector<std::uint32_t> storage(q.workspace_size/4);
    const auto workspace=std::as_writable_bytes(std::span{storage});
    std::vector<LzssTypedToken> tokens(capacity),actual(capacity);
    std::vector<marc::context::internal::ModeledOperation> operations(2*capacity);
    std::vector<std::byte> frame_a(18*capacity+85),frame_b(frame_a.size()),restored(capacity);
    LazySixPrefixFinder lazy(capacity);SixPrefixFinder original(capacity);
    std::array<std::array<double,7>,3> times{},initialization{},catch_up{};
    std::array<std::uint64_t,7> transitions{},historical_positions{};
    std::uint64_t total_tokens{},frame_bytes{};
    for(std::size_t offset=0;offset<input.size();offset+=capacity) {
        const auto raw=std::span{input}.subspan(offset,std::min(capacity,input.size()-offset));
        const auto oracle=tokenize_lzss_position_distance_1m_candidate(raw,parameters,limits,3,
            LzssPositionDistance1mSearch::indexed,tokens,workspace);
        if(oracle.error!=LzssShortMatchCandidateError::none) return 1;
        const auto expected=std::span{tokens}.first(oracle.token_count);total_tokens+=expected.size();
        std::array<std::size_t,7> checkpoints;checkpoints.fill(raw.size());
        if(raw.size()>=6) checkpoints[3]=0;
        std::size_t p{};
        for(const auto& t:expected) {
            if(raw.size()-p>=6) {
                if(p>=raw.size()/4 && checkpoints[4]==raw.size()) checkpoints[4]=p;
                if(p>=raw.size()/2 && checkpoints[5]==raw.size()) checkpoints[5]=p;
                checkpoints[6]=p;
            }
            p+=t.kind==LzssTypedTokenKind::literal?1:t.length;
        }
        for(std::size_t mode=3;mode<7;++mode) {
            if(checkpoints[mode]<raw.size()) {++transitions[mode];historical_positions[mode]+=checkpoints[mode];}
            std::cout<<"frame_"<<offset/capacity<<'_'<<names[mode]<<"_checkpoint="<<checkpoints[mode]<<'\n';
        }
        for(int iteration=-1;iteration<(verify_only?0:3);++iteration) for(int order=0;order<7;++order) {
            const auto mode=(order+iteration+1+offset/capacity)%7;
            const auto start=Clock::now();std::size_t count{};
            if(mode==0) {
                LzssPositionDistance1mFivePrefixFinder finder;
                if(initialize_lzss_position_distance_1m_five_prefix_finder(raw,parameters,limits,workspace,finder)!=LzssShortPrefixError::none) return 1;
                count=replay(finder,raw,actual);
            } else if(mode==1) {original.reset(raw);count=replay(original,raw,actual);}
            else {
                if(!lazy.reset(raw)) return 1;
                ForcedFinder finder{lazy,checkpoints[mode]};count=replay(finder,raw,actual);
                if(!finder.ok || lazy.active()!=(checkpoints[mode]<raw.size())) return 1;
            }
            const auto elapsed=std::chrono::duration<double>(Clock::now()-start).count();
            if(count>actual.size() || !same(expected,std::span{actual}.first(count))) return 1;
            if(iteration>=0) times[iteration][mode]+=elapsed;
        }
        using namespace marc::frame::internal;
        const TypedContextStreamHeader stream{capacity,input.size(),parameters,32768,44,9,1,10};
        const auto encoded=encode_lzss_position_distance_1m_frame(stream,limits,offset/capacity,offset,expected,operations,frame_a);
        if(encoded.error!=LzssShortMatchFrameEncodeError::none) return 1;
        frame_bytes+=encoded.serialized_size;
        for(std::size_t mode=2;mode<7;++mode) {
            if(!lazy.reset(raw)) return 1;
            ForcedFinder finder{lazy,checkpoints[mode]};const auto count=replay(finder,raw,actual);
            if(!finder.ok || count!=expected.size() || !same(expected,std::span{actual}.first(count))) return 1;
            const auto encoded_trial=encode_lzss_position_distance_1m_frame(stream,limits,offset/capacity,offset,
                std::span{actual}.first(count),operations,frame_b);
            if(encoded_trial.error!=LzssShortMatchFrameEncodeError::none || encoded_trial.serialized_size!=encoded.serialized_size
                || !std::equal(frame_a.begin(),frame_a.begin()+encoded.serialized_size,frame_b.begin())) return 1;
            const auto decoded=decode_lzss_position_distance_1m_frame_scratch(std::span{frame_b}.first(encoded_trial.serialized_size),
                {stream,limits,offset/capacity,offset},actual,restored);
            if(decoded.error!=LzssShortMatchFrameDecodeError::none || !std::equal(raw.begin(),raw.end(),restored.begin())) return 1;
            // Separate phase experiment: no searches during history preparation.
            // These cache conditions differ from full replay; do not subtract phases.
            if(!verify_only && checkpoints[mode]<raw.size()) for(std::size_t i=0;i<3;++i) {
                if(!lazy.reset(raw) || !lazy.advance(0,checkpoints[mode])) return 1;
                LazyActivationTimes phase;
                if(!lazy.activate_measured(checkpoints[mode],phase)) return 1;
                LzssExhaustiveMatchFinder reference(raw,parameters);
                if(lazy.find_match(checkpoints[mode])!=reference.find_match(checkpoints[mode])) return 1;
                initialization[i][mode]+=phase.initialize;catch_up[i][mode]+=phase.catch_up;
            }
        }
    }
    std::cout<<std::setprecision(12)<<"input_bytes="<<input.size()<<"\ntokens="<<total_tokens
        <<"\nframe_bytes="<<frame_bytes<<"\nverified_iterations="<<(verify_only?0:3)<<"\nframe_identity=1\nboundary_verified=1\n"
        <<"reserved_array_bytes=17825792\nadditional_array_bytes=4456448\n";
    for(std::size_t mode=0;mode<7;++mode) {
        std::cout<<names[mode]<<"_transitions="<<transitions[mode]<<'\n'
            <<names[mode]<<"_historical_positions="<<historical_positions[mode]<<'\n';
        for(std::size_t i=0;i<3;++i) std::cout<<"iteration_"<<i<<'_'<<names[mode]<<"_seconds="<<times[i][mode]<<'\n'
            <<"phase_"<<i<<'_'<<names[mode]<<"_initialize_seconds="<<initialization[i][mode]<<'\n'
            <<"phase_"<<i<<'_'<<names[mode]<<"_catch_up_seconds="<<catch_up[i][mode]<<'\n';
    }
}
