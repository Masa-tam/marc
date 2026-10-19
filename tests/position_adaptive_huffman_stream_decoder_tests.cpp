#include "frame/lzss_position_adaptive_huffman_stream_decoder.hpp"
#include <algorithm>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace marc::frame::internal;
using namespace marc::entropy::internal;
using namespace marc::dictionary::internal;
using S=marc::core::StreamStatus;
constexpr auto profile=PositionAdaptiveHuffmanProfile::window_64k;
constexpr auto end=marc::core::flag_value(marc::core::ProcessFlags::end_input);
void check(bool b,const std::source_location p=std::source_location::current()) {
    if (!b) throw std::runtime_error("native FGK stream check failed at line "+std::to_string(p.line()));
}
struct Scratch {
    std::vector<std::byte> serialized=std::vector<std::byte>(4096),raw=std::vector<std::byte>(64);
    std::vector<LzssTypedToken> tokens=std::vector<LzssTypedToken>(64);
    std::vector<AdaptiveHuffmanNode> nodes=std::vector<AdaptiveHuffmanNode>(9188);
    std::vector<std::uint16_t> symbols=std::vector<std::uint16_t>(4570);
};
std::vector<std::byte> stream(std::size_t frames) {
    marc::core::DecoderLimits limits;
    PositionAdaptiveHuffmanStreamHeader h{};h.frame_size=5;h.original_size=frames*5;
    std::vector<std::byte> data(112);check(serialize_position_adaptive_huffman_stream(h,limits,data)==PositionAdaptiveHuffmanFormatError::none);
    const std::vector<LzssTypedToken> tokens{{LzssTypedTokenKind::literal,'A',0,0},
        {LzssTypedTokenKind::literal,'B',0,0},{LzssTypedTokenKind::match,0,2,3}};
    Scratch s;std::vector<std::byte> payload(512),staged(1024),out(1024);
    for (std::size_t i=0;i<frames;++i) {
        auto r=encode_position_adaptive_huffman_frame(tokens,{h,limits,i,i*5},s.nodes,s.symbols,payload,staged,out);
        check(r.error==PositionAdaptiveHuffmanFrameError::none);
        data.insert(data.end(),out.begin(),out.begin()+r.serialized_size);
    }
    return data;
}
struct Run { bool failed{};std::vector<std::byte> output;std::size_t consumed{}; };
Run run(const std::vector<std::byte>& input,std::size_t in_chunk,std::size_t out_chunk,bool final_with_input=true,
    std::size_t first_split=0) {
    Scratch work;PositionAdaptiveHuffmanStreamDecoder d(profile,{},work.serialized,work.tokens,work.raw,work.nodes,work.symbols);
    Run result;std::size_t offered{};bool ended{};
    for (std::size_t call=0;call<10000;++call) {
        if (offered==result.consumed) offered=std::min(input.size(),offered+(call==0 && first_split ? first_split : in_chunk));
        auto supplied=std::span(input).subspan(result.consumed,offered-result.consumed);
        const bool final=offered==input.size() && (final_with_input || supplied.empty());
        std::vector<std::byte> out(out_chunk,std::byte{0x6a});
        const auto r=d.process(supplied,out,final ? end : 0);
        check(marc::core::is_valid(r,supplied.size(),out.size()));
        result.consumed+=r.input_consumed;
        if (r.status==S::error) {
            check(!r.output_produced && std::all_of(out.begin(),out.end(),[](auto b){return b==std::byte{0x6a};}));
            const auto again=d.process({},out,0);check(again.status==S::error && again.error.code==r.error.code
                && again.error.byte_position==r.error.byte_position && !again.input_consumed && !again.output_produced);
            result.failed=true;return result;
        }
        result.output.insert(result.output.end(),out.begin(),out.begin()+r.output_produced);
        check(std::all_of(out.begin()+r.output_produced,out.end(),[](auto b){return b==std::byte{0x6a};}));
        if (r.status==S::end_of_stream) {ended=true;break;}
        if (r.status==S::need_input && offered==input.size()) check(!final);
    }
    check(ended);
    std::byte marker{0x72};const auto again=d.process({},std::span(&marker,1),0);
    check(again.status==S::end_of_stream && !again.input_consumed && !again.output_produced && marker==std::byte{0x72});
    return result;
}
void directed() {
    const std::vector<std::byte> expected{std::byte{'A'},std::byte{'B'},std::byte{'A'},std::byte{'B'},std::byte{'A'}};
    for (const auto frames:{0u,1u,2u}) {
        const auto data=stream(frames);std::vector<std::byte> raw;
        for (unsigned n=0;n<frames;++n) raw.insert(raw.end(),expected.begin(),expected.end());
        for (const auto in:{1u,2u,17u,112u,4096u}) for (const auto out:{1u,2u,7u,64u}) {
            auto r=run(data,in,out);check(!r.failed && r.consumed==data.size() && r.output==raw);
            r=run(data,in,out,false);check(!r.failed && r.output==raw);
        }
    }
    const auto one=stream(1);
    for (std::size_t split=1;split<one.size();++split) {
        const auto r=run(one,4096,1,true,split);check(!r.failed && r.output==expected);
    }
    for (std::size_t cut=0;cut<one.size();++cut) {
        std::vector<std::byte> truncated(one.begin(),one.begin()+cut);
        const auto r=run(truncated,4096,64);check(r.failed && r.output.empty());
    }
    auto bad=one;bad.back()|=std::byte{0x80};check(run(bad,1,1).failed);
    auto two=stream(2);two.back()|=std::byte{0x80};
    const auto late=run(two,4096,64);check(late.failed && late.output==expected);
    auto trailing=one;trailing.push_back(std::byte{1});const auto t=run(trailing,4096,64);
    check(t.failed && t.output==expected);
    Scratch work;PositionAdaptiveHuffmanStreamDecoder d(profile,{},work.serialized,work.tokens,work.raw,work.nodes,work.symbols);
    auto r=d.process(one,{},end);check(r.status==S::need_output && !r.output_produced && r.input_consumed==one.size());
    std::vector<std::byte> raw(5);r=d.process({},raw,0);check(r.output_produced==5 && raw==expected);
    r=d.process({},raw,0);check(r.status==S::end_of_stream);
    Scratch alias;PositionAdaptiveHuffmanStreamDecoder a(profile,{},alias.serialized,alias.tokens,alias.raw,alias.nodes,alias.symbols);
    r=a.process(one,alias.raw,0);check(r.status==S::error && !r.input_consumed && !r.output_produced);
    Scratch unsupported;PositionAdaptiveHuffmanStreamDecoder u(profile,{},unsupported.serialized,unsupported.tokens,unsupported.raw,unsupported.nodes,unsupported.symbols);
    r=u.process(one,raw,marc::core::flag_value(marc::core::ProcessFlags::reset_block));check(r.status==S::error && !r.input_consumed);
    const auto exact_test=[&](bool one_short) {
        Scratch w;marc::core::DecoderLimits limits;limits.max_block_size=65536;
        limits.max_internal_buffered_bytes=sizeof(PositionAdaptiveHuffmanStreamDecoder)
            +position_adaptive_huffman_stream_decode_control_bytes+w.serialized.size()+w.raw.size()
            +w.tokens.size()*sizeof(LzssTypedToken)+w.nodes.size()*sizeof(AdaptiveHuffmanNode)
            +w.symbols.size()*sizeof(std::uint16_t)-(one_short ? 1 : 0);
        check(marc::core::validate_limits(limits)==marc::core::LimitError::none);
        PositionAdaptiveHuffmanStreamDecoder decoder(profile,limits,w.serialized,w.tokens,w.raw,w.nodes,w.symbols);
        std::vector<std::byte> out(5,std::byte{0x63});auto result=decoder.process(one,out,end);
        if (one_short) check(result.status==S::error && result.error.code==marc::core::ErrorCode::limit_exceeded
            && !result.input_consumed && !result.output_produced && out==std::vector<std::byte>(5,std::byte{0x63}));
        else check(result.status==S::progress && result.output_produced==5 && out==expected);
    };
    exact_test(false);exact_test(true);
    for (const auto shortage:{0,1,2}) {
        Scratch w;
        if (shortage==0) w.serialized.resize(79);
        if (shortage==1) w.tokens.resize(2);
        if (shortage==2) w.raw.resize(4);
        PositionAdaptiveHuffmanStreamDecoder decoder(profile,{},w.serialized,w.tokens,w.raw,w.nodes,w.symbols);
        std::vector<std::byte> out(5,std::byte{0x63});const auto result=decoder.process(one,out,end);
        check(result.status==S::error && result.error.code==marc::core::ErrorCode::limit_exceeded
            && !result.output_produced && out==std::vector<std::byte>(5,std::byte{0x63}));
    }
}
}
int main() {try {directed();std::cout<<"native FGK stream decoder checks passed\n";return 0;}
    catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
