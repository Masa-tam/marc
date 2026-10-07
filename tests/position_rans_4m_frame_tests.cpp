#include "frame/lzss_position_rans_4m_frame.hpp"
#include "frame/lzss_contextual_rans_format.hpp"
#include "core/endian.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <tuple>
#include <vector>

namespace {
using namespace marc::frame::internal;
using Token=marc::dictionary::internal::LzssTypedToken;
using Kind=marc::dictionary::internal::LzssTypedTokenKind;
using Error=PositionRans4mFrameError;
using FormatError=PositionRans4mFormatError;
int failures{};
void check(bool good,const char* message) { if (!good) { ++failures; std::cerr<<message<<'\n'; } }
marc::core::DecoderLimits limits() {
    marc::core::DecoderLimits result{}; result.max_block_size=UINT64_C(40)<<20; return result;
}
bool equal(std::span<const Token> a,std::span<const Token> b) {
    return a.size()==b.size() && std::equal(a.begin(),a.end(),b.begin(),[](auto x,auto y) {
        return x.kind==y.kind && x.literal==y.literal && x.distance==y.distance && x.length==y.length;
    });
}
auto fields(const PositionRans4mFrameHeader& h) {
    return std::tuple{h.flags,h.sequence,h.raw_size,h.token_count,h.event_count,h.decision_count,
        h.payload_size,h.descriptor_size,h.side_data_size,h.trailer_size};
}
std::vector<std::byte> encode(std::span<const Token> tokens,const PositionRans4mFrameContext& context) {
    std::vector<std::byte> bytes(18*static_cast<std::size_t>(context.stream.frame_size)+5224,std::byte{0xa5});
    const auto result=encode_position_rans_4m_frame(tokens,context,bytes);
    check(result.error==Error::none,"frame encode");
    bytes.resize(result.serialized_size);
    return bytes;
}
void roundtrip(std::span<const Token> tokens,std::uint32_t raw_size) {
    const auto l=limits();
    PositionRans4mStreamHeader stream{raw_size,raw_size};
    const PositionRans4mFrameContext context{stream,l,0,0};
    const auto wire=encode(tokens,context);
    std::vector<Token> decoded(tokens.size());
    std::vector<std::byte> raw(raw_size);
    auto r=decode_position_rans_4m_frame(wire,context,decoded,raw);
    check(r.error==Error::none && r.serialized_size==wire.size(),"frame decode");
    check(equal(tokens,decoded),"frame tokens");
    const auto saved=raw;
    r=decode_position_rans_4m_frame_scratch(wire,context,decoded,raw);
    check(r.error==Error::none && raw==saved,"scratch frame decode");
}
} // namespace

