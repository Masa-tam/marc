#include "context/lzss_position_rans_4m_tokens.hpp"
#include "dictionary/lzss_typed_reconstructor.hpp"
#include "core/endian.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>

namespace {
using namespace marc::context::internal;
using namespace marc::dictionary::internal;
using namespace marc::entropy::internal;
using Error = LzssPositionRans4mTokenError;
constexpr LzssParameters parameters{4194304,3,258,0};
marc::core::DecoderLimits limits() {
    marc::core::DecoderLimits l{};
    l.max_block_size = UINT64_C(40)<<20;
    return l;
}
bool equal(std::span<const LzssTypedToken> a, std::span<const LzssTypedToken> b) {
    return a.size() == b.size() && std::equal(a.begin(),a.end(),b.begin(),[](auto x,auto y) {
        return x.kind == y.kind && x.literal == y.literal && x.distance == y.distance && x.length == y.length;
    });
}
int failures{};
void check(bool good,const char* message) { if (!good) { ++failures; std::cerr<<message<<'\n'; } }
struct Wire { std::vector<std::byte> descriptor,payload; LzssFieldContextValidationContext context{}; };
Wire encode(std::span<const LzssTypedToken> tokens, std::uint32_t raw_size) {
    PositionRans4mDescriptor d{};
    const LzssTypedFrameValidationContext context{static_cast<std::uint32_t>(tokens.size()),raw_size,0};
    const auto l = limits();
    const auto r = plan_lzss_position_rans_4m_tokens(tokens,parameters,context,l,d);
    check(r.error == Error::none,"plan");
    Wire wire;
    wire.context = {context.declared_token_count,r.event_count,r.decision_count,raw_size,0};
    wire.descriptor.resize(r.descriptor_size); wire.payload.resize(r.payload_size);
    const auto actual = encode_lzss_position_rans_4m_tokens(tokens,parameters,context,l,wire.descriptor,wire.payload);
    check(actual.error == Error::none,"encode");
    return wire;
}
void roundtrip(std::span<const LzssTypedToken> tokens,std::uint32_t raw_size) {
    const auto wire = encode(tokens,raw_size);
    const auto l = limits();
    std::vector<LzssTypedToken> decoded(tokens.size());
    check(decode_lzss_position_rans_4m_tokens(wire.descriptor,wire.payload,parameters,wire.context,l,decoded).error == Error::none,"decode");
    check(equal(tokens,decoded),"transactional tokens");
    std::fill(decoded.begin(),decoded.end(),LzssTypedToken{});
    check(decode_lzss_position_rans_4m_token_scratch(wire.descriptor,wire.payload,parameters,wire.context,l,decoded).error == Error::none,"scratch decode");
    check(equal(tokens,decoded),"scratch tokens");
}
} // namespace

