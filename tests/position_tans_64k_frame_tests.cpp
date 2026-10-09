#include "frame/lzss_position_tans_64k_frame.hpp"
#include "core/endian.hpp"
#include "dictionary/lzss_short_length_escape_candidate.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
using namespace marc::frame::internal;
using namespace marc::entropy::internal;
using namespace marc::dictionary::internal;
using Error=PositionTans64kFrameError;
int failures{};
void check(bool ok,const char* text) { if (!ok) { ++failures;std::cerr<<text<<'\n'; } }
constexpr auto marker=std::byte{0x5a};
bool marked(std::span<const std::byte> bytes) {
    return std::all_of(bytes.begin(),bytes.end(),[](auto b){return b==marker;});
}
int fixture(const char* path,bool raw_parse=false) {
    std::ifstream file(path,std::ios::binary);std::vector<char> chars((std::istreambuf_iterator<char>(file)),{});
    if (!file || chars.size()<120 || chars.size()>(UINT64_C(2)<<20)) return 2;
    std::vector<std::byte> data(chars.size());
    std::transform(chars.begin(),chars.end(),data.begin(),[](char c){return static_cast<std::byte>(c);});
    std::uint32_t fs{},rs{};(void)marc::core::load_le(data,0,fs);(void)marc::core::load_le(data,4,rs);
    if (rs>65536 || UINT64_C(120)+fs+rs!=data.size()) return 3;
    const auto bytes=std::span<const std::byte>(data);
    marc::core::DecoderLimits limits{};PositionTans64kStreamHeader stream{};std::size_t n{};
    if (parse_position_tans_64k_stream(bytes.subspan(8,112),limits,stream,n)!=PositionTans64kFormatError::none
        || n!=112 || stream.original_size!=rs) return 4;
    if (rs==0) {
        if (fs!=0) return 5;
        if (raw_parse) {
            const auto a=tokenize_lzss_short_length_escape_candidate({},stream.dictionary,limits,5,{});
            const auto b=tokenize_lzss_short_length_escape_candidate_indexed({},stream.dictionary,limits,5,{},{});
            const auto c=tokenize_lzss_short_length_escape_candidate_indexed_scratch({},stream.dictionary,limits,5,{},{});
            if (a.error!=LzssShortMatchCandidateError::none || b.error!=LzssShortMatchCandidateError::none
                || c.error!=LzssShortMatchCandidateError::none || a.token_count || b.token_count || c.token_count) return 13;
        }
        return 0;
    }
    const PositionTans64kFrameContext context{stream,limits,0,0};
    const auto frame=bytes.subspan(120,fs);const auto expected=bytes.subspan(120+fs,rs);
    PositionTans64kFrameLayout layout{};
    if (preflight_position_tans_64k_frame(frame,context,layout)!=PositionTans64kFormatError::none
        || layout.requirements.serialized_size!=fs) return 6;
    std::vector<TansDecodeEntry> dt(position_tans_64k_table_entries);
    std::vector<LzssTypedToken> tokens(layout.header.token_count);
    std::vector<std::byte> raw(rs,marker),output(rs+2,marker);
    auto decoded=decode_position_tans_64k_frame(frame,context,dt,tokens,raw,output);
    if (decoded.error!=Error::none || decoded.raw_size!=rs || decoded.serialized_size!=fs
        || !std::equal(expected.begin(),expected.end(),output.begin()) || output[rs]!=marker || output[rs+1]!=marker) return 7;
    std::fill(raw.begin(),raw.end(),marker);
    if (decode_position_tans_64k_frame_scratch(frame,context,dt,tokens,raw).error!=Error::none
        || !std::equal(expected.begin(),expected.end(),raw.begin())) return 8;
    if (raw_parse) {
        const auto f=calculate_lzss_short_prefix_workspace(rs,stream.dictionary,limits,
            LzssTypedTokenVariant::field_context_64k_short_length_escape);
        if (f.error!=LzssShortPrefixError::none) return 10;
        std::vector<std::uint32_t> storage((f.workspace_size+3)/4);
        auto workspace=std::as_writable_bytes(std::span(storage)).first(f.workspace_size);
        std::vector<LzssTypedToken> reference(rs),indexed(rs),private_tokens(rs);
        const auto a=tokenize_lzss_short_length_escape_candidate(expected,stream.dictionary,limits,5,reference);
        const auto b=tokenize_lzss_short_length_escape_candidate_indexed(expected,stream.dictionary,limits,5,indexed,workspace);
        const auto c=tokenize_lzss_short_length_escape_candidate_indexed_scratch(expected,stream.dictionary,limits,5,private_tokens,workspace);
        if (a.error!=LzssShortMatchCandidateError::none || b.error!=LzssShortMatchCandidateError::none
            || c.error!=LzssShortMatchCandidateError::none || a.token_count!=tokens.size()
            || b.token_count!=tokens.size() || c.token_count!=tokens.size()) return 11;
        const auto equal=[](const auto& x,const auto& y) { return x.kind==y.kind && x.literal==y.literal && x.distance==y.distance && x.length==y.length; };
        for (const auto& sequence : {std::span(reference),std::span(indexed),std::span(private_tokens)})
            if (!std::equal(tokens.begin(),tokens.end(),sequence.begin(),equal)) return 12;
    }
    // Re-encode restored tokens and compare complete independently authored frame.
    std::vector<std::uint16_t> et(position_tans_64k_table_entries);
    std::vector<std::byte> payload(layout.header.payload_size),scratch(fs),encoded(fs+2,marker);
    const auto result=encode_position_tans_64k_frame(tokens,context,et,payload,scratch,encoded);
    if (result.error!=Error::none || result.serialized_size!=fs || result.raw_size!=rs
        || !std::equal(frame.begin(),frame.end(),encoded.begin()) || encoded[fs]!=marker || encoded[fs+1]!=marker) return 9;
    return 0;
}
}
int main(int argc,char** argv) {
    if (argc==3 && std::string(argv[1])=="--fixture") return fixture(argv[2]);
    if (argc==3 && std::string(argv[1])=="--raw-fixture") return fixture(argv[2],true);
    marc::core::DecoderLimits limits{};
    PositionTans64kStreamHeader stream{};stream.original_size=1;
    const PositionTans64kFrameContext context{stream,limits,0,0};
    std::vector<LzssTypedToken> tokens{{LzssTypedTokenKind::literal,65,0,0}};
    std::vector<std::uint16_t> et(position_tans_64k_table_entries);
    std::vector<TansDecodeEntry> dt(position_tans_64k_table_entries);
    std::vector<std::byte> payload(2,marker),scratch(100,marker),frame(102,marker);
    const auto encoded=encode_position_tans_64k_frame(tokens,context,et,payload,scratch,frame);
    check(encoded.error==Error::none && encoded.serialized_size==100 && frame[100]==marker && frame[101]==marker,"complete frame success and tail");
    frame.resize(100);
    std::vector<LzssTypedToken> temporary(1);
    std::vector<std::byte> raw(1,marker),output(3,marker);
    auto good=decode_position_tans_64k_frame(frame,context,dt,temporary,raw,output);
    check(good.error==Error::none && good.raw_size==1 && good.serialized_size==100
        && output[0]==std::byte{65} && output[1]==marker && output[2]==marker,"ordinary frame success only prefix");
    for (std::size_t n=0;n<frame.size();++n) {
        std::fill(output.begin(),output.end(),marker);std::fill(raw.begin(),raw.end(),marker);
        const auto r=decode_position_tans_64k_frame(std::span(frame).first(n),context,dt,temporary,raw,output);
        check(r.error!=Error::none && r.raw_size==0 && r.serialized_size==0 && marked(output) && marked(raw),"every frame cut unpublished");
    }
    auto late=frame;late[98]=std::byte{1};std::fill(output.begin(),output.end(),marker);
    const auto failed=decode_position_tans_64k_frame(late,context,dt,temporary,raw,output);
    check(failed.error==Error::token_error && failed.token_result.token_count==1
        && failed.token_result.entropy_error==PositionTans64kError::invalid_terminal_state
        && failed.raw_size==0 && failed.serialized_size==0 && temporary[0].literal==65 && marked(output) && marked(raw),
        "late finish failure after all private tokens keeps raw unpublished");
    check(decode_position_tans_64k_frame_scratch(late,context,dt,temporary,raw).error==Error::token_error
        && marked(raw),"scratch frame never restores before entropy finish");
    auto invalid=frame;invalid[98]=invalid[99]=std::byte{0xff};
    check(decode_position_tans_64k_frame(invalid,context,dt,temporary,raw,output).error==Error::token_error
        && marked(output),"invalid state unpublished");
    std::vector<std::byte> short_output;
    check(decode_position_tans_64k_frame(frame,context,dt,temporary,raw,short_output).error==Error::output_too_small
        && marked(raw),"short ordinary raw preflight");
    check(decode_position_tans_64k_frame(frame,context,dt,temporary,raw,raw).error==Error::overlapping_buffers
        && marked(raw),"private raw and ordinary alias rejected");
    auto alias=frame;
    check(decode_position_tans_64k_frame(alias,context,dt,temporary,raw,std::span(alias).first(1)).error==Error::overlapping_buffers
        && alias==frame,"input/output alias rejected unchanged");
    check(decode_position_tans_64k_frame(frame,context,std::span(dt).first(dt.size()-1),temporary,raw,output).error==Error::token_error
        && marked(output) && marked(raw),"short private table bank unpublished");
    std::vector<LzssTypedToken> short_tokens;
    check(decode_position_tans_64k_frame(frame,context,dt,short_tokens,raw,output).error==Error::output_too_small
        && marked(output),"short private token span unpublished");
    std::vector<std::byte> short_raw;
    check(decode_position_tans_64k_frame(frame,context,dt,temporary,short_raw,output).error==Error::output_too_small
        && marked(output),"short private raw span unpublished");
    // A mathematically valid entropy frame with a match before any history.
    PositionTans64kModelBuilder builder;
    const std::array<std::pair<int,std::uint32_t>,4> invalid_events{{{0,1},{20,8},{-1,0},{31,0}}};
    for (const auto [c,s]:invalid_events) check(builder.add(c,s)==PositionTans64kError::none,"negative model decision");
    PositionTans64kDescriptor model{};
    check(builder.finish(model)==PositionTans64kError::none
        && build_position_tans_64k_encode_tables(model,limits,et)==PositionTans64kError::none,"negative model tables");
    PositionTans64kReverseWriter measure(model,limits,et,{});
    for (auto i=invalid_events.size();i;--i) check(measure.add(invalid_events[i-1].first,invalid_events[i-1].second)==PositionTans64kError::none,"negative model measure");
    std::size_t ps{};std::uint8_t valid{};
    check(measure.finish(ps,valid)==PositionTans64kError::none,"negative payload size");
    model.payload_size=static_cast<std::uint32_t>(ps);model.final_valid_bits=valid;
    std::vector<std::byte> negative_payload(ps),negative_descriptor(9122);std::size_t ds{};
    PositionTans64kReverseWriter writer(model,limits,et,negative_payload);
    for (auto i=invalid_events.size();i;--i) check(writer.add(invalid_events[i-1].first,invalid_events[i-1].second)==PositionTans64kError::none,"negative payload write");
    check(writer.finish(ps,valid)==PositionTans64kError::none
        && serialize_position_tans_64k_descriptor(model,limits,negative_descriptor,ds)==PositionTans64kError::none,"negative descriptor serialize");
    auto negative_stream=stream;negative_stream.original_size=3;
    const PositionTans64kFrameContext negative_context{negative_stream,limits,0,0};
    PositionTans64kFrameHeader nh{0,0,3,1,4,4,static_cast<std::uint32_t>(ps),static_cast<std::uint32_t>(ds),0,0};
    std::vector<std::byte> negative_frame(64+ds+ps),negative_raw(3,marker),negative_output(5,marker);
    check(serialize_position_tans_64k_frame_header(nh,negative_context,negative_frame)==PositionTans64kFormatError::none,"negative frame header");
    std::copy_n(negative_descriptor.begin(),ds,negative_frame.begin()+64);
    std::copy(negative_payload.begin(),negative_payload.end(),negative_frame.begin()+64+ds);
    const auto reference_error=decode_position_tans_64k_frame(negative_frame,negative_context,dt,temporary,negative_raw,negative_output);
    check(reference_error.error==Error::token_error && reference_error.token_result.token_error==LzssTypedTokenError::invalid_distance
        && reference_error.raw_size==0 && marked(negative_raw) && marked(negative_output),"valid entropy invalid reference unpublished");
    negative_frame.back()|=std::byte{0x80};
    const auto padding_error=decode_position_tans_64k_frame(negative_frame,negative_context,dt,temporary,negative_raw,negative_output);
    check(padding_error.error==Error::token_error && padding_error.token_result.entropy_error==PositionTans64kError::nonzero_padding
        && marked(negative_output),"nonzero high padding unpublished");
    std::fill(frame.begin(),frame.end(),marker);
    auto bad_tokens=tokens;bad_tokens[0]={LzssTypedTokenKind::match,0,1,3};
    check(encode_position_tans_64k_frame(bad_tokens,context,et,payload,scratch,frame).error==Error::token_error && marked(frame),"invalid history encode unchanged");
    for (std::size_t n=0;n<100;++n) {
        std::vector<std::byte> short_frame(n,marker);
        const auto r=encode_position_tans_64k_frame(tokens,context,et,payload,scratch,short_frame);
        check(r.error==Error::output_too_small && r.raw_size==0 && r.serialized_size==0 && marked(short_frame),"every short serialized output unchanged");
    }
    check(encode_position_tans_64k_frame(tokens,context,et,payload,scratch,scratch).error==Error::overlapping_buffers,"private serialized and ordinary alias");
    const auto restored=encode_position_tans_64k_frame(tokens,context,et,payload,scratch,frame);
    check(restored.error==Error::none,"restore valid fixture");
    // Actual retained spans + controls + bounded local work, not a public grant.
    auto exact=limits;exact.max_block_size=65536;
    exact.max_internal_buffered_bytes=131072+sizeof(PositionTans64kDescriptor)
        +sizeof(marc::context::internal::PositionTans64kTokenContext)+sizeof(LzssTypedFrameValidationContext)
        +frame.size()+dt.size()*sizeof(TansDecodeEntry)+temporary.size()*sizeof(LzssTypedToken)+raw.size()+output.size()
        +sizeof(PositionTans64kFrameContext)+sizeof(stream)+sizeof(limits);
    const PositionTans64kFrameContext tight{stream,exact,0,0};
    check(decode_position_tans_64k_frame(frame,tight,dt,temporary,raw,output).error==Error::none,"exact aggregate decoder budget");
    --exact.max_internal_buffered_bytes;std::fill(output.begin(),output.end(),marker);
    std::fill(raw.begin(),raw.end(),marker);std::fill(dt.begin(),dt.end(),TansDecodeEntry{123,45,6});
    check(decode_position_tans_64k_frame(frame,tight,dt,temporary,raw,output).error==Error::limit_exceeded
        && marked(output) && marked(raw) && dt[0].state_base==123,"one byte below rejects before scratch prep");
    auto second_stream=stream;second_stream.frame_size=1;second_stream.original_size=2;
    const PositionTans64kFrameContext second{second_stream,limits,1,1};
    check(encode_position_tans_64k_frame(tokens,second,et,payload,scratch,frame).error==Error::none
        && decode_position_tans_64k_frame(frame,second,dt,temporary,raw,output).error==Error::none,"later frame resets models and restores");
    auto stale=tokens;stale[0]={LzssTypedTokenKind::match,0,1,3};
    std::fill(frame.begin(),frame.end(),marker);
    check(encode_position_tans_64k_frame(stale,second,et,payload,scratch,frame).error!=Error::none
        && marked(frame),"previous frame provides no match history");
    return failures ? 1 : 0;
}