int main(int argc,char** argv) {
    if (argc==3 && std::string(argv[1])=="--fixture") {
        // Trusted diagnostic fixture: u32 frame_size, u64 original, u64 seq,
        // u64 committed, u32 token_count, u32 wire_size; tokens(10 bytes),
        // independent expected stream/header/frame and raw frame bytes.
        std::ifstream file(argv[2],std::ios::binary);
        std::vector<char> input((std::istreambuf_iterator<char>(file)),{});
        if (!file || input.size()<36) return 2;
        std::vector<std::byte> storage(input.size());
        std::transform(input.begin(),input.end(),storage.begin(),[](char v) { return static_cast<std::byte>(v); });
        const auto bytes=std::span<const std::byte>(storage);
        PositionRans4mStreamHeader stream{};
        std::uint64_t sequence{},committed{};
        std::uint32_t count{},wire_size{};
        (void)marc::core::load_le(bytes,0,stream.frame_size);
        (void)marc::core::load_le(bytes,4,stream.original_size);
        (void)marc::core::load_le(bytes,12,sequence);
        (void)marc::core::load_le(bytes,20,committed);
        (void)marc::core::load_le(bytes,28,count);
        (void)marc::core::load_le(bytes,32,wire_size);
        if (count>4194304 || stream.frame_size==0 || stream.frame_size>4194304 || committed>=stream.original_size) return 3;
        const auto raw_size=std::min<std::uint64_t>(stream.frame_size,stream.original_size-committed);
        if (UINT64_C(36)+UINT64_C(10)*count+112+wire_size+raw_size!=bytes.size()) return 4;
        std::vector<Token> tokens(count);
        for (std::size_t i=0;i<count;++i) {
            const auto offset=36+10*i;
            tokens[i].kind=static_cast<Kind>(std::to_integer<unsigned>(bytes[offset]));
            tokens[i].literal=std::to_integer<std::uint8_t>(bytes[offset+1]);
            (void)marc::core::load_le(bytes,offset+2,tokens[i].distance);
            (void)marc::core::load_le(bytes,offset+6,tokens[i].length);
        }
        const auto l=limits();
        std::array<std::byte,112> header{};
        if (serialize_position_rans_4m_stream(stream,l,header)!=FormatError::none) return 5;
        const auto start=36+10*static_cast<std::size_t>(count);
        if (!std::equal(header.begin(),header.end(),bytes.begin()+start)) return 6;
        const PositionRans4mFrameContext context{stream,l,sequence,committed};
        const auto encoded=encode(tokens,context);
        if (failures || encoded.size()!=wire_size || !std::equal(encoded.begin(),encoded.end(),bytes.begin()+start+112)) return 7;
        std::vector<Token> decoded(count);
        std::vector<std::byte> raw(static_cast<std::size_t>(raw_size));
        const auto result=decode_position_rans_4m_frame_scratch(encoded,context,decoded,raw);
        if (result.error!=Error::none || !equal(tokens,decoded)
            || !std::equal(raw.begin(),raw.end(),bytes.begin()+start+112+wire_size)) return 8;
        return 0;
    }
    const auto l=limits();
    PositionRans4mStreamHeader empty{};
    std::array<std::byte,112> header{};
    check(serialize_position_rans_4m_stream(empty,l,header)==FormatError::none,"empty header");
    PositionRans4mStreamHeader parsed{}; parsed.original_size=777;
    std::size_t consumed=777;
    check(parse_position_rans_4m_stream(header,l,parsed,consumed)==FormatError::none
        && parsed.original_size==0 && consumed==112,"empty parse");
    for (std::size_t n=0;n<112;++n) {
        parsed.original_size=777; consumed=777;
        check(parse_position_rans_4m_stream(std::span(header).first(n),l,parsed,consumed)==FormatError::truncated,"header truncation");
        check(parsed.original_size==777 && consumed==777,"failed header mutation");
    }
    for (std::size_t n=0;n<112;++n) {
        if ((n>=20 && n<24) || (n>=40 && n<48)) continue;
        auto bad=header; bad[n]^=std::byte{0x80}; parsed.original_size=777; consumed=777;
        check(parse_position_rans_4m_stream(bad,l,parsed,consumed)!=FormatError::none,"header field corruption");
        check(parsed.original_size==777 && consumed==777,"corrupt header mutation");
    }
    LzssContextualRansStreamHeader old{};
    std::size_t old_consumed=777;
    check(parse_lzss_contextual_rans_stream_header(header,l,old,old_consumed)!=LzssContextualRansStreamHeaderError::none
        && old_consumed==777,"old parser admits new identity");
    old.frame_size=1; old.original_size=1; old.dictionary={1048576,5,258,0};
    old.dictionary_variant=3; old.context_variant=2; old.frequency_entry_count=4550;
    std::array<std::byte,112> old_header{};
    check(serialize_lzss_contextual_rans_stream_header(old,l,old_header)==LzssContextualRansStreamHeaderError::none,"old canonical header");
    check(parse_position_rans_4m_stream(old_header,l,parsed,consumed)==FormatError::unsupported_identity,"new parser admits old identity");
    old_header[18]=std::byte{4}; old_consumed=777;
    check(parse_lzss_contextual_rans_stream_header(old_header,l,old,old_consumed)==LzssContextualRansStreamHeaderError::unsupported_entropy_variant,"old parser mixed identity");
    for (unsigned value=0;value<256;++value) {
        const Token token{Kind::literal,static_cast<std::uint8_t>(value),0,0}; roundtrip(std::span(&token,1),1);
    }
    for (unsigned length=3;length<=258;++length) {
        const std::array tokens{Token{Kind::literal,65,0,0},Token{Kind::match,0,1,length}};
        roundtrip(tokens,length+1);
    }
    const Token a{Kind::literal,65,0,0},sentinel{Kind::match,77,777,777};
    PositionRans4mStreamHeader stream{1,1};
    const PositionRans4mFrameContext context{stream,l,0,0};
    const auto wire=encode(std::span(&a,1),context);
    std::array<Token,1> tokens{sentinel};
    std::array<std::byte,1> raw{std::byte{0xa5}};
    for (std::size_t n=0;n<wire.size();++n) {
        const auto r=decode_position_rans_4m_frame(std::span(wire).first(n),context,tokens,raw);
        check(r.error!=Error::none && r.serialized_size==0,"frame truncation");
        check(equal(tokens,std::span(&sentinel,1)) && raw[0]==std::byte{0xa5},"truncated frame output mutation");
    }
    auto bad=wire;
    (void)marc::core::store_le<std::uint64_t>(bad,bad.size()-8,(UINT64_C(1)<<31)+1);
    auto r=decode_position_rans_4m_frame(bad,context,tokens,raw);
    check(r.error==Error::token_error && r.serialized_size==0,"late terminal state");
    check(equal(tokens,std::span(&sentinel,1)) && raw[0]==std::byte{0xa5},"late transactional mutation");
    r=decode_position_rans_4m_frame_scratch(bad,context,tokens,raw);
    check(r.error==Error::token_error && raw[0]==std::byte{0xa5} && tokens[0].literal==65,"failed private tokens published raw");
    auto extra=wire; extra.push_back(std::byte{0xff});
    check(decode_position_rans_4m_frame(extra,context,tokens,raw).serialized_size==wire.size(),"exact one-frame consumption");
    PositionRans4mFrameLayout layout{};
    check(preflight_position_rans_4m_frame(wire,context,layout)==FormatError::none,"frame preflight");
    const auto saved=layout;
    bad=wire; bad[48]=std::byte{1};
    check(preflight_position_rans_4m_frame(bad,context,layout)==FormatError::nonzero_reserved,"reserved frame byte");
    check(fields(layout.header)==fields(saved.header) && layout.requirements==saved.requirements,"preflight mutation");
    auto limited=l;
    limited.max_frame_size=1; limited.max_block_size=2;
    limited.max_internal_buffered_bytes=saved.requirements.aggregate_bytes-1;
    check(preflight_position_rans_4m_frame(wire,{stream,limited,0,0},layout)==FormatError::limit_exceeded,"below aggregate bound");
    limited.max_internal_buffered_bytes=saved.requirements.aggregate_bytes;
    check(preflight_position_rans_4m_frame(wire,{stream,limited,0,0},layout)==FormatError::none,"exact aggregate bound");
    limited=l; limited.max_entropy_table_entries=2521;
    check(validate_position_rans_4m_stream(stream,limited)==FormatError::limit_exceeded,"frequency table limit");
    std::vector<std::byte> output(wire.size(),std::byte{0xa5});
    const auto saved_output=output;
    check(encode_position_rans_4m_frame(std::span(&a,1),context,std::span(output).first(output.size()-1)).error==Error::serialized_output_too_small,"encode capacity");
    check(output==saved_output,"failed encoded output mutation");
    const Token invalid{Kind::match,0,1,3};
    PositionRans4mStreamHeader three{3,3};
    check(encode_position_rans_4m_frame(std::span(&invalid,1),{three,l,0,0},output).error==Error::token_error,"invalid token encode");
    check(output==saved_output,"bad token encode mutation");
    std::cout<<"failures="<<failures<<'\n';
    return failures ? 1 : 0;
}
