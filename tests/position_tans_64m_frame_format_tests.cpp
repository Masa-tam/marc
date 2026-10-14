#include "frame/lzss_position_tans_64m_format.hpp"
#include "core/endian.hpp"
#include <algorithm>
#include <iostream>
#include <string_view>
#include <vector>

namespace {
using namespace marc::frame::internal;
using Error=PositionTans64mFormatError;
int failures{};
void check(bool ok,const char* label) {
    if (!ok) { ++failures; std::cerr<<label<<'\n'; }
}
std::vector<std::byte> hex(std::string_view text) {
    std::vector<std::byte> bytes;
    auto nibble=[](char c) { return c<='9' ? c-'0' : c-'a'+10; };
    for (std::size_t i=0;i<text.size();i+=2)
        bytes.push_back(static_cast<std::byte>(16*nibble(text[i])+nibble(text[i+1])));
    return bytes;
}
template<class T> void put(std::span<std::byte> bytes,std::size_t at,T value) {
    check(marc::core::store_le(bytes,at,value),"test store");
}
bool marker(const PositionTans64mFrameLayout& l) {
    return l.header.raw_size==999 && l.requirements.aggregate_bytes==999;
}
PositionTans64mFrameLayout sentinel() {
    PositionTans64mFrameLayout l{};l.header.raw_size=999;l.requirements.aggregate_bytes=999;return l;
}
}
int main() {
    // Hand-decoded family headers specialized to DD-1606: singleton literal A.
    auto expected_stream=hex(
        "4d415243020000004000010002000a0005000500000010000000000010000000"
        "1000000000000000010000000000000010000000000000000000000000000000"
        "000010000300000002010000000000000c013400061200000000000000000000"
        "01001600000000000000000000000000");
    const auto expected_frame=hex(
        "4d52463240000000000000000000000001000000010000000200000002000000"
        "0200000024000000000000000000000000000000000000000000000000000000");
    auto descriptor=hex("02000000020000000c000000390000003d12000000000000090000000000000000000041");
    put(expected_stream,20,std::uint32_t{67108864});
    put(expected_stream,64,std::uint32_t{67108864});
    put(expected_stream,14,std::uint16_t{14});
    put(expected_stream,18,std::uint16_t{9});
    put(expected_stream,82,std::uint16_t{58});
    put(expected_stream,84,std::uint32_t{4680});
    put(expected_stream,98,std::uint16_t{26});
    put(descriptor,12,std::uint16_t{58});
    put(descriptor,16,std::uint32_t{4680});
    check(expected_stream.size()==112 && expected_frame.size()==64 && descriptor.size()==36,"frozen extents");
    marc::core::DecoderLimits limits{};limits.max_total_output_size=UINT64_C(1)<<40;
    limits.max_frame_size=67108864;limits.max_block_size=UINT64_C(8)*67108864;
    limits.max_compressed_payload_size=UINT64_C(512)*1024*1024;
    limits.max_internal_buffered_bytes=UINT64_C(1)<<30;
    limits.max_lz_distance=67108864;limits.max_entropy_table_entries=59*4096;
    PositionTans64mStreamHeader stream{};stream.original_size=1;
    PositionTans64mFrameHeader h{};
    h.raw_size=1;h.token_count=1;h.event_count=2;h.decision_count=2;h.payload_size=2;h.descriptor_size=36;
    const PositionTans64mFrameContext context{stream,limits,0,0};
    std::vector<std::byte> output(112,std::byte{0x5a});
    check(serialize_position_tans_64m_stream(stream,limits,output)==Error::none && output==expected_stream,"independent stream bytes");
    output.assign(64,std::byte{0x5a});
    check(serialize_position_tans_64m_frame_header(h,context,output)==Error::none && output==expected_frame,"independent frame bytes");
    PositionTans64mStreamHeader decoded{};std::size_t consumed=999;
    check(parse_position_tans_64m_stream(expected_stream,limits,decoded,consumed)==Error::none
        && consumed==112 && decoded.frame_size==67108864 && decoded.original_size==1,"parse frozen stream");
    std::vector<std::byte> frame=expected_frame;
    frame.insert(frame.end(),descriptor.begin(),descriptor.end());frame.insert(frame.end(),2,std::byte{0});
    auto l=sentinel();
    check(preflight_position_tans_64m_frame(frame,context,l)==Error::none
        && l.requirements.serialized_size==102 && l.requirements.prefix_size==100
        && l.requirements.decode_table_bytes==241664*sizeof(marc::entropy::internal::TansDecodeEntry),"complete extent and tables");
    const auto requirements=l.requirements;
    for (std::size_t n=0;n<112;++n) {
        PositionTans64mStreamHeader dest{};dest.original_size=999;consumed=999;
        check(parse_position_tans_64m_stream(std::span(expected_stream).first(n),limits,dest,consumed)==Error::truncated
            && dest.original_size==999 && consumed==999,"every stream cut unchanged");
    }
    for (std::size_t n=0;n<101;++n) {
        l=sentinel();
        check(preflight_position_tans_64m_frame(std::span(frame).first(n),context,l)==Error::truncated
            && marker(l),"every frame cut unchanged");
    }
    for (std::size_t n=0;n<112;++n) {
        output.assign(n,std::byte{0x5a});
        check(serialize_position_tans_64m_stream(stream,limits,output)==Error::output_too_small
            && std::all_of(output.begin(),output.end(),[](auto b){return b==std::byte{0x5a};}),"short stream destination");
    }
    for (std::size_t n=0;n<64;++n) {
        output.assign(n,std::byte{0x5a});
        check(serialize_position_tans_64m_frame_header(h,context,output)==Error::output_too_small
            && std::all_of(output.begin(),output.end(),[](auto b){return b==std::byte{0x5a};}),"short frame destination");
    }
    // Every identity, parameter and reserved byte; original size remains variable.
    for (std::size_t at=0;at<112;++at) {
        if ((at>=20 && at<24) || (at>=40 && at<48)) continue;
        auto bad=expected_stream;bad[at]^=std::byte{0x80};
        decoded.original_size=999;consumed=999;
        check(parse_position_tans_64m_stream(bad,limits,decoded,consumed)!=Error::none
            && decoded.original_size==999 && consumed==999,"stream identity mutation unchanged");
    }
    for (std::size_t at=0;at<64;++at) {
        auto bad=frame;bad[at]^=std::byte{0x80};l=sentinel();
        check(preflight_position_tans_64m_frame(bad,context,l)!=Error::none && marker(l),"frame mutation unchanged");
    }
    for (auto at : {0U,4U,8U,9U,10U,12U,14U,16U,20U,30U,31U}) {
        auto bad=frame;bad[64+at]^=std::byte{0x80};l=sentinel();
        check(preflight_position_tans_64m_frame_prefix(bad,context,l)==Error::invalid_descriptor
            && marker(l),"descriptor mutation before payload");
    }
    l=sentinel();
    check(preflight_position_tans_64m_frame_prefix(std::span(frame).first(100),context,l)==Error::none,"prefix needs no payload");
    auto damaged=frame;damaged[99]=std::byte{0xff};damaged[100]=std::byte{0xff};
    check(preflight_position_tans_64m_frame(damaged,context,l)==Error::none,"preflight does not claim entropy validation");
    auto followed=frame;followed.push_back(std::byte{0xff});
    check(preflight_position_tans_64m_frame(followed,context,l)==Error::none && l.requirements.serialized_size==102,"one frame from stream span");
    for (auto size : {0U,67108865U}) {
        auto bad=stream;bad.frame_size=size;
        auto permissive=limits;permissive.max_frame_size=67108865;
        check(validate_position_tans_64m_stream(bad,permissive)==Error::invalid_parameters,"invalid configured frame");
    }
    auto tight=limits;tight.max_block_size=65536;tight.max_internal_buffered_bytes=requirements.aggregate_bytes;
    auto tiny_stream=stream;tiny_stream.frame_size=1;
    const PositionTans64mFrameContext exact{tiny_stream,tight,0,0};
    PositionTans64mFrameRequirements r{};
    check(validate_position_tans_64m_frame_header(h,exact,r)==Error::none,"exact minimum private footprint");
    --tight.max_internal_buffered_bytes;r.aggregate_bytes=999;
    check(validate_position_tans_64m_frame_header(h,exact,r)==Error::limit_exceeded && r.aggregate_bytes==999,"one byte below footprint unchanged");
    tight=limits;tight.max_entropy_table_entries=59*4096-1;
    check(validate_position_tans_64m_stream(stream,tight)==Error::limit_exceeded,"all context tables charged");
    tight=limits;tight.max_lz_distance=16777215;
    check(validate_position_tans_64m_stream(stream,tight)==Error::limit_exceeded,"dictionary window limit");
    tight=limits;tight.max_lz_match_length=257;
    check(validate_position_tans_64m_stream(stream,tight)==Error::limit_exceeded,"dictionary match limit");
    for (int field=0;field<5;++field) {
        tight=limits;
        if (field==0) tight.max_frame_size=33554431;
        if (field==1) tight.max_block_size=33554431;
        if (field==2) { tight.max_frame_size=1;tight.max_total_output_size=1; }
        if (field==3) tight.max_compressed_payload_size=1;
        if (field==4) { tight.max_expansion_ratio=1;tight.expansion_slack=0; }
        auto candidate_stream=stream;auto candidate_header=h;
        if (field==2) candidate_stream.original_size=2;
        if (field==4) { candidate_stream.original_size=10;candidate_header.raw_size=10;candidate_header.token_count=10;
            candidate_header.event_count=candidate_header.decision_count=20; }
        const PositionTans64mFrameContext limited{candidate_stream,tight,0,0};
        r.aggregate_bytes=999;
        check(validate_position_tans_64m_frame_header(candidate_header,limited,r)==Error::limit_exceeded
            && r.aggregate_bytes==999,"configured frame/block/total/payload/expansion limits");
    }
    for (auto decision : {1U,33U,UINT32_MAX}) {
        auto bad=h;bad.event_count=bad.decision_count=decision;r.aggregate_bytes=999;
        check(validate_position_tans_64m_frame_header(bad,context,r)==Error::invalid_counts
            && r.aggregate_bytes==999,"decision raw lower and upper bounds");
    }
    for (auto size : {31U,9335U}) {
        auto bad=h;bad.descriptor_size=size;
        check(validate_position_tans_64m_frame_header(bad,context,r)==Error::invalid_counts,"descriptor finite extents");
    }
    const PositionTans64mFrameContext unaligned{stream,limits,0,1};
    check(validate_position_tans_64m_frame_header(h,unaligned,r)==Error::invalid_sequence,"committed extent cannot start frame");
    auto huge=stream;huge.original_size=UINT64_MAX;
    check(validate_position_tans_64m_stream(huge,limits)==Error::limit_exceeded,"original size bounded");
    // Exact two-frame partition and terminal refusal; empty stream has no frame.
    auto multi=stream;multi.frame_size=1;multi.original_size=2;auto second=h;second.sequence=1;
    const PositionTans64mFrameContext c2{multi,limits,1,1};
    check(validate_position_tans_64m_frame_header(second,c2,r)==Error::none,"second short frame");
    const PositionTans64mFrameContext wrong{multi,limits,0,1};
    check(validate_position_tans_64m_frame_header(second,wrong,r)==Error::invalid_sequence,"sequence exact");
    const PositionTans64mFrameContext ended{multi,limits,2,2};
    check(validate_position_tans_64m_frame_header(second,ended,r)==Error::invalid_sequence,"no frames after original");
    auto empty=stream;empty.original_size=0;
    const PositionTans64mFrameContext c0{empty,limits,0,0};
    check(validate_position_tans_64m_stream(empty,limits)==Error::none
        && validate_position_tans_64m_frame_header(h,c0,r)==Error::invalid_sequence,"empty stream only header");
    auto max=stream;max.original_size=67108864;auto maxh=h;maxh.raw_size=67108864;
    maxh.token_count=16777216;maxh.event_count=maxh.decision_count=67108864;
    maxh.payload_size=100663298;
    auto wide_limits=limits;wide_limits.max_frame_size=67108864;wide_limits.max_block_size=UINT64_C(1024)*1024*1024;
    wide_limits.max_compressed_payload_size=UINT64_C(512)*1024*1024;
    wide_limits.max_lz_distance=67108864;wide_limits.max_lz_match_length=67108864;
    wide_limits.max_internal_buffered_bytes=UINT64_C(1024)*1024*1024;
    const PositionTans64mFrameContext cm{max,wide_limits,0,0};
    check(validate_position_tans_64m_frame_header(maxh,cm,r)==Error::none,"wide decision and payload arithmetic");
    ++maxh.payload_size;
    check(validate_position_tans_64m_frame_header(maxh,cm,r)==Error::invalid_counts,"payload ceiling exact");
    maxh.token_count=20000000;maxh.event_count=maxh.decision_count=671088640;maxh.payload_size=503316482;maxh.descriptor_size=9334;
    check(validate_position_tans_64m_frame_header(maxh,cm,r)==Error::none
        && r.serialized_size==503325880,"ten decisions per raw format ceiling and serialized extent");
    ++maxh.event_count;++maxh.decision_count;r.aggregate_bytes=999;
    check(validate_position_tans_64m_frame_header(maxh,cm,r)==Error::invalid_counts
        && r.aggregate_bytes==999,"one beyond ten-per-raw retains requirements");
    auto four=stream;four.original_size=4;auto per_token=h;per_token.raw_size=4;
    per_token.event_count=per_token.decision_count=34;per_token.payload_size=53;
    const PositionTans64mFrameContext c4{four,limits,0,0};
    check(validate_position_tans_64m_frame_header(per_token,c4,r)==Error::none,"thirty-four decisions per token metadata ceiling");
    per_token.event_count=per_token.decision_count=35;r.aggregate_bytes=999;
    check(validate_position_tans_64m_frame_header(per_token,c4,r)==Error::invalid_counts
        && r.aggregate_bytes==999,"per-token ceiling independent of raw bound");
    maxh=h;maxh.raw_size=67108864;maxh.token_count=67108864;maxh.event_count=maxh.decision_count=UINT32_MAX;
    check(validate_position_tans_64m_frame_header(maxh,cm,r)==Error::invalid_counts,"oversized count safe");
    return failures ? 1 : 0;
}
