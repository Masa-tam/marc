#include "frame/lzss_position_blocked_huffman_64k_frame.hpp"
#include "core/endian.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace marc::frame::internal;
using namespace marc::entropy::internal;
using namespace marc::dictionary::internal;
using Error=PositionBlockedHuffman64kFrameError;
using F=PositionBlockedHuffman64kFormatError;
using Stream=PositionBlockedHuffman64kStreamHeader;
using Context=PositionBlockedHuffman64kFrameContext;
using Layout=PositionBlockedHuffman64kFrameLayout;
constexpr auto marker=std::byte{0xa5};
int failures{};
void check(bool ok,const char* name) { if (!ok) { ++failures;std::cerr<<name<<'\n'; } }
bool marked(std::span<const std::byte> bytes) { return std::all_of(bytes.begin(),bytes.end(),[](auto b){return b==marker;}); }
template<class T> T load(std::span<const std::byte> data,std::size_t at) {
    T value{};check(marc::core::load_le(data,at,value),"load fixture");return value;
}
void save(const std::string& path,std::span<const std::byte> bytes) {
    std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    check(static_cast<bool>(file),"save encoded frame");
}
int fixture(const char* path) {
    std::ifstream file(path,std::ios::binary);std::vector<char> chars((std::istreambuf_iterator<char>(file)),{});
    if (!file || chars.size()<124 || chars.size()>2*1024*1024) return 2;
    std::vector<std::byte> data(chars.size());std::transform(chars.begin(),chars.end(),data.begin(),[](char c){return static_cast<std::byte>(c);});
    const auto bytes=std::span<const std::byte>(data);
    const auto fs=load<std::uint32_t>(bytes,0),rs=load<std::uint32_t>(bytes,4),mode=load<std::uint32_t>(bytes,8);
    if (rs>65536 || mode>1 || UINT64_C(124)+fs+rs!=data.size()) return 3;
    Stream stream{};std::size_t used=777;
    check(parse_position_blocked_huffman_64k_stream(bytes.subspan(12,112),{},stream,used)==F::none && used==112,"independent stream parsed");
    std::array<std::byte,112> header{};
    check(serialize_position_blocked_huffman_64k_stream(stream,{},header)==F::none
        && std::equal(header.begin(),header.end(),bytes.begin()+12),"independent stream bytes");
    if (!fs) { check(rs==0 && stream.original_size==0 && mode==0,"empty stream header only");return failures ? 1 : 0; }
    marc::core::DecoderLimits limits{};const Context context{stream,limits,0,0};
    const auto frame=bytes.subspan(124,fs),expected=bytes.subspan(124+fs,rs);
    Layout layout{};
    check(preflight_position_blocked_huffman_64k_frame(frame,context,layout)==F::none,"independent frame structurally valid");
    if (failures) return 4;
    auto dt=std::make_unique<std::array<HuffmanDecodeTable,53>>();
    std::vector<LzssTypedToken> tokens(layout.header.token_count+1);
    std::vector<std::byte> private_raw(layout.header.raw_size+1,marker),output(layout.header.raw_size+1,marker);
    const auto result=decode_position_blocked_huffman_64k_frame(frame,context,*dt,tokens,private_raw,output);
    if (mode) {
        check(result.error!=Error::none && result.raw_size==0 && result.serialized_size==0 && marked(output),"failed independent frame never publishes");
    } else {
        check(result.error==Error::none && result.raw_size==rs && result.serialized_size==fs,"independent frame decoded counts");
        check(std::equal(expected.begin(),expected.end(),output.begin()) && output.back()==marker && private_raw.back()==marker,"independent raw restoration and guards");
        const auto restored=decode_position_blocked_huffman_64k_frame_scratch(frame,context,*dt,tokens,private_raw);
        check(restored.error==Error::none && restored.raw_size==rs && std::equal(expected.begin(),expected.end(),private_raw.begin()),"private raw restoration");
        auto et=std::make_unique<std::array<CanonicalHuffmanTable,53>>();
        std::vector<std::byte> payload(860160),serialized(862742),encoded(862743,marker);
        const auto r=encode_position_blocked_huffman_64k_frame(std::span<const LzssTypedToken>(tokens).first(layout.header.token_count),context,*et,payload,serialized,encoded);
        check(r.error==Error::none && r.raw_size==rs && encoded[r.serialized_size]==marker,"independent token frame re-encode");
        if (r.error==Error::none) save(std::string(path)+".encoded",std::span<const std::byte>(encoded).first(r.serialized_size));
    }
    check(std::equal(data.begin(),data.end(),chars.begin(),[](auto a,char b){return a==static_cast<std::byte>(b);}),"independent frame input retained");
    return failures ? 1 : 0;
}
void units() {
    marc::core::DecoderLimits limits{};Stream stream{};stream.original_size=267;
    const Context context{stream,limits,0,0};
    std::array<std::byte,113> stream_wire{};stream_wire.fill(marker);
    check(serialize_position_blocked_huffman_64k_stream(stream,limits,stream_wire)==F::none && stream_wire.back()==marker,"stream guard");
    const auto header=std::span<const std::byte>(stream_wire).first(112);
    Stream sentinel{};sentinel.original_size=777;std::size_t consumed=777;
    for (std::size_t cut=0;cut<112;++cut) {
        check(parse_position_blocked_huffman_64k_stream(header.first(cut),limits,sentinel,consumed)==F::truncated
            && sentinel.original_size==777 && consumed==777,"all stream cuts transactional");
    }
    for (std::size_t i=0;i<112;++i) {
        if ((i>=20 && i<24) || (i>=40 && i<48)) continue;
        auto changed=stream_wire;changed[i]^=std::byte{0x80};
        check(parse_position_blocked_huffman_64k_stream(changed,limits,sentinel,consumed)!=F::none
            && sentinel.original_size==777 && consumed==777,"all fixed stream fields reject transactionally");
    }
    check(parse_position_blocked_huffman_64k_stream(header,limits,sentinel,consumed)==F::none
        && consumed==112 && sentinel.original_size==267,"valid stream identity");
    const auto saved_stream=stream;
    check(serialize_position_blocked_huffman_64k_stream(stream,limits,std::as_writable_bytes(std::span(&stream,1)))==F::overlapping_buffers
        && stream.original_size==saved_stream.original_size,"stream serialization alias");
    check(parse_position_blocked_huffman_64k_stream(std::as_bytes(std::span(&sentinel,1)),limits,sentinel,consumed)==F::overlapping_buffers,"stream parser alias");
    for (auto size : {0U,65537U}) {
        auto invalid_stream=stream;invalid_stream.frame_size=size;
        check(validate_position_blocked_huffman_64k_stream(invalid_stream,limits)==F::invalid_parameters,"frame size endpoints");
    }
    auto short_codes=limits;short_codes.max_huffman_code_length=14;
    check(validate_position_blocked_huffman_64k_stream(stream,short_codes)==F::limit_exceeded,"stream Huffman length limit");
    const std::vector<LzssTypedToken> tokens{{LzssTypedTokenKind::literal,65,0,0},{LzssTypedTokenKind::match,0,1,3},
        {LzssTypedTokenKind::literal,66,0,0},{LzssTypedTokenKind::match,0,2,258},{LzssTypedTokenKind::match,0,263,4}};
    auto et=std::make_unique<std::array<CanonicalHuffmanTable,53>>();auto dt=std::make_unique<std::array<HuffmanDecodeTable,53>>();
    std::vector<std::byte> private_payload(1024,marker),serialized(4096,marker),encoded(4097,marker);
    auto r=encode_position_blocked_huffman_64k_frame(tokens,context,*et,private_payload,serialized,encoded);
    check(r.error==Error::none && r.raw_size==267 && encoded[r.serialized_size]==marker,"mixed native frame encode");
    if (r.error!=Error::none) return;
    const auto frame=std::span<const std::byte>(encoded).first(r.serialized_size);
    Layout layout{};check(preflight_position_blocked_huffman_64k_frame(frame,context,layout)==F::none,"native frame preflight");
    Layout retained{};retained.header.sequence=777;retained.requirements.serialized_size=777;
    for (std::size_t cut=0;cut<frame.size();++cut) {
        auto out=retained;
        check(preflight_position_blocked_huffman_64k_frame(frame.first(cut),context,out)!=F::none && out==retained,"every frame cut transactional");
    }
    for (std::size_t i=0;i<64;++i) {
        auto changed=encoded;changed[i]^=std::byte{0x80};auto out=retained;
        check(preflight_position_blocked_huffman_64k_frame(changed,context,out)!=F::none && out==retained,"all changed frame fields rejected");
    }
    check(preflight_position_blocked_huffman_64k_frame_prefix(frame.first(layout.requirements.prefix_size),context,retained)==F::none,"prefix succeeds before payload arrives");
    const auto saved_layout=layout;
    check(preflight_position_blocked_huffman_64k_frame(std::as_bytes(std::span(&layout,1)),context,layout)==F::overlapping_buffers && layout==saved_layout,"layout/input alias");
    std::vector<LzssTypedToken> token_scratch(6);
    std::vector<std::byte> raw(268,marker),output(269,marker);
    r=decode_position_blocked_huffman_64k_frame(frame,context,*dt,token_scratch,raw,output);
    check(r.error==Error::none && r.serialized_size==frame.size() && r.raw_size==267
        && output[0]==std::byte{65} && output[4]==std::byte{66} && output[267]==marker && raw.back()==marker,"native raw reconstruction and tails");
    auto reject=[&](std::span<const std::byte> input,const Context& c) {
        std::fill(output.begin(),output.end(),marker);
        const auto result=decode_position_blocked_huffman_64k_frame(input,c,*dt,token_scratch,raw,output);
        check(result.error!=Error::none && result.serialized_size==0 && result.raw_size==0 && marked(output),"failed frame never publishes");return result;
    };
    auto changed=encoded;changed[layout.requirements.prefix_size-1]^=std::byte{0x80};(void)reject(std::span(changed).first(frame.size()),context);
    // Declaring one extra valid bit reaches entropy finish after the complete
    // private token sequence, yet must not reconstruct or publish raw bytes.
    if (std::to_integer<unsigned>(encoded[72])<8 && layout.header.payload_size) {
        changed=encoded;changed[72]=static_cast<std::byte>(std::to_integer<unsigned>(changed[72])+1);std::fill(raw.begin(),raw.end(),marker);
        r=reject(std::span(changed).first(frame.size()),context);
        check(r.error==Error::token_error && r.token_result.token_count==5 && marked(raw),"late entropy failure retains private raw");
    }
    Stream later=stream;later.original_size=65537;
    Context bad_sequence{later,limits,1,0};(void)reject(frame,bad_sequence);
    const Context bad_committed{later,limits,0,65536};(void)reject(frame,bad_committed);
    check(decode_position_blocked_huffman_64k_frame(frame,context,*dt,token_scratch,raw,raw).error==Error::overlapping_buffers,"raw scratch/output alias");
    const auto before_input=encoded;
    check(decode_position_blocked_huffman_64k_frame(frame,context,*dt,token_scratch,raw,encoded).error==Error::overlapping_buffers
        && encoded==before_input,"raw output/source alias");
    check(decode_position_blocked_huffman_64k_frame(frame,context,*dt,token_scratch,raw,std::span(output).first(266)).error==Error::output_too_small,"short ordinary raw");
    check(decode_position_blocked_huffman_64k_frame(frame,context,*dt,token_scratch,std::span(raw).first(266),output).error==Error::output_too_small,"short private raw");
    auto quota=limits;quota.max_block_size=65536;
    quota.max_internal_buffered_bytes=position_blocked_huffman_64k_decode_frame_workspace+frame.size()+sizeof(*dt)
        +token_scratch.size()*sizeof(LzssTypedToken)+raw.size()+output.size()+sizeof(Context)+sizeof(Stream)+sizeof(quota);
    const Context exact{stream,quota,0,0};
    check(decode_position_blocked_huffman_64k_frame(frame,exact,*dt,token_scratch,raw,output).error==Error::none,"exact frame decode quota");
    --quota.max_internal_buffered_bytes;r=reject(frame,exact);check(r.error==Error::limit_exceeded,"one short frame decode quota");
    quota=limits;quota.max_block_size=65536;
    quota.max_internal_buffered_bytes=position_blocked_huffman_64k_encode_frame_workspace+tokens.size()*sizeof(LzssTypedToken)+sizeof(*et)
        +private_payload.size()+serialized.size()+encoded.size()+sizeof(Context)+sizeof(Stream)+sizeof(quota);
    const Context exact_encode{stream,quota,0,0};
    check(encode_position_blocked_huffman_64k_frame(tokens,exact_encode,*et,private_payload,serialized,encoded).error==Error::none && encoded==before_input,"exact frame encode quota and determinism");
    --quota.max_internal_buffered_bytes;
    r=encode_position_blocked_huffman_64k_frame(tokens,exact_encode,*et,private_payload,serialized,encoded);
    check(r.error==Error::limit_exceeded && r.serialized_size==0 && r.raw_size==0 && encoded==before_input,"one short frame encode quota");
    auto invalid=tokens;invalid[1].distance=2;
    r=encode_position_blocked_huffman_64k_frame(invalid,context,*et,private_payload,serialized,encoded);
    check(r.error==Error::token_error && !r.serialized_size && !r.raw_size && encoded==before_input,"invalid history frame encoder retention");
    Stream empty{};const Context empty_context{empty,limits,0,0};
    check(encode_position_blocked_huffman_64k_frame({},empty_context,*et,private_payload,serialized,encoded).error==Error::format_error && encoded==before_input,"empty native frame forbidden");
    Stream partition{};partition.frame_size=4;partition.original_size=9;
    std::array<std::byte,10> partition_output{};partition_output.fill(marker);
    std::uint64_t committed=0;
    for (std::uint64_t sequence=0;sequence<3;++sequence) {
        const auto size=static_cast<std::uint32_t>(std::min(UINT64_C(4),UINT64_C(9)-committed));
        std::vector<LzssTypedToken> literals(size,{LzssTypedTokenKind::literal,static_cast<std::uint8_t>(65+sequence),0,0});
        const Context next{partition,limits,sequence,committed};
        r=encode_position_blocked_huffman_64k_frame(literals,next,*et,private_payload,serialized,encoded);
        check(r.error==Error::none,"partition frame encode");
        const auto decoded=decode_position_blocked_huffman_64k_frame(std::span<const std::byte>(encoded).first(r.serialized_size),next,*dt,token_scratch,raw,
            std::span(partition_output).subspan(static_cast<std::size_t>(committed),size));
        check(decoded.error==Error::none && decoded.raw_size==size,"partition final short frame decode");committed+=size;
    }
    check(partition_output[0]==std::byte{65} && partition_output[4]==std::byte{66}
        && partition_output[8]==std::byte{67} && partition_output[9]==marker,"partition keeps all committed frames and guard");
    // A grammar-valid reference in frame1 must not see frame0's history.
    Stream reset{};reset.frame_size=3;reset.original_size=6;
    const Context first{reset,limits,0,0},second{reset,limits,1,3};
    const std::array<LzssTypedToken,3> first_tokens{{{LzssTypedTokenKind::literal,65,0,0},{LzssTypedTokenKind::literal,65,0,0},{LzssTypedTokenKind::literal,65,0,0}}};
    r=encode_position_blocked_huffman_64k_frame(first_tokens,first,*et,private_payload,serialized,encoded);
    std::array<std::byte,7> combined{};combined.fill(marker);
    check(r.error==Error::none && decode_position_blocked_huffman_64k_frame(std::span<const std::byte>(encoded).first(r.serialized_size),first,*dt,token_scratch,raw,std::span(combined).first(3)).error==Error::none,"first valid reset frame committed");
    const std::array<PositionBlockedHuffmanDecision,4> requests{{{0,1},{20,8},{-1,0},{31,0}}};
    auto builder=std::make_unique<PositionBlockedHuffman64kModelBuilder>();PositionBlockedHuffman64kDescriptor model{};
    for (const auto request:requests) check(builder->add(request.context,request.symbol)==PositionBlockedHuffmanError::none,"reset-invalid model count");
    check(builder->finish(limits,model)==PositionBlockedHuffmanError::none,"reset-invalid model plan");
    std::size_t ds{},ps{};
    check(serialize_position_blocked_huffman_64k_descriptor(model,limits,std::span(serialized).subspan(64),ds)==PositionBlockedHuffmanError::none,"reset-invalid descriptor");
    check(encode_position_blocked_huffman_64k(model,requests,limits,*et,std::span(serialized).subspan(64+ds),ps)==PositionBlockedHuffmanError::none,"reset-invalid payload");
    const PositionBlockedHuffman64kFrameHeader reset_header{0,1,3,1,4,4,static_cast<std::uint32_t>(ps),static_cast<std::uint32_t>(ds),0,0};
    check(serialize_position_blocked_huffman_64k_frame_header(reset_header,second,std::span(serialized).first(64))==F::none,"reset-invalid header");
    r=decode_position_blocked_huffman_64k_frame(std::span<const std::byte>(serialized).first(64+ds+ps),second,*dt,token_scratch,raw,std::span(combined).subspan(3,3));
    check(r.error==Error::token_error && !r.serialized_size && !r.raw_size && r.token_result.token_error==LzssTypedTokenError::invalid_distance
        && combined[0]==std::byte{65} && combined[1]==std::byte{65} && combined[2]==std::byte{65}
        && marked(std::span(combined).subspan(3)),"failed reset frame publishes nothing and preserves prior frame");
    std::cout<<"native envelope, prefix preflight, private raw restoration and failure publication checked\n";
}
}
int main(int argc,char** argv) { if (argc==2) return fixture(argv[1]);units();return failures ? 1 : 0; }
