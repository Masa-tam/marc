#include "context/lzss_position_adaptive_huffman_tokens.hpp"
#include "dictionary/lzss_typed_reconstructor.hpp"
#include <algorithm>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace marc::entropy::internal;
using namespace marc::context::internal;
using namespace marc::dictionary::internal;
constexpr auto profile=PositionAdaptiveHuffmanProfile::window_64k;
using Error=PositionAdaptiveHuffmanTokenError;
constexpr LzssParameters parameters{65536,3,258,0};
constexpr auto variant=LzssTypedTokenVariant::field_context_64k_short_length_escape;
void check(bool condition,const std::source_location where=std::source_location::current()) {
    if (!condition) throw std::runtime_error("native FGK token check failed at line "+std::to_string(where.line()));
}
bool same(const LzssTypedToken& a,const LzssTypedToken& b) {
    return a.kind==b.kind && a.literal==b.literal && a.distance==b.distance && a.length==b.length;
}
struct Workspace {
    std::vector<AdaptiveHuffmanNode> nodes{position_adaptive_huffman_spec(profile)->node_entries()};
    std::vector<std::uint16_t> symbols=std::vector<std::uint16_t>(4570);
};
std::vector<std::byte> unhex(const std::string& text) {
    check(text.size()%2==0);std::vector<std::byte> result;
    for (std::size_t i=0;i<text.size();i+=2) result.push_back(static_cast<std::byte>(std::stoul(text.substr(i,2),nullptr,16)));
    return result;
}
std::vector<LzssTypedToken> parse_tokens(const std::string& text) {
    std::vector<LzssTypedToken> result;std::size_t cursor{};
    while (cursor<text.size()) {
        const auto comma=text.find(',',cursor);const auto end=comma==std::string::npos ? text.size() : comma;
        const auto field=text.substr(cursor,end-cursor);LzssTypedToken token{};
        check(field.size()>=3 && field[1]==':');
        if (field[0]=='L') { const auto value=std::stoul(field.substr(2));check(value<256);token.literal=static_cast<std::uint8_t>(value); }
        else {
            check(field[0]=='M');const auto colon=field.find(':',2);check(colon!=std::string::npos);
            token.kind=LzssTypedTokenKind::match;
            token.length=static_cast<std::uint32_t>(std::stoul(field.substr(2,colon-2)));
            token.distance=static_cast<std::uint32_t>(std::stoul(field.substr(colon+1)));
        }
        result.push_back(token);check(result.size()<=65536);cursor=end+1;
    }
    return result;
}
void failed(const PositionAdaptiveHuffmanTokenResult& r) {
    check(r.error!=Error::none && !r.token_count && !r.raw_size && !r.decision_count && !r.descriptor_size && !r.payload_size);
}
void exercise(const std::vector<LzssTypedToken>& tokens,const std::vector<std::byte>& raw,
              const std::vector<std::byte>* expected_descriptor=nullptr,const std::vector<std::byte>* expected_payload=nullptr) {
    Workspace work;const auto limits=marc::core::DecoderLimits{};
    const LzssTypedFrameValidationContext context{static_cast<std::uint32_t>(tokens.size()),static_cast<std::uint32_t>(raw.size()),0};
    PositionAdaptiveHuffmanDescriptor d{};
    const auto planned=plan_position_adaptive_huffman_tokens(profile,tokens,context,limits,work.nodes,work.symbols,d);
    check(planned.error==Error::none && planned.token_count==tokens.size() && planned.raw_size==raw.size());
    std::vector<std::byte> staging(d.payload_size),description(20,std::byte{0x49}),payload(d.payload_size+3,std::byte{0x53});
    const auto encoded=encode_position_adaptive_huffman_tokens(profile,tokens,context,limits,work.nodes,work.symbols,staging,description,payload);
    check(encoded.error==Error::none && encoded.decision_count==d.decision_count);
    check(std::all_of(description.begin()+16,description.end(),[](auto b){return b==std::byte{0x49};}));
    check(std::all_of(payload.begin()+d.payload_size,payload.end(),[](auto b){return b==std::byte{0x53};}));
    if (expected_descriptor) check(std::equal(expected_descriptor->begin(),expected_descriptor->end(),description.begin()));
    if (expected_payload) check(std::equal(expected_payload->begin(),expected_payload->end(),payload.begin()));
    const LzssTypedToken sentinel{LzssTypedTokenKind::match,7,13,17};
    std::vector<LzssTypedToken> scratch(tokens.size()),output(tokens.size()+2,sentinel);
    const PositionAdaptiveHuffmanTokenContext expected{context.declared_token_count,context.declared_raw_size,d.decision_count,0};
    const auto decoded=decode_position_adaptive_huffman_tokens(profile,std::span(description).first(16),
        std::span(payload).first(d.payload_size),expected,limits,work.nodes,work.symbols,scratch,output);
    check(decoded.error==Error::none && decoded.token_count==tokens.size());
    check(std::equal(tokens.begin(),tokens.end(),output.begin(),same));
    check(same(output[tokens.size()],sentinel) && same(output.back(),sentinel));
    std::vector<std::byte> restored(raw.size());
    check(reconstruct_lzss_typed_frame(std::span(output).first(tokens.size()),parameters,context,limits,restored,variant).error==LzssTypedReconstructError::none);
    check(restored==raw);
    if (!tokens.empty()) {
        std::fill(output.begin(),output.end(),sentinel);
        auto wrong=expected;++wrong.decision_count;
        failed(decode_position_adaptive_huffman_tokens(profile,std::span(description).first(16),
            std::span(payload).first(d.payload_size),wrong,limits,work.nodes,work.symbols,scratch,output));
        check(std::all_of(output.begin(),output.end(),[&](const auto& t){return same(t,sentinel);}));
        failed(decode_position_adaptive_huffman_tokens(profile,std::span(description).first(16),
            std::span(payload).first(d.payload_size),expected,limits,work.nodes,work.symbols,output,output));
        check(std::all_of(output.begin(),output.end(),[&](const auto& t){return same(t,sentinel);}));
    }
}
void directed() {
    const auto tokens=parse_tokens("L:65,M:3:1");const auto raw=unhex("41414141");exercise(tokens,raw);
    Workspace work;auto limits=marc::core::DecoderLimits{};limits.max_block_size=65536;
    const LzssTypedFrameValidationContext context{2,4,0};
    const PositionAdaptiveHuffmanDescriptor sentinel{123,456,57,3,1};
    std::size_t low=65536,high=1048576;
    while (high-low>1) {
        const auto middle=low+(high-low)/2;limits.max_internal_buffered_bytes=middle;
        check(marc::core::validate_limits(limits)==marc::core::LimitError::none);
        auto output=sentinel;
        const auto result=plan_position_adaptive_huffman_tokens(profile,tokens,context,limits,work.nodes,work.symbols,output);
        if (result.error==Error::none) high=middle;
        else { failed(result);check(result.error==Error::limit_exceeded && output==sentinel);low=middle; }
    }
    limits.max_internal_buffered_bytes=high;PositionAdaptiveHuffmanDescriptor d{};
    check(plan_position_adaptive_huffman_tokens(profile,tokens,context,limits,work.nodes,work.symbols,d).error==Error::none);
    limits.max_internal_buffered_bytes=high-1;auto unchanged=sentinel;
    failed(plan_position_adaptive_huffman_tokens(profile,tokens,context,limits,work.nodes,work.symbols,unchanged));check(unchanged==sentinel);
    auto invalid=tokens;invalid[0].distance=1;
    std::vector<std::byte> scratch(128),descriptor(20,std::byte{0x37}),payload(128,std::byte{0x62});
    failed(encode_position_adaptive_huffman_tokens(profile,invalid,context,{},work.nodes,work.symbols,scratch,descriptor,payload));
    check(std::all_of(descriptor.begin(),descriptor.end(),[](auto b){return b==std::byte{0x37};}));
    check(std::all_of(payload.begin(),payload.end(),[](auto b){return b==std::byte{0x62};}));
    failed(encode_position_adaptive_huffman_tokens(profile,tokens,context,{},work.nodes,work.symbols,scratch,std::span(descriptor).first(15),payload));
    check(std::all_of(descriptor.begin(),descriptor.end(),[](auto b){return b==std::byte{0x37};}));
    check(std::all_of(payload.begin(),payload.end(),[](auto b){return b==std::byte{0x62};}));
    failed(encode_position_adaptive_huffman_tokens(profile,tokens,context,{},work.nodes,work.symbols,scratch,
        descriptor,std::span(payload).first(d.payload_size-1)));
    check(std::all_of(descriptor.begin(),descriptor.end(),[](auto b){return b==std::byte{0x37};}));
    check(std::all_of(payload.begin(),payload.end(),[](auto b){return b==std::byte{0x62};}));
    // The frozen ABABA payload needs33 bits. Retain the first32 to force a
    // late entropy failure after private tokens have already been reconstructed.
    PositionAdaptiveHuffmanDescriptor shortened{10,4,48,8,0};std::array<std::byte,16> short_descriptor{};
    check(serialize_position_adaptive_huffman_descriptor(profile,shortened,{},short_descriptor)==PositionAdaptiveHuffmanError::none);
    const auto late_payload=unhex("82081574");
    std::vector<LzssTypedToken> private_late(5),ordinary_late(7,{LzssTypedTokenKind::match,7,13,17});const auto saved_late=ordinary_late;
    const auto late_error=decode_position_adaptive_huffman_tokens(profile,short_descriptor,late_payload,{5,5,10,0},{},
        work.nodes,work.symbols,private_late,ordinary_late);
    failed(late_error);check(late_error.entropy_error==PositionAdaptiveHuffmanError::truncated);
    check(private_late[0].literal==65 && private_late[3].literal==66);
    check(std::equal(ordinary_late.begin(),ordinary_late.end(),saved_late.begin(),same));
    // A maximum legal late distance within a reset64KiB frame; exact window
    // distance cannot be followed by a match inside that same raw frame.
    std::vector<LzssTypedToken> late(65534);for (auto& t:late) t.literal=65;
    late.back()={LzssTypedTokenKind::match,0,65533,3};
    exercise(late,std::vector<std::byte>(65536,std::byte{65}));
    const auto corrupt=[&](const std::vector<std::pair<int,std::uint32_t>>& decisions,std::uint32_t raw_size,std::uint32_t count) {
        PositionAdaptiveHuffmanEncoder encoder;
        check(encoder.begin_plan(profile,{},work.nodes,work.symbols)==PositionAdaptiveHuffmanError::none);
        for (auto [c,v]:decisions) check(encoder.emit(c,v)==PositionAdaptiveHuffmanError::none);
        PositionAdaptiveHuffmanDescriptor model{};check(encoder.finish_plan(model)==PositionAdaptiveHuffmanError::none);
        std::vector<std::byte> bytes(model.payload_size);std::array<std::byte,16> desc{};
        check(encoder.begin_write(profile,model,{},work.nodes,work.symbols,bytes)==PositionAdaptiveHuffmanError::none);
        for (auto [c,v]:decisions) check(encoder.emit(c,v)==PositionAdaptiveHuffmanError::none);
        check(encoder.finish_write()==PositionAdaptiveHuffmanError::none);
        check(serialize_position_adaptive_huffman_descriptor(profile,model,{},desc)==PositionAdaptiveHuffmanError::none);
        std::vector<LzssTypedToken> private_tokens(count),output(count,{LzssTypedTokenKind::match,7,13,17});const auto saved=output;
        const PositionAdaptiveHuffmanTokenContext expected{count,raw_size,model.decision_count,0};
        const auto result=decode_position_adaptive_huffman_tokens(profile,desc,bytes,expected,{},work.nodes,work.symbols,private_tokens,output);
        failed(result);check(result.error==Error::invalid_token);
        check(std::equal(output.begin(),output.end(),saved.begin(),same));
    };
    corrupt({{0,1},{20,8},{-1,0},{31,0}},3,1); // Valid entropy, reference before history.
    std::vector<std::pair<int,std::uint32_t>> too_long{{0,0},{3,65},{1,1},{21,7}};
    for (int b=0;b<7;++b) too_long.emplace_back(-1,1);
    too_long.emplace_back(30,0);corrupt(too_long,259,2); // Class7 residual127 means259, forbidden.
}
}
int main(int argc,char** argv) {
    try {
        if (argc==5) {
            const auto descriptor=unhex(argv[1]),payload=unhex(argv[2]);
            exercise(parse_tokens(argv[4]),unhex(argv[3]),&descriptor,&payload);return 0;
        }
        check(argc==1);directed();std::cout<<"native FGK64KiB token checks passed\n";
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
