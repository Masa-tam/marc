#include "frame/lzss_position_tans_1m_stream_decoder.hpp"
#include <algorithm>
#include <iostream>
#include <vector>

namespace {
using namespace marc::frame::internal;
using namespace marc::entropy::internal;
using namespace marc::dictionary::internal;
using namespace marc::core;
constexpr auto end=flag_value(ProcessFlags::end_input);
constexpr auto flush=flag_value(ProcessFlags::flush);
constexpr auto marker=std::byte{0x5a};
int failures{};
void check(bool condition,const char* name) { if (!condition) { ++failures;std::cerr<<name<<'\n'; } }
struct Run { std::vector<std::byte> raw;StreamStatus status{};StreamError error{};std::size_t consumed{}; };
Run run(std::span<const std::byte> input,std::size_t first,std::size_t capacity,bool one_byte=false,std::size_t frame_capacity=1) {
    const auto extent=frame_capacity==1 ? 101 : 64+9207+2+(96*frame_capacity+7)/8;
    std::vector<std::byte> serialized(extent,marker),raw(frame_capacity,marker),output(capacity,marker);
    std::vector<LzssTypedToken> tokens(frame_capacity);
    std::vector<TansDecodeEntry> tables(position_tans_1m_table_entries);
    DecoderLimits limits{};limits.max_block_size=UINT64_C(8)*1048576;
    PositionTans1mStreamDecoder decoder(limits,serialized,tokens,raw,tables);
    Run result{};std::size_t boundary=first;
    for (std::size_t calls=0;calls<2*(input.size()+frame_capacity+1);++calls) {
        if (result.consumed==boundary) boundary=one_byte ? std::min(input.size(),boundary+1) : input.size();
        const auto chunk=input.subspan(result.consumed,boundary-result.consumed);
        std::fill(output.begin(),output.end(),marker);
        const auto r=decoder.process(chunk,output,(boundary==input.size() ? end : 0)|flush);
        check(r.input_consumed<=chunk.size() && r.output_produced<=output.size()
            && !(r.status==StreamStatus::progress && r.input_consumed==0 && r.output_produced==0),"process counts and progress");
        check(std::all_of(output.begin()+r.output_produced,output.end(),[](auto v){return v==marker;}),"unproduced output tail unchanged");
        result.consumed+=r.input_consumed;
        result.raw.insert(result.raw.end(),output.begin(),output.begin()+r.output_produced);
        result.status=r.status;result.error=r.error;
        if (r.status==StreamStatus::error || r.status==StreamStatus::end_of_stream) {
            std::fill(output.begin(),output.end(),marker);
            const auto again=decoder.process({},output,end);
            check(again.status==r.status && again.input_consumed==0 && again.output_produced==0
                && again.error.code==r.error.code && again.error.byte_position==r.error.byte_position,"sticky terminal state");
            check(std::all_of(output.begin(),output.end(),[](auto v){return v==marker;}),"terminal call unchanged");
            return result;
        }
    }
    check(false,"bounded driver completed");return result;
}
}
int check_position_tans_1m_stream_fixture(std::span<const std::byte> stream,std::span<const std::byte> expected) {
    const auto r=run(stream,1,1,true,std::max<std::size_t>(1,expected.size()));
    return failures==0 && r.status==StreamStatus::end_of_stream && r.consumed==stream.size()
        && std::equal(r.raw.begin(),r.raw.end(),expected.begin(),expected.end()) ? 0 : 1;
}
int check_position_tans_1m_failed_stream_fixture(std::span<const std::byte> stream,std::size_t frame_capacity) {
    const auto whole=run(stream,stream.size(),7,false,frame_capacity);
    const auto split=run(stream,1,1,true,frame_capacity);
    return failures==0 && whole.status==StreamStatus::error && split.status==StreamStatus::error
        && whole.raw.empty() && split.raw.empty() && whole.error.code==ErrorCode::malformed_stream
        && whole.error.code==split.error.code && whole.error.byte_position==split.error.byte_position
        && whole.consumed==split.consumed ? 0 : 1;
}

