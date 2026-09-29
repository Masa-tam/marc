#include "position_distance_1m_runtime_width.hpp"
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
template<unsigned Bits>
bool check_boundaries() {
    const LzssParameters parameters{1048576,3,258,0};
    for(std::size_t n:{0U,1U,2U,3U,4U,5U,6U,7U,16U,257U,258U,259U,513U})
    for(unsigned pattern=0;pattern<3;++pattern) {
        std::vector<std::byte> raw(n);std::uint32_t state=719;
        for(std::size_t i=0;i<n;++i) {
            state=state*1664525U+1013904223U;
            raw[i]=pattern==0?std::byte{0}:pattern==1?std::byte(i%7):std::byte(state>>24);
        }
        marc::benchmark::RuntimeWidthFinder finder(n);finder.reset(raw,Bits);
        LzssExhaustiveMatchFinder reference(raw,parameters);
        for(std::size_t p=0;p<n;++p) {
            if(finder.find_match(p)!=reference.find_match(p)) return false;
            finder.advance(p,p+1);
        }
    }
    for(std::size_t distance:{65535U,65536U,65537U,1048570U}) {
        std::vector<std::byte> raw(distance+6);
        for(std::size_t i=0;i<6;++i) raw[i]=raw[distance+i]=std::byte(0xa1+i);
        marc::benchmark::RuntimeWidthFinder finder(raw.size());finder.reset(raw,Bits);
        finder.advance(0,distance);
        if(finder.find_match(distance)!=LzssMatch{static_cast<std::uint32_t>(distance),6}) return false;
    }
    const std::array raw{std::byte{'a'},std::byte{'b'},std::byte{'c'},std::byte{'d'},std::byte{'e'},std::byte{'f'},std::byte{'X'},
        std::byte{'a'},std::byte{'b'},std::byte{'c'},std::byte{'d'},std::byte{'e'},std::byte{'f'},std::byte{'Y'},
        std::byte{'a'},std::byte{'b'},std::byte{'c'},std::byte{'d'},std::byte{'e'},std::byte{'f'},std::byte{'Z'}};
    marc::benchmark::RuntimeWidthFinder finder(raw.size());finder.reset(raw,Bits);finder.advance(0,14);
    if(finder.find_match(14)!=LzssMatch{7,6}) return false;
    std::array<std::byte,22> oversized{};
    if(finder.reset(oversized,Bits==16 ? 20 : 16) || finder.active_bits()!=Bits
       || finder.find_match(14)!=LzssMatch{7,6}) return false;
    for(unsigned invalid:{0U,15U,17U,19U,21U,32U})
        if(finder.reset(raw,invalid) || finder.active_bits()!=Bits || finder.find_match(14)!=LzssMatch{7,6}) return false;
    // Change width repeatedly on the same object, including empty resets.
    for(unsigned bits:{20U,16U,18U,16U,20U}) {
        if(!finder.reset({},bits) || finder.active_bits()!=bits || finder.find_match(0)!=LzssMatch{}) return false;
        if(!finder.reset(raw,bits) || !finder.advance(0,14) || finder.find_match(14)!=LzssMatch{7,6}) return false;
    }
    return true;

}

struct Sample { std::size_t count{}; double seconds{}; };
// One runtime code path and one storage instance for all logical slots.
#if defined(_MSC_VER)
__declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
Sample measured_replay(RuntimeWidthFinder& finder,unsigned bits, std::span<const std::byte> raw,
                       std::span<LzssTypedToken> output) {
    const auto start=Clock::now();
    const auto count=finder.reset(raw,bits) ? replay(finder,raw,output) : output.size()+1;
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
    if(!check_boundaries<16>() || !check_boundaries<18>() || !check_boundaries<20>()) return 1;
    constexpr std::size_t capacity=1048576;
    const LzssParameters parameters{capacity,3,258,0};
    const marc::core::DecoderLimits limits{};
    const auto q=calculate_lzss_position_distance_1m_five_prefix_workspace(capacity,parameters,limits);
    if(q.error!=LzssShortPrefixError::none) return 1;
    std::vector<std::uint32_t> storage(q.workspace_size/4);
    const auto workspace=std::as_writable_bytes(std::span{storage});
    std::vector<LzssTypedToken> tokens(capacity),actual(capacity);
    std::vector<marc::context::internal::ModeledOperation> operations(2*capacity);
    std::vector<std::byte> frame_a(18*capacity+85),frame_b(frame_a.size()),restored(capacity);
    RuntimeWidthFinder finder(capacity);
    constexpr std::array<unsigned,4> widths{16,16,18,20};
    std::array<std::array<double,4>,4> totals{};
    std::uint64_t total_tokens{},frame_bytes{};
    std::cout<<std::setprecision(12);
    for(std::size_t offset=0;offset<input.size();offset+=capacity) {
        const auto frame=offset/capacity;
        const auto raw=std::span{input}.subspan(offset,std::min(capacity,input.size()-offset));
        const auto oracle=tokenize_lzss_position_distance_1m_candidate(raw,parameters,limits,3,
            LzssPositionDistance1mSearch::indexed,tokens,workspace);
        if(oracle.error!=LzssShortMatchCandidateError::none) return 1;
        const auto expected=std::span{tokens}.first(oracle.token_count);total_tokens+=expected.size();
        std::array<std::array<double,4>,4> seconds{};
        std::array<std::array<std::size_t,4>,4> ranks{};
        for(int iteration=-1;iteration<(verify_only?0:4);++iteration) {
            for(std::size_t rank=0;rank<4;++rank) {
                const auto rotated=(rank+static_cast<std::size_t>(iteration+1)+frame)%4;
                const auto slot=reverse_execution ? 3-rotated : rotated;
                const auto result=measured_replay(finder,widths[slot],raw,actual);
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
        if(!verify_only) for(std::size_t i=0;i<4;++i) for(std::size_t slot=0;slot<4;++slot) {
            std::cout<<"frame_"<<frame<<"_iteration_"<<i<<"_slot_"<<slot<<"_seconds="<<seconds[i][slot]<<'\n'
                     <<"frame_"<<frame<<"_iteration_"<<i<<"_slot_"<<slot<<"_rank="<<ranks[i][slot]<<'\n';
        }
    }
    std::cout<<"input_bytes="<<input.size()<<"\ntokens="<<total_tokens<<"\nframe_bytes="<<frame_bytes
             <<"\nverified_iterations="<<(verify_only?0:4)<<"\nframe_identity=1\nruntime_width_verified=1\n"
             <<"execution_reverse="<<reverse_execution
             <<"\nslot_0_bits=16\nslot_1_bits=16\nslot_2_bits=18\nslot_3_bits=20\narray_bytes_total=17301504\n";
    for(std::size_t slot=0;slot<4;++slot) for(std::size_t i=0;i<4;++i)
        std::cout<<"iteration_"<<i<<"_slot_"<<slot<<"_seconds="<<totals[i][slot]<<'\n';
}
