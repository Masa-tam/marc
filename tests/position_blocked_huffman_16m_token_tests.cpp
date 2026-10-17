#include "context/lzss_position_blocked_huffman_16m_tokens.hpp"
#include "core/endian.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace marc::context::internal;
using namespace marc::entropy::internal;
using namespace marc::dictionary::internal;
using Error=PositionBlockedHuffman16mTokenError;
using E=PositionBlockedHuffmanError;
using Descriptor=PositionBlockedHuffman16mDescriptor;
int failures{};
void check(bool ok,const char* name) { if (!ok) { ++failures;std::cerr<<name<<'\n'; } }
bool equal(const LzssTypedToken& a,const LzssTypedToken& b) {
    return a.kind==b.kind && a.literal==b.literal && a.distance==b.distance && a.length==b.length;
}
bool equal(std::span<const LzssTypedToken> a,std::span<const LzssTypedToken> b) {
    return a.size()==b.size() && std::equal(a.begin(),a.end(),b.begin(),[](const auto& x,const auto& y){return equal(x,y);});
}
constexpr LzssTypedToken sentinel{LzssTypedTokenKind::literal,0xe7,123,456};
template<class T> T load(std::span<const std::byte> data,std::size_t at) {
    T result{};check(marc::core::load_le(data,at,result),"fixture load");return result;
}
void save(const std::string& path,std::span<const std::byte> bytes) {
    std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    check(static_cast<bool>(file),"save planned wire");
}
int fixture(const char* path) {
    std::ifstream file(path,std::ios::binary);std::vector<char> raw((std::istreambuf_iterator<char>(file)),{});
    if (!file || raw.size()<28 || raw.size()>4*1024*1024) return 2;
    std::vector<std::byte> storage(raw.size());std::transform(raw.begin(),raw.end(),storage.begin(),[](char c){return static_cast<std::byte>(c);});
    const auto data=std::span<const std::byte>(storage);
    const auto ds=load<std::uint32_t>(data,0),ps=load<std::uint32_t>(data,4),tc=load<std::uint32_t>(data,8),rs=load<std::uint32_t>(data,12);
    const auto dc=load<std::uint32_t>(data,16),mode=load<std::uint32_t>(data,20),prefix=load<std::uint32_t>(data,24);
    if (ds>2576 || ps>283115521 || tc>16777216 || mode>1 || UINT64_C(28)+ds+ps+UINT64_C(16)*tc!=data.size()) return 3;
    std::vector<LzssTypedToken> tokens(tc);
    for (std::size_t i=0;i<tc;++i) {
        const auto at=28+ds+ps+16*i;
        const auto k=load<std::uint32_t>(data,at),v=load<std::uint32_t>(data,at+4);
        if (k>1 || v>255) return 4;
        tokens[i]={static_cast<LzssTypedTokenKind>(k),static_cast<std::uint8_t>(v),load<std::uint32_t>(data,at+8),load<std::uint32_t>(data,at+12)};
    }
    const auto description=data.subspan(28,ds),payload=data.subspan(28+ds,ps);
    auto fixture_limits=marc::core::DecoderLimits{};fixture_limits.max_block_size=150994944;fixture_limits.max_internal_buffered_bytes=150994944;
    auto tables=std::make_unique<std::array<HuffmanDecodeTable,61>>();
    std::vector<LzssTypedToken> scratch(tc+1,sentinel),output(tc+1,sentinel);
    PositionBlockedHuffman16mTokenContext context{tc,rs,dc,0};
    auto r=decode_position_blocked_huffman_16m_tokens(description,payload,context,fixture_limits,*tables,scratch,output);
    if (mode) {
        check(r.error!=Error::none,"independent negative rejected");
        check(std::all_of(output.begin(),output.end(),[](const auto& t){return equal(t,sentinel);}),"negative ordinary destination retained");
        if (prefix) check(r.token_count>=prefix && !equal(scratch[0],sentinel),"independent late private prefix");
        check(r.token_count==prefix,"exact private prefix before failed token or terminal check");
        if (prefix<tc) check(equal(scratch[prefix],sentinel),"failed token was not assigned to scratch");
    } else {
        check(r.error==Error::none && r.raw_size==rs && r.decision_count==dc,"independent decode counts");
        check(equal(tokens,std::span(output).first(tc)),"independent decoded tokens");
        auto et=std::make_unique<std::array<CanonicalHuffmanTable,61>>();Descriptor d{};
        r=plan_position_blocked_huffman_16m_tokens(tokens,{tc,rs,0},fixture_limits,d);
        check(r.error==Error::none,"fixture token plan");
        if (r.error==Error::none) {
            std::vector<std::byte> desc(r.descriptor_size),encoded(r.payload_size),private_payload(r.payload_size);
            r=encode_position_blocked_huffman_16m_tokens(tokens,{tc,rs,0},fixture_limits,*et,private_payload,desc,encoded);
            check(r.error==Error::none,"fixture token encode");
            if (r.error==Error::none) { save(std::string(path)+".descriptor",desc);save(std::string(path)+".payload",encoded); }
        }
    }
    check(equal(scratch.back(),sentinel) && equal(output.back(),sentinel),"fixture token tails");
    check(std::equal(storage.begin(),storage.end(),raw.begin(),[](auto a,char b){return a==static_cast<std::byte>(b);}),"fixture input unchanged");
    return failures ? 1 : 0;
}
void units() {
    const std::vector<LzssTypedToken> tokens{{LzssTypedTokenKind::literal,65,0,0},{LzssTypedTokenKind::match,0,1,3},
        {LzssTypedTokenKind::literal,66,0,0},{LzssTypedTokenKind::match,0,2,258},{LzssTypedTokenKind::match,0,263,4}};
    const LzssTypedFrameValidationContext frame{5,267,0};
    auto et=std::make_unique<std::array<CanonicalHuffmanTable,61>>();
    auto dt=std::make_unique<std::array<HuffmanDecodeTable,61>>();Descriptor model{};
    auto r=plan_position_blocked_huffman_16m_tokens(tokens,frame,{},model);
    check(r.error==Error::none && r.raw_size==267 && r.token_count==5,"mixed token plan");
    const auto ds=r.descriptor_size,ps=r.payload_size;
    std::vector<std::byte> descriptor(ds+1,std::byte{0xa5}),payload(ps+1,std::byte{0xa5}),scratch_payload(ps+1,std::byte{0xa5});
    r=encode_position_blocked_huffman_16m_tokens(tokens,frame,{},*et,scratch_payload,descriptor,payload);
    check(r.error==Error::none && descriptor.back()==std::byte{0xa5} && payload.back()==std::byte{0xa5}
        && scratch_payload.back()==std::byte{0xa5},"encoded tails");
    const auto desc=std::span<const std::byte>(descriptor).first(ds),bytes=std::span<const std::byte>(payload).first(ps);
    const PositionBlockedHuffman16mTokenContext context{5,267,model.decision_count,0};
    std::vector<LzssTypedToken> scratch(6,sentinel),output(6,sentinel);
    r=decode_position_blocked_huffman_16m_tokens(desc,bytes,context,{},*dt,scratch,output);
    check(r.error==Error::none && equal(tokens,std::span(output).first(5)) && equal(output.back(),sentinel),"mixed token round trip");
    auto reject=[&](std::span<const std::byte> d,std::span<const std::byte> p,PositionBlockedHuffman16mTokenContext c) {
        std::fill(output.begin(),output.end(),sentinel);
        const auto bad=decode_position_blocked_huffman_16m_tokens(d,p,c,{},*dt,scratch,output);
        check(bad.error!=Error::none,"bad token block rejected");
        check(std::all_of(output.begin(),output.end(),[](const auto& t){return equal(t,sentinel);}),"failed ordinary tokens unchanged");
        return bad;
    };
    for (std::size_t n=0;n<ds;++n) (void)reject(desc.first(n),bytes,context);
    for (std::size_t n=0;n<ps;++n) (void)reject(desc,bytes.first(n),context);
    auto altered=context;++altered.raw_size;
    std::fill(scratch.begin(),scratch.end(),sentinel);
    r=reject(desc,bytes,altered);
    check(r.error==Error::invalid_counts && r.token_count==5 && !equal(scratch[0],sentinel),"late raw mismatch stays private");
    altered=context;--altered.raw_size;r=reject(desc,bytes,altered);
    check(r.token_error==LzssTypedTokenError::output_size_mismatch && r.token_count==4,"history validated before scratch assignment");
    altered=context;++altered.decision_count;(void)reject(desc,bytes,altered);
    altered=context;altered.token_count=0;(void)reject(desc,bytes,altered);
    altered=context;altered.output_already_committed=UINT64_MAX;
    r=reject(desc,bytes,altered);check(r.error==Error::arithmetic_overflow,"committed size overflow");
    auto padded=payload;
    if (model.final_valid_bits<8 && ps) { padded[ps-1]|=std::byte{0x80};(void)reject(desc,std::span<const std::byte>(padded).first(ps),context); }
    // A valid descriptor may declare an unconsumed trailing bit. The complete
    // token prefix must still remain private when entropy finish rejects it.
    if (model.final_valid_bits<8 && ps) {
        auto longer=model;++longer.final_valid_bits;std::array<std::byte,2575> wire{};std::size_t n{};
        check(serialize_position_blocked_huffman_16m_descriptor(longer,{},wire,n)==E::none,"extra-bit descriptor");
        r=reject(std::span<const std::byte>(wire).first(n),bytes,context);
        check(r.token_count==5 && r.entropy_error==E::trailing_bits,"late entropy finish retains ordinary tokens");
    }
    auto retained=output;
    auto limits=marc::core::DecoderLimits{};limits.max_block_size=267;
    constexpr std::uint64_t workspace=2*sizeof(Descriptor)+sizeof(PositionBlockedHuffman16mDecoder)+16384+2575;
    limits.max_internal_buffered_bytes=workspace+sizeof(*dt)+scratch.size()*sizeof(LzssTypedToken)+output.size()*sizeof(LzssTypedToken)
        +desc.size()+bytes.size()+sizeof(context)+sizeof(limits);
    r=decode_position_blocked_huffman_16m_tokens(desc,bytes,context,limits,*dt,scratch,output);
    check(r.error==Error::none,"exact aggregate decode quota");retained=output;
    --limits.max_internal_buffered_bytes;
    r=decode_position_blocked_huffman_16m_tokens(desc,bytes,context,limits,*dt,scratch,output);
    check(r.error==Error::limit_exceeded && equal(output,retained),"one short aggregate quota retention");
    limits={};limits.max_lz_distance=16777215;
    check(decode_position_blocked_huffman_16m_tokens(desc,bytes,context,limits,*dt,scratch,output).error==Error::limit_exceeded && equal(output,retained),"dictionary limit preflight");
    check(decode_position_blocked_huffman_16m_tokens(desc,bytes,context,{},*dt,scratch,std::span(output).first(4)).error==Error::output_too_small && equal(output,retained),"short ordinary tokens");
    check(decode_position_blocked_huffman_16m_tokens(desc,bytes,context,{},*dt,std::span(scratch).first(4),output).error==Error::output_too_small && equal(output,retained),"short private tokens");
    check(decode_position_blocked_huffman_16m_tokens(desc,bytes,context,{},std::span(*dt).first(60),scratch,output).error!=Error::none && equal(output,retained),"short decode tables");
    const auto before_scratch=scratch;
    check(decode_position_blocked_huffman_16m_tokens(desc,bytes,context,{},*dt,scratch,scratch).error==Error::overlapping_buffers && equal(scratch,before_scratch),"ordinary/private alias preflight");
    check(decode_position_blocked_huffman_16m_token_scratch(std::as_bytes(std::span(scratch)),bytes,context,{},*dt,scratch).error==Error::overlapping_buffers && equal(scratch,before_scratch),"descriptor/private alias preflight");
    const auto saved_desc=descriptor,saved_payload=payload;
    auto invalid=tokens;invalid[1].distance=2;
    check(encode_position_blocked_huffman_16m_tokens(invalid,frame,{},*et,scratch_payload,descriptor,payload).error==Error::invalid_token
        && descriptor==saved_desc && payload==saved_payload,"invalid encoder history retains outputs");
    auto plan_sentinel=model;
    check(plan_position_blocked_huffman_16m_tokens(invalid,frame,{},plan_sentinel).error==Error::invalid_token && plan_sentinel==model,"invalid plan retains descriptor");
    check(encode_position_blocked_huffman_16m_tokens(tokens,frame,{},*et,scratch_payload,std::span(descriptor).first(ds-1),payload).error==Error::output_too_small
        && descriptor==saved_desc && payload==saved_payload,"short descriptor retains both outputs");
    check(encode_position_blocked_huffman_16m_tokens(tokens,frame,{},*et,scratch_payload,descriptor,std::span(payload).first(ps-1)).error==Error::output_too_small
        && descriptor==saved_desc && payload==saved_payload,"short payload retains both outputs");
    check(encode_position_blocked_huffman_16m_tokens(tokens,frame,{},*et,payload,descriptor,payload).error==Error::overlapping_buffers
        && descriptor==saved_desc && payload==saved_payload,"encode scratch/output alias");
    check(encode_position_blocked_huffman_16m_tokens(tokens,frame,{},std::span(*et).first(60),scratch_payload,descriptor,payload).error!=Error::none
        && descriptor==saved_desc && payload==saved_payload,"short encode tables retain both outputs");
    limits={};limits.max_block_size=267;
    constexpr std::uint64_t plan_work=sizeof(PositionBlockedHuffman16mModelBuilder)+2*sizeof(Descriptor)+131072+2575+position_blocked_huffman_16m_refit_workspace;
    limits.max_internal_buffered_bytes=plan_work+tokens.size()*sizeof(LzssTypedToken)+sizeof(model)+sizeof(frame)+sizeof(limits)+ds+ps;
    check(plan_position_blocked_huffman_16m_tokens(tokens,frame,limits,plan_sentinel).error==Error::none && plan_sentinel==model,"exact aggregate plan quota");
    --limits.max_internal_buffered_bytes;
    check(plan_position_blocked_huffman_16m_tokens(tokens,frame,limits,plan_sentinel).error==Error::limit_exceeded && plan_sentinel==model,"one short plan quota retention");
    constexpr std::uint64_t encode_work=plan_work+sizeof(Descriptor)+sizeof(PositionBlockedHuffman16mPrivateWriter)+2*2575;
    limits.max_internal_buffered_bytes=encode_work+tokens.size()*sizeof(LzssTypedToken)+sizeof(*et)+scratch_payload.size()+descriptor.size()+payload.size()+sizeof(frame)+sizeof(limits);
    check(encode_position_blocked_huffman_16m_tokens(tokens,frame,limits,*et,scratch_payload,descriptor,payload).error==Error::none
        && descriptor==saved_desc && payload==saved_payload,"exact aggregate encode quota and determinism");
    --limits.max_internal_buffered_bytes;
    check(encode_position_blocked_huffman_16m_tokens(tokens,frame,limits,*et,scratch_payload,descriptor,payload).error==Error::limit_exceeded
        && descriptor==saved_desc && payload==saved_payload,"one short encode quota retention");
    for (unsigned mutation=0;mutation<4;++mutation) {
        invalid=tokens;
        if (mutation==0) invalid.back().kind=static_cast<LzssTypedTokenKind>(2);
        if (mutation==1) invalid.back().literal=1;
        if (mutation==2) invalid.back().length=259;
        if (mutation==3) invalid.front().length=1;
        check(encode_position_blocked_huffman_16m_tokens(invalid,frame,{},*et,scratch_payload,descriptor,payload).error==Error::invalid_token
            && descriptor==saved_desc && payload==saved_payload,"invalid encoder token fields retain outputs");
    }
    auto aliased_tokens=tokens;const auto saved_tokens=aliased_tokens;
    check(encode_position_blocked_huffman_16m_tokens(aliased_tokens,frame,{},*et,scratch_payload,descriptor,
        std::as_writable_bytes(std::span(aliased_tokens))).error==Error::overlapping_buffers && equal(aliased_tokens,saved_tokens)
        && descriptor==saved_desc,"encoder token source/output alias preflight");
    Descriptor empty{};
    r=plan_position_blocked_huffman_16m_tokens({}, {},{},empty);
    check(r.error==Error::none && r.decision_count==0 && r.payload_size==0 && r.descriptor_size==31,"empty isolated token plan");
    std::array<std::byte,31> empty_wire{};std::size_t n{};
    check(serialize_position_blocked_huffman_16m_descriptor(empty,{},empty_wire,n)==E::none,"empty isolated description");
    check(decode_position_blocked_huffman_16m_tokens(empty_wire,{}, {},{},*dt,{},{}).error==Error::none,"empty isolated decode");
    check(encode_position_blocked_huffman_16m_tokens({}, {},{},*et,{},empty_wire,{}).error==Error::none,"empty isolated encode");
    // Full reset-frame literal and overlapping-match cases are finite token
    // tests, independent of a public encoder's match-selection heuristics.
    for (bool matches : {false,true}) {
        std::vector<LzssTypedToken> full;
        std::uint32_t raw=0;
        while (raw<16777216) {
            if (matches && raw && 16777216-raw>=3) {
                const auto length=std::min(258U,16777216-raw);
                full.push_back({LzssTypedTokenKind::match,0,1,length});raw+=length;
            } else { full.push_back({LzssTypedTokenKind::literal,static_cast<std::uint8_t>(raw),0,0});++raw; }
        }
        const auto count=static_cast<std::uint32_t>(full.size());Descriptor m{};
        auto full_limits=marc::core::DecoderLimits{};full_limits.max_block_size=150994944;
        const LzssTypedFrameValidationContext full_frame{count,16777216,0};
        const auto plan_charge=position_blocked_huffman_16m_plan_workspace+full.size()*sizeof(LzssTypedToken)
            +sizeof(m)+sizeof(full_frame)+sizeof(full_limits)+2575+UINT64_C(283115520);
        full_limits.max_internal_buffered_bytes=std::max<std::uint64_t>(plan_charge,full_limits.max_block_size);
        auto planned=plan_position_blocked_huffman_16m_tokens(full,{count,16777216,0},full_limits,m);
        check(planned.error==Error::none,"full frame token plan");
        if (planned.error!=Error::none) continue;
        std::vector<std::byte> d(planned.descriptor_size),p(planned.payload_size),private_bytes(planned.payload_size);
        const auto encode_charge=position_blocked_huffman_16m_encode_workspace+full.size()*sizeof(LzssTypedToken)
            +sizeof(*et)+private_bytes.size()+d.size()+p.size()+sizeof(full_frame)+sizeof(full_limits);
        full_limits.max_internal_buffered_bytes=std::max<std::uint64_t>(encode_charge,full_limits.max_block_size);
        r=encode_position_blocked_huffman_16m_tokens(full,full_frame,full_limits,*et,private_bytes,d,p);
        check(r.error==Error::none,"full frame token encode");
        std::vector<LzssTypedToken> work(full.size()),out(full.size());
        const PositionBlockedHuffman16mTokenContext full_context{count,16777216,m.decision_count,0};
        const auto decode_charge=position_blocked_huffman_16m_decode_workspace+sizeof(*dt)
            +(work.size()+out.size())*sizeof(LzssTypedToken)+d.size()+p.size()+sizeof(full_context)+sizeof(full_limits);
        // The shared limits contract also requires block ceiling <= grant.
        full_limits.max_block_size=std::max<std::uint64_t>(16777216,m.decision_count);
        const auto admission=std::max<std::uint64_t>(decode_charge,full_limits.max_block_size);
        // Exact finite-test storage grant, not a proposed public default.
        full_limits.max_internal_buffered_bytes=admission;
        r=decode_position_blocked_huffman_16m_tokens(d,p,full_context,full_limits,*dt,work,out);
        check(r.error==Error::none && equal(full,out),"full frame token restoration");
        --full_limits.max_internal_buffered_bytes;
        r=decode_position_blocked_huffman_16m_tokens(d,p,full_context,full_limits,*dt,work,out);
        check(r.error==Error::limit_exceeded && equal(full,out) && equal(full,work),"full one-short admission retains both token spans");
        std::cout<<"full-token-charge "<<count<<' '<<decode_charge<<' '<<admission<<' '<<full_limits.max_block_size<<'\n';
    }
    std::cout<<"private single-pass tokens, ordinary failure retention and bounded regions checked\n";
}
}
int main(int argc,char** argv) { if (argc==2) return fixture(argv[1]);units();return failures ? 1 : 0; }
