#include "position_distance_1m_five_prefix.hpp"
#include "position_distance_1m_six_prefix.hpp"
#include "dictionary/lzss_position_distance_1m_five_prefix_finder.hpp"
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
bool check_boundaries() {
    const LzssParameters parameters{1048576,3,258,0};
    for(std::size_t n:{0U,1U,2U,3U,4U,5U,6U,7U,16U,257U,258U,259U,513U})
    for(unsigned pattern=0;pattern<3;++pattern) {
        std::vector<std::byte> raw(n);std::uint32_t state=719;
        for(std::size_t i=0;i<n;++i) {
            state=state*1664525U+1013904223U;
            raw[i]=pattern==0?std::byte{0}:pattern==1?std::byte(i%7):std::byte(state>>24);
        }
        marc::benchmark::SixPrefixFinder finder(n);finder.reset(raw);
        LzssExhaustiveMatchFinder reference(raw,parameters);
        for(std::size_t p=0;p<n;++p) {
            if(finder.find_match(p)!=reference.find_match(p)) return false;
            finder.advance(p,p+1);
        }
    }
    for(std::size_t distance:{65535U,65536U,65537U,1048570U}) {
        std::vector<std::byte> raw(distance+6);
        for(std::size_t i=0;i<6;++i) raw[i]=raw[distance+i]=std::byte(0xa1+i);
        marc::benchmark::SixPrefixFinder finder(raw.size());finder.reset(raw);
        finder.advance(0,distance);
        if(finder.find_match(distance)!=LzssMatch{static_cast<std::uint32_t>(distance),6}) return false;
    }
    const std::array raw{std::byte{'a'},std::byte{'b'},std::byte{'c'},std::byte{'d'},std::byte{'e'},std::byte{'f'},std::byte{'X'},
        std::byte{'a'},std::byte{'b'},std::byte{'c'},std::byte{'d'},std::byte{'e'},std::byte{'f'},std::byte{'Y'},
        std::byte{'a'},std::byte{'b'},std::byte{'c'},std::byte{'d'},std::byte{'e'},std::byte{'f'},std::byte{'Z'}};
    marc::benchmark::SixPrefixFinder finder(raw.size());finder.reset(raw);finder.advance(0,14);
    return finder.find_match(14)==LzssMatch{7,6};
}
int main(int argc,char** argv) {
    if(argc!=2) return 2;
    std::ifstream file(argv[1],std::ios::binary|std::ios::ate);if(!file) return 2;
    const auto size=file.tellg();if(size<=0 || size>64*1024*1024) return 2;
    std::vector<std::byte> input(static_cast<std::size_t>(size));file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(input.data()),size)) return 2;
    if(!check_boundaries()) return 1;
    using Clock=std::chrono::steady_clock;
    constexpr std::size_t frame_size=1048576;
    const LzssParameters parameters{frame_size,3,258,0};
    const marc::core::DecoderLimits limits{};
    const auto needed=calculate_lzss_position_distance_1m_five_prefix_workspace(frame_size,parameters,limits);
    if(needed.error!=LzssShortPrefixError::none) return 1;
    std::vector<std::max_align_t> storage((needed.workspace_size+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
    const auto workspace=std::as_writable_bytes(std::span{storage}).first(needed.workspace_size);
    std::vector<LzssTypedToken> oracle(frame_size),baseline(frame_size),prototype(frame_size);
    marc::benchmark::SharedFivePrefixFinder five(frame_size);
    marc::benchmark::SixPrefixFinder shared(frame_size);
    std::vector<marc::context::internal::ModeledOperation> operations(2*frame_size);
    std::vector<std::byte> frame_a(18*frame_size+85),frame_b(frame_a.size()),restored(frame_size);
    std::array<std::array<double,3>,3> times{};
    std::uint64_t token_count{},frame_bytes{};
    for(std::size_t offset=0;offset<input.size();offset+=frame_size) {
        const auto raw=std::span{input}.subspan(offset,std::min(frame_size,input.size()-offset));
        const auto parsed=tokenize_lzss_position_distance_1m_candidate(raw,parameters,limits,3,
            LzssPositionDistance1mSearch::indexed,oracle,workspace);
        if(parsed.error!=LzssShortMatchCandidateError::none) return 1;
        const auto expected=std::span{oracle}.first(parsed.token_count);token_count+=expected.size();
        for(int iteration=-1;iteration<3;++iteration) {
            for(int order=0;order<3;++order) {
                const auto which=(order+iteration+1+(offset/frame_size)%3)%3;
                std::size_t count{};auto output=which?std::span{prototype}:std::span{baseline};
                const auto begin=Clock::now();
                if(which==2) {shared.reset(raw);count=replay(shared,raw,output);}
                else if(which==1) {five.reset(raw);count=replay(five,raw,output);}
                else {
                    LzssPositionDistance1mFivePrefixFinder finder;
                    if(initialize_lzss_position_distance_1m_five_prefix_finder(raw,parameters,limits,workspace,finder)!=LzssShortPrefixError::none) return 1;
                    count=replay(finder,raw,output);
                }
                const auto elapsed=std::chrono::duration<double>(Clock::now()-begin).count();
                if(!same(expected,output.first(count))) return 1;
                if(iteration>=0) times[iteration][which]+=elapsed;
            }
        }
        shared.reset(raw);
        if(replay(shared,raw,std::span{prototype})!=expected.size() || !same(expected,std::span{prototype}.first(expected.size()))) return 1;
        using namespace marc::frame::internal;
        const TypedContextStreamHeader stream{frame_size,input.size(),parameters,32768,44,9,1,10};
        const auto a=encode_lzss_position_distance_1m_frame(stream,limits,offset/frame_size,offset,expected,operations,frame_a);
        const auto b=encode_lzss_position_distance_1m_frame(stream,limits,offset/frame_size,offset,std::span{prototype}.first(expected.size()),operations,frame_b);
        if(a.error!=LzssShortMatchFrameEncodeError::none || b.error!=LzssShortMatchFrameEncodeError::none
            || a.serialized_size!=b.serialized_size || !std::equal(frame_a.begin(),frame_a.begin()+a.serialized_size,frame_b.begin())) return 1;
        const auto decoded=decode_lzss_position_distance_1m_frame_scratch(
            std::span{frame_b}.first(b.serialized_size),{stream,limits,offset/frame_size,offset},baseline,restored);
        if(decoded.error!=LzssShortMatchFrameDecodeError::none
            || !std::equal(raw.begin(),raw.end(),restored.begin())) return 1;
        frame_bytes+=a.serialized_size;

    }
    std::cout<<std::setprecision(12)<<"input_bytes="<<input.size()<<"\ntokens="<<token_count
        <<"\nframe_bytes="<<frame_bytes<<"\nverified_iterations=3\nframe_identity=1\n"
        <<"additional_array_bytes=4456448\n";
    const std::array names{"baseline","vector_five","six"};
    for(std::size_t i=0;i<times.size();++i) for(std::size_t j=0;j<names.size();++j)
        std::cout<<"iteration_"<<i<<'_'<<names[j]<<"_seconds="<<times[i][j]<<'\n';

}
