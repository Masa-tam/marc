#include "frame/lzss_position_tans_8m_frame.hpp"
#include "core/endian.hpp"
#include <algorithm>
#include <iostream>
#include <vector>

namespace {
using namespace marc::frame::internal;
using namespace marc::entropy::internal;
using namespace marc::dictionary::internal;
using Error=PositionTans8mFrameError;
int failures{};
void check(bool ok,const char* text) { if (!ok) { ++failures;std::cerr<<text<<'\n'; } }
constexpr auto marker=std::byte{0x5a};
bool marked(std::span<const std::byte> bytes) {
    return std::all_of(bytes.begin(),bytes.end(),[](auto b){return b==marker;});
}
}
int main() {
    marc::core::DecoderLimits limits{};limits.max_block_size=UINT64_C(8)*8388608;
    PositionTans8mStreamHeader stream{};stream.original_size=1;
    const PositionTans8mFrameContext context{stream,limits,0,0};
    std::vector<LzssTypedToken> tokens{{LzssTypedTokenKind::literal,65,0,0}};
    std::vector<std::uint16_t> et(position_tans_8m_table_entries);
    std::vector<TansDecodeEntry> dt(position_tans_8m_table_entries);
    std::vector<std::byte> payload(2,marker),scratch(101,marker),frame(103,marker);
    const auto encoded=encode_position_tans_8m_frame(tokens,context,et,payload,scratch,frame);
    check(encoded.error==Error::none && encoded.serialized_size==101 && frame[101]==marker && frame[102]==marker,"complete frame success and tail");
    frame.resize(101);
    std::vector<LzssTypedToken> temporary(1);
    std::vector<std::byte> raw(1,marker),output(3,marker);
    auto good=decode_position_tans_8m_frame(frame,context,dt,temporary,raw,output);
    check(good.error==Error::none && good.raw_size==1 && good.serialized_size==101
        && output[0]==std::byte{65} && output[1]==marker && output[2]==marker,"ordinary frame success only prefix");
    for (std::size_t n=0;n<frame.size();++n) {
        std::fill(output.begin(),output.end(),marker);std::fill(raw.begin(),raw.end(),marker);
        const auto r=decode_position_tans_8m_frame(std::span(frame).first(n),context,dt,temporary,raw,output);
        check(r.error!=Error::none && r.raw_size==0 && r.serialized_size==0 && marked(output) && marked(raw),"every frame cut unpublished");
    }
    auto late=frame;late[99]=std::byte{1};std::fill(output.begin(),output.end(),marker);
    const auto failed=decode_position_tans_8m_frame(late,context,dt,temporary,raw,output);
    check(failed.error==Error::token_error && failed.token_result.token_count==1
        && failed.token_result.entropy_error==PositionTans8mError::invalid_terminal_state
        && failed.raw_size==0 && failed.serialized_size==0 && temporary[0].literal==65 && marked(output) && marked(raw),
        "late finish failure after all private tokens keeps raw unpublished");
    check(decode_position_tans_8m_frame_scratch(late,context,dt,temporary,raw).error==Error::token_error
        && marked(raw),"scratch frame never restores before entropy finish");
    auto invalid=frame;invalid[99]=invalid[100]=std::byte{0xff};
    check(decode_position_tans_8m_frame(invalid,context,dt,temporary,raw,output).error==Error::token_error
        && marked(output),"invalid state unpublished");
    std::vector<std::byte> short_output;
    check(decode_position_tans_8m_frame(frame,context,dt,temporary,raw,short_output).error==Error::output_too_small
        && marked(raw),"short ordinary raw preflight");
    check(decode_position_tans_8m_frame(frame,context,dt,temporary,raw,raw).error==Error::overlapping_buffers
        && marked(raw),"private raw and ordinary alias rejected");
    auto alias=frame;
    check(decode_position_tans_8m_frame(alias,context,dt,temporary,raw,std::span(alias).first(1)).error==Error::overlapping_buffers
        && alias==frame,"input/output alias rejected unchanged");
    check(decode_position_tans_8m_frame(frame,context,std::span(dt).first(dt.size()-1),temporary,raw,output).error==Error::token_error
        && marked(output) && marked(raw),"short private table bank unpublished");
    std::vector<LzssTypedToken> short_tokens;
    check(decode_position_tans_8m_frame(frame,context,dt,short_tokens,raw,output).error==Error::output_too_small
        && marked(output),"short private token span unpublished");
    std::vector<std::byte> short_raw;
    check(decode_position_tans_8m_frame(frame,context,dt,temporary,short_raw,output).error==Error::output_too_small
        && marked(output),"short private raw span unpublished");
    // A mathematically valid entropy frame with a match before any history.
    PositionTans8mModelBuilder builder;
    const std::array<std::pair<int,std::uint32_t>,4> invalid_events{{{0,1},{20,8},{-1,0},{31,0}}};
    for (const auto [c,s]:invalid_events) check(builder.add(c,s)==PositionTans8mError::none,"negative model decision");
    PositionTans8mDescriptor model{};
    check(builder.finish(model)==PositionTans8mError::none
        && build_position_tans_8m_encode_tables(model,limits,et)==PositionTans8mError::none,"negative model tables");
    PositionTans8mReverseWriter measure(model,limits,et,{});
    for (auto i=invalid_events.size();i;--i) check(measure.add(invalid_events[i-1].first,invalid_events[i-1].second)==PositionTans8mError::none,"negative model measure");
    std::size_t ps{};std::uint8_t valid{};
    check(measure.finish(ps,valid)==PositionTans8mError::none,"negative payload size");
    model.payload_size=static_cast<std::uint32_t>(ps);model.final_valid_bits=valid;
    std::vector<std::byte> negative_payload(ps),negative_descriptor(9270);std::size_t ds{};
    PositionTans8mReverseWriter writer(model,limits,et,negative_payload);
    for (auto i=invalid_events.size();i;--i) check(writer.add(invalid_events[i-1].first,invalid_events[i-1].second)==PositionTans8mError::none,"negative payload write");
    check(writer.finish(ps,valid)==PositionTans8mError::none
        && serialize_position_tans_8m_descriptor(model,limits,negative_descriptor,ds)==PositionTans8mError::none,"negative descriptor serialize");
    auto negative_stream=stream;negative_stream.original_size=3;
    const PositionTans8mFrameContext negative_context{negative_stream,limits,0,0};
    PositionTans8mFrameHeader nh{0,0,3,1,4,4,static_cast<std::uint32_t>(ps),static_cast<std::uint32_t>(ds),0,0};
    std::vector<std::byte> negative_frame(64+ds+ps),negative_raw(3,marker),negative_output(5,marker);
    check(serialize_position_tans_8m_frame_header(nh,negative_context,negative_frame)==PositionTans8mFormatError::none,"negative frame header");
    std::copy_n(negative_descriptor.begin(),ds,negative_frame.begin()+64);
    std::copy(negative_payload.begin(),negative_payload.end(),negative_frame.begin()+64+ds);
    const auto reference_error=decode_position_tans_8m_frame(negative_frame,negative_context,dt,temporary,negative_raw,negative_output);
    check(reference_error.error==Error::token_error && reference_error.token_result.token_error==LzssTypedTokenError::invalid_distance
        && reference_error.raw_size==0 && marked(negative_raw) && marked(negative_output),"valid entropy invalid reference unpublished");
    negative_frame.back()|=std::byte{0x80};
    const auto padding_error=decode_position_tans_8m_frame(negative_frame,negative_context,dt,temporary,negative_raw,negative_output);
    check(padding_error.error==Error::token_error && padding_error.token_result.entropy_error==PositionTans8mError::nonzero_padding
        && marked(negative_output),"nonzero high padding unpublished");
    std::fill(frame.begin(),frame.end(),marker);
    auto bad_tokens=tokens;bad_tokens[0]={LzssTypedTokenKind::match,0,1,3};
    check(encode_position_tans_8m_frame(bad_tokens,context,et,payload,scratch,frame).error==Error::token_error && marked(frame),"invalid history encode unchanged");
    for (std::size_t n=0;n<101;++n) {
        std::vector<std::byte> short_frame(n,marker);
        const auto r=encode_position_tans_8m_frame(tokens,context,et,payload,scratch,short_frame);
        check(r.error==Error::output_too_small && r.raw_size==0 && r.serialized_size==0 && marked(short_frame),"every short serialized output unchanged");
    }
    check(encode_position_tans_8m_frame(tokens,context,et,payload,scratch,scratch).error==Error::overlapping_buffers,"private serialized and ordinary alias");
    const auto restored=encode_position_tans_8m_frame(tokens,context,et,payload,scratch,frame);
    check(restored.error==Error::none,"restore valid fixture");
    // Actual retained spans + controls + bounded local work, not a public grant.
    auto exact=limits;exact.max_block_size=65536;
    exact.max_internal_buffered_bytes=131072+sizeof(PositionTans8mDescriptor)
        +sizeof(marc::context::internal::PositionTans8mTokenContext)+sizeof(LzssTypedFrameValidationContext)
        +frame.size()+dt.size()*sizeof(TansDecodeEntry)+temporary.size()*sizeof(LzssTypedToken)+raw.size()+output.size()
        +sizeof(PositionTans8mFrameContext)+sizeof(stream)+sizeof(limits);
    auto tiny_stream=stream;tiny_stream.frame_size=1;
    const PositionTans8mFrameContext tight{tiny_stream,exact,0,0};
    check(decode_position_tans_8m_frame(frame,tight,dt,temporary,raw,output).error==Error::none,"exact aggregate decoder budget");
    --exact.max_internal_buffered_bytes;std::fill(output.begin(),output.end(),marker);
    std::fill(raw.begin(),raw.end(),marker);std::fill(dt.begin(),dt.end(),TansDecodeEntry{123,45,6});
    check(decode_position_tans_8m_frame(frame,tight,dt,temporary,raw,output).error==Error::limit_exceeded
        && marked(output) && marked(raw) && dt[0].state_base==123,"one byte below rejects before scratch prep");
    auto second_stream=stream;second_stream.frame_size=1;second_stream.original_size=2;
    const PositionTans8mFrameContext second{second_stream,limits,1,1};
    check(encode_position_tans_8m_frame(tokens,second,et,payload,scratch,frame).error==Error::none
        && decode_position_tans_8m_frame(frame,second,dt,temporary,raw,output).error==Error::none,"later frame resets models and restores");
    auto stale=tokens;stale[0]={LzssTypedTokenKind::match,0,1,3};
    std::fill(frame.begin(),frame.end(),marker);
    check(encode_position_tans_8m_frame(stale,second,et,payload,scratch,frame).error!=Error::none
        && marked(frame),"previous frame provides no match history");
    return failures ? 1 : 0;
}



