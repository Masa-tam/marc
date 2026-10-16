#include "entropy/position_blocked_huffman_1m.hpp"
#include "core/endian.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace marc::entropy::internal;
using Error=PositionBlockedHuffmanError;
using Descriptor=PositionBlockedHuffman1mDescriptor;
int failures{};
void check(bool value,const char* label) { if (!value) { ++failures;std::cerr<<label<<'\n'; } }
std::vector<std::byte> wire(const Descriptor& d) {
    std::vector<std::byte> bytes(2547);std::size_t n=999;
    check(serialize_position_blocked_huffman_1m_descriptor(d,{},bytes,n)==Error::none,"serialize fixture");
    bytes.resize(n);return bytes;
}
template<class T> T load(std::span<const std::byte> input,std::size_t at) {
    T v{};check(marc::core::load_le(input,at,v),"test load");return v;
}
int fixture(const char* path) {
    std::ifstream f(path,std::ios::binary);std::vector<char> raw((std::istreambuf_iterator<char>(f)),{});
    if (!f || raw.size()<16 || raw.size()>2*1024*1024) return 2;
    std::vector<std::byte> input(raw.size());
    std::transform(raw.begin(),raw.end(),input.begin(),[](char c){return static_cast<std::byte>(c);});
    const auto data=std::span<const std::byte>(input);
    const auto ds=load<std::uint32_t>(data,0),ps=load<std::uint32_t>(data,4),dc=load<std::uint32_t>(data,8),mode=load<std::uint32_t>(data,12);
    if (ds>2548 || ps>150000 || dc>100000 || mode>2 || UINT64_C(16)+ds+ps+UINT64_C(4)*dc!=data.size()) return 3;
    const auto description=data.subspan(16,ds),payload=data.subspan(16+ds,ps);
    Descriptor sentinel{};sentinel.decision_count=777;sentinel.models[0].lengths.fill(7);
    auto d=sentinel;
    const auto parsed=parse_position_blocked_huffman_1m_descriptor(description,dc,ps,{},d);
    if (mode==1) {
        check(parsed!=Error::none,"negative descriptor accepted");check(d==sentinel,"failed parser retained destination");
        return failures ? 1 : 0;
    }
    check(parsed==Error::none,"valid descriptor rejected");if (parsed!=Error::none) return 4;
    const auto serialized=wire(d);
    check(serialized.size()==description.size() && std::equal(serialized.begin(),serialized.end(),description.begin()),"canonical bytes");
    std::vector<PositionBlockedHuffmanDecision> decisions;
    for (std::size_t i=0;i<dc;++i) {
        const auto context=load<std::uint16_t>(data,16+ds+ps+4*i),symbol=load<std::uint16_t>(data,18+ds+ps+4*i);
        decisions.push_back({context==65535 ? -1 : context,symbol});
    }
    auto decode_tables=std::make_unique<std::array<HuffmanDecodeTable,57>>();
    PositionBlockedHuffman1mDecoder decoder;
    auto result=decoder.begin(d,payload,{},*decode_tables);
    for (const auto request:decisions) {
        if (result!=Error::none) break;
        std::uint32_t value=999;
        result=decoder.read(request.context,value);
        if (result==Error::none && mode==0) check(value==request.symbol,"decoded oracle symbol");
        if (result!=Error::none) check(value==999,"failed read retained symbol");
    }
    if (result==Error::none) result=decoder.finish();
    if (mode==0) {
        check(result==Error::none,"valid payload rejected");check(decoder.finish()==Error::none,"idempotent finish");
        std::uint32_t value=999;check(decoder.read(0,value)==Error::already_finished && value==999,"read after finish");
    } else {
        check(result!=Error::none,"negative payload accepted");
        std::uint32_t value=999;check(decoder.read(0,value)==result && value==999 && decoder.finish()==result,"sticky decoder failure");
    }
    auto encode_tables=std::make_unique<std::array<CanonicalHuffmanTable,57>>();
    std::vector<std::byte> output(ps+2,std::byte{0xa5});std::size_t written=777;
    const auto encoded=encode_position_blocked_huffman_1m(d,decisions,{},*encode_tables,std::span(output).subspan(1,ps),written);
    if (mode==0) {
        check(encoded==Error::none && written==ps,"oracle encode");
        check(std::equal(payload.begin(),payload.end(),output.begin()+1),"oracle payload bytes");
    } else if (encoded!=Error::none) {
        check(written==777 && std::all_of(output.begin(),output.end(),[](auto v){return v==std::byte{0xa5};}),"failed encode retained ordinary destinations");
    }
    check(output.front()==std::byte{0xa5} && output.back()==std::byte{0xa5},"output guards");
    return failures ? 1 : 0;
}
void units() {
    Descriptor d{};d.decision_count=4;d.payload_size=1;d.final_valid_bits=2;d.field_mask=3;
    d.models[0].single_symbol=0;d.models[1].lengths[65]=d.models[1].lengths[66]=1;
    const auto expected=wire(d);
    const std::array<PositionBlockedHuffmanDecision,4> decisions{{{0,0},{3,65},{1,0},{8,66}}};
    auto encode_tables=std::make_unique<std::array<CanonicalHuffmanTable,57>>();
    auto decode_tables=std::make_unique<std::array<HuffmanDecodeTable,57>>();
    std::array<std::byte,3> output{};output.fill(std::byte{0xa5});std::size_t written=777;
    const auto unchanged=output;
    auto limits=marc::core::DecoderLimits{};
    limits.max_block_size=d.decision_count;
    const std::uint64_t format_charge=sizeof(d)+2547+expected.size()+d.payload_size;
    limits.max_internal_buffered_bytes=format_charge;
    std::size_t extent=777;
    check(validate_position_blocked_huffman_1m_descriptor(d,limits,extent)==Error::none && extent==expected.size(),"exact format budget");
    --limits.max_internal_buffered_bytes;extent=777;
    check(validate_position_blocked_huffman_1m_descriptor(d,limits,extent)==Error::limit_exceeded && extent==777,"one short format budget");
    Descriptor sentinel{};sentinel.decision_count=777;
    auto parsed=sentinel;
    check(parse_position_blocked_huffman_1m_descriptor(expected,4,1,limits,parsed)==Error::limit_exceeded && parsed==sentinel,"parse budget retention");
    for (std::size_t n=0;n<expected.size();++n) {
        parsed=sentinel;check(parse_position_blocked_huffman_1m_descriptor(std::span(expected).first(n),4,1,{},parsed)!=Error::none && parsed==sentinel,"every descriptor cut retained");
    }
    std::vector<std::byte> small(expected.size()-1,std::byte{0xa5});
    check(serialize_position_blocked_huffman_1m_descriptor(d,{},small,written)==Error::output_too_small && written==777
        && std::all_of(small.begin(),small.end(),[](auto x){return x==std::byte{0xa5};}),"short descriptor output retained");
    const auto saved=d;
    check(serialize_position_blocked_huffman_1m_descriptor(d,{},std::as_writable_bytes(std::span(&d,1)),written)==Error::overlapping_buffers && d==saved,"serializer overlap");
    check(parse_position_blocked_huffman_1m_descriptor(std::as_bytes(std::span(&d,1)).first(30),4,1,{},d)==Error::overlapping_buffers && d==saved,"parser overlap");
    const auto target=std::span(output).subspan(1,1);
    const std::uint64_t encode_charge=16384+sizeof(d)+sizeof(limits)+sizeof(decisions)+sizeof(*encode_tables)+target.size()+sizeof(written);
    limits={};limits.max_block_size=d.decision_count;limits.max_internal_buffered_bytes=encode_charge-1;
    check(encode_position_blocked_huffman_1m(d,decisions,limits,*encode_tables,target,written)==Error::limit_exceeded && written==777 && output==unchanged,"one short encode budget");
    ++limits.max_internal_buffered_bytes;
    check(encode_position_blocked_huffman_1m(d,decisions,limits,*encode_tables,target,written)==Error::none && written==1 && output[1]==std::byte{2},"exact encode budget and AB bits");
    output=unchanged;written=777;
    auto bad=decisions;bad.back().symbol=67;
    check(encode_position_blocked_huffman_1m(d,bad,{},*encode_tables,target,written)==Error::invalid_symbol && output==unchanged && written==777,"late invalid symbol encode retention");
    check(encode_position_blocked_huffman_1m(d,decisions,{},std::span(*encode_tables).first(56),target,written)==Error::scratch_too_small && output==unchanged,"short encode scratch");
    std::array<std::byte,1> payload{std::byte{2}};PositionBlockedHuffman1mDecoder decoder;
    limits={};limits.max_block_size=d.decision_count;limits.max_internal_buffered_bytes=16384+sizeof(decoder)+sizeof(d)+sizeof(limits)+payload.size()+sizeof(*decode_tables)-1;
    check(decoder.begin(d,payload,limits,*decode_tables)==Error::limit_exceeded,"one short decoder budget");
    ++limits.max_internal_buffered_bytes;check(decoder.begin(d,payload,limits,*decode_tables)==Error::none,"exact decoder budget");
    for (const auto request:decisions) { std::uint32_t value=999;check(decoder.read(request.context,value)==Error::none && value==request.symbol,"unit round trip"); }
    check(decoder.finish()==Error::none,"unit decoder finish");
    check(decoder.begin(d,payload,{},std::span(*decode_tables).first(56))==Error::scratch_too_small,"short decode scratch");
    check(decoder.begin(d,payload,{},*decode_tables)==Error::none,"restart");
    std::uint32_t value=999;check(decoder.read(52,value)==Error::invalid_context && value==999,"bad context retains symbol");
    check(decoder.read(0,value)==Error::invalid_context && value==999 && decoder.finish()==Error::invalid_context,"sticky context error");
    check(decoder.begin(d,payload,{},*decode_tables)==Error::none,"restart for unused bits");
    check(decoder.finish()==Error::count_mismatch,"early finish");
    limits={};limits.max_huffman_code_length=14;extent=777;
    check(validate_position_blocked_huffman_1m_descriptor(d,limits,extent)==Error::limit_exceeded && extent==777,"code length limit");
    limits={};limits.max_entropy_table_entries=2*511-1;
    check(validate_position_blocked_huffman_1m_descriptor(d,limits,extent)==Error::limit_exceeded,"model entry limit");
    Descriptor single{};single.decision_count=2;single.field_mask=3;single.models[0].single_symbol=0;single.models[1].single_symbol=65;
    PositionBlockedHuffman1mDecoder empty_bits;
    check(empty_bits.begin(single,{}, {},*decode_tables)==Error::none,"zero-bit singleton begin");
    value=999;check(empty_bits.read(0,value)==Error::none && value==0,"zero-bit kind");
    check(empty_bits.read(3,value)==Error::none && value==65 && empty_bits.finish()==Error::none,"zero-bit literal");
    // Long codes exercise the bounded tree fallback and a non-byte-aligned end.
    Descriptor longest{};longest.decision_count=1;longest.payload_size=2;longest.final_valid_bits=7;longest.field_mask=2;
    for (std::size_t s=0;s<14;++s) longest.models[1].lengths[s]=static_cast<std::uint8_t>(s+1);
    longest.models[1].lengths[14]=longest.models[1].lengths[15]=15;
    const std::array<PositionBlockedHuffmanDecision,1> last{{{3,15}}};
    std::array<std::byte,4> long_output{};long_output.fill(std::byte{0xa5});written=777;
    check(encode_position_blocked_huffman_1m(longest,last,{},*encode_tables,std::span(long_output).subspan(1,2),written)==Error::none
        && written==2 && long_output[1]==std::byte{0xff} && long_output[2]==std::byte{0x7f}
        && long_output.front()==std::byte{0xa5} && long_output.back()==std::byte{0xa5},"fifteen-bit hand code and tail");
    check(decoder.begin(longest,std::span<const std::byte>(long_output).subspan(1,2),{},*decode_tables)==Error::none,"long-code begin");
    check(decoder.read(3,value)==Error::none && value==15 && decoder.finish()==Error::none,"long-code fallback");
    std::uint32_t aliased{};auto aliased_bytes=std::as_writable_bytes(std::span(&aliased,1));aliased_bytes[0]=std::byte{2};
    const auto saved_alias=aliased;
    check(decoder.begin(d,std::span<const std::byte>(aliased_bytes).first(1),{},*decode_tables)==Error::none,"alias probe begin");
    check(decoder.read(0,aliased)==Error::overlapping_buffers && aliased==saved_alias,"read rejects payload overlap before writes");
    value=999;check(decoder.read(0,value)==Error::none && value==0,"overlap rejection retained read state");
    written=777;const auto old_descriptor=d;
    check(encode_position_blocked_huffman_1m(d,decisions,{},*encode_tables,
        std::as_writable_bytes(std::span(&d,1)),written)==Error::overlapping_buffers && d==old_descriptor && written==777,"encoder source/output overlap");
    Descriptor oversized{};oversized.decision_count=UINT32_MAX;oversized.payload_size=UINT32_MAX;oversized.final_valid_bits=8;
    extent=777;check(validate_position_blocked_huffman_1m_descriptor(oversized,{},extent)==Error::limit_exceeded && extent==777,"large counts rejected before allocation");
    // The final four contexts are new to this specialization. Context51 sets
    // bit3 of the seventh mask byte; bits4..7 must still be rejected.
    Descriptor high{};high.decision_count=1;high.field_mask=16;
    high.override_mask=UINT64_C(1)<<51;high.models[4].single_symbol=1;high.models[56].single_symbol=1;
    const auto high_wire=wire(high);
    check(high_wire.size()==35 && high_wire[30]==std::byte{8},"last valid override mask byte");
    parsed=sentinel;check(parse_position_blocked_huffman_1m_descriptor(high_wire,1,0,{},parsed)==Error::none && parsed==high,"last context parsed");
    check(decoder.begin(high,{}, {},*decode_tables)==Error::none,"last context begin");
    value=999;check(decoder.read(51,value)==Error::none && value==1 && decoder.finish()==Error::none,"last context decoded");
    for (unsigned bit=4;bit<8;++bit) {
        auto corrupt=high_wire;corrupt[30]|=static_cast<std::byte>(1U<<bit);parsed=sentinel;
        check(parse_position_blocked_huffman_1m_descriptor(corrupt,1,0,{},parsed)==Error::invalid_metadata && parsed==sentinel,"unused mask bit rejected transactionally");
    }
    Descriptor endpoint{};endpoint.decision_count=1;endpoint.field_mask=8;endpoint.models[3].single_symbol=20;
    check(decoder.begin(endpoint,{}, {},*decode_tables)==Error::none,"distance class20 begin");
    value=999;check(decoder.read(31,value)==Error::none && value==20 && decoder.finish()==Error::none,"distance class20 decoded");
    endpoint.models[3].single_symbol=21;extent=777;
    check(validate_position_blocked_huffman_1m_descriptor(endpoint,{},extent)==Error::invalid_model && extent==777,"distance class21 rejected");
    // Maximum frame decision count is a finite zero-bit scalar block here,
    // not a claim of valid dictionary history or a full public frame test.
    Descriptor maximum{};maximum.decision_count=8388608;maximum.field_mask=16;maximum.models[4].single_symbol=0;
    limits={};limits.max_block_size=8388607;
    check(decoder.begin(maximum,{},limits,*decode_tables)==Error::limit_exceeded,"maximum decisions one-short limit");
    ++limits.max_block_size;
    check(decoder.begin(maximum,{},limits,*decode_tables)==Error::none,"maximum finite zero-bit begin");
    for (std::uint32_t i=0;i<8388608;++i) {
        value=999;
        if (decoder.read(51,value)!=Error::none || value!=0) { check(false,"maximum finite read");break; }
    }
    check(decoder.finish()==Error::none,"maximum finite zero-bit finish");
    std::cout<<"bounded one-MiB descriptor/scalar invariants checked\n";
}
}
int main(int argc,char** argv) { if (argc==2) return fixture(argv[1]); units(); return failures ? 1 : 0; }

