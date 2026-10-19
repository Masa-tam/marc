#include "frame/lzss_position_adaptive_huffman_format.hpp"
#include "core/endian.hpp"
#include <algorithm>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace marc::frame::internal;
using namespace marc::entropy::internal;
using Error=PositionAdaptiveHuffmanFormatError;
constexpr auto profile=PositionAdaptiveHuffmanProfile::window_64k;
void check(bool condition,const std::source_location where=std::source_location::current()) {
    if (!condition) throw std::runtime_error("native FGK frame format check failed at line "+std::to_string(where.line()));
}
std::vector<std::byte> unhex(const std::string& text) {
    check(text.size()%2==0);std::vector<std::byte> bytes;
    for (std::size_t i=0;i<text.size();i+=2) bytes.push_back(static_cast<std::byte>(std::stoul(text.substr(i,2),nullptr,16)));
    return bytes;
}
void fixture(const std::vector<std::byte>& stream_bytes,const std::vector<std::byte>& frame_bytes) {
    const auto limits=marc::core::DecoderLimits{};
    PositionAdaptiveHuffmanStreamHeader stream{};std::size_t consumed=777;
    check(parse_position_adaptive_huffman_stream(profile,stream_bytes,limits,stream,consumed)==Error::none && consumed==112);
    std::vector<std::byte> output(116,std::byte{0x47});
    check(serialize_position_adaptive_huffman_stream(stream,limits,output)==Error::none);
    check(std::equal(stream_bytes.begin(),stream_bytes.end(),output.begin()));
    check(std::all_of(output.begin()+112,output.end(),[](auto b){return b==std::byte{0x47};}));
    if (frame_bytes.empty()) { check(stream.original_size==0);return; }
    PositionAdaptiveHuffmanFrameContext context{stream,limits,0,0};PositionAdaptiveHuffmanFrameLayout layout{};
    check(preflight_position_adaptive_huffman_frame(frame_bytes,context,layout)==Error::none);
    check(layout.requirements.serialized_size==frame_bytes.size() && layout.requirements.prefix_size==80);
    check(layout.requirements.node_entries==9188 && layout.requirements.symbol_entries==4570);
    std::vector<std::byte> header(68,std::byte{0x64});
    check(serialize_position_adaptive_huffman_frame_header(layout.header,context,header)==Error::none);
    check(std::equal(header.begin(),header.begin()+64,frame_bytes.begin()));
    check(std::all_of(header.begin()+64,header.end(),[](auto b){return b==std::byte{0x64};}));
}
void directed() {
    const auto limits=marc::core::DecoderLimits{};
    PositionAdaptiveHuffmanStreamHeader stream{};stream.original_size=1;
    std::vector<std::byte> stream_bytes(112);
    check(serialize_position_adaptive_huffman_stream(stream,limits,stream_bytes)==Error::none);
    const PositionAdaptiveHuffmanFrameContext context{stream,limits,0,0};
    PositionAdaptiveHuffmanFrameHeader header{};header.raw_size=1;header.token_count=1;
    header.event_count=header.decision_count=2;header.payload_size=2;
    std::vector<std::byte> frame(82);check(serialize_position_adaptive_huffman_frame_header(header,context,frame)==Error::none);
    const auto descriptor=unhex("02000000020000003000010000000000");
    std::copy(descriptor.begin(),descriptor.end(),frame.begin()+64);frame[80]=std::byte{0x82};
    fixture(stream_bytes,frame);
    for (std::size_t n=0;n<112;++n) {
        PositionAdaptiveHuffmanStreamHeader output{};output.original_size=999;std::size_t consumed=777;
        check(parse_position_adaptive_huffman_stream(profile,std::span(stream_bytes).first(n),limits,output,consumed)==Error::truncated);
        check(output.original_size==999 && consumed==777);
    }
    const auto unchanged=[&](std::span<const std::byte> input) {
        PositionAdaptiveHuffmanFrameLayout output{};output.header.raw_size=999;output.requirements.aggregate_bytes=999;
        check(preflight_position_adaptive_huffman_frame(input,context,output)!=Error::none);
        check(output.header.raw_size==999 && output.requirements.aggregate_bytes==999);
    };
    for (std::size_t n=0;n<frame.size();++n) unchanged(std::span(frame).first(n));
    for (std::size_t at=0;at<80;++at) { auto bad=frame;bad[at]^=std::byte{0x80};unchanged(bad); }
    for (std::size_t at=64;at<112;++at) {
        auto bad=stream_bytes;bad[at]^=std::byte{0x80};
        PositionAdaptiveHuffmanStreamHeader output{};output.original_size=999;std::size_t consumed=777;
        check(parse_position_adaptive_huffman_stream(profile,bad,limits,output,consumed)!=Error::none);
        check(output.original_size==999 && consumed==777);
    }
    PositionAdaptiveHuffmanFrameLayout layout{};
    check(preflight_position_adaptive_huffman_frame_prefix(std::span(frame).first(80),context,layout)==Error::none);
    check(layout.requirements.serialized_size==82);
    auto configured=limits;configured.max_block_size=65536;configured.max_internal_buffered_bytes=layout.requirements.aggregate_bytes;
    check(marc::core::validate_limits(configured)==marc::core::LimitError::none);
    const PositionAdaptiveHuffmanFrameContext exact{stream,configured,0,0};PositionAdaptiveHuffmanFrameRequirements req{};
    check(validate_position_adaptive_huffman_frame_header(header,exact,req)==Error::none);
    --configured.max_internal_buffered_bytes;req.aggregate_bytes=999;
    check(validate_position_adaptive_huffman_frame_header(header,exact,req)==Error::limit_exceeded && req.aggregate_bytes==999);
    std::vector<std::byte> small(111,std::byte{0x36});
    check(serialize_position_adaptive_huffman_stream(stream,limits,small)==Error::output_too_small);
    check(std::all_of(small.begin(),small.end(),[](auto b){return b==std::byte{0x36};}));
    small.resize(63);
    check(serialize_position_adaptive_huffman_frame_header(header,context,small)==Error::output_too_small);
    check(std::all_of(small.begin(),small.end(),[](auto b){return b==std::byte{0x36};}));
    auto unknown=stream;unknown.original_size=UINT64_MAX;auto permissive=limits;permissive.max_total_output_size=UINT64_MAX;
    check(validate_position_adaptive_huffman_stream(unknown,permissive)==Error::invalid_parameters);
    std::size_t consumed=777;PositionAdaptiveHuffmanStreamHeader untouched{};untouched.original_size=999;
    check(parse_position_adaptive_huffman_stream(PositionAdaptiveHuffmanProfile::window_1m,stream_bytes,limits,untouched,consumed)==Error::unsupported_identity);
    check(untouched.original_size==999 && consumed==777);
}
}
int main(int argc,char** argv) {
    try {
        if (argc==3) { fixture(unhex(argv[1]),unhex(argv[2]));return 0; }
        check(argc==1);directed();std::cout<<"native FGK64KiB frame format checks passed\n";
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