int position_tans_1m_stream_tests() {
    DecoderLimits limits{};PositionTans1mStreamHeader stream{};stream.frame_size=1;stream.original_size=2;
    std::vector<std::byte> bytes(314),serialized(101),payload(2),scratch(101);
    check(serialize_position_tans_1m_stream(stream,limits,bytes)==PositionTans1mFormatError::none,"stream fixture header");
    std::vector<std::uint16_t> et(position_tans_1m_table_entries);
    for (std::uint64_t i=0;i<2;++i) {
        const std::array<LzssTypedToken,1> tokens{{{LzssTypedTokenKind::literal,static_cast<std::uint8_t>(65+i),0,0}}};
        const auto encoded=encode_position_tans_1m_frame(tokens,{stream,limits,i,i},et,payload,scratch,serialized);
        check(encoded.error==PositionTans1mFrameError::none && encoded.serialized_size==101,"two reset frames fixture");
        std::copy(serialized.begin(),serialized.end(),bytes.begin()+112+101*i);
    }
    const std::vector<std::byte> expected{std::byte{65},std::byte{66}};
    for (std::size_t split=0;split<=bytes.size();++split) for (auto capacity : {1U,2U,7U}) {
        const auto r=run(bytes,split,capacity);
        check(r.status==StreamStatus::end_of_stream && r.consumed==bytes.size() && r.raw==expected,"every split and output capacity");
    }
    check(run(bytes,1,1,true).raw==expected,"one-byte input/output");
    // Length-258 overlapping copy and literal history across matches.
    auto mixed_stream=stream;mixed_stream.frame_size=264;mixed_stream.original_size=264;
    const std::array<LzssTypedToken,4> mixed_tokens{{{LzssTypedTokenKind::literal,65,0,0},
        {LzssTypedTokenKind::match,0,1,258},{LzssTypedTokenKind::literal,66,0,0},{LzssTypedTokenKind::match,0,3,4}}};
    std::vector<std::byte> mixed_frame(1024),mixed_payload(1024),mixed_scratch(1024);
    const auto mixed=encode_position_tans_1m_frame(mixed_tokens,{mixed_stream,limits,0,0},et,mixed_payload,mixed_scratch,mixed_frame);
    check(mixed.error==PositionTans1mFrameError::none,"mixed match frame fixture");
    mixed_frame.resize(mixed.serialized_size);
    std::vector<std::byte> mixed_bytes(112);
    check(serialize_position_tans_1m_stream(mixed_stream,limits,mixed_bytes)==PositionTans1mFormatError::none,"mixed stream header");
    mixed_bytes.insert(mixed_bytes.end(),mixed_frame.begin(),mixed_frame.end());
    std::vector<std::byte> mixed_expected(259,std::byte{65});
    mixed_expected.insert(mixed_expected.end(),{std::byte{66},std::byte{65},std::byte{65},std::byte{66},std::byte{65}});
    for (std::size_t split=0;split<=mixed_bytes.size();++split) for (auto capacity : {1U,7U}) {
        const auto m=run(mixed_bytes,split,capacity,false,264);
        check(m.status==StreamStatus::end_of_stream && m.raw==mixed_expected,"every mixed-frame split and byte drain");
    }
    check(run(mixed_bytes,1,1,true,264).raw==mixed_expected,"mixed one-byte source/sink");
    for (std::size_t cut=0;cut<bytes.size();++cut) {
        const auto r=run(std::span(bytes).first(cut),cut,2);
        const std::vector<std::byte> committed=cut>=213 ? std::vector<std::byte>{std::byte{65}} : std::vector<std::byte>{};
        check(r.status==StreamStatus::error && r.error.code==ErrorCode::malformed_stream && r.raw==committed,
            "every truncation retains only earlier validated frames");
    }
    auto bad=bytes;bad[312]=std::byte{1};
    const auto late=run(bad,1,1,true);
    check(late.status==StreamStatus::error && late.error.code==ErrorCode::malformed_stream
        && late.error.byte_position==312 && late.raw==std::vector<std::byte>{std::byte{65}},"late second-frame finish error never drains B");
    bad=bytes;bad[211]=std::byte{1};
    check(run(bad,bytes.size(),2).raw.empty(),"late first frame finish error publishes nothing");
    bad=bytes;bad[277]=std::byte{0xff};
    check(run(bad,bad.size(),2).raw==std::vector<std::byte>{std::byte{65}},"second descriptor rejects before payload");
    bad=bytes;bad.push_back(std::byte{0});
    const auto trailing=run(bad,bad.size(),2);
    check(trailing.status==StreamStatus::error && trailing.raw==expected && trailing.consumed==314,"strict trailing bytes after validated frames");
    auto empty=stream;empty.original_size=0;
    std::vector<std::byte> header(112);
    check(serialize_position_tans_1m_stream(empty,limits,header)==PositionTans1mFormatError::none
        && run(header,1,1,true).status==StreamStatus::end_of_stream,"empty stream EOS");
    std::vector<TansDecodeEntry> dt(position_tans_1m_table_entries);
    std::vector<LzssTypedToken> tokens(1);std::vector<std::byte> raw(1,marker),output(2,marker);
    PositionTans1mStreamDecoder decoder(limits,serialized,tokens,raw,dt);
    auto r=decoder.process(bytes,{},end);
    check(r.status==StreamStatus::need_output && r.input_consumed==213 && r.output_produced==0,"zero output stops at validated first frame");
    auto last=decoder.process(std::span(bytes).subspan(r.input_consumed),output,end);
    check(last.status==StreamStatus::end_of_stream && last.input_consumed==101 && last.output_produced==2 && output==expected,"drain then consume final suffix");
    PositionTans1mStreamDecoder awaiting(limits,serialized,tokens,raw,dt);
    r=awaiting.process(bytes,output,0);
    check(r.status==StreamStatus::progress && r.input_consumed==314 && r.output_produced==2,"known output is not EOS without endinput");
    r=awaiting.process({},output,0);
    check(r.status==StreamStatus::need_input && r.input_consumed==0 && r.output_produced==0,"await explicit endinput");
    r=awaiting.process({},output,end);
    check(r.status==StreamStatus::end_of_stream && r.output_produced==0,"empty final EndInput");
    PositionTans1mStreamDecoder reset(limits,serialized,tokens,raw,dt);
    std::fill(output.begin(),output.end(),marker);
    r=reset.process(bytes,output,flag_value(ProcessFlags::reset_block));
    check(r.status==StreamStatus::error && r.error.code==ErrorCode::unsupported
        && r.input_consumed==0 && r.output_produced==0 && output[0]==marker,"unsupported flags before output");
    PositionTans1mStreamDecoder alias(limits,serialized,tokens,raw,dt);
    const auto before=bytes;r=alias.process(bytes,std::span(bytes).first(2),end);
    check(r.status==StreamStatus::error && r.error.code==ErrorCode::invalid_argument && bytes==before,"source/output alias unchanged");
    auto tight=limits;tight.max_block_size=65536;
    tight.max_internal_buffered_bytes=sizeof(PositionTans1mStreamDecoder)+serialized.size()+raw.size()
        +tokens.size()*sizeof(LzssTypedToken)+dt.size()*sizeof(TansDecodeEntry)+131072
        +sizeof(PositionTans1mDescriptor)+sizeof(marc::context::internal::PositionTans1mTokenContext)
        +sizeof(LzssTypedFrameValidationContext);
    PositionTans1mStreamDecoder exact(tight,serialized,tokens,raw,dt);
    r=exact.process(bytes,output,end);
    check(r.status==StreamStatus::end_of_stream && r.output_produced==2,"exact constructor and nested frame budget");
    --tight.max_internal_buffered_bytes;
    PositionTans1mStreamDecoder below(tight,serialized,tokens,raw,dt);
    std::fill(output.begin(),output.end(),marker);r=below.process(bytes,output,end);
    check(r.status==StreamStatus::error && r.error.code==ErrorCode::limit_exceeded
        && r.input_consumed==0 && r.output_produced==0 && output[0]==marker,"one byte below before input or output");
    PositionTans1mStreamDecoder short_bank(limits,serialized,tokens,raw,std::span(dt).first(dt.size()-1));
    r=short_bank.process(bytes,output,end);
    check(r.status==StreamStatus::error && r.error.code==ErrorCode::limit_exceeded && r.input_consumed==0,"short private decode bank");
    return failures ? 1 : 0;
}


