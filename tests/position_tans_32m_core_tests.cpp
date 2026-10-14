#include "entropy/position_tans_32m.hpp"
#include "core/endian.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
using namespace marc::entropy::internal;
using Error=PositionTans32mError;
int failures{};
void check(bool ok,const char* name) { if (!ok) { ++failures;std::cerr<<name<<'\n'; } }
std::vector<std::byte> serialize(const PositionTans32mDescriptor& d) {
    std::vector<std::byte> b(9313);std::size_t n=777;
    check(serialize_position_tans_32m_descriptor(d,{},b,n)==Error::none,"serialize");
    b.resize(n);return b;
}
bool rejects(std::span<const std::byte> b,std::uint32_t dc,std::uint32_t ps,
             const marc::core::DecoderLimits& limits={}) {
    PositionTans32mDescriptor sentinel{};sentinel.decision_count=771;sentinel.frequencies.fill(7);
    auto output=sentinel;
    const auto error=parse_position_tans_32m_descriptor(b,dc,ps,limits,output);
    check(output==sentinel,"failed parse changed ordinary descriptor");
    return error!=Error::none;
}
int fixture(const char* path) {
    std::ifstream file(path,std::ios::binary);
    std::vector<char> chars((std::istreambuf_iterator<char>(file)),{});
    if (!file || chars.size()<12 || chars.size()>(UINT64_C(2)<<20)) return 2;
    std::vector<std::byte> data(chars.size());
    std::transform(chars.begin(),chars.end(),data.begin(),[](char c){return static_cast<std::byte>(c);});
    std::uint32_t ds{},ps{},dc{};
    (void)marc::core::load_le(data,0,ds);(void)marc::core::load_le(data,4,ps);(void)marc::core::load_le(data,8,dc);
    if (ds>9313 || ps>503316482 || dc>100000 || UINT64_C(12)+ds+ps+UINT64_C(4)*dc!=data.size()) return 3;
    PositionTans32mDescriptor reference{},calculated{};
    const auto bytes=std::span<const std::byte>(data);
    if (parse_position_tans_32m_descriptor(bytes.subspan(12,ds),dc,ps,{},reference)!=Error::none) return 4;
    PositionTans32mModelBuilder builder;
    std::vector<std::pair<int,std::uint32_t>> decisions;
    for (std::size_t i=0;i<dc;++i) {
        std::uint16_t c{},s{};
        (void)marc::core::load_le(bytes,12+ds+ps+4*i,c);
        (void)marc::core::load_le(bytes,14+ds+ps+4*i,s);
        const int context=c==65535 ? -1 : c;
        decisions.emplace_back(context,s);
        if (builder.add(context,s)!=Error::none) return 5;
    }
    if (builder.finish(calculated)!=Error::none) return 6;
    calculated.payload_size=ps;calculated.final_valid_bits=reference.final_valid_bits;
    if (calculated!=reference || serialize(reference)!=std::vector<std::byte>(bytes.begin()+12,bytes.begin()+12+ds)) return 7;
    std::vector<std::uint16_t> et(position_tans_32m_table_entries);
    if (build_position_tans_32m_encode_tables(reference,{},et)!=Error::none) return 8;
    std::vector<std::byte> encoded(ps,std::byte{0x77});
    for (unsigned pass=0;pass<2;++pass) {
        PositionTans32mReverseWriter writer(reference,{},et,pass==0 ? std::span<std::byte>{} : std::span<std::byte>(encoded));
        for (auto i=decisions.rbegin();i!=decisions.rend();++i) if (writer.add(i->first,i->second)!=Error::none) return 9;
        std::size_t n=777;std::uint8_t valid=77;
        if (writer.finish(n,valid)!=Error::none || n!=ps || valid!=reference.final_valid_bits) return 10;
    }
    if (!std::equal(encoded.begin(),encoded.end(),bytes.begin()+12+ds)) return 11;
    std::vector<TansDecodeEntry> dt(position_tans_32m_table_entries);
    PositionTans32mDecoder decoder;
    if (decoder.begin(reference,encoded,{},dt)!=Error::none) return 12;
    for (const auto& [c,s]:decisions) {
        std::uint32_t symbol=777;
        if (decoder.read(c,symbol)!=Error::none || symbol!=s) return 13;
    }
    if (decoder.finish()!=Error::none) return 14;
    return failures ? 15 : 0;
}
}
int main(int argc,char** argv) {
    if (argc==3 && std::string(argv[1])=="--fixture") return fixture(argv[2]);
    PositionTans32mDescriptor empty{};
    // Uniform residuals have their own bank after all 57 adaptive contexts.
    // No adaptive model is active here; reading bank 55 instead of 57 fails.
    PositionTans32mDescriptor uniform{};uniform.decision_count=2;
    uniform.payload_size=3;uniform.final_valid_bits=2;
    std::vector<std::uint16_t> uniform_encode(position_tans_32m_table_entries);
    check(build_position_tans_32m_encode_tables(uniform,{},uniform_encode)==Error::none,"uniform tables");
    std::vector<std::byte> uniform_payload(3);
    PositionTans32mReverseWriter uniform_writer(uniform,{},uniform_encode,uniform_payload);
    std::size_t uniform_size=777;std::uint8_t uniform_valid=77;
    check(uniform_writer.add(-1,1)==Error::none && uniform_writer.add(-1,0)==Error::none
          && uniform_writer.finish(uniform_size,uniform_valid)==Error::none
          && uniform_size==3 && uniform_valid==2,"uniform residual bank encode");
    std::vector<TansDecodeEntry> uniform_decode(position_tans_32m_table_entries);
    PositionTans32mDecoder uniform_reader;std::uint32_t uniform_symbol=777;
    check(uniform_reader.begin(uniform,uniform_payload,{},uniform_decode)==Error::none
          && uniform_reader.read(-1,uniform_symbol)==Error::none && uniform_symbol==0
          && uniform_reader.read(-1,uniform_symbol)==Error::none && uniform_symbol==1
          && uniform_reader.finish()==Error::none,"uniform residual bank decode");
    auto zero=serialize(empty);
    check(zero.size()==32,"empty descriptor extent");
    PositionTans32mDescriptor parsed{};
    check(parse_position_tans_32m_descriptor(zero,0,2,{},parsed)==Error::none && parsed==empty,"empty parse");
    PositionTans32mModelBuilder builder;
    check(builder.add(3,65)==Error::none && builder.add(3,66)==Error::none,"model symbols");
    PositionTans32mDescriptor d{};check(builder.finish(d)==Error::none,"model finish");
    check(d.frequencies[position_tans_32m_offsets[3]+65]==2048 &&
          d.frequencies[position_tans_32m_offsets[3]+66]==2048,"equal normalize");
    std::vector<std::uint16_t> et(position_tans_32m_table_entries,777);
    check(build_position_tans_32m_encode_tables(d,{},et)==Error::none,"encode tables");
    PositionTans32mReverseWriter sizing(d,{},et,{});
    check(sizing.add(3,66)==Error::none && sizing.add(3,65)==Error::none,"reverse measure");
    std::size_t n=77;std::uint8_t valid=77;
    check(sizing.finish(n,valid)==Error::none && n==3 && valid==2,"hand AB size");
    d.payload_size=3;d.final_valid_bits=2;
    std::vector<std::byte> payload(3,std::byte{0x77});
    PositionTans32mReverseWriter writer(d,{},et,payload);
    check(writer.add(3,66)==Error::none && writer.add(3,65)==Error::none && writer.finish(n,valid)==Error::none,"AB encode");
    check(payload==std::vector<std::byte>{std::byte{6},std::byte{0},std::byte{0}},"hand AB bytes");
    auto bytes=serialize(d);
    for (std::size_t cut=0;cut<bytes.size();++cut) check(rejects(std::span<const std::byte>(bytes).first(cut),2,3),"descriptor truncation");
    for (std::size_t i=0;i<24;++i) {
        auto changed=bytes;
        if (i==9) changed[i]=std::byte{0}; // Other valid-bit counts require payload validation.
        else changed[i]^=std::byte{1};
        check(rejects(changed,2,3),"metadata mutation");
    }
    auto extra=bytes;extra.push_back(std::byte{0});check(rejects(extra,2,3),"trailing descriptor");
    for (unsigned bit=4;bit<8;++bit) {
        auto changed=bytes;changed[31]|=static_cast<std::byte>(1U<<bit);
        check(rejects(changed,2,3),"active mask bit without a model retains descriptor");
    }
    auto invalid_mode=bytes;invalid_mode[32]=std::byte{3};check(rejects(invalid_mode,2,3),"bad mode");
    PositionTans32mDescriptor single{};single.decision_count=1;single.frequencies[0]=4096;
    auto noncanonical=serialize(single);noncanonical.resize(35);
    noncanonical[32]=std::byte{1};noncanonical[33]=std::byte{0};noncanonical[34]=std::byte{0x10};
    check(rejects(noncanonical,1,2),"noncanonical dense single");
    noncanonical[34]=std::byte{0x20};check(rejects(noncanonical,1,2),"dense sum overflow");
    auto bad=d;bad.frequencies[position_tans_32m_offsets[3]+65]=2047;
    std::vector<std::byte> destination(9313,std::byte{0x71});const auto retained=destination;
    std::size_t count=777;
    check(serialize_position_tans_32m_descriptor(bad,{},destination,count)==Error::invalid_frequencies
          && destination==retained && count==777,"failed serialize retains bytes/count");
    count=777;check(serialize_position_tans_32m_descriptor(d,{},std::span(destination).first(1),count)==Error::output_too_small
          && destination==retained && count==777,"short serialize retains bytes/count");
    std::vector<TansDecodeEntry> dt(position_tans_32m_table_entries);
    PositionTans32mDecoder decoder;
    check(decoder.begin(d,payload,{},dt)==Error::none,"decoder begin");
    std::uint32_t symbol=777;
    check(decoder.read(3,symbol)==Error::none && symbol==65,"decode A");
    check(decoder.read(3,symbol)==Error::none && symbol==66 && decoder.finish()==Error::none,"decode B/finish");
    symbol=777;check(decoder.read(3,symbol)==Error::already_finished && symbol==777,"ended read retains symbol");
    auto padding=payload;padding.back()=std::byte{0x80};
    check(decoder.begin(d,padding,{},dt)==Error::nonzero_padding,"padding rejection");
    auto state=payload;state[1]=std::byte{0x10};
    check(decoder.begin(d,state,{},dt)==Error::invalid_state,"state rejection");
    auto truncated=payload;truncated.pop_back();check(decoder.begin(d,truncated,{},dt)==Error::invalid_payload_size,"payload truncation");
    check(decoder.begin(d,payload,{},dt)==Error::none,"restart");
    symbol=777;check(decoder.read(57,symbol)==Error::invalid_context && symbol==777,"invalid context retains symbol");
    check(decoder.read(3,symbol)==Error::invalid_context && symbol==777,"sticky failure");
    check(decoder.begin(d,payload,{},dt)==Error::none,"unused begin");
    check(decoder.finish()==Error::count_mismatch,"early finish");
    auto unused=d;unused.frequencies[0]=4096;
    check(decoder.begin(unused,payload,{},dt)==Error::none,"extra model begin");
    check(decoder.read(3,symbol)==Error::none && decoder.read(3,symbol)==Error::none
          && decoder.finish()==Error::unused_context,"unrequested model rejection");
    auto padded_count=d;padded_count.final_valid_bits=3;
    check(decoder.begin(padded_count,payload,{},dt)==Error::none,"extra valid bit begin");
    check(decoder.read(3,symbol)==Error::none && decoder.read(3,symbol)==Error::none && decoder.finish()==Error::trailing_bits,"extra bit rejection");
    auto short_bits=d;short_bits.final_valid_bits=1;
    check(decoder.begin(short_bits,payload,{},dt)==Error::none && decoder.read(3,symbol)==Error::none,"short bits begin");
    symbol=777;check(decoder.read(3,symbol)==Error::truncated && symbol==777,"late truncation retains symbol");
    check(decoder.begin(d,payload,{},dt)==Error::none,"corrupt table begin");
    for (auto& e:dt) e.state_base=0;
    symbol=777;check(decoder.read(3,symbol)==Error::invalid_table && symbol==777,"invalid table retains symbol");
    check(decoder.begin(d,payload,{},std::span(dt).first(1))==Error::output_too_small,"short table output");
    auto table_bytes=std::as_writable_bytes(std::span(dt));
    table_bytes[0]=std::byte{6};table_bytes[1]=table_bytes[2]=std::byte{0};
    const auto before_alias=dt;
    check(decoder.begin(d,table_bytes.first(3),{},dt)==Error::overlapping_buffers && dt==before_alias,"payload/table overlap rejects before write");
    auto encoder_bytes=std::as_writable_bytes(std::span(et));
    const auto retained_tables=et;
    PositionTans32mReverseWriter overlapping(d,{},et,encoder_bytes.first(3));
    n=777;valid=77;
    check(overlapping.finish(n,valid)==Error::overlapping_buffers && n==777 && valid==77 && et==retained_tables,"encoder overlap retains table and counts");
    marc::core::DecoderLimits limits{};limits.max_entropy_table_entries=position_tans_32m_table_entries-1;
    check(rejects(bytes,2,3,limits),"table limit preflight");
    limits={};limits.max_block_size=100;
    limits.max_internal_buffered_bytes=bytes.size()+3+et.size()*sizeof(et[0])+131072;
    check(build_position_tans_32m_encode_tables(d,limits,et)==Error::none,"exact scratch budget");
    --limits.max_internal_buffered_bytes;std::fill(et.begin(),et.end(),777);
    check(build_position_tans_32m_encode_tables(d,limits,et)==Error::limit_exceeded &&
          std::all_of(et.begin(),et.end(),[](auto v){return v==777;}),"one below budget retains scratch before build");
    std::cout<<"finite descriptor/model/automaton/ordinary failure checks: "<<(failures ? "FAIL" : "PASS")<<'\n';
    return failures ? 1 : 0;
}




