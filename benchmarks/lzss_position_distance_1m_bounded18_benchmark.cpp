#include "dictionary/lzss_position_distance_1m_bounded18_finder.hpp"
#include "dictionary/lzss_position_distance_1m_five_prefix_finder.hpp"
#include "dictionary/lzss_position_distance_1m_candidate.hpp"
#include "frame/lzss_position_distance_1m_frame.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <memory>
using namespace marc::dictionary::internal;
using Clock=std::chrono::steady_clock;

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
using Base=LzssPositionDistance1mFivePrefixFinder;
using Wide=LzssPositionDistance1mBounded18Finder;
LzssShortPrefixError initialize(std::span<const std::byte> raw,const LzssParameters& p,
    const marc::core::DecoderLimits& l,std::span<std::byte> w,Base& f) {
    return initialize_lzss_position_distance_1m_five_prefix_finder(raw,p,l,w,f);
}
LzssShortPrefixError initialize(std::span<const std::byte> raw,const LzssParameters& p,
    const marc::core::DecoderLimits& l,std::span<std::byte> w,Wide& f) {
    return initialize_lzss_position_distance_1m_bounded18_finder(raw,p,l,w,f);
}
struct Sample { std::size_t count{}; double seconds{}; };
// Duplicate baseline instances share the same measured specialization.
template<class Finder>
#if defined(_MSC_VER)
__declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
Sample measured_replay(Finder& finder, std::span<const std::byte> raw,
                       std::span<LzssTypedToken> output,const LzssParameters& parameters,
                       const marc::core::DecoderLimits& limits,std::span<std::byte> workspace) {
    const auto start=Clock::now();
    const auto count=initialize(raw,parameters,limits,workspace,finder)==LzssShortPrefixError::none ? replay(finder,raw,output) : output.size()+1;
    const auto elapsed=std::chrono::duration<double>(Clock::now()-start).count();
    return {count,elapsed};
}
int main(int argc,char** argv) {
    const bool verify_only=argc==4 && std::string_view(argv[3])=="--verify-only";
    if(argc!=3 && !verify_only) return 2;
    const auto valid=[](std::string_view s) {return s=="forward" || s=="reverse";};
    if(!valid(argv[2])) return 2;
    const bool reverse_execution=std::string_view(argv[2])=="reverse";
    std::ifstream file(argv[1],std::ios::binary|std::ios::ate);if(!file) return 2;
    const auto size=file.tellg();if(size<=0 || size>64*1024*1024) return 2;
    std::vector<std::byte> input(static_cast<std::size_t>(size));file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(input.data()),size)) return 2;
    constexpr std::size_t capacity=1048576;
    const LzssParameters parameters{capacity,3,258,0};
    const marc::core::DecoderLimits limits{};
    const auto q=calculate_lzss_position_distance_1m_bounded18_workspace(capacity,parameters,limits);
    if(q.error!=LzssShortPrefixError::none) return 1;
    std::vector<std::uint32_t> storage(q.workspace_size/4);
    const auto workspace=std::as_writable_bytes(std::span{storage});
    std::vector<LzssTypedToken> tokens(capacity),actual(capacity);
    std::vector<marc::context::internal::ModeledOperation> operations(2*capacity);
    std::vector<std::byte> frame_a(18*capacity+85),frame_b(frame_a.size()),restored(capacity);
    std::array<Base,2> base{};Wide wide;
    std::array<std::array<double,3>,3> totals{};
    std::uint64_t total_tokens{},frame_bytes{};
    std::cout<<std::setprecision(12);
    for(std::size_t offset=0;offset<input.size();offset+=capacity) {
        const auto frame=offset/capacity;
        const auto raw=std::span{input}.subspan(offset,std::min(capacity,input.size()-offset));
        const auto oracle=tokenize_lzss_position_distance_1m_candidate(raw,parameters,limits,3,
            LzssPositionDistance1mSearch::indexed,tokens,workspace);
        if(oracle.error!=LzssShortMatchCandidateError::none) return 1;
        const auto expected=std::span{tokens}.first(oracle.token_count);total_tokens+=expected.size();
        std::array<std::array<double,3>,3> seconds{};
        std::array<std::array<std::size_t,3>,3> ranks{};
        for(int iteration=-1;iteration<(verify_only?0:3);++iteration) {
            for(std::size_t rank=0;rank<3;++rank) {
                const auto rotated=(rank+static_cast<std::size_t>(iteration+1)+frame)%3;
                const auto slot=reverse_execution ? 2-rotated : rotated;
                const auto result=slot<2 ? measured_replay(base[slot],raw,actual,parameters,limits,workspace)
                    : measured_replay(wide,raw,actual,parameters,limits,workspace);
                if(result.count>actual.size() || !same(expected,std::span{actual}.first(result.count))) return 1;
                if(iteration>=0) {
                    seconds[iteration][slot]=result.seconds;
                    ranks[iteration][slot]=rank;
                    totals[iteration][slot]+=result.seconds;
                }
            }
        }
        // The last replay has matched the oracle, as have all other instances.
        using namespace marc::frame::internal;
        const TypedContextStreamHeader stream{capacity,input.size(),parameters,32768,44,9,1,10};
        const auto a=encode_lzss_position_distance_1m_frame(stream,limits,frame,offset,expected,operations,frame_a);
        const auto b=encode_lzss_position_distance_1m_frame(stream,limits,frame,offset,
            std::span{actual}.first(expected.size()),operations,frame_b);
        if(a.error!=LzssShortMatchFrameEncodeError::none || b.error!=LzssShortMatchFrameEncodeError::none
           || a.serialized_size!=b.serialized_size || !std::equal(frame_a.begin(),frame_a.begin()+a.serialized_size,frame_b.begin())) return 1;
        const auto decoded=decode_lzss_position_distance_1m_frame_scratch(std::span{frame_b}.first(b.serialized_size),
            {stream,limits,frame,offset},actual,restored);
        if(decoded.error!=LzssShortMatchFrameDecodeError::none || !std::equal(raw.begin(),raw.end(),restored.begin())) return 1;
        frame_bytes+=a.serialized_size;
        std::cout<<"frame_"<<frame<<"_size="<<raw.size()<<'\n';
        if(!verify_only) for(std::size_t i=0;i<3;++i) for(std::size_t slot=0;slot<3;++slot) {
            std::cout<<"frame_"<<frame<<"_iteration_"<<i<<"_slot_"<<slot<<"_seconds="<<seconds[i][slot]<<'\n'
                     <<"frame_"<<frame<<"_iteration_"<<i<<"_slot_"<<slot<<"_rank="<<ranks[i][slot]<<'\n';
        }
    }
    std::cout<<"input_bytes="<<input.size()<<"\ntokens="<<total_tokens<<"\nframe_bytes="<<frame_bytes
             <<"\nverified_iterations="<<(verify_only?0:3)<<"\nframe_identity=1\nbounded18_verified=1\n"
             <<"execution_reverse="<<reverse_execution
             <<"\nworkspace_bytes=14155776\n";
    for(std::size_t slot=0;slot<3;++slot) for(std::size_t i=0;i<3;++i)
        std::cout<<"iteration_"<<i<<"_slot_"<<slot<<"_seconds="<<totals[i][slot]<<'\n';
}
