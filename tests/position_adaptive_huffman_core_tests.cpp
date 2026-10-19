#include "entropy/position_adaptive_huffman.hpp"
#include <algorithm>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace marc::entropy::internal;
using Error=PositionAdaptiveHuffmanError;
constexpr auto profile=PositionAdaptiveHuffmanProfile::window_64k;
void check(bool condition,const std::source_location where=std::source_location::current()) {
    if (!condition) throw std::runtime_error("native FGK core check failed at line "+std::to_string(where.line()));
}
void ok(Error error,const std::source_location where=std::source_location::current()) {
    if (error!=Error::none) std::cerr<<"error code "<<static_cast<unsigned>(error)<<'\n';
    check(error==Error::none,where);
}
struct Workspace {
    std::vector<AdaptiveHuffmanNode> nodes{position_adaptive_huffman_spec(profile)->node_entries()};
    std::vector<std::uint16_t> symbols=std::vector<std::uint16_t>(4570);
};
std::vector<std::byte> unhex(const std::string& text) {
    check(text.size()%2==0);std::vector<std::byte> bytes;
    for (std::size_t i=0;i<text.size();i+=2) bytes.push_back(static_cast<std::byte>(std::stoul(text.substr(i,2),nullptr,16)));
    return bytes;
}
using Decisions=std::vector<std::pair<int,std::uint32_t>>;
Decisions parse_decisions(const std::string& text) {
    Decisions result;std::size_t cursor{};
    while (cursor<text.size()) {
        const auto colon=text.find(':',cursor);check(colon!=std::string::npos);
        const auto comma=text.find(',',colon);const auto end=comma==std::string::npos ? text.size() : comma;
        result.emplace_back(std::stoi(text.substr(cursor,colon-cursor)),static_cast<std::uint32_t>(std::stoul(text.substr(colon+1,end-colon-1))));
        check(result.size()<=100000);cursor=end+1;
    }
    return result;
}
void vector_case(const std::vector<std::byte>& descriptor,const std::vector<std::byte>& payload,const Decisions& decisions) {
    Workspace work;
    PositionAdaptiveHuffmanDescriptor expected{};
    ok(parse_position_adaptive_huffman_descriptor(profile,descriptor,static_cast<std::uint32_t>(decisions.size()),
        static_cast<std::uint32_t>(payload.size()),{},expected));
    PositionAdaptiveHuffmanEncoder encoder;
    ok(encoder.begin_plan(profile,{},work.nodes,work.symbols));
    for (auto [c,v]:decisions) ok(encoder.emit(c,v));
    PositionAdaptiveHuffmanDescriptor planned{};ok(encoder.finish_plan(planned));check(planned==expected);
    auto output=std::vector<std::byte>(payload.size()+4,std::byte{0x65});
    ok(encoder.begin_write(profile,planned,{},work.nodes,work.symbols,std::span(output).first(payload.size())));
    for (auto [c,v]:decisions) ok(encoder.emit(c,v));
    ok(encoder.finish_write());
    check(std::equal(payload.begin(),payload.end(),output.begin()));
    check(std::all_of(output.begin()+payload.size(),output.end(),[](auto b){return b==std::byte{0x65};}));
    PositionAdaptiveHuffmanDecoder decoder;
    ok(decoder.begin(profile,expected,payload,{},work.nodes,work.symbols));
    for (auto [c,expected_value]:decisions) {
        std::uint32_t actual=UINT32_MAX;ok(decoder.read(c,actual));check(actual==expected_value);
    }
    ok(decoder.finish());
}
void directed() {
    Workspace work;PositionAdaptiveHuffmanEncoder encoder;PositionAdaptiveHuffmanDecoder decoder;
    std::uint32_t symbol=717;check(decoder.read(0,symbol)==Error::not_started);check(symbol==717);
    check(encoder.emit(0,0)==Error::not_started);
    for (unsigned value=0;value<256;++value) {
        const PositionAdaptiveHuffmanDescriptor d{2,2,48,1,0};
        std::array<std::byte,16> bytes{};ok(serialize_position_adaptive_huffman_descriptor(profile,d,{},bytes));
        vector_case({bytes.begin(),bytes.end()},{static_cast<std::byte>((value<<1)&255),static_cast<std::byte>(value>>7)},{{0,0},{3,value}});
    }
    std::size_t exact{};ok(position_adaptive_huffman_workspace(profile,2,exact));
    auto limits=marc::core::DecoderLimits{};limits.max_internal_buffered_bytes=exact;limits.max_block_size=65536;
    check(marc::core::validate_limits(limits)==marc::core::LimitError::none);
    ok(encoder.begin_plan(profile,limits,work.nodes,work.symbols));
    ok(encoder.emit(0,0));
    ok(encoder.emit(3,65));
    PositionAdaptiveHuffmanDescriptor d{};ok(encoder.finish_plan(d));
    const auto payload=unhex("8200");ok(decoder.begin(profile,d,payload,limits,work.nodes,work.symbols));
    --limits.max_internal_buffered_bytes;
    check(marc::core::validate_limits(limits)==marc::core::LimitError::none);
    check(decoder.begin(profile,d,payload,limits,work.nodes,work.symbols)==Error::limit_exceeded);
    ok(encoder.begin_plan(profile,limits,work.nodes,work.symbols));ok(encoder.emit(0,0));
    check(encoder.emit(3,65)==Error::limit_exceeded);
    const PositionAdaptiveHuffmanDescriptor sentinel{123,456,57,3,1};auto unchanged=sentinel;
    check(encoder.finish_plan(unchanged)==Error::limit_exceeded);check(unchanged==sentinel);
    check(encoder.begin_plan(profile,{},std::span(work.nodes).first(work.nodes.size()-1),work.symbols)==Error::output_too_small);
    check(encoder.begin_plan(profile,{},work.nodes,std::span(work.symbols).first(4569))==Error::output_too_small);
    std::vector<std::byte> short_output(1,std::byte{0x71});
    check(encoder.begin_write(profile,d,{},work.nodes,work.symbols,short_output)==Error::output_too_small);
    check(short_output[0]==std::byte{0x71});
    ok(encoder.begin_plan(profile,{},work.nodes,work.symbols));
    check(encoder.emit(48,0)==Error::invalid_context);unchanged=sentinel;
    check(encoder.finish_plan(unchanged)==Error::invalid_context && unchanged==sentinel);
    ok(encoder.begin_plan(profile,{},work.nodes,work.symbols));check(encoder.emit(20,9)==Error::invalid_symbol);
    auto nonzero=payload;nonzero.back()|=std::byte{0x80};
    check(decoder.begin(profile,d,nonzero,{},work.nodes,work.symbols)==Error::nonzero_padding);
    auto truncated=d;truncated.payload_size=1;truncated.final_valid_bits=8;
    ok(decoder.begin(profile,truncated,std::span(payload).first(1),{},work.nodes,work.symbols));
    ok(decoder.read(0,symbol));symbol=717;
    check(decoder.read(3,symbol)==Error::truncated && symbol==717);
    auto trailing=d;trailing.final_valid_bits=2;
    ok(decoder.begin(profile,trailing,payload,{},work.nodes,work.symbols));
    ok(decoder.read(0,symbol));ok(decoder.read(3,symbol));check(decoder.finish()==Error::trailing_bits);
    const PositionAdaptiveHuffmanDescriptor duplicate{2,1,48,3,0};const auto zero=unhex("00");
    ok(decoder.begin(profile,duplicate,zero,{},work.nodes,work.symbols));ok(decoder.read(0,symbol));symbol=717;
    check(decoder.read(0,symbol)==Error::invalid_nyt_symbol && symbol==717);
    const PositionAdaptiveHuffmanDescriptor unused{1,1,48,4,0};const auto impossible=unhex("0f");
    ok(decoder.begin(profile,unused,impossible,{},work.nodes,work.symbols));symbol=717;
    check(decoder.read(20,symbol)==Error::invalid_nyt_symbol && symbol==717);
    ok(decoder.begin(profile,d,payload,{},work.nodes,work.symbols));
    const auto weight=work.nodes[0].weight;
    check(decoder.read(0,work.nodes[0].weight)==Error::overlapping_buffers && work.nodes[0].weight==weight);
    ok(decoder.read(0,symbol));check(symbol==0);ok(decoder.read(3,symbol));check(symbol==65);ok(decoder.finish());
    symbol=717;check(decoder.read(0,symbol)==Error::already_finished && symbol==717);
    const auto aliased=std::span(reinterpret_cast<std::uint16_t*>(work.nodes.data()),work.symbols.size());
    check(encoder.begin_plan(profile,{},work.nodes,aliased)==Error::overlapping_buffers);
    PositionAdaptiveHuffmanDescriptor empty{};empty.context_count=48;
    std::array<std::byte,16> empty_bytes{};ok(serialize_position_adaptive_huffman_descriptor(profile,empty,{},empty_bytes));
    vector_case({empty_bytes.begin(),empty_bytes.end()},{},{});
}
}
int main(int argc,char** argv) {
    try {
        if (argc==4) { vector_case(unhex(argv[1]),unhex(argv[2]),parse_decisions(argv[3]));return 0; }
        check(argc==1);directed();std::cout<<"native FGK64KiB directed core checks passed\n";
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
