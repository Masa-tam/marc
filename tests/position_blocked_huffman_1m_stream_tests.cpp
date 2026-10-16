#include "frame/lzss_position_blocked_huffman_1m_stream_decoder.hpp"
#include "core/endian.hpp"
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
    const auto extent=frame_capacity==1 ? 99 : 64+2547+(120*frame_capacity+7)/8;
    std::vector<std::byte> serialized(extent,marker),raw(frame_capacity,marker),output(capacity,marker);
    std::vector<LzssTypedToken> tokens(frame_capacity);
    std::vector<HuffmanDecodeTable> tables(position_blocked_huffman_1m_tables);
    PositionBlockedHuffman1mStreamDecoder decoder({},serialized,tokens,raw,tables);
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
int check_position_blocked_huffman_1m_stream_fixture(std::span<const std::byte> stream,std::span<const std::byte> expected) {
    const auto r=run(stream,1,1,true,std::max<std::size_t>(1,expected.size()));
    return r.status==StreamStatus::end_of_stream && r.consumed==stream.size()
        && std::equal(r.raw.begin(),r.raw.end(),expected.begin(),expected.end()) ? 0 : 1;
}
int position_blocked_huffman_1m_stream_tests() {
    DecoderLimits limits{};PositionBlockedHuffman1mStreamHeader stream{};stream.frame_size=1;stream.original_size=2;
    std::vector<std::byte> bytes(310),serialized(99),payload(0),scratch(99);
    check(serialize_position_blocked_huffman_1m_stream(stream,limits,bytes)==PositionBlockedHuffman1mFormatError::none,"stream fixture header");
    std::vector<CanonicalHuffmanTable> et(position_blocked_huffman_1m_tables);
    for (std::uint64_t i=0;i<2;++i) {
        const std::array<LzssTypedToken,1> tokens{{{LzssTypedTokenKind::literal,static_cast<std::uint8_t>(65+i),0,0}}};
        const auto encoded=encode_position_blocked_huffman_1m_frame(tokens,{stream,limits,i,i},et,payload,scratch,serialized);
        check(encoded.error==PositionBlockedHuffman1mFrameError::none && encoded.serialized_size==99,"two reset frames fixture");
        std::copy(serialized.begin(),serialized.end(),bytes.begin()+112+99*i);
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
    const auto mixed=encode_position_blocked_huffman_1m_frame(mixed_tokens,{mixed_stream,limits,0,0},et,mixed_payload,mixed_scratch,mixed_frame);
    check(mixed.error==PositionBlockedHuffman1mFrameError::none,"mixed match frame fixture");
    mixed_frame.resize(mixed.serialized_size);
    std::vector<std::byte> mixed_bytes(112);
    check(serialize_position_blocked_huffman_1m_stream(mixed_stream,limits,mixed_bytes)==PositionBlockedHuffman1mFormatError::none,"mixed stream header");
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
        const std::vector<std::byte> committed=cut>=211 ? std::vector<std::byte>{std::byte{65}} : std::vector<std::byte>{};
        check(r.status==StreamStatus::error && r.error.code==ErrorCode::malformed_stream && r.raw==committed,
            "every truncation retains only earlier validated frames");
    }
    auto bad=bytes;bad[307]=std::byte{1};
    const auto late=run(bad,1,1,true);
    check(late.status==StreamStatus::error && late.error.code==ErrorCode::malformed_stream
        && late.error.byte_position==310 && late.raw==std::vector<std::byte>{std::byte{65}},"late second-frame finish error never drains B");
    bad=bytes;bad[208]=std::byte{1};
    check(run(bad,bytes.size(),2).raw.empty(),"late first frame finish error publishes nothing");
    bad=bytes;bad[277]=std::byte{0xff};
    check(run(bad,bad.size(),2).raw==std::vector<std::byte>{std::byte{65}},"second descriptor rejects before payload");
    bad=bytes;bad.push_back(std::byte{0});
    const auto trailing=run(bad,bad.size(),2);
    check(trailing.status==StreamStatus::error && trailing.raw==expected && trailing.consumed==310,"strict trailing bytes after validated frames");
    auto empty=stream;empty.original_size=0;
    std::vector<std::byte> header(112);
    check(serialize_position_blocked_huffman_1m_stream(empty,limits,header)==PositionBlockedHuffman1mFormatError::none
        && run(header,1,1,true).status==StreamStatus::end_of_stream,"empty stream EOS");
    std::vector<HuffmanDecodeTable> dt(position_blocked_huffman_1m_tables);
    std::vector<LzssTypedToken> tokens(1);std::vector<std::byte> raw(1,marker),output(2,marker);
    PositionBlockedHuffman1mStreamDecoder decoder(limits,serialized,tokens,raw,dt);
    auto r=decoder.process(bytes,{},end);
    check(r.status==StreamStatus::need_output && r.input_consumed==211 && r.output_produced==0,"zero output stops at validated first frame");
    auto last=decoder.process(std::span(bytes).subspan(r.input_consumed),output,end);
    check(last.status==StreamStatus::end_of_stream && last.input_consumed==99 && last.output_produced==2 && output==expected,"drain then consume final suffix");
    PositionBlockedHuffman1mStreamDecoder awaiting(limits,serialized,tokens,raw,dt);
    r=awaiting.process(bytes,output,0);
    check(r.status==StreamStatus::progress && r.input_consumed==310 && r.output_produced==2,"known output is not EOS without endinput");
    r=awaiting.process({},output,0);
    check(r.status==StreamStatus::need_input && r.input_consumed==0 && r.output_produced==0,"await explicit endinput");
    r=awaiting.process({},output,end);
    check(r.status==StreamStatus::end_of_stream && r.output_produced==0,"empty final EndInput");
    PositionBlockedHuffman1mStreamDecoder reset(limits,serialized,tokens,raw,dt);
    std::fill(output.begin(),output.end(),marker);
    r=reset.process(bytes,output,flag_value(ProcessFlags::reset_block));
    check(r.status==StreamStatus::error && r.error.code==ErrorCode::unsupported
        && r.input_consumed==0 && r.output_produced==0 && output[0]==marker,"unsupported flags before output");
    PositionBlockedHuffman1mStreamDecoder alias(limits,serialized,tokens,raw,dt);
    const auto before=bytes;r=alias.process(bytes,std::span(bytes).first(2),end);
    check(r.status==StreamStatus::error && r.error.code==ErrorCode::invalid_argument && bytes==before,"source/output alias unchanged");
    auto tight=limits;tight.max_block_size=2;
    tight.max_internal_buffered_bytes=sizeof(PositionBlockedHuffman1mStreamDecoder)+serialized.size()+raw.size()
        +tokens.size()*sizeof(LzssTypedToken)+dt.size()*sizeof(HuffmanDecodeTable)+position_blocked_huffman_1m_stream_decode_working_bytes;
    PositionBlockedHuffman1mStreamDecoder exact(tight,serialized,tokens,raw,dt);
    r=exact.process(bytes,output,end);
    check(r.status==StreamStatus::end_of_stream && r.output_produced==2,"exact constructor and nested frame budget");
    --tight.max_internal_buffered_bytes;
    PositionBlockedHuffman1mStreamDecoder below(tight,serialized,tokens,raw,dt);
    std::fill(output.begin(),output.end(),marker);r=below.process(bytes,output,end);
    check(r.status==StreamStatus::error && r.error.code==ErrorCode::limit_exceeded
        && r.input_consumed==0 && r.output_produced==0 && output[0]==marker,"one byte below before input or output");
    PositionBlockedHuffman1mStreamDecoder short_bank(limits,serialized,tokens,raw,std::span(dt).first(dt.size()-1));
    r=short_bank.process(bytes,output,end);
    check(r.status==StreamStatus::error && r.error.code==ErrorCode::limit_exceeded && r.input_consumed==0,"short private decode bank");
    // Give frame1 a payload byte, then invalidate its descriptor. Admission
    // must stop before that byte is consumed or written to private storage.
    auto rejected_prefix=bytes;rejected_prefix.push_back(std::byte{0x7b});
    check(store_le(rejected_prefix,211+32,std::uint32_t{1})
        && store_le(rejected_prefix,211+64+4,std::uint32_t{1}),"nonempty payload declaration");
    rejected_prefix[211+64+8]=std::byte{1};
    rejected_prefix[211+64+10]=std::byte{4};
    std::vector<std::byte> prefix_storage(100,marker);
    PositionBlockedHuffman1mStreamDecoder prefix_decoder(limits,prefix_storage,tokens,raw,dt);
    std::fill(output.begin(),output.end(),marker);
    r=prefix_decoder.process(rejected_prefix,output,end);
    check(r.status==StreamStatus::error && r.error.code==ErrorCode::malformed_stream
        && r.input_consumed==310 && r.output_produced==1 && output[0]==std::byte{65}
        && output[1]==marker && prefix_storage[99]==marker,"bad descriptor rejected before payload consume/store and failed raw publication");
    return failures ? 1 : 0;
}
int main() { return position_blocked_huffman_1m_stream_tests(); }

