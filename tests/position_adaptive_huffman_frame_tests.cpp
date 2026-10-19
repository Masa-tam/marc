#include "frame/lzss_position_adaptive_huffman_frame.hpp"
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
using Error=PositionAdaptiveHuffmanFrameError;
constexpr auto profile=PositionAdaptiveHuffmanProfile::window_64k;
void check(bool condition,const std::source_location where=std::source_location::current()) {
    if (!condition) throw std::runtime_error("native FGK frame check failed at line "+std::to_string(where.line()));
}
struct Workspace {
    std::vector<AdaptiveHuffmanNode> nodes{position_adaptive_huffman_spec(profile)->node_entries()};
    std::vector<std::uint16_t> symbols=std::vector<std::uint16_t>(4570);
};
std::vector<std::byte> unhex(const std::string& text) {
    if (text.starts_with("repeat:")) {
        const auto colon=text.find(':',7);check(colon!=std::string::npos);
        const auto value=std::stoul(text.substr(7,colon-7),nullptr,16),count=std::stoul(text.substr(colon+1));
        check(value<256 && count<=65536);return std::vector<std::byte>(count,static_cast<std::byte>(value));
    }
    check(text.size()%2==0);std::vector<std::byte> bytes;
    for (std::size_t i=0;i<text.size();i+=2) bytes.push_back(static_cast<std::byte>(std::stoul(text.substr(i,2),nullptr,16)));
    return bytes;
}
std::vector<LzssTypedToken> parse_tokens(const std::string& text) {
    std::vector<LzssTypedToken> tokens;std::size_t cursor{};
    while (cursor<text.size()) {
        const auto comma=text.find(',',cursor);const auto end=comma==std::string::npos ? text.size() : comma;
        const auto field=text.substr(cursor,end-cursor);LzssTypedToken token{};
        check(field.size()>=3 && field[1]==':');
        if (field[0]=='L') { auto value=std::stoul(field.substr(2));check(value<256);token.literal=static_cast<std::uint8_t>(value); }
        else {
            check(field[0]=='M');const auto colon=field.find(':',2);check(colon!=std::string::npos);
            token.kind=LzssTypedTokenKind::match;
            token.length=static_cast<std::uint32_t>(std::stoul(field.substr(2,colon-2)));
            token.distance=static_cast<std::uint32_t>(std::stoul(field.substr(colon+1)));
        }
        tokens.push_back(token);check(tokens.size()<=65536);cursor=end+1;
    }
    return tokens;
}
void failed(const PositionAdaptiveHuffmanFrameResult& r) {
    check(r.error!=Error::none && !r.serialized_size && !r.raw_size && !r.token_result.token_count
        && !r.token_result.raw_size && !r.token_result.decision_count && !r.requirements.serialized_size);
}
void fixture(const std::vector<std::byte>& stream_bytes,const std::vector<std::byte>& frame,
             const std::vector<std::byte>& raw,const std::vector<LzssTypedToken>& tokens) {
    const auto limits=marc::core::DecoderLimits{};PositionAdaptiveHuffmanStreamHeader stream{};std::size_t used{};
    check(parse_position_adaptive_huffman_stream(profile,stream_bytes,limits,stream,used)==PositionAdaptiveHuffmanFormatError::none);
    check(stream.original_size==raw.size());
    if (raw.empty()) { check(frame.empty() && tokens.empty());return; }
    const PositionAdaptiveHuffmanFrameContext context{stream,limits,0,0};Workspace work;
    PositionAdaptiveHuffmanFrameLayout layout{};
    check(preflight_position_adaptive_huffman_frame(frame,context,layout)==PositionAdaptiveHuffmanFormatError::none);
    std::vector<std::byte> payload(layout.header.payload_size),staged(frame.size()),output(frame.size()+3,std::byte{0x76});
    const auto encoded=encode_position_adaptive_huffman_frame(tokens,context,work.nodes,work.symbols,payload,staged,output);
    check(encoded.error==Error::none && encoded.serialized_size==frame.size() && encoded.raw_size==raw.size());
    check(std::equal(frame.begin(),frame.end(),output.begin()));
    check(std::all_of(output.begin()+frame.size(),output.end(),[](auto b){return b==std::byte{0x76};}));
    std::vector<LzssTypedToken> scratch(tokens.size());
    std::vector<std::byte> private_raw(raw.size()+1,std::byte{0x39}),ordinary(raw.size()+3,std::byte{0x64});
    const auto decoded=decode_position_adaptive_huffman_frame(frame,context,work.nodes,work.symbols,scratch,private_raw,ordinary);
    check(decoded.error==Error::none && decoded.raw_size==raw.size() && decoded.serialized_size==frame.size());
    check(std::equal(raw.begin(),raw.end(),ordinary.begin()));
    check(std::all_of(ordinary.begin()+raw.size(),ordinary.end(),[](auto b){return b==std::byte{0x64};}));
    check(private_raw.back()==std::byte{0x39});
    std::fill(private_raw.begin(),private_raw.end(),std::byte{0x39});
    const auto controller=decode_position_adaptive_huffman_frame_scratch(frame,context,work.nodes,work.symbols,scratch,private_raw);
    check(controller.error==Error::none && std::equal(raw.begin(),raw.end(),private_raw.begin()) && private_raw.back()==std::byte{0x39});
    for (std::size_t n=0;n<frame.size();++n) {
        std::fill(ordinary.begin(),ordinary.end(),std::byte{0x64});
        failed(decode_position_adaptive_huffman_frame(std::span(frame).first(n),context,work.nodes,work.symbols,scratch,private_raw,ordinary));
        check(std::all_of(ordinary.begin(),ordinary.end(),[](auto b){return b==std::byte{0x64};}));
    }
    for (unsigned which=0;which<3;++which) {
        std::fill(output.begin(),output.end(),std::byte{0x76});
        failed(encode_position_adaptive_huffman_frame(tokens,context,work.nodes,work.symbols,
            std::span(payload).first(which==0 ? payload.size()-1 : payload.size()),
            std::span(staged).first(which==1 ? staged.size()-1 : staged.size()),
            std::span(output).first(which==2 ? frame.size()-1 : output.size())));
        check(std::all_of(output.begin(),output.end(),[](auto b){return b==std::byte{0x76};}));
    }
    std::fill(ordinary.begin(),ordinary.end(),std::byte{0x64});
    failed(decode_position_adaptive_huffman_frame(frame,context,work.nodes,work.symbols,scratch,ordinary,ordinary));
    check(std::all_of(ordinary.begin(),ordinary.end(),[](auto b){return b==std::byte{0x64};}));
    const auto valid=std::to_integer<unsigned>(frame[74]);
    if (valid<8) {
        auto padding=frame;padding.back()|=std::byte{0x80};
        failed(decode_position_adaptive_huffman_frame(padding,context,work.nodes,work.symbols,scratch,private_raw,ordinary));
        check(std::all_of(ordinary.begin(),ordinary.end(),[](auto b){return b==std::byte{0x64};}));
    }
}
void directed() {
    Workspace work;const auto limits=marc::core::DecoderLimits{};
    PositionAdaptiveHuffmanStreamHeader stream{};stream.original_size=5;
    const PositionAdaptiveHuffmanFrameContext context{stream,limits,0,0};
    // Canonical metadata declares a32-bit payload, but ABABA requires33 bits.
    PositionAdaptiveHuffmanFrameHeader header{};header.raw_size=header.token_count=5;
    header.event_count=header.decision_count=10;header.payload_size=4;
    std::vector<std::byte> frame(84);
    check(serialize_position_adaptive_huffman_frame_header(header,context,frame)==PositionAdaptiveHuffmanFormatError::none);
    const auto descriptor=unhex("0a000000040000003000080000000000"),payload=unhex("82081574");
    std::copy(descriptor.begin(),descriptor.end(),frame.begin()+64);std::copy(payload.begin(),payload.end(),frame.begin()+80);
    std::vector<LzssTypedToken> tokens(5);std::vector<std::byte> private_raw(5,std::byte{0x3d}),ordinary(8,std::byte{0x74});
    auto result=decode_position_adaptive_huffman_frame(frame,context,work.nodes,work.symbols,tokens,private_raw,ordinary);
    failed(result);check(result.token_result.entropy_error==PositionAdaptiveHuffmanError::truncated);
    check(tokens[0].literal==65 && tokens[3].literal==66);
    check(std::all_of(ordinary.begin(),ordinary.end(),[](auto b){return b==std::byte{0x74};}));
    check(std::all_of(private_raw.begin(),private_raw.end(),[](auto b){return b==std::byte{0x3d};}));
    result=decode_position_adaptive_huffman_frame_scratch(frame,context,work.nodes,work.symbols,tokens,private_raw);
    failed(result);check(std::all_of(private_raw.begin(),private_raw.end(),[](auto b){return b==std::byte{0x3d};}));
    stream.original_size=3;header.raw_size=3;header.token_count=1;header.event_count=header.decision_count=4;
    PositionAdaptiveHuffmanEncoder encoder;check(encoder.begin_plan(profile,limits,work.nodes,work.symbols)==PositionAdaptiveHuffmanError::none);
    const std::array<std::pair<int,std::uint32_t>,4> operations{{{0,1},{20,8},{-1,0},{31,0}}};
    for (auto [c,v]:operations) check(encoder.emit(c,v)==PositionAdaptiveHuffmanError::none);
    PositionAdaptiveHuffmanDescriptor d{};check(encoder.finish_plan(d)==PositionAdaptiveHuffmanError::none);
    header.payload_size=d.payload_size;frame.resize(80+d.payload_size);
    check(serialize_position_adaptive_huffman_frame_header(header,context,frame)==PositionAdaptiveHuffmanFormatError::none);
    check(serialize_position_adaptive_huffman_descriptor(profile,d,limits,std::span(frame).subspan(64,16))==PositionAdaptiveHuffmanError::none);
    check(encoder.begin_write(profile,d,limits,work.nodes,work.symbols,std::span(frame).subspan(80))==PositionAdaptiveHuffmanError::none);
    for (auto [c,v]:operations) check(encoder.emit(c,v)==PositionAdaptiveHuffmanError::none);
    check(encoder.finish_write()==PositionAdaptiveHuffmanError::none);
    result=decode_position_adaptive_huffman_frame(frame,context,work.nodes,work.symbols,tokens,private_raw,ordinary);
    failed(result);check(result.token_result.token_error==LzssTypedTokenError::invalid_distance);
    check(std::all_of(ordinary.begin(),ordinary.end(),[](auto b){return b==std::byte{0x74};}));
}
}
int main(int argc,char** argv) {
    try {
        if (argc==5) { fixture(unhex(argv[1]),unhex(argv[2]),unhex(argv[3]),parse_tokens(argv[4]));return 0; }
        check(argc==1);directed();std::cout<<"native FGK64KiB complete frame checks passed\n";
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