int main(int argc,char** argv) {
    if (argc == 4 && std::string(argv[1]) == "--pdop") {
        std::ifstream file(argv[2],std::ios::binary);
        std::vector<char> raw((std::istreambuf_iterator<char>(file)),{});
        if (!file || raw.size() < 28 || std::string(raw.data(),4) != "PDOP") return 2;
        std::vector<std::byte> storage(raw.size());
        std::transform(raw.begin(),raw.end(),storage.begin(),[](char v) { return static_cast<std::byte>(v); });
        const auto bytes = std::span<const std::byte>(storage);
        std::array<std::uint32_t,6> header{};
        for (std::size_t i = 0; i < header.size(); ++i) (void)marc::core::load_le(bytes,4+4*i,header[i]);
        const auto [rs,tc,range,control_descriptor,control_payload,minimum] = header;
        (void)range;
        if (rs > 4194304 || tc > rs || (minimum != 3 && minimum != 5)
            || UINT64_C(28)+rs+UINT64_C(10)*tc != bytes.size()) return 3;
        std::vector<LzssTypedToken> tokens(tc);
        for (std::size_t i = 0; i < tc; ++i) {
            const auto cursor = 28+rs+10*i;
            tokens[i].kind = static_cast<LzssTypedTokenKind>(std::to_integer<unsigned>(bytes[cursor]));
            tokens[i].literal = std::to_integer<std::uint8_t>(bytes[cursor+1]);
            (void)marc::core::load_le(bytes,cursor+2,tokens[i].distance);
            (void)marc::core::load_le(bytes,cursor+6,tokens[i].length);
        }
        const auto wire = encode(tokens,rs);
        if (failures) return 4;
        std::vector<LzssTypedToken> output(tc);
        const auto l = limits();
        if (decode_lzss_position_rans_4m_token_scratch(wire.descriptor,wire.payload,parameters,wire.context,l,output).error != Error::none
            || !equal(tokens,output)) return 5;
        std::vector<std::byte> restored(rs);
        if (reconstruct_lzss_typed_frame(output,parameters,{tc,rs,0},l,restored,
                LzssTypedTokenVariant::field_context_4m_short_length_escape).error != LzssTypedReconstructError::none
            || !std::equal(restored.begin(),restored.end(),bytes.begin()+28)) return 6;
        std::ofstream descriptor(std::string(argv[3])+".descriptor",std::ios::binary);
        std::ofstream payload(std::string(argv[3])+".payload",std::ios::binary);
        if (!descriptor || !payload) return 7;
        descriptor.write(reinterpret_cast<const char*>(wire.descriptor.data()),wire.descriptor.size());
        payload.write(reinterpret_cast<const char*>(wire.payload.data()),wire.payload.size());
        descriptor.close(); payload.close();
        if (!descriptor || !payload) return 8;
        std::cout<<"{\"raw\":"<<rs<<",\"tokens\":"<<tc
                 <<",\"events\":"<<wire.context.declared_event_count
                 <<",\"decisions\":"<<wire.context.declared_decision_count
                 <<",\"descriptor\":"<<wire.descriptor.size()<<",\"payload\":"<<wire.payload.size()
                 <<",\"contextual_descriptor\":"<<control_descriptor
                 <<",\"contextual_payload\":"<<control_payload<<",\"minimum\":"<<minimum<<"}\n";
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--fixture") {
        std::ifstream file(argv[2],std::ios::binary);
        std::vector<char> raw((std::istreambuf_iterator<char>(file)),{});
        if (!file || raw.size() < 24) return 2;
        std::vector<std::byte> storage(raw.size());
        std::transform(raw.begin(),raw.end(),storage.begin(),[](char v) { return static_cast<std::byte>(v); });
        const auto bytes = std::span<const std::byte>(storage);
        std::array<std::uint32_t,6> header{};
        for (std::size_t i = 0; i < header.size(); ++i) (void)marc::core::load_le(bytes,4*i,header[i]);
        const auto [rs,tc,ec,dc,ds,ps] = header;
        if (rs > 4194304 || tc > rs || ds > 5152 || ps > 18*UINT64_C(4194304)+8
            || UINT64_C(24)+10*static_cast<std::uint64_t>(tc)+ds+ps+rs != bytes.size()) return 3;
        std::vector<LzssTypedToken> tokens(tc);
        for (std::size_t i = 0; i < tc; ++i) {
            const auto cursor = 24+10*i;
            tokens[i].kind = static_cast<LzssTypedTokenKind>(std::to_integer<unsigned>(bytes[cursor]));
            tokens[i].literal = std::to_integer<std::uint8_t>(bytes[cursor+1]);
            (void)marc::core::load_le(bytes,cursor+2,tokens[i].distance);
            (void)marc::core::load_le(bytes,cursor+6,tokens[i].length);
        }
        const auto wire = encode(tokens,rs);
        if (failures || wire.context.declared_event_count != ec || wire.context.declared_decision_count != dc
            || wire.descriptor.size() != ds || wire.payload.size() != ps) return 4;
        const auto start = 24+10*static_cast<std::size_t>(tc);
        if (!std::equal(wire.descriptor.begin(),wire.descriptor.end(),bytes.begin()+start)
            || !std::equal(wire.payload.begin(),wire.payload.end(),bytes.begin()+start+ds)) return 5;
        std::vector<LzssTypedToken> output(tc);
        const auto l = limits();
        if (decode_lzss_position_rans_4m_token_scratch(wire.descriptor,wire.payload,parameters,wire.context,l,output).error != Error::none
            || !equal(tokens,output)) return 6;
        std::vector<std::byte> restored(rs);
        if (reconstruct_lzss_typed_frame(output,parameters,{tc,rs,0},l,restored,
                LzssTypedTokenVariant::field_context_4m_short_length_escape).error != LzssTypedReconstructError::none
            || !std::equal(restored.begin(),restored.end(),bytes.begin()+start+ds+ps)) return 7;
        return 0;
    }
    roundtrip({},0);
    for (unsigned value = 0; value < 256; ++value) {
        const LzssTypedToken token{LzssTypedTokenKind::literal,static_cast<std::uint8_t>(value),0,0};
        roundtrip(std::span(&token,1),1);
    }
    for (unsigned length = 3; length <= 258; ++length) {
        const std::array tokens{LzssTypedToken{LzssTypedTokenKind::literal,65,0,0},
            LzssTypedToken{LzssTypedTokenKind::match,0,1,length}};
        roundtrip(tokens,length+1);
    }
    const std::array tokens{LzssTypedToken{LzssTypedTokenKind::literal,65,0,0},
        LzssTypedToken{LzssTypedTokenKind::match,0,1,5},LzssTypedToken{LzssTypedTokenKind::literal,3,0,0},
        LzssTypedToken{LzssTypedTokenKind::match,0,3,4},LzssTypedToken{LzssTypedTokenKind::literal,255,0,0}};
    roundtrip(tokens,12);
    const auto wire = encode(tokens,12);
    auto l = limits();
    const LzssTypedToken sentinel{LzssTypedTokenKind::match,99,999,999};
    std::vector<LzssTypedToken> output(tokens.size(),sentinel), saved = output;
    for (std::size_t n = 0; n < wire.payload.size(); ++n) {
        check(decode_lzss_position_rans_4m_tokens(wire.descriptor,std::span(wire.payload).first(n),
            parameters,wire.context,l,output).error != Error::none,"truncated payload admitted");
        check(equal(output,saved),"truncated output mutation");
    }
    check(decode_lzss_position_rans_4m_tokens(wire.descriptor,wire.payload,parameters,wire.context,l,
        std::span(output).first(output.size()-1)).error == Error::output_too_small,"capacity");
    check(equal(output,saved),"capacity mutation");
    auto context = wire.context; context.declared_raw_size = 11;
    check(decode_lzss_position_rans_4m_tokens(wire.descriptor,wire.payload,parameters,context,l,output).error != Error::none,"raw mismatch");
    check(equal(output,saved),"raw mismatch mutation");
    check(decode_lzss_position_rans_4m_token_scratch(wire.descriptor,wire.payload,parameters,context,l,output).error != Error::none,"scratch late failure");
    check(!equal(output,saved),"scratch did not exercise retained private prefix");
    output = saved;
    const auto reference_error = decode_lzss_position_rans_4m_tokens({},wire.payload,parameters,wire.context,l,{}).error;
    check(reference_error == Error::entropy_error,"malformed precedence");
    check(decode_lzss_position_rans_4m_token_scratch({},wire.payload,parameters,wire.context,l,{}).error == reference_error,"scratch capacity precedence");
    l.max_total_output_size = 11;
    check(decode_lzss_position_rans_4m_tokens(wire.descriptor,wire.payload,parameters,wire.context,l,output).error == Error::limit_exceeded,"total ceiling");
    check(equal(output,saved),"limit mutation");
    l = limits();
    auto broken = tokens; broken[1].distance = 2;
    std::vector<std::byte> desc(5152,std::byte{0xa5}), payload(128,std::byte{0xa5});
    const auto old_desc = desc, old_payload = payload;
    check(encode_lzss_position_rans_4m_tokens(broken,parameters,{5,12,0},l,desc,payload).error == Error::invalid_token,"invalid history");
    check(desc == old_desc && payload == old_payload,"invalid encode mutated");
    check(encode_lzss_position_rans_4m_tokens(tokens,parameters,{5,12,0},l,std::span(desc).first(1),payload).error == Error::output_too_small,"short encode descriptor");
    check(desc == old_desc && payload == old_payload,"short encode mutated");
    check(encode_lzss_position_rans_4m_tokens(tokens,parameters,{5,12,0},l,desc,desc).error == Error::overlapping_buffers,"encode output overlap");
    check(desc == old_desc,"overlap mutation");
    auto mutable_tokens = tokens;
    const auto aliased_payload = std::as_writable_bytes(std::span(mutable_tokens));
    check(encode_lzss_position_rans_4m_tokens(mutable_tokens,parameters,{5,12,0},l,desc,aliased_payload).error == Error::overlapping_buffers,"encode input overlap");
    check(equal(mutable_tokens,tokens) && desc == old_desc,"encode overlap mutated input");
    std::vector<LzssTypedToken> shared((wire.descriptor.size()+sizeof(LzssTypedToken)-1)/sizeof(LzssTypedToken)+tokens.size());
    const auto shared_bytes = std::as_writable_bytes(std::span(shared));
    std::copy(wire.descriptor.begin(),wire.descriptor.end(),shared_bytes.begin());
    const std::vector<std::byte> before_shared(shared_bytes.begin(),shared_bytes.end());
    check(decode_lzss_position_rans_4m_tokens(shared_bytes.first(wire.descriptor.size()),wire.payload,
        parameters,wire.context,l,shared).error == Error::overlapping_buffers,"decode descriptor overlap");
    check(std::equal(before_shared.begin(),before_shared.end(),shared_bytes.begin()),"decode overlap mutation");
    context = wire.context; context.output_already_committed = UINT64_MAX-1;
    check(decode_lzss_position_rans_4m_tokens(wire.descriptor,wire.payload,parameters,context,l,output).error == Error::arithmetic_overflow,"total overflow");
    check(equal(output,saved),"overflow mutation");
    const auto required = tokens.size()*sizeof(LzssTypedToken)+wire.descriptor.size()+wire.payload.size()+lzss_position_rans_4m_fixed_working_bytes;
    l.max_frame_size = 12; l.max_block_size = wire.context.declared_decision_count;
    l.max_internal_buffered_bytes = required-1;
    check(decode_lzss_position_rans_4m_tokens(wire.descriptor,wire.payload,parameters,wire.context,l,output).error == Error::limit_exceeded,"aggregate decode below bound");
    check(encode_lzss_position_rans_4m_tokens(tokens,parameters,{5,12,0},l,desc,payload).error == Error::limit_exceeded,"aggregate encode below bound");
    check(desc == old_desc && payload == old_payload && equal(output,saved),"aggregate failure mutation");
    l.max_internal_buffered_bytes = required;
    check(decode_lzss_position_rans_4m_tokens(wire.descriptor,wire.payload,parameters,wire.context,l,output).error == Error::none,"aggregate exact bound");
    std::cout<<"failures="<<failures<<'\n';
    return failures ? 1 : 0;
}
