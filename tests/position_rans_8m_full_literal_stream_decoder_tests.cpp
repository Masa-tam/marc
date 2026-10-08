#include "frame/lzss_position_rans_8m_full_literal_stream_decoder.hpp"
#include "core/endian.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <new>
#include <vector>

namespace {
thread_local bool track_allocations{};
thread_local std::size_t process_allocations{};
}
void* operator new(std::size_t size) {
    if (track_allocations) ++process_allocations;
    if (auto* p=std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }

namespace {
using namespace marc::frame::internal;
using Token=marc::dictionary::internal::LzssTypedToken;
using Kind=marc::dictionary::internal::LzssTypedTokenKind;
using Status=marc::core::StreamStatus;
using Code=marc::core::ErrorCode;
constexpr auto end=marc::core::flag_value(marc::core::ProcessFlags::end_input);
constexpr auto flush=marc::core::flag_value(marc::core::ProcessFlags::flush);
int failures{};
void check(bool good,const char* text) { if (!good) { ++failures; std::cerr<<text<<'\n'; } }
marc::core::DecoderLimits limits() {
    marc::core::DecoderLimits l{}; l.max_block_size=UINT64_C(80)<<20;
    l.max_internal_buffered_bytes=UINT64_C(512)<<20; return l;
}
struct Decode { std::vector<std::byte> output; marc::core::ProcessResult result{}; std::size_t consumed{}; };
Decode decode(std::span<const std::byte> bytes,std::size_t input_chunk,std::size_t output_chunk,
    bool inject_zero_output=false) {
    auto l=limits();
    std::vector<std::byte> serialized(18*UINT64_C(8388608)+9334),raw(8388608);
    std::vector<Token> tokens(8388608);
    PositionRans8mFullLiteralStreamDecoder decoder(l,serialized,tokens,raw);
    std::vector<std::byte> output(output_chunk+2,std::byte{0xa5});
    Decode r;
    for (std::size_t call=0;call<bytes.size()*2+10485760;++call) {
        const auto count=std::min(input_chunk,bytes.size()-r.consumed);
        const auto input=bytes.subspan(r.consumed,count);
        const auto capacity=inject_zero_output && call%3==0 ? 0 : output_chunk;
        std::fill(output.begin(),output.end(),std::byte{0xa5});
        const auto flags=(r.consumed+count==bytes.size() ? end : 0) | (call%2 ? flush : 0);
        const auto before=process_allocations;
        track_allocations=true;
        r.result=decoder.process(input,std::span(output).subspan(1,capacity),flags);
        track_allocations=false;
        check(process_allocations==before,"process allocated");
        check(r.result.input_consumed<=input.size() && r.result.output_produced<=capacity,"invalid counts");
        check(r.result.status!=Status::progress || r.result.input_consumed || r.result.output_produced,"zero progress");
        check(output.front()==std::byte{0xa5} && output.back()==std::byte{0xa5},"output guard");
        check(std::all_of(output.begin()+1+r.result.output_produced,output.end(),[](auto v) { return v==std::byte{0xa5}; }),"uncommitted bytes written");
        r.consumed+=r.result.input_consumed;
        r.output.insert(r.output.end(),output.begin()+1,output.begin()+1+r.result.output_produced);
        if (r.result.status==Status::error || r.result.status==Status::end_of_stream) {
            const auto repeated=decoder.process({}, {}, 0);
            check(repeated.input_consumed==0 && repeated.output_produced==0 && repeated.status==r.result.status,"sticky state");
            if (r.result.status==Status::error)
                check(repeated.error.code==r.result.error.code && repeated.error.byte_position==r.result.error.byte_position,"sticky error category/position");
            return r;
        }
    }
    check(false,"nontermination"); return r;
}
std::vector<std::byte> two_frames() {
    const auto l=limits();
    PositionRans8mFullLiteralStreamHeader stream{1,2};
    std::vector<std::byte> wire(112);
    check(serialize_position_rans_8m_full_literal_stream(stream,l,wire)==PositionRans8mFullLiteralFormatError::none,"header encode");
    for (unsigned index=0;index<2;++index) {
        const Token token{Kind::literal,static_cast<std::uint8_t>(65+index),0,0};
        std::array<std::byte,5200> frame{};
        const auto r=encode_position_rans_8m_full_literal_frame(std::span(&token,1),{stream,l,index,index},frame);
        check(r.error==PositionRans8mFullLiteralFrameError::none,"frame encode");
        wire.insert(wire.end(),frame.begin(),frame.begin()+r.serialized_size);
    }
    return wire;
}
} // namespace

int main(int argc,char** argv) {
    if (argc==3 && std::string(argv[1])=="--stream") {
        std::ifstream file(argv[2],std::ios::binary);
        std::vector<char> input((std::istreambuf_iterator<char>(file)),{});
        if (!file || input.size()<8) return 2;
        std::vector<std::byte> storage(input.size());
        std::transform(input.begin(),input.end(),storage.begin(),[](char v) { return static_cast<std::byte>(v); });
        const auto bytes=std::span<const std::byte>(storage);
        std::uint32_t encoded{},expected{};
        (void)marc::core::load_le(bytes,0,encoded); (void)marc::core::load_le(bytes,4,expected);
        if (UINT64_C(8)+encoded+expected!=bytes.size()) return 3;
        const auto r=decode(bytes.subspan(8,encoded),4093,1031,true);
        if (failures || r.result.status!=Status::end_of_stream || r.consumed!=encoded
            || r.output.size()!=expected || !std::equal(r.output.begin(),r.output.end(),bytes.begin()+8+encoded)) return 4;
        return 0;
    }
    const auto wire=two_frames();
    const std::vector<std::byte> expected{std::byte{65},std::byte{66}};
    for (const auto input : {std::size_t{1},std::size_t{2},std::size_t{7},wire.size()})
        for (const auto output : {std::size_t{1},std::size_t{2},std::size_t{5}}) {
            const auto r=decode(wire,input,output,true);
            check(r.result.status==Status::end_of_stream && r.output==expected && r.consumed==wire.size(),"chunks");
        }
    for (std::size_t n=0;n<wire.size();++n) {
        const auto r=decode(std::span(wire).first(n),7,1);
        check(r.result.status==Status::error && r.result.error.code==Code::malformed_stream,"truncated stream");
        check(r.output.size()<=1 && (r.output.empty() || r.output[0]==std::byte{65}),"truncated frame published");
    }
    auto broken=wire;
    (void)marc::core::store_le<std::uint64_t>(broken,broken.size()-8,(UINT64_C(1)<<31)+1);
    for (const auto input : {std::size_t{1},std::size_t{7},wire.size()}) {
        const auto r=decode(broken,input,1,true);
        check(r.result.status==Status::error && r.result.error.code==Code::malformed_stream
            && r.output==std::vector<std::byte>{std::byte{65}},"failed later frame published");
        check(r.result.error.byte_position==wire.size()-8,"absolute payload position");
    }
    auto trailing=wire; trailing.push_back(std::byte{0});
    const auto late=decode(trailing,trailing.size(),2);
    check(late.result.status==Status::error && late.output==expected && late.result.error.byte_position==wire.size(),"trailing bytes");
    std::array<std::byte,112> empty{};
    const auto l=limits();
    check(serialize_position_rans_8m_full_literal_stream({},l,empty)==PositionRans8mFullLiteralFormatError::none,"empty encode");
    check(decode(empty,1,1).result.status==Status::end_of_stream,"empty decode");
    std::array<std::byte,128> serial{};
    std::array<std::byte,1> raw{};
    std::array<Token,1> tokens{};
    PositionRans8mFullLiteralStreamDecoder unsupported(l,serial,tokens,raw);
    check(unsupported.process({}, {},marc::core::flag_value(marc::core::ProcessFlags::reset_block)).error.code==Code::unsupported,"reset block");
    auto limited=l; limited.max_block_size=32; limited.max_frame_size=1;
    const auto required=sizeof(PositionRans8mFullLiteralStreamDecoder)+serial.size()+raw.size()+sizeof(tokens)
        +marc::context::internal::lzss_position_rans_8m_full_literal_fixed_working_bytes;
    limited.max_internal_buffered_bytes=required-1;
    PositionRans8mFullLiteralStreamDecoder below(limited,serial,tokens,raw);
    check(below.process({}, {},0).error.code==Code::limit_exceeded,"constructor aggregate below");
    limited.max_internal_buffered_bytes=required;
    PositionRans8mFullLiteralStreamDecoder exact(limited,serial,tokens,raw);
    check(exact.process({}, {},0).status==Status::need_input,"constructor aggregate exact");
    PositionRans8mFullLiteralStreamDecoder waiting(l,serial,tokens,raw);
    const auto header_only=waiting.process(empty,{},0);
    check(header_only.status==Status::progress && header_only.input_consumed==112,"empty stream awaits EndInput");
    check(waiting.process({}, {},0).status==Status::need_input,"no terminal flag starvation");
    check(waiting.process({}, {},end).status==Status::end_of_stream,"zero-byte final EndInput");
    PositionRans8mFullLiteralStreamDecoder aliased(l,serial,tokens,std::span(serial).first(1));
    check(aliased.process({}, {},0).error.code==Code::invalid_argument,"workspace overlap");
    PositionRans8mFullLiteralStreamDecoder external_alias(l,serial,tokens,raw);
    auto alias_input=empty;
    const auto bad_alias=external_alias.process(alias_input,alias_input,end);
    check(bad_alias.error.code==Code::invalid_argument && !bad_alias.input_consumed && !bad_alias.output_produced
        && alias_input==empty,"I/O overlap unchanged");
    std::array<std::byte,64> short_serial{};
    PositionRans8mFullLiteralStreamDecoder insufficient(l,short_serial,tokens,raw);
    std::array<std::byte,2> unpublished{std::byte{0xa5},std::byte{0xa5}};
    const auto refused=insufficient.process(wire,unpublished,end);
    check(refused.status==Status::error && refused.error.code==Code::limit_exceeded
        && refused.input_consumed==112+64 && !refused.output_produced
        && unpublished[0]==std::byte{0xa5} && unpublished[1]==std::byte{0xa5},"reject workspace before descriptor");
    std::cout<<"failures="<<failures<<'\n';
    return failures ? 1 : 0;
}
