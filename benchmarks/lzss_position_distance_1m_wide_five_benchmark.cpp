#include "position_distance_1m_wide_five.hpp"
#include "position_distance_1m_five_prefix.hpp"
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
constexpr std::array names{"bounded_five","original","wide16","wide18","wide20"};

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
        marc::benchmark::WideFiveFinder<Bits> finder(n);finder.reset(raw);
        LzssExhaustiveMatchFinder reference(raw,parameters);
        for(std::size_t p=0;p<n;++p) {
            if(finder.find_match(p)!=reference.find_match(p)) return false;
            finder.advance(p,p+1);
        }
    }
    for(std::size_t distance:{65535U,65536U,65537U,1048570U}) {
        std::vector<std::byte> raw(distance+6);
        for(std::size_t i=0;i<6;++i) raw[i]=raw[distance+i]=std::byte(0xa1+i);
        marc::benchmark::WideFiveFinder<Bits> finder(raw.size());finder.reset(raw);
        finder.advance(0,distance);
        if(finder.find_match(distance)!=LzssMatch{static_cast<std::uint32_t>(distance),6}) return false;
    }
    const std::array raw{std::byte{'a'},std::byte{'b'},std::byte{'c'},std::byte{'d'},std::byte{'e'},std::byte{'f'},std::byte{'X'},
        std::byte{'a'},std::byte{'b'},std::byte{'c'},std::byte{'d'},std::byte{'e'},std::byte{'f'},std::byte{'Y'},
        std::byte{'a'},std::byte{'b'},std::byte{'c'},std::byte{'d'},std::byte{'e'},std::byte{'f'},std::byte{'Z'}};
    marc::benchmark::WideFiveFinder<Bits> finder(raw.size());finder.reset(raw);finder.advance(0,14);
    if(finder.find_match(14)!=LzssMatch{7,6}) return false;
    std::array<std::byte,22> oversized{};
    if(finder.reset(oversized) || finder.find_match(14)!=LzssMatch{7,6}) return false;
    return finder.reset({}) && finder.find_match(0)==LzssMatch{};

}
int main(int argc,char** argv) {
    const bool verify_only=argc==3 && std::string_view(argv[2])=="--verify-only";
    if(argc!=2 && !verify_only) return 2;
    std::ifstream file(argv[1],std::ios::binary|std::ios::ate);if(!file) return 2;
    const auto size=file.tellg();if(size<=0 || size>64*1024*1024) return 2;
    std::vector<std::byte> input(static_cast<std::size_t>(size));file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(input.data()),size)) return 2;
    if(!check_boundaries<16>() || !check_boundaries<18>() || !check_boundaries<20>()) return 1;
    constexpr std::size_t capacity=1048576;
    const LzssParameters parameters{capacity,3,258,0};const marc::core::DecoderLimits limits{};
    const auto q=calculate_lzss_position_distance_1m_five_prefix_workspace(capacity,parameters,limits);
    if(q.error!=LzssShortPrefixError::none) return 1;
    std::vector<std::uint32_t> storage(q.workspace_size/4);const auto workspace=std::as_writable_bytes(std::span{storage});
    std::vector<LzssTypedToken> tokens(capacity),actual(capacity);
    std::vector<marc::context::internal::ModeledOperation> operations(2*capacity);
    std::vector<std::byte> frame_a(18*capacity+85),frame_b(frame_a.size()),restored(capacity);
    SharedFivePrefixFinder original(capacity);
    WideFiveFinder<16> w16(capacity);WideFiveFinder<18> w18(capacity);WideFiveFinder<20> w20(capacity);
    WideFiveFinder<16,true> c16(capacity);WideFiveFinder<18,true> c18(capacity);WideFiveFinder<20,true> c20(capacity);
    std::array<std::array<double,5>,3> times{};
    std::array<std::array<double,3>,3> initialization{};
    std::array<WideFiveCounts,3> totals{};
    std::uint64_t total_tokens{},frame_bytes{};
    for(std::size_t offset=0;offset<input.size();offset+=capacity) {
        const auto raw=std::span{input}.subspan(offset,std::min(capacity,input.size()-offset));
        const auto oracle=tokenize_lzss_position_distance_1m_candidate(raw,parameters,limits,3,
            LzssPositionDistance1mSearch::indexed,tokens,workspace);
        if(oracle.error!=LzssShortMatchCandidateError::none) return 1;
        const auto expected=std::span{tokens}.first(oracle.token_count);total_tokens+=expected.size();
        const auto run=[&](auto& finder) {if(!finder.reset(raw)) return actual.size()+1;return replay(finder,raw,actual);};
        for(int iteration=-1;iteration<(verify_only?0:3);++iteration) for(std::size_t order=0;order<names.size();++order) {
            const auto mode=(order+static_cast<std::size_t>(iteration+1)+offset/capacity)%names.size();
            const auto start=Clock::now();std::size_t count{};
            if(mode==0) {
                LzssPositionDistance1mFivePrefixFinder finder;
                if(initialize_lzss_position_distance_1m_five_prefix_finder(raw,parameters,limits,workspace,finder)!=LzssShortPrefixError::none) return 1;
                count=replay(finder,raw,actual);
            } else if(mode==1) {original.reset(raw);count=replay(original,raw,actual);}
            else if(mode==2) count=run(w16);
            else if(mode==3) count=run(w18);
            else count=run(w20);
            const auto elapsed=std::chrono::duration<double>(Clock::now()-start).count();
            if(count>actual.size() || !same(expected,std::span{actual}.first(count))) return 1;
            if(iteration>=0) times[iteration][mode]+=elapsed;
        }
        // Separate warmed-array initialization diagnostic, not subtractable from replay.
        if(!verify_only) for(std::size_t i=0;i<3;++i) for(std::size_t order=0;order<3;++order) {
            const auto mode=(order+i+offset/capacity)%3;
            const auto start=Clock::now();bool ok{};
            if(mode==0) ok=w16.reset(raw);else if(mode==1) ok=w18.reset(raw);else ok=w20.reset(raw);
            const auto elapsed=std::chrono::duration<double>(Clock::now()-start).count();
            if(!ok) return 1;initialization[i][mode]+=elapsed;
        }
        const auto counted=[&](auto& finder) {const auto count=run(finder);return count==expected.size() && same(expected,std::span{actual}.first(count));};
        if(!counted(c16) || !counted(c18) || !counted(c20)) return 1;
        const std::array counts{c16.counts(),c18.counts(),c20.counts()};
        for(std::size_t i=0;i<3;++i) {
            const auto c=counts[i];
            if(c.visits!=c.exact+c.collisions || (i && (c.visits>counts[i-1].visits || c.exact!=counts[0].exact))) return 1;
            totals[i].visits+=c.visits;totals[i].exact+=c.exact;totals[i].collisions+=c.collisions;
        }
        std::cout<<"frame_"<<offset/capacity<<"_size="<<raw.size()<<'\n';
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
        <<"\nverified_iterations="<<(verify_only?0:3)<<"\nframe_identity=1\nwide_verified=1\n";
    for(std::size_t mode=0;mode<names.size();++mode) {
        for(std::size_t i=0;i<3;++i) std::cout<<"iteration_"<<i<<'_'<<names[mode]<<"_seconds="<<times[i][mode]<<'\n';
        if(mode>=2) {
            const auto c=totals[mode-2];
            std::cout<<names[mode]<<"_visits="<<c.visits<<'\n'<<names[mode]<<"_exact="<<c.exact<<'\n'<<names[mode]<<"_collisions="<<c.collisions<<'\n';
            std::cout<<names[mode]<<"_array_bytes="<<4*(2*65536+(std::size_t{1}<<(16+2*(mode-2)))+3*capacity)<<'\n';
            for(std::size_t i=0;i<3;++i) std::cout<<"initialize_"<<i<<'_'<<names[mode]<<"_seconds="<<initialization[i][mode-2]<<'\n';
        }
    }
}
