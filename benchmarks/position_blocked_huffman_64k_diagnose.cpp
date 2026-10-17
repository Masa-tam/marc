// First-party retained-wire diagnostic. Does not change production planning.
#include "context/lzss_position_blocked_huffman_64k_tokens.hpp"
#include "context/lzss_contextual_blocked_huffman_decoder.hpp"
#include "context/lzss_short_length_escape.hpp"
#include <algorithm>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <vector>
using namespace marc;
using namespace entropy::internal;
using namespace context::internal;
using namespace dictionary::internal;
using Descriptor=PositionBlockedHuffman64kDescriptor;
using Model=PositionBlockedHuffmanModel;
using Hist=std::array<HuffmanFrequencies,48>;
void check(bool ok,const char* label) { if(!ok) throw std::runtime_error(label); }
std::uint32_t u32(std::span<const std::byte> b,std::size_t p) {
    check(p+4<=b.size(),"integer extent"); std::uint32_t v{};
    for(unsigned i=0;i<4;++i) v|=std::to_integer<std::uint32_t>(b[p+i])<<(8*i);
    return v;
}
void put(std::span<std::byte> b,std::size_t p,std::uint32_t v) {
    for(unsigned i=0;i<4;++i) b[p+i]=static_cast<std::byte>((v>>(8*i))&255);
}
std::vector<std::byte> file(const char* path) {
    const auto size=std::filesystem::file_size(path);
    check(size>=112 && size<=UINT64_C(512)*1048576,"diagnostic input bound");
    std::vector<std::byte> b(static_cast<std::size_t>(size));
    std::ifstream in(path,std::ios::binary); check(bool(in.read(reinterpret_cast<char*>(b.data()),b.size())),"read");return b;
}
std::uint64_t cost(const HuffmanFrequencies& h,const Model& m) {
    if(m.single_symbol!=position_blocked_huffman_no_single) {
        for(std::size_t s=0;s<256;++s) check(!h[s] || s==m.single_symbol,"single coverage");
        return 0;
    }
    std::uint64_t bits{};
    for(std::size_t s=0;s<256;++s) { check(!h[s] || m.lengths[s],"model coverage");bits+=h[s]*m.lengths[s]; }
    return bits;
}
std::size_t record(const Model& m,unsigned alphabet) {
    const auto count=std::count_if(m.lengths.begin(),m.lengths.end(),[](auto n){return n!=0;});
    return m.single_symbol!=position_blocked_huffman_no_single ? 2 : std::min(std::size_t(1+(alphabet+1)/2),std::size_t(3+2*count));
}
Model build(const HuffmanFrequencies& h) {
    Model m{}; unsigned count{},sole{};
    for(unsigned s=0;s<256;++s) if(h[s]) { ++count;sole=s; }
    if(count<=1) m.single_symbol=static_cast<std::uint16_t>(sole);
    else check(build_length_limited_code_lengths(h,m.lengths)==HuffmanBuildError::none,"length build");
    return m;
}
std::array<std::uint64_t,5> bits(const Descriptor& d,const Hist& h) {
    std::array<std::uint64_t,5> result{};
    for(unsigned c=0;c<48;++c) {
        const auto f=position_blocked_huffman_64k_field(c);
        result[f]+=cost(h[c],d.models[(d.override_mask&(UINT64_C(1)<<c)) ? 5+c : f]);
    }
    return result;
}
std::array<std::uint64_t,5> records(const Descriptor& d) {
    std::array<std::uint64_t,5> result{};
    for(unsigned f=0;f<5;++f) if(d.field_mask&(1U<<f)) result[f]+=record(d.models[f],position_blocked_huffman_64k_fields[f]);
    for(unsigned c=0;c<48;++c) if(d.override_mask&(UINT64_C(1)<<c)) result[position_blocked_huffman_64k_field(c)]+=record(d.models[5+c],position_blocked_huffman_64k_alphabets[c]);
    return result;
}
void recalc(Descriptor& d,const Hist& h,std::uint64_t uniform) {
    const auto b=bits(d,h); const auto total=std::accumulate(b.begin(),b.end(),uniform);
    d.payload_size=static_cast<std::uint32_t>((total+7)/8);
    d.final_valid_bits=total ? static_cast<std::uint8_t>((total-1)%8+1) : 0;
}
Descriptor refit(Descriptor d,const Hist& h,std::uint64_t uniform) {
    for(unsigned f=0;f<5;++f) if(d.field_mask&(1U<<f)) {
        HuffmanFrequencies remaining{};
        for(unsigned c=0;c<48;++c) if(position_blocked_huffman_64k_field(c)==f && !(d.override_mask&(UINT64_C(1)<<c)))
            for(unsigned s=0;s<256;++s) remaining[s]+=h[c][s];
        d.models[f]=build(remaining);
    }
    recalc(d,h,uniform);return d;
}
template<class T> void array(const T& a) {
    std::cout<<'[';bool first=true;for(auto v:a) { if(!first) std::cout<<',';first=false;std::cout<<v; }std::cout<<']';
}
int main(int argc,char** argv) {
    try {
        check(argc==4,"usage: position.marc contextual.marc candidate.marc");
        check(!std::filesystem::exists(argv[3]) && !std::filesystem::exists(std::string(argv[3])+".tmp"),"existing candidate");
        auto p=file(argv[1]),c=file(argv[2]);
        check(u32(p,20)==65536 && u32(c,20)==65536,"frame extent");
        core::DecoderLimits limits{};
        std::ofstream out(std::string(argv[3])+".tmp",std::ios::binary);
        check(!std::filesystem::exists(argv[3]) && bool(out),"candidate open");
        out.write(reinterpret_cast<char*>(p.data()),112);
        std::size_t pi=112,ci=112,sequence{};std::uint64_t committed{};
        std::vector<HuffmanDecodeTable> decode_tables(53);
        std::vector<CanonicalHuffmanTable> encode_tables(53);
        while(pi<p.size()) {
            check(pi<=p.size() && p.size()-pi>=64 && ci<=c.size() && c.size()-ci>=64,"frame header extent");
            const auto ph=std::span(p).subspan(pi,64),ch=std::span(c).subspan(ci,64);
            const auto raw=u32(ph,16),nt=u32(ph,20),events=u32(ph,24),decisions=u32(ph,28);
            const auto ps=u32(ph,32),ds=u32(ph,36),cs=u32(ch,32),cds=u32(ch,36);
            check(raw<=65536 && nt<=raw && decisions<=458752,"frame limits");
            check(u32(ch,16)==raw && u32(ch,20)==nt,"matched counts");
            check(u32(ph,40)==0 && u32(ph,44)==0 && u32(ch,40)==0 && u32(ch,44)==0,"side fields");
            check(ds<=p.size()-pi-64 && ps<=p.size()-pi-64-ds && cds<=c.size()-ci-64 && cs<=c.size()-ci-64-cds,"body extent");
            const auto pd=std::span(p).subspan(pi+64,ds),pp=std::span(p).subspan(pi+64+ds,ps);
            Descriptor d{};check(parse_position_blocked_huffman_64k_descriptor(pd,decisions,ps,limits,d)==PositionBlockedHuffmanError::none,"native parse");
            std::vector<LzssTypedToken> pt(nt),ct(nt);
            check(decode_position_blocked_huffman_64k_token_scratch(pd,pp,{nt,raw,decisions,committed},limits,decode_tables,pt).error==PositionBlockedHuffman64kTokenError::none,"native decode");
            ContextualBlockedHuffmanDescriptor control{};
            check(parse_contextual_blocked_huffman_descriptor(std::span(c).subspan(ci+64,cds),u32(ch,28),cs,limits,control)==ContextualBlockedHuffmanFormatError::none,"control parse");
            check(decode_lzss_contextual_blocked_huffman_tokens(control,std::span(c).subspan(ci+64+cds,cs),{65536,5,258,0},
                {nt,u32(ch,24),u32(ch,28),raw,committed},limits,decode_tables,ct).error==LzssContextualBlockedHuffmanDecodeError::none,"control decode");
            for(unsigned i=0;i<nt;++i) check(pt[i].kind==ct[i].kind && pt[i].literal==ct[i].literal && pt[i].distance==ct[i].distance && pt[i].length==ct[i].length,"token equality");
            Hist hist{};std::uint64_t uniform{};std::vector<PositionBlockedHuffmanDecision> requests;
            auto emit=[&](int context,std::uint32_t symbol) { requests.push_back({context,symbol});if(context<0) ++uniform;else ++hist[context][symbol]; };
            unsigned previous=0;int literal=-1;
            for(const auto& token:pt) {
                emit(previous,token.kind==LzssTypedTokenKind::literal ? 0 : 1);
                if(token.kind==LzssTypedTokenKind::literal) { emit(literal<0 ? 3 : 4+(literal>>4),token.literal);previous=1;literal=token.literal; }
                else {
                    const auto length=encode_lzss_short_length_escape(token.length);
                    emit(20+previous,length.length_class);
                    for(unsigned b=0;b<length.bit_count;++b) emit(-1,(length.extra>>b)&1U);
                    const auto dc=std::bit_width(token.distance)-1U;emit(23+length.length_class,dc);
                    for(unsigned b=0;b<dc;++b) emit(32+b,(token.distance>>b)&1U);
                    previous=2;
                }
            }
            check(requests.size()==decisions && events==decisions,"requests");
            std::vector<std::byte> original(ps);std::size_t written{};
            check(encode_position_blocked_huffman_64k(d,requests,limits,encode_tables,original,written)==PositionBlockedHuffmanError::none
                && written==ps && std::equal(original.begin(),original.end(),pp.begin()),"exact native replay");
            auto candidate=refit(d,hist,uniform);
            std::size_t nds{};check(validate_position_blocked_huffman_64k_descriptor(candidate,limits,nds)==PositionBlockedHuffmanError::none,"candidate validation");
            if(nds+candidate.payload_size>=ds+ps) { candidate=d;nds=ds; }
            std::vector<std::byte> description(nds),payload(candidate.payload_size);
            check(serialize_position_blocked_huffman_64k_descriptor(candidate,limits,description,written)==PositionBlockedHuffmanError::none && written==nds,"candidate descriptor");
            check(encode_position_blocked_huffman_64k(candidate,requests,limits,encode_tables,payload,written)==PositionBlockedHuffmanError::none && written==payload.size(),"candidate encoding");
            std::vector<std::byte> header(ph.begin(),ph.end());put(header,32,candidate.payload_size);put(header,36,static_cast<std::uint32_t>(nds));
            for(const auto& part:{header,description,payload}) out.write(reinterpret_cast<const char*>(part.data()),part.size());
            const auto pb=bits(d,hist),pr=records(d);
            std::array<std::uint64_t,5> cb{},cr{};
            const unsigned fa[4]{2,256,8,17};
            auto control_model=[&](const ContextualBlockedHuffmanModel& m,unsigned a) {
                const auto n=std::count_if(m.lengths.begin(),m.lengths.end(),[](auto x){return x!=0;});
                return m.single_symbol!=contextual_blocked_huffman_no_single_symbol ? std::size_t(4) : 4+std::min(std::size_t((a+1)/2),std::size_t(2*n)); };
            for(unsigned f=0;f<4;++f) if(control.field_active_mask&(1U<<f)) cr[f]+=control_model(control.field_models[f],fa[f]);
            for(unsigned i=0;i<31;++i) {
                const auto f=position_blocked_huffman_64k_field(i);
                const auto& m=(control.override_mask&(1U<<i)) ? control.context_models[i] : control.field_models[f];
                cb[f]+=cost(hist[i],{m.lengths,m.single_symbol});
                if(control.override_mask&(1U<<i)) cr[f]+=control_model(m,fa[f]);
            }
            for(unsigned i=32;i<48;++i) cb[4]+=std::accumulate(hist[i].begin(),hist[i].end(),UINT64_C(0));
            check(std::accumulate(cb.begin(),cb.end(),uniform)==(cs ? UINT64_C(8)*(cs-1)+control.final_valid_bits : 0),"control bit accounting");
            check(std::accumulate(cr.begin(),cr.end(),UINT64_C(16))==cds,"control descriptor accounting");
            check(std::accumulate(pr.begin(),pr.end(),UINT64_C(30))==ds,"native descriptor accounting");
            std::cout<<"{\"frame\":"<<sequence<<",\"raw\":"<<raw<<",\"tokens\":"<<nt<<",\"native_descriptor\":"<<ds<<",\"control_descriptor\":"<<cds
                <<",\"native_payload\":"<<ps<<",\"control_payload\":"<<cs<<",\"candidate_descriptor\":"<<nds<<",\"candidate_payload\":"<<payload.size()<<",\"uniform_length_bits\":"<<uniform<<",\"native_bits\":";array(pb);
            std::cout<<",\"control_bits\":";array(cb);std::cout<<",\"native_records\":";array(pr);std::cout<<",\"control_records\":";array(cr);
            std::cout<<",\"candidate_bits\":";array(bits(candidate,hist));std::cout<<",\"candidate_records\":";array(records(candidate));std::cout<<"}\n";
            pi+=64+ds+ps;ci+=64+cds+cs;committed+=raw;++sequence;
        }
        check(pi==p.size() && ci==c.size(),"stream extent");out.close();check(bool(out),"candidate close");
        std::filesystem::rename(std::string(argv[3])+".tmp",argv[3]);return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
