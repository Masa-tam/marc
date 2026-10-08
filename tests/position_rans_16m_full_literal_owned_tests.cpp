#include "frame/lzss_position_rans_16m_full_literal_owned.hpp"
#include "dictionary/lzss_position_distance_16m_five_prefix_candidate.hpp"
#include "core/endian.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <new>
#include <random>
#include <string>
#include <vector>

namespace {
thread_local bool tracked{};
thread_local std::size_t allocations{},fail_at{},live{};
}
void* operator new(std::size_t size) {
    if (tracked && ++allocations==fail_at) throw std::bad_alloc();
    if (auto* p=std::malloc(size ? size : 1)) { ++live; return p; }
    throw std::bad_alloc();
}
void* operator new[](std::size_t s) { return ::operator new(s); }
void operator delete(void* p) noexcept { if (p) { --live; std::free(p); } }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p,std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p,std::size_t) noexcept { ::operator delete(p); }

namespace {
using namespace marc::frame::internal;
using Token=marc::dictionary::internal::LzssTypedToken;
using Status=marc::core::StreamStatus;
using Code=marc::core::ErrorCode;
constexpr auto end=marc::core::flag_value(marc::core::ProcessFlags::end_input);
constexpr auto flush=marc::core::flag_value(marc::core::ProcessFlags::flush);
int failures{};
void check(bool good,const char* text) { if (!good) { ++failures; std::cerr<<text<<'\n'; } }
marc::core::DecoderLimits limits() { marc::core::DecoderLimits l{}; l.max_block_size=10*UINT64_C(16777216); l.max_compressed_payload_size=20*UINT64_C(16777216)+8; l.max_internal_buffered_bytes=UINT64_C(1024)<<20; return l; }
void qualify_finder() {
    using namespace marc::dictionary::internal;
    std::mt19937 generator(1537);
    const LzssParameters parameters{16777216,3,258,0};
    for (unsigned sample=0;sample<200;++sample) {
        std::vector<std::byte> raw(sample<4 ? sample : generator()%513);
        for (std::size_t i=0;i<raw.size();++i)
            raw[i]=static_cast<std::byte>(sample%3==0 ? i%7 : generator()%(sample%3==1 ? 8 : 256));
        std::vector<Token> expected;
        for (std::size_t position=0;position<raw.size();) {
            std::size_t best=0,distance=0;
            for (std::size_t d=1;d<=position;++d) {
                std::size_t length=0;
                while (length<258 && position+length<raw.size()
                       && raw[position+length]==raw[position-d+length]) ++length;
                if (length>best) { best=length;distance=d; }
            }
            if (best>=5) {
                expected.push_back({LzssTypedTokenKind::match,0,
                    static_cast<std::uint32_t>(distance),static_cast<std::uint32_t>(best)});
                position+=best;
            } else {
                expected.push_back({LzssTypedTokenKind::literal,std::to_integer<std::uint8_t>(raw[position]),0,0});
                ++position;
            }
        }
        const auto query=calculate_lzss_position_distance_16m_five_prefix_workspace(raw.size(),parameters,limits());
        check(query.error==LzssShortPrefixError::none,"finder query");
        std::vector<std::uint32_t> storage((query.workspace_size+3)/4);
        std::vector<Token> actual(raw.size());
        const auto result=tokenize_lzss_position_distance_16m_five_prefix_candidate(raw,parameters,limits(),5,
            actual,std::as_writable_bytes(std::span(storage)));
        check(result.error==LzssShortMatchCandidateError::none && result.token_count==expected.size(),"finder reference count");
        actual.resize(result.token_count);
        check(actual.size()==expected.size() && std::equal(actual.begin(),actual.end(),expected.begin(),
            [](const auto& a,const auto& b) {return a.kind==b.kind && a.literal==b.literal
                && a.distance==b.distance && a.length==b.length;}),"finder independent nearest-longest reference");
    }
}
struct Run { std::vector<std::byte> bytes; marc::core::ProcessResult result{}; std::size_t consumed{}; };
Run run(marc::core::Transform& codec,std::span<const std::byte> input,std::size_t ichunk,std::size_t ochunk,bool varies=true) {
    Run r;
    std::vector<std::byte> output(ochunk+2);
    for (std::size_t call=0;call<input.size()*3+41943040;++call) {
        const auto count=std::min(ichunk,input.size()-r.consumed);
        const auto capacity=varies && call%3==0 ? 0 : ochunk;
        std::fill(output.begin(),output.end(),std::byte{0xa5});
        const auto flags=(r.consumed+count==input.size() ? end : 0) | (varies && call%2 ? flush : 0);
        allocations=0; fail_at=0; tracked=true;
        r.result=codec.process(input.subspan(r.consumed,count),std::span(output).subspan(1,capacity),flags);
        tracked=false;
        check(allocations==0,"process allocations");
        check(r.result.input_consumed<=count && r.result.output_produced<=capacity,"process bounds");
        check(r.result.status!=Status::progress || r.result.input_consumed || r.result.output_produced,"zero progress");
        check(output.front()==std::byte{0xa5}
            && std::all_of(output.begin()+1+r.result.output_produced,output.end(),[](auto b) { return b==std::byte{0xa5}; }),"output guards");
        r.consumed+=r.result.input_consumed;
        r.bytes.insert(r.bytes.end(),output.begin()+1,output.begin()+1+r.result.output_produced);
        if (r.result.status==Status::end_of_stream || r.result.status==Status::error) {
            const auto again=codec.process({}, {},0);
            check(again.status==r.result.status && !again.input_consumed && !again.output_produced,"sticky status");
            check(again.error.code==r.result.error.code && again.error.byte_position==r.result.error.byte_position,"sticky error");
            return r;
        }
    }
    check(false,"nontermination"); return r;
}
std::vector<std::byte> encode(const std::vector<std::byte>& raw,std::uint32_t fs,std::size_t i,std::size_t o,std::uint32_t eligibility=3) {
    Code error{};
    auto p=PositionRans16mFullLiteralOwnedEncoder::create({fs,raw.size()},limits(),error,eligibility);
    check(p && error==Code::none,"encoder create");
    if (!p) return {};
    const auto r=run(*p,raw,i,o);
    check(r.result.status==Status::end_of_stream && r.consumed==raw.size(),"encoder finish");
    return r.bytes;
}
void decode(const std::vector<std::byte>& wire,const std::vector<std::byte>& raw,std::uint32_t fs,std::size_t i,std::size_t o) {
    Code error{};
    auto p=PositionRans16mFullLiteralOwnedDecoder::create(fs,limits(),error);
    check(p && error==Code::none,"decoder create");
    if (!p) return;
    const auto r=run(*p,wire,i,o);
    check(r.result.status==Status::end_of_stream && r.consumed==wire.size() && r.bytes==raw,"owned roundtrip");
}
std::vector<std::byte> reference(const std::vector<std::byte>& raw,std::uint32_t fs,std::uint32_t eligibility) {
    const auto l=limits(); PositionRans16mFullLiteralStreamHeader stream{fs,raw.size()};
    std::vector<std::byte> bytes(112);
    check(serialize_position_rans_16m_full_literal_stream(stream,l,bytes)==PositionRans16mFullLiteralFormatError::none,"reference header");
    std::vector<Token> tokens(fs);
    std::vector<std::byte> output(20*fs+9355);
    for (std::size_t pos=0,sequence=0;pos<raw.size();pos+=fs,++sequence) {
        const auto size=std::min<std::size_t>(fs,raw.size()-pos);
        // Independent nearest-first exhaustive search; no index or production cursor.
        std::size_t count{};
        for (std::size_t at=0;at<size;) {
            std::size_t best{},distance{};
            for (std::size_t d=1;d<=at;++d) {
                std::size_t length{};
                while (length<std::min<std::size_t>(258,size-at) && raw[pos+at+length]==raw[pos+at+length-d]) ++length;
                if (length>best) { best=length; distance=d; }
            }
            using Kind=marc::dictionary::internal::LzssTypedTokenKind;
            tokens[count++]=best>=eligibility ? Token{Kind::match,0,static_cast<std::uint32_t>(distance),static_cast<std::uint32_t>(best)}
                : Token{Kind::literal,std::to_integer<std::uint8_t>(raw[pos+at]),0,0};
            at+=best>=eligibility ? best : 1;
        }
        const auto f=encode_position_rans_16m_full_literal_frame(std::span(tokens).first(count),{stream,l,sequence,pos},output);
        check(f.error==PositionRans16mFullLiteralFrameError::none,"reference frame");
        bytes.insert(bytes.end(),output.begin(),output.begin()+f.serialized_size);
    }
    return bytes;
}
void budgets() {
    auto l=limits(); PositionRans16mFullLiteralStreamHeader s{257,1028};
    l.max_block_size=10*257;
    PositionRans16mFullLiteralWorkspace e{},d{};
    check(PositionRans16mFullLiteralOwnedEncoder::requirements(s,l,e)==Code::none,"encoder query");
    check(PositionRans16mFullLiteralOwnedDecoder::requirements(257,l,d)==Code::none,"decoder query");
    for (const auto encoder : {false,true}) {
        const auto r=encoder ? e : d;
        auto low=l; low.max_internal_buffered_bytes=r.aggregate_bytes-1;
        auto sentinel=r;
        const auto queried=encoder ? PositionRans16mFullLiteralOwnedEncoder::requirements(s,low,sentinel)
            : PositionRans16mFullLiteralOwnedDecoder::requirements(257,low,sentinel);
        check(queried==Code::limit_exceeded && sentinel==r,"query strong output");
        Code error{}; allocations=0; fail_at=0; tracked=true;
        if (encoder) check(!PositionRans16mFullLiteralOwnedEncoder::create(s,low,error),"encoder budget refuse");
        else check(!PositionRans16mFullLiteralOwnedDecoder::create(257,low,error),"decoder budget refuse");
        tracked=false;
        check(error==Code::limit_exceeded && allocations==0,"budget refusal before allocation");
        auto exact=l; exact.max_internal_buffered_bytes=r.aggregate_bytes;
        for (std::size_t fail=1;fail<=(encoder ? 5u : 4u);++fail) {
            const auto before=live;
            allocations=0; fail_at=fail; tracked=true;
            if (encoder) check(!PositionRans16mFullLiteralOwnedEncoder::create(s,exact,error),"encoder allocation refusal");
            else check(!PositionRans16mFullLiteralOwnedDecoder::create(257,exact,error),"decoder allocation refusal");
            tracked=false; fail_at=0;
            check(error==Code::out_of_memory && live==before && allocations==fail,"allocation cleanup");
        }
        if (encoder) {
            auto p=PositionRans16mFullLiteralOwnedEncoder::create(s,exact,error);
            check(p && error==Code::none,"exact encoder budget");
        } else {
            auto p=PositionRans16mFullLiteralOwnedDecoder::create(257,exact,error);
            check(p && error==Code::none && p->process({}, {},0).status==Status::need_input,"exact decoder budget");
        }
    }
    PositionRans16mFullLiteralWorkspace full_e{},full_d{};
    check(PositionRans16mFullLiteralOwnedEncoder::requirements({},limits(),full_e)==Code::none,"full encoder query");
    check(PositionRans16mFullLiteralOwnedDecoder::requirements(16777216,limits(),full_d)==Code::none,"full decoder query");
    std::cout<<"encoder_workspace="<<full_e.aggregate_bytes<<" decoder_workspace="<<full_d.aggregate_bytes<<'\n';
}
}
int main(int argc,char** argv) {
    if(argc==2 && std::string(argv[1])=="--resources") {
        for(const auto f : {65536u,1048576u,4194304u,16777216u}) {
            PositionRans16mFullLiteralWorkspace e{},d{};
            if(PositionRans16mFullLiteralOwnedEncoder::requirements({f,0},limits(),e)!=Code::none
                || PositionRans16mFullLiteralOwnedDecoder::requirements(f,limits(),d)!=Code::none) return 2;
            std::cout<<f<<' '<<e.raw_bytes<<' '<<e.token_count*sizeof(Token)<<' '
                <<e.serialized_bytes<<' '<<e.finder_bytes<<' '<<e.aggregate_bytes<<' '
                <<d.aggregate_bytes<<'\n';
        }
        return 0;
    }

    if (argc==4 && (std::string(argv[1])=="--file" || std::string(argv[1])=="--file-5")) {
        std::ifstream input(argv[2],std::ios::binary|std::ios::ate);
        if (!input || input.tellg()<0 || input.tellg()>128*1024*1024) return 2;
        input.seekg(0);
        std::vector<char> chars((std::istreambuf_iterator<char>(input)),{});
        if (!input) return 2;
        std::vector<std::byte> raw(chars.size());
        std::transform(chars.begin(),chars.end(),raw.begin(),[](char c) { return static_cast<std::byte>(c); });
        const auto wire=encode(raw,16777216,65521,65537,std::string(argv[1])=="--file-5" ? 5 : 3);
        decode(wire,raw,16777216,4093,1031);
        if (failures) return 3;
        std::ofstream output(argv[3],std::ios::binary);
        output.write(reinterpret_cast<const char*>(wire.data()),static_cast<std::streamsize>(wire.size()));
        return output ? 0 : 4;
    }
    budgets();
    for (unsigned value=0;value<256;++value) {
        const std::vector<std::byte> raw{static_cast<std::byte>(value)};
        const auto wire=encode(raw,1,1,1);
        decode(wire,raw,1,1,1);
    }
    std::vector<std::byte> alphabet(256);
    for (unsigned value=0;value<256;++value) alphabet[value]=static_cast<std::byte>(value);
    decode(encode(alphabet,17,1,1),alphabet,17,1,1);
    std::mt19937 rng(1517);
    for (std::uint32_t fs : {1u,2u,3u,17u,257u}) {
        for (const std::size_t size : {std::size_t{0},std::size_t{1},std::size_t{fs},std::size_t{3*fs-1},std::size_t{3*fs+1}}) {
            std::vector<std::byte> raw(size);
            for (std::size_t i=0;i<size;++i) raw[i]=static_cast<std::byte>(i%7 ? rng()%4 : rng()%256);
            for (const auto eligibility : {3u,5u}) {
              const auto expected=reference(raw,fs,eligibility);
              for (const auto chunk : {1u,7u,4096u}) {
                const auto wire=encode(raw,fs,chunk,chunk,eligibility);
                check(wire==expected,"exhaustive/chunk deterministic bytes");
                decode(wire,raw,fs,chunk,chunk);
              }
            }
        }
    }
    qualify_finder();
    Code error{};
    auto early=PositionRans16mFullLiteralOwnedEncoder::create({17,17},limits(),error);
    std::array<std::byte,4> raw{},guard{std::byte{0xa5},std::byte{0xa5},std::byte{0xa5},std::byte{0xa5}};
    const auto r=early->process(raw,guard,end);
    check(r.status==Status::error && r.error.code==Code::invalid_argument && !r.input_consumed && !r.output_produced
        && std::all_of(guard.begin(),guard.end(),[](auto b) { return b==std::byte{0xa5}; }),"premature EndInput unchanged");
    auto low=limits(); low.max_block_size=1;
    auto refused=PositionRans16mFullLiteralOwnedEncoder::create({1,1},low,error);
    const std::array<std::byte,1> one{std::byte{65}};
    const auto failure=run(*refused,one,1,4096);
    std::array<std::byte,112> header{};
    check(serialize_position_rans_16m_full_literal_stream({1,1},low,header)==PositionRans16mFullLiteralFormatError::none,"refusal header");
    check(failure.result.status==Status::error && failure.result.error.code==Code::limit_exceeded
        && failure.bytes==std::vector<std::byte>(header.begin(),header.end()),"encoder failed frame withheld");
    auto invalid=PositionRans16mFullLiteralOwnedEncoder::create({1,1},limits(),error,4);
    check(!invalid && error==Code::invalid_argument,"invalid eligibility");
    auto unsupported=PositionRans16mFullLiteralOwnedEncoder::create({1,1},limits(),error);
    check(unsupported->process({}, {},marc::core::flag_value(marc::core::ProcessFlags::reset_block)).error.code==Code::unsupported,"owned reset block");
    auto broken=encode({std::byte{65},std::byte{66}},1,4096,4096);
    (void)marc::core::store_le<std::uint64_t>(broken,broken.size()-8,(UINT64_C(1)<<31)+1);
    auto protected_decoder=PositionRans16mFullLiteralOwnedDecoder::create(1,limits(),error);
    const auto later=run(*protected_decoder,broken,7,1);
    check(later.result.status==Status::error && later.result.error.code==Code::malformed_stream
        && later.bytes==std::vector<std::byte>{std::byte{65}},"owned failed later frame withheld");
    std::array<std::byte,112> larger_header{};
    check(serialize_position_rans_16m_full_literal_stream({257,1},limits(),larger_header)==PositionRans16mFullLiteralFormatError::none,"large capacity header");
    auto smaller=PositionRans16mFullLiteralOwnedDecoder::create(1,limits(),error);
    const auto limited_header=smaller->process(larger_header,guard,end);
    check(limited_header.status==Status::error && limited_header.error.code==Code::limit_exceeded
        && limited_header.input_consumed==112 && !limited_header.output_produced,"declared frame exceeds decoder capacity");
    std::cout<<"failures="<<failures<<'\n'; return failures ? 1 : 0;
}
