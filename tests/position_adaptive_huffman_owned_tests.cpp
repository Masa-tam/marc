#include "frame/lzss_position_adaptive_huffman_owned.hpp"
#include "dictionary/lzss_short_length_escape_candidate.hpp"
#include <algorithm>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace marc::frame::internal;
using namespace marc::entropy::internal;
using Code=marc::core::ErrorCode;using S=marc::core::StreamStatus;
constexpr auto profile=PositionAdaptiveHuffmanProfile::window_64k;
constexpr auto end=marc::core::flag_value(marc::core::ProcessFlags::end_input);
void check(bool b,const std::source_location p=std::source_location::current()) {
    if (!b) throw std::runtime_error("native FGK owner check failed at line "+std::to_string(p.line()));
}
std::vector<std::byte> drive(marc::core::Transform& codec,const std::vector<std::byte>& input,
    std::size_t ic,std::size_t oc,bool late_end=false) {
    std::vector<std::byte> result;std::size_t used{},offered{};
    for (std::size_t calls=0;calls<200000;++calls) {
        if (used==offered) offered=std::min(input.size(),offered+ic);
        const auto in=std::span(input).subspan(used,offered-used);
        std::vector<std::byte> out(oc,std::byte{0x7a});
        const auto flags=offered==input.size() && (!late_end || in.empty()) ? end : 0;
        const auto r=codec.process(in,out,flags);
        check(marc::core::is_valid(r,in.size(),out.size()));check(r.status!=S::error);
        used+=r.input_consumed;result.insert(result.end(),out.begin(),out.begin()+r.output_produced);
        check(std::all_of(out.begin()+r.output_produced,out.end(),[](auto b){return b==std::byte{0x7a};}));
        if (r.status==S::end_of_stream) {
            check(used==input.size());const auto done=codec.process({},out,0);
            check(done.status==S::end_of_stream && !done.input_consumed && !done.output_produced);return result;
        }
    }
    check(false);return {};
}
std::vector<std::byte> encode(const std::vector<std::byte>& raw,std::uint32_t frame,std::size_t ic,std::size_t oc,bool late=false) {
    PositionAdaptiveHuffmanStreamHeader h{};h.frame_size=frame;h.original_size=raw.size();Code error{};
    auto p=PositionAdaptiveHuffmanOwnedEncoder::create(h,{},error);check(p && error==Code::none);
    return drive(*p,raw,ic,oc,late);
}
void roundtrip(const std::vector<std::byte>& raw,std::uint32_t frame,bool chunk_grid) {
    const auto bytes=encode(raw,frame,131072,131072);
    PositionAdaptiveHuffmanStreamHeader h{};std::size_t consumed{};
    check(parse_position_adaptive_huffman_stream(profile,bytes,{},h,consumed)==PositionAdaptiveHuffmanFormatError::none);
    check(h.original_size==raw.size() && h.frame_size==frame && h.dictionary.window_size==65536);
    for (const auto ic:chunk_grid ? std::vector<std::size_t>{1,7,131072} : std::vector<std::size_t>{131072})
        for (const auto oc:chunk_grid ? std::vector<std::size_t>{1,19,131072} : std::vector<std::size_t>{131072}) {
            check(encode(raw,frame,ic,oc,true)==bytes);
            Code error{};auto p=PositionAdaptiveHuffmanOwnedDecoder::create(profile,frame,{},error);
            check(p && error==Code::none && drive(*p,bytes,ic,oc,true)==raw);
        }
}
void admission() {
    PositionAdaptiveHuffmanStreamHeader h{};h.frame_size=256;h.original_size=257;
    marc::core::DecoderLimits limits;limits.max_block_size=65536;PositionAdaptiveHuffmanWorkspace w{};
    check(PositionAdaptiveHuffmanOwnedEncoder::requirements(h,limits,w)==Code::none);
    limits.max_internal_buffered_bytes=w.aggregate_bytes;Code error{};
    auto p=PositionAdaptiveHuffmanOwnedEncoder::create(h,limits,error);check(p && error==Code::none);
    std::vector<std::byte> raw(257,std::byte{'A'});check(!drive(*p,raw,4096,4096).empty());
    --limits.max_internal_buffered_bytes;auto sentinel=w;sentinel.aggregate_bytes=17;
    check(PositionAdaptiveHuffmanOwnedEncoder::requirements(h,limits,sentinel)==Code::limit_exceeded && sentinel.aggregate_bytes==17);
    check(!PositionAdaptiveHuffmanOwnedEncoder::create(h,limits,error) && error==Code::limit_exceeded);
    limits={};limits.max_block_size=65536;
    check(PositionAdaptiveHuffmanOwnedDecoder::requirements(profile,256,limits,w)==Code::none);
    limits.max_internal_buffered_bytes=w.aggregate_bytes;
    auto d=PositionAdaptiveHuffmanOwnedDecoder::create(profile,256,limits,error);check(d && error==Code::none);
    auto bytes=encode(raw,256,4096,4096);check(drive(*d,bytes,4096,4096)==raw);
    --limits.max_internal_buffered_bytes;check(!PositionAdaptiveHuffmanOwnedDecoder::create(profile,256,limits,error) && error==Code::limit_exceeded);
    h.profile=PositionAdaptiveHuffmanProfile::window_32m;limits={};
    check(PositionAdaptiveHuffmanOwnedEncoder::requirements(h,limits,w)==Code::unsupported);
    h.profile=profile;h.original_size=5;h.frame_size=5;
    auto e=PositionAdaptiveHuffmanOwnedEncoder::create(h,{},error);check(static_cast<bool>(e));
    std::vector<std::byte> out(128,std::byte{0x61}),short_input(4);
    const auto r=e->process(short_input,out,end);check(r.status==S::error && !r.input_consumed && !r.output_produced
        && out==std::vector<std::byte>(128,std::byte{0x61}));
    limits={};limits.max_compressed_payload_size=1;
    e=PositionAdaptiveHuffmanOwnedEncoder::create(h,limits,error);check(static_cast<bool>(e));
    std::vector<std::byte> input(5,std::byte{'A'});
    const auto header=e->process(input,out,end);check(header.status==S::progress && header.output_produced==112 && !header.input_consumed);
    std::fill(out.begin(),out.end(),std::byte{0x61});
    const auto failure=e->process(input,out,end);check(failure.status==S::error && !failure.output_produced
        && out==std::vector<std::byte>(128,std::byte{0x61}));
    const auto stable=e->process({},out,0);check(stable.status==S::error && stable.error.code==failure.error.code);
}
void reference_parse() {
    const std::vector<std::byte> raw{std::byte{'A'},std::byte{'B'},std::byte{'A'},std::byte{'B'},std::byte{'A'},
        std::byte{'B'},std::byte{'A'},std::byte{0},std::byte{'A'},std::byte{'B'},std::byte{'A'}};
    PositionAdaptiveHuffmanStreamHeader h{};h.original_size=raw.size();h.frame_size=64;
    marc::core::DecoderLimits limits;
    std::vector<marc::dictionary::internal::LzssTypedToken> tokens(raw.size());
    const auto parsed=marc::dictionary::internal::tokenize_lzss_short_length_escape_candidate(raw,h.dictionary,limits,5,tokens);
    check(parsed.error==marc::dictionary::internal::LzssShortMatchCandidateError::none);
    std::vector<AdaptiveHuffmanNode> nodes(9188);std::vector<std::uint16_t> symbols(4570);
    std::vector<std::byte> payload(4096),private_bytes(8192),frame(8192),header(112);
    const auto result=encode_position_adaptive_huffman_frame(std::span(tokens).first(parsed.token_count),{h,limits,0,0},
        nodes,symbols,payload,private_bytes,frame);check(result.error==PositionAdaptiveHuffmanFrameError::none);
    check(serialize_position_adaptive_huffman_stream(h,limits,header)==PositionAdaptiveHuffmanFormatError::none);
    header.insert(header.end(),frame.begin(),frame.begin()+result.serialized_size);
    check(encode(raw,64,1,1)==header);
}
void directed() {
    roundtrip({},5,true);roundtrip({std::byte{0}},5,true);roundtrip({std::byte{255}},5,true);
    std::vector<std::byte> all;for (unsigned i=0;i<256;++i) all.push_back(static_cast<std::byte>(i));
    roundtrip(all,17,true);
    for (const auto length:{65535,65536,65537}) {
        std::vector<std::byte> raw(length,std::byte{'A'});roundtrip(raw,65536,false);
    }
    std::vector<std::byte> mixed(1025);std::uint32_t state=17;
    for (auto& b:mixed) {state=state*1664525u+1013904223u;b=static_cast<std::byte>(state>>24);}
    roundtrip(mixed,256,true);admission();reference_parse();
}
}
int main() {try {directed();std::cout<<"native FGK owned checks passed\n";
    PositionAdaptiveHuffmanWorkspace e{},d{};PositionAdaptiveHuffmanStreamHeader h{};
    check(PositionAdaptiveHuffmanOwnedEncoder::requirements(h,{},e)==Code::none);
    check(PositionAdaptiveHuffmanOwnedDecoder::requirements(profile,65536,{},d)==Code::none);
    std::cout<<"64KiB logical owned requirements encode="<<e.aggregate_bytes<<" decode="<<d.aggregate_bytes<<'\n';return 0;}
    catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
