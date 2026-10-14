#include "context/lzss_position_tans_4m_tokens.hpp"
#include "core/endian.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
using namespace marc::context::internal;
using namespace marc::entropy::internal;
using namespace marc::dictionary::internal;
using Error=PositionTans4mTokenError;
int failures{};
void check(bool ok,const char* name) { if (!ok) { ++failures;std::cerr<<name<<'\n'; } }
bool equal(const LzssTypedToken& a,const LzssTypedToken& b) {
    return a.kind==b.kind && a.literal==b.literal && a.distance==b.distance && a.length==b.length;
}
bool equal(std::span<const LzssTypedToken> a,std::span<const LzssTypedToken> b) {
    return a.size()==b.size() && std::equal(a.begin(),a.end(),b.begin(),[](const auto& x,const auto& y){return equal(x,y);});
}
int fixture(const char* path,bool reject_history=false) {
    std::ifstream file(path,std::ios::binary);std::vector<char> raw((std::istreambuf_iterator<char>(file)),{});
    if (!file || raw.size()<16 || raw.size()>(UINT64_C(32)<<20)) return 2;
    std::vector<std::byte> data(raw.size());std::transform(raw.begin(),raw.end(),data.begin(),[](char c){return static_cast<std::byte>(c);});
    auto bytes=std::span<const std::byte>(data);
    std::uint32_t ds{},ps{},tc{},rs{};
    (void)marc::core::load_le(bytes,0,ds);(void)marc::core::load_le(bytes,4,ps);
    (void)marc::core::load_le(bytes,8,tc);(void)marc::core::load_le(bytes,12,rs);
    if (ds>9249 || rs>4194304 || tc>rs || UINT64_C(16)+ds+ps+UINT64_C(16)*tc!=bytes.size()) return 3;
    std::vector<LzssTypedToken> tokens(tc);
    for (std::size_t i=0;i<tc;++i) {
        std::uint32_t k{},v{},distance{},length{};const auto at=16+ds+ps+16*i;
        (void)marc::core::load_le(bytes,at,k);(void)marc::core::load_le(bytes,at+4,v);
        (void)marc::core::load_le(bytes,at+8,distance);(void)marc::core::load_le(bytes,at+12,length);
        if (k>1 || v>255) return 4;
        tokens[i]={static_cast<LzssTypedTokenKind>(k),static_cast<std::uint8_t>(v),distance,length};
    }
    std::vector<std::uint16_t> et(position_tans_4m_table_entries);
    std::vector<TansDecodeEntry> dt(position_tans_4m_table_entries);
    marc::core::DecoderLimits limits{};limits.max_block_size=UINT64_C(8)*4194304;
    if (reject_history) {
        std::uint32_t dc{};(void)marc::core::load_le(bytes,16,dc);
        const PositionTans4mTokenContext context{tc,rs,dc,0};
        const LzssTypedToken sentinel{LzssTypedTokenKind::match,77,77,77};
        std::vector<LzssTypedToken> work(tc+2,sentinel),output(tc+2,sentinel);
        const auto result=decode_position_tans_4m_tokens(bytes.subspan(16,ds),bytes.subspan(16+ds,ps),
            context,limits,dt,std::span(work).first(tc),std::span(output).first(tc));
        if (result.error!=Error::invalid_token || result.token_error!=LzssTypedTokenError::invalid_distance) return 16;
        if (!std::all_of(output.begin(),output.end(),[&](const auto& t){return equal(t,sentinel);})) return 17;
        if (!equal(work[tc],sentinel) || !equal(work[tc+1],sentinel)) return 18;
        return 0;
    }
    PositionTans4mDescriptor model{};
    const auto planned=plan_position_tans_4m_tokens(tokens,{tc,rs,0},limits,et,model);
    if (planned.error!=Error::none || planned.descriptor_size!=ds || planned.payload_size!=ps) return 5;
    std::vector<std::byte> descriptor(ds),payload(ps),scratch(ps);
    const auto encoded=encode_position_tans_4m_tokens(tokens,{tc,rs,0},limits,et,scratch,descriptor,payload);
    if (encoded.error!=Error::none || !std::equal(descriptor.begin(),descriptor.end(),bytes.begin()+16)
        || !std::equal(payload.begin(),payload.end(),bytes.begin()+16+ds)) return 6;
    const PositionTans4mTokenContext context{tc,rs,model.decision_count,0};
    std::vector<LzssTypedToken> work(tc),output(tc);
    if (decode_position_tans_4m_tokens(descriptor,payload,context,limits,dt,work,output).error!=Error::none || !equal(output,tokens)) return 7;
    std::fill(work.begin(),work.end(),LzssTypedToken{});
    if (decode_position_tans_4m_token_scratch(descriptor,payload,context,limits,dt,work).error!=Error::none || !equal(work,tokens)) return 8;
    return 0;
}
}
int main(int argc,char** argv) {
    if (argc==3 && std::string(argv[1])=="--fixture") return fixture(argv[2]);
    if (argc==3 && std::string(argv[1])=="--reject-history-fixture") return fixture(argv[2],true);
    std::vector<LzssTypedToken> tokens{{LzssTypedTokenKind::literal,255,0,0},
        {LzssTypedTokenKind::match,0,1,3},{LzssTypedTokenKind::literal,17,0,0},
        {LzssTypedTokenKind::match,0,3,258},{LzssTypedTokenKind::literal,65,0,0}};
    constexpr std::uint32_t raw_size=264;
    const LzssTypedFrameValidationContext frame{5,raw_size,0};
    std::vector<std::uint16_t> et(position_tans_4m_table_entries);
    std::vector<TansDecodeEntry> dt(position_tans_4m_table_entries);
    PositionTans4mDescriptor model{};
    auto r=plan_position_tans_4m_tokens(tokens,frame,{},et,model);
    check(r.error==Error::none && r.token_count==5 && r.raw_size==raw_size,"valid plan");
    std::vector<std::byte> descriptor(r.descriptor_size),payload(r.payload_size),scratch(r.payload_size);
    check(encode_position_tans_4m_tokens(tokens,frame,{},et,scratch,descriptor,payload).error==Error::none,"encode");
    const PositionTans4mTokenContext context{5,raw_size,model.decision_count,0};
    const LzssTypedToken sentinel{LzssTypedTokenKind::match,77,77,77};
    std::vector<LzssTypedToken> work(5,sentinel),output(7,sentinel);
    r=decode_position_tans_4m_tokens(descriptor,payload,context,{},dt,work,output);
    check(r.error==Error::none && equal(std::span(output).first(5),tokens) && equal(output[5],sentinel) && equal(output[6],sentinel),"ordinary decode and unused tail");
    const auto bad_decode=[&](std::span<const std::byte> d,std::span<const std::byte> p,const PositionTans4mTokenContext& c) {
        std::fill(output.begin(),output.end(),sentinel);
        const auto result=decode_position_tans_4m_tokens(d,p,c,{},dt,work,output);
        check(result.error!=Error::none,"negative rejected");
        check(std::all_of(output.begin(),output.end(),[&](const auto& t){return equal(t,sentinel);}),"ordinary failure preserves all tokens");
    };
    for (std::size_t cut=0;cut<descriptor.size();++cut) bad_decode(std::span(descriptor).first(cut),payload,context);
    for (std::size_t cut=0;cut<payload.size();++cut) bad_decode(descriptor,std::span(payload).first(cut),context);
    auto altered=context;++altered.raw_size;bad_decode(descriptor,payload,altered);
    altered=context;--altered.token_count;bad_decode(descriptor,payload,altered);
    altered=context;++altered.decision_count;bad_decode(descriptor,payload,altered);
    auto bad_state=payload;bad_state[1]=std::byte{0x10};bad_decode(descriptor,bad_state,context);
    auto trailing=payload;trailing.push_back(std::byte{0});bad_decode(descriptor,trailing,context);
    // A terminal mismatch can arise after all private tokens are reconstructed.
    bool late=false;
    for (unsigned offset=0;offset<4096 && !late;++offset) {
        auto changed=payload;(void)marc::core::store_le(changed,0,static_cast<std::uint16_t>(offset));
        std::fill(work.begin(),work.end(),sentinel);
        const auto result=decode_position_tans_4m_token_scratch(descriptor,changed,context,{},dt,work);
        if (result.error!=Error::none && result.token_count>0) {
            late=true;check(!equal(work[0],sentinel),"failed private scratch prefix is not published");
            bad_decode(descriptor,changed,context);
        }
    }
    check(late,"directed late scratch failure found");
    marc::core::DecoderLimits budget{};budget.max_block_size=raw_size;
    budget.max_internal_buffered_bytes=131072+dt.size()*sizeof(TansDecodeEntry)
        +work.size()*sizeof(LzssTypedToken)+output.size()*sizeof(LzssTypedToken)
        +descriptor.size()+payload.size()+sizeof(context)+sizeof(budget);
    check(decode_position_tans_4m_tokens(descriptor,payload,context,budget,dt,work,output).error==Error::none,"exact aggregate token decode budget");
    --budget.max_internal_buffered_bytes;
    const auto before_budget=output;
    check(decode_position_tans_4m_tokens(descriptor,payload,context,budget,dt,work,output).error==Error::limit_exceeded
          && equal(output,before_budget),"one byte below preserves ordinary tokens");
    auto small_window=budget;small_window.max_internal_buffered_bytes=UINT64_C(128)<<20;
    small_window.max_lz_distance=1048575;
    check(decode_position_tans_4m_tokens(descriptor,payload,context,small_window,dt,work,output).error==Error::limit_exceeded
          && equal(output,before_budget),"dictionary limit preflight");
    auto invalid=tokens;invalid[1].distance=2;
    std::fill(descriptor.begin(),descriptor.end(),std::byte{0x71});std::fill(payload.begin(),payload.end(),std::byte{0x71});
    const auto retained_descriptor=descriptor,retained_payload=payload;
    r=encode_position_tans_4m_tokens(invalid,frame,{},et,scratch,descriptor,payload);
    check(r.error==Error::invalid_token && descriptor==retained_descriptor && payload==retained_payload,"invalid reference retains encoded destinations");
    r=encode_position_tans_4m_tokens(tokens,frame,{},et,scratch,std::span(descriptor).first(1),payload);
    check(r.error==Error::output_too_small && descriptor==retained_descriptor && payload==retained_payload,"short output retains destinations");
    r=encode_position_tans_4m_tokens(tokens,frame,{},et,payload,descriptor,payload);
    check(r.error==Error::overlapping_buffers && descriptor==retained_descriptor && payload==retained_payload,"encoder scratch/output overlap");
    model.frequencies.fill(77);const auto retained_model=model;
    check(plan_position_tans_4m_tokens(invalid,frame,{},et,model).error==Error::invalid_token && model==retained_model,"failed plan preserves descriptor");
    check(decode_position_tans_4m_tokens(retained_descriptor,retained_payload,context,{},dt,work,work).error==Error::overlapping_buffers,"ordinary/private token overlap");
    std::cout<<"typed mapping, private prefix discard and ordinary failure retention: "<<(failures ? "FAIL" : "PASS")<<'\n';
    return failures ? 1 : 0;
}



