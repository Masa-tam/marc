#include "frame/lzss_position_distance_1m_frame.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <tuple>

namespace {
using namespace marc::frame::internal;
using namespace marc::dictionary::internal;
constexpr std::byte sentinel{0xcc};

void compare(std::span<const std::byte> input,const TypedContextStreamHeader& stream,
    const marc::core::DecoderLimits& limits,std::size_t token_capacity,std::size_t raw_capacity) {
    std::array<LzssTypedToken,131> a{},b{};
    for(auto& t:a) t.literal=0xcc;
    b=a;
    std::array<std::byte,387> x{},y{};x.fill(sentinel);y=x;
    const auto r=decode_lzss_position_distance_1m_frame(input,{stream,limits},
        std::span{a}.subspan(1,token_capacity),std::span{x}.subspan(1,raw_capacity));
    const auto q=decode_lzss_position_distance_1m_frame_scratch(input,{stream,limits},
        std::span{b}.subspan(1,token_capacity),std::span{y}.subspan(1,raw_capacity));
    const auto fields=[](const auto& v) {
        return std::tuple{v.error,v.preflight_error,v.serialized_consumed,v.required_token_count,v.required_raw_size,
            v.token_decode.error,v.token_decode.token_error,v.token_decode.token_count,v.token_decode.token_index,
            v.token_decode.raw_size,v.token_decode.entropy.error,v.token_decode.entropy.payload_consumed,
            v.token_decode.entropy.event_count,v.token_decode.entropy.decision_count,v.reconstruction.error};
    };
    if(fields(r)!=fields(q) || x!=y || x.front()!=sentinel || x.back()!=sentinel
        || a.front().literal!=0xcc || a.back().literal!=0xcc || b.front().literal!=0xcc || b.back().literal!=0xcc) std::abort();
    if(r.error!=LzssShortMatchFrameDecodeError::none) {
        if(r.serialized_consumed || !std::ranges::all_of(x,[](auto v){return v==sentinel;})
            || !std::ranges::all_of(a,[](auto t){return t.literal==0xcc;})) std::abort();
    } else {
        for(std::size_t i=0;i<a.size();++i) if(a[i].kind!=b[i].kind || a[i].literal!=b[i].literal
            || a[i].distance!=b[i].distance || a[i].length!=b[i].length) std::abort();
    }
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,std::size_t size) {
    if(size>8192) return 0;
    const auto input=std::as_bytes(std::span{data,size});
    marc::core::DecoderLimits limits{};
    limits.max_frame_size=limits.max_block_size=384;
    limits.max_total_output_size=768;limits.max_internal_buffered_bytes=1<<20;
    TypedContextStreamHeader stream{};
    stream.frame_size=384;stream.original_size=1;stream.dictionary={1048576,3,258,0};
    stream.dictionary_variant=9;stream.context_variant=10;stream.context_count=44;stream.range_model_total=32768;
    std::array<LzssTypedToken,129> tokens{};
    const auto count=std::min<std::size_t>(size,128);
    std::size_t raw=0;
    for(std::size_t i=0;i<count;++i) {
        if(i && (data[i]&1)) {tokens[i]={LzssTypedTokenKind::match,0,1,3};raw+=3;}
        else {tokens[i]={LzssTypedTokenKind::literal,data[i],0,0};++raw;}
    }
    if(!count) {tokens[0]={LzssTypedTokenKind::literal,0,0,0};raw=1;}
    const auto used=std::max<std::size_t>(count,1);
    stream.original_size=raw;
    compare(input,stream,limits,used,raw);
    std::array<marc::context::internal::ModeledOperation,645> ops{};
    std::array<std::byte,6997> bytes{};
    const auto encoded=encode_lzss_position_distance_1m_frame(stream,limits,0,0,std::span{tokens}.first(used),ops,bytes);
    if(encoded.error!=LzssShortMatchFrameEncodeError::none) std::abort();
    auto frame=std::span{bytes}.first(encoded.serialized_size);
    compare(frame,stream,limits,used,raw);
    if(size) {
        const auto position=(static_cast<std::size_t>(data[0])*257+data[size-1])%frame.size();
        bytes[position]^=static_cast<std::byte>(data[size/2]|1);
        compare(frame,stream,limits,used,raw);
        compare(frame.first(data[0]%frame.size()),stream,limits,used,raw);
        compare(frame,stream,limits,used-1,raw-1);
    }
    return 0;
}
