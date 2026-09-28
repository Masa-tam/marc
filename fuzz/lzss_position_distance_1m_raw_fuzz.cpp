#include "frame/lzss_position_distance_1m_raw_frame_encoder.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,std::size_t size) {
    using namespace marc::dictionary::internal;
    using namespace marc::frame::internal;
    using Search=LzssPositionDistance1mSearch;
    if(size>256) return 0;
    const auto raw=std::as_bytes(std::span{data,size});
    const LzssParameters parameters{size?1U+static_cast<std::uint32_t>(data[0])*4096U:1048576U,3,
        size?3U+static_cast<std::uint32_t>(data[size-1]):258U,0};
    const unsigned eligibility=size?3U+data[0]%3:3U;
    const auto needed=calculate_lzss_position_distance_1m_match_workspace(size,parameters,{});
    if(needed.error!=LzssShortPrefixError::none) std::abort();
    std::vector<std::uint32_t> words(needed.workspace_size/4);
    const auto workspace=std::as_writable_bytes(std::span{words});
    std::array<LzssTypedToken,257> a{},b{};
    const LzssTypedToken sentinel{LzssTypedTokenKind::literal,0xcc,77,99};
    a.fill(sentinel);b=a;
    const auto same=[](const auto& x,const auto& y){return x.kind==y.kind && x.literal==y.literal && x.distance==y.distance && x.length==y.length;};
    const auto r=tokenize_lzss_position_distance_1m_candidate(raw,parameters,{},eligibility,Search::exhaustive,std::span{a}.first(size),{});
    const auto q=tokenize_lzss_position_distance_1m_candidate(raw,parameters,{},eligibility,Search::indexed,std::span{b}.first(size),workspace);
    if(r.error!=LzssShortMatchCandidateError::none || q.error!=r.error || r.token_count!=q.token_count
        || !std::equal(a.begin(),a.end(),b.begin(),same)) std::abort();
    if(!size) return 0;
    // Also query a position that need not lie on the greedy token path.
    const std::size_t position=data[size/2]%size;
    LzssPositionDistance1mMatchFinder finder;
    if(initialize_lzss_position_distance_1m_match_finder(raw,parameters,{},workspace,finder)!=LzssShortPrefixError::none) std::abort();
    finder.advance(0,position);
    LzssExhaustiveMatchFinder exhaustive(raw,parameters);
    if(finder.find_match(position)!=exhaustive.find_match(position)
        || finder.find_match_reference(position)!=exhaustive.find_match(position)) std::abort();
    const TypedContextStreamHeader stream{256,size,parameters,32768,44,9,1,10};
    std::array<marc::context::internal::ModeledOperation,512> ops{};
    std::array<std::byte,4693> x{},y{};
    const auto e=encode_lzss_position_distance_1m_raw_frame(stream,{},0,0,raw,eligibility,Search::exhaustive,a,ops,{},x);
    const auto f=encode_lzss_position_distance_1m_raw_frame(stream,{},0,0,raw,eligibility,Search::indexed,b,ops,workspace,y);
    if(e.error!=LzssPositionDistanceRawFrameError::none || f.error!=e.error
        || e.frame.serialized_size!=f.frame.serialized_size || x!=y) std::abort();
    std::array<std::byte,256> decoded{};
    const auto d=decode_lzss_position_distance_1m_frame_scratch(std::span{y}.first(f.frame.serialized_size),{stream,{}},b,decoded);
    if(d.error!=LzssShortMatchFrameDecodeError::none || !std::equal(raw.begin(),raw.end(),decoded.begin())) std::abort();
    const auto capacity=data[0]%(size+1);
    a.fill(sentinel);b=a;
    const auto u=tokenize_lzss_position_distance_1m_candidate(raw,parameters,{},eligibility,Search::exhaustive,std::span{a}.first(capacity),{});
    const auto v=tokenize_lzss_position_distance_1m_candidate(raw,parameters,{},eligibility,Search::indexed,std::span{b}.first(capacity),workspace);
    if(u.error!=v.error || u.token_count!=v.token_count || !std::equal(a.begin(),a.end(),b.begin(),same)) std::abort();
    if(u.error!=LzssShortMatchCandidateError::none && !std::ranges::all_of(a,[&](auto token){return same(token,sentinel);})) std::abort();
    return 0;
}
