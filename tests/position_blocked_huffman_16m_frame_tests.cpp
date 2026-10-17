#include "frame/lzss_position_blocked_huffman_16m_stream_decoder.hpp"
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
using Error=PositionBlockedHuffman16mFrameError;
using F=PositionBlockedHuffman16mFormatError;
using Stream=PositionBlockedHuffman16mStreamHeader;
using Context=PositionBlockedHuffman16mFrameContext;
using Layout=PositionBlockedHuffman16mFrameLayout;
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
void check_stream(std::span<const std::byte> input,std::span<const std::byte> expected,
                  bool reject,std::size_t raw_capacity,std::size_t token_capacity) {
    marc::core::DecoderLimits limits{};limits.max_block_size=150994944;limits.max_internal_buffered_bytes=150994944;limits.expansion_slack=16777216;
    std::vector<std::byte> serialized(std::max<std::size_t>(64,input.size()-112));
    std::vector<std::byte> raw(std::max<std::size_t>(1,raw_capacity)),committed;
    std::vector<LzssTypedToken> tokens(std::max<std::size_t>(1,token_capacity));
    auto tables=std::make_unique<std::array<HuffmanDecodeTable,61>>();
    limits.max_internal_buffered_bytes=std::max<std::uint64_t>(limits.max_block_size,
        sizeof(PositionBlockedHuffman16mStreamDecoder)+serialized.size()+tokens.size()*sizeof(LzssTypedToken)
        +raw.size()+sizeof(*tables)+position_blocked_huffman_16m_stream_decode_working_bytes);
    PositionBlockedHuffman16mStreamDecoder decoder(limits,serialized,tokens,raw,*tables);
    std::size_t consumed=0;
    for (std::size_t calls=0;calls<4*(input.size()+raw_capacity+1);++calls) {
        const auto part=input.subspan(consumed,std::min<std::size_t>(13,input.size()-consumed));
        std::array<std::byte,31> output{};output.fill(marker);
        const auto flags=consumed+part.size()==input.size() ? marc::core::flag_value(marc::core::ProcessFlags::end_input) : 0;
        const auto r=decoder.process(part,output,flags);
        check(r.input_consumed<=part.size() && r.output_produced<=output.size(),"stream fixture process extents");
        check(marked(std::span(output).subspan(r.output_produced)),"stream fixture unproduced tail");
        check(r.status!=marc::core::StreamStatus::progress || r.input_consumed || r.output_produced,"stream fixture bounded progress");
        consumed+=r.input_consumed;committed.insert(committed.end(),output.begin(),output.begin()+r.output_produced);
        if (r.status==marc::core::StreamStatus::error || r.status==marc::core::StreamStatus::end_of_stream) {
            if (reject) check(r.status==marc::core::StreamStatus::error && committed.empty(),"independent failed stream frame published nothing");
            else check(r.status==marc::core::StreamStatus::end_of_stream && consumed==input.size()
                && std::equal(committed.begin(),committed.end(),expected.begin(),expected.end()),"independent stream raw restoration");
            return;
        }
    }
    check(false,"stream fixture bounded completion");
}
int fixture(const char* path) {
    std::ifstream file(path,std::ios::binary);std::vector<char> chars((std::istreambuf_iterator<char>(file)),{});
    if (!file || chars.size()<124 || chars.size()>UINT64_C(124)+283118159+16777216) return 2;
    std::vector<std::byte> data(chars.size());std::transform(chars.begin(),chars.end(),data.begin(),[](char c){return static_cast<std::byte>(c);});
    const auto bytes=std::span<const std::byte>(data);
    const auto fs=load<std::uint32_t>(bytes,0),rs=load<std::uint32_t>(bytes,4),mode=load<std::uint32_t>(bytes,8);
    if (rs>16777216 || mode>1 || UINT64_C(124)+fs+rs!=data.size()) return 3;
    marc::core::DecoderLimits limits{};limits.max_block_size=150994944;limits.max_internal_buffered_bytes=150994944;
    limits.max_compressed_payload_size=283115520;limits.expansion_slack=16777216;
    Stream stream{};std::size_t used=777;
    check(parse_position_blocked_huffman_16m_stream(bytes.subspan(12,112),limits,stream,used)==F::none && used==112,"independent stream parsed");
    std::array<std::byte,112> header{};
    check(serialize_position_blocked_huffman_16m_stream(stream,limits,header)==F::none
        && std::equal(header.begin(),header.end(),bytes.begin()+12),"independent stream bytes");
    if (!fs) { check(rs==0 && stream.original_size==0 && mode==0,"empty stream header only");check_stream(bytes.subspan(12,112),{},false,1,1);return failures ? 1 : 0; }
    const Context context{stream,limits,0,0};
    const auto frame=bytes.subspan(124,fs),expected=bytes.subspan(124+fs,rs);
    Layout layout{};
    check(preflight_position_blocked_huffman_16m_frame(frame,context,layout)==F::none,"independent frame structurally valid");
    if (failures) return 4;
    check_stream(bytes.subspan(12,112+fs),expected,mode!=0,layout.header.raw_size,layout.header.token_count);
    auto dt=std::make_unique<std::array<HuffmanDecodeTable,61>>();
    std::vector<LzssTypedToken> tokens(layout.header.token_count+1);
    std::vector<std::byte> private_raw(layout.header.raw_size+1,marker),output(layout.header.raw_size+1,marker);
    const auto result=decode_position_blocked_huffman_16m_frame(frame,context,*dt,tokens,private_raw,output);
    if (mode) {
        check(result.error!=Error::none && result.raw_size==0 && result.serialized_size==0 && marked(output),"failed independent frame never publishes");
    } else {
        check(result.error==Error::none && result.raw_size==rs && result.serialized_size==fs,"independent frame decoded counts");
        check(std::equal(expected.begin(),expected.end(),output.begin()) && output.back()==marker && private_raw.back()==marker,"independent raw restoration and guards");
        const auto restored=decode_position_blocked_huffman_16m_frame_scratch(frame,context,*dt,tokens,private_raw);
        check(restored.error==Error::none && restored.raw_size==rs && std::equal(expected.begin(),expected.end(),private_raw.begin()),"private raw restoration");
        auto et=std::make_unique<std::array<CanonicalHuffmanTable,61>>();
        PositionBlockedHuffman16mDescriptor model{};
        const auto planned=marc::context::internal::plan_position_blocked_huffman_16m_tokens(
            std::span<const LzssTypedToken>(tokens).first(layout.header.token_count),
            {layout.header.token_count,layout.header.raw_size,0},limits,model);
        check(planned.error==marc::context::internal::PositionBlockedHuffman16mTokenError::none,"re-encode buffer plan");
        if (planned.error!=marc::context::internal::PositionBlockedHuffman16mTokenError::none) return 5;
        const auto extent=64+planned.descriptor_size+planned.payload_size;
        std::vector<std::byte> payload(planned.payload_size),serialized(extent),encoded(extent+1,marker);
        const auto r=encode_position_blocked_huffman_16m_frame(std::span<const LzssTypedToken>(tokens).first(layout.header.token_count),context,*et,payload,serialized,encoded);
        check(r.error==Error::none && r.raw_size==rs && encoded[r.serialized_size]==marker,"independent token frame re-encode");
        if (r.error==Error::none) save(std::string(path)+".encoded",std::span<const std::byte>(encoded).first(r.serialized_size));
    }
    check(std::equal(data.begin(),data.end(),chars.begin(),[](auto a,char b){return a==static_cast<std::byte>(b);}),"independent frame input retained");
    return failures ? 1 : 0;
}
void units() {
    marc::core::DecoderLimits limits{};limits.max_block_size=150994944;limits.max_internal_buffered_bytes=150994944;limits.expansion_slack=16777216;
    Stream stream{};stream.frame_size=267;stream.original_size=267;
    const Context context{stream,limits,0,0};
    std::array<std::byte,113> stream_wire{};stream_wire.fill(marker);
    check(serialize_position_blocked_huffman_16m_stream(stream,limits,stream_wire)==F::none && stream_wire.back()==marker,"stream guard");
    const auto header=std::span<const std::byte>(stream_wire).first(112);
    Stream sentinel{};sentinel.original_size=777;std::size_t consumed=777;
    for (std::size_t cut=0;cut<112;++cut) {
        check(parse_position_blocked_huffman_16m_stream(header.first(cut),limits,sentinel,consumed)==F::truncated
            && sentinel.original_size==777 && consumed==777,"all stream cuts transactional");
    }
    for (std::size_t i=0;i<112;++i) {
        if ((i>=20 && i<24) || (i>=40 && i<48)) continue;
        sentinel.original_size=777;consumed=777;
        auto changed=stream_wire;changed[i]^=std::byte{0x80};
        check(parse_position_blocked_huffman_16m_stream(changed,limits,sentinel,consumed)!=F::none
            && sentinel.original_size==777 && consumed==777,"all fixed stream fields reject transactionally");
    }
    check(parse_position_blocked_huffman_16m_stream(header,limits,sentinel,consumed)==F::none
        && consumed==112 && sentinel.original_size==267,"valid stream identity");
    const auto saved_stream=stream;
    check(serialize_position_blocked_huffman_16m_stream(stream,limits,std::as_writable_bytes(std::span(&stream,1)))==F::overlapping_buffers
        && stream.original_size==saved_stream.original_size,"stream serialization alias");
    check(parse_position_blocked_huffman_16m_stream(std::as_bytes(std::span(&sentinel,1)),limits,sentinel,consumed)==F::overlapping_buffers,"stream parser alias");
    for (auto offset : {52U,53U}) {
        auto changed=stream_wire;changed[offset]=std::byte{1};sentinel.original_size=777;consumed=777;
        check(parse_position_blocked_huffman_16m_stream(changed,limits,sentinel,consumed)==F::nonzero_reserved
            && sentinel.original_size==777 && consumed==777,"reserved bytes52/53 stay reserved at every window size");
    }
    for (auto size : {0U,16777217U}) {
        auto invalid_stream=stream;invalid_stream.frame_size=size;
        auto structural_limits=limits;structural_limits.max_frame_size=16777217;
        check(validate_position_blocked_huffman_16m_stream(invalid_stream,structural_limits)==F::invalid_parameters,"frame size endpoints");
    }
    auto short_codes=limits;short_codes.max_huffman_code_length=14;
    check(validate_position_blocked_huffman_16m_stream(stream,short_codes)==F::limit_exceeded,"stream Huffman length limit");
    const std::vector<LzssTypedToken> tokens{{LzssTypedTokenKind::literal,65,0,0},{LzssTypedTokenKind::match,0,1,3},
        {LzssTypedTokenKind::literal,66,0,0},{LzssTypedTokenKind::match,0,2,258},{LzssTypedTokenKind::match,0,263,4}};
    auto et=std::make_unique<std::array<CanonicalHuffmanTable,61>>();auto dt=std::make_unique<std::array<HuffmanDecodeTable,61>>();
    std::vector<std::byte> private_payload(1024,marker),serialized(4096,marker),encoded(4097,marker);
    auto r=encode_position_blocked_huffman_16m_frame(tokens,context,*et,private_payload,serialized,encoded);
    check(r.error==Error::none && r.raw_size==267 && encoded[r.serialized_size]==marker,"mixed native frame encode");
    if (r.error!=Error::none) return;
    const auto frame=std::span<const std::byte>(encoded).first(r.serialized_size);
    Layout layout{};check(preflight_position_blocked_huffman_16m_frame(frame,context,layout)==F::none,"native frame preflight");
    Layout retained{};retained.header.sequence=777;retained.requirements.serialized_size=777;
    for (std::size_t cut=0;cut<frame.size();++cut) {
        auto out=retained;
        check(preflight_position_blocked_huffman_16m_frame(frame.first(cut),context,out)!=F::none && out==retained,"every frame cut transactional");
    }
    for (std::size_t i=0;i<64;++i) {
        auto changed=encoded;changed[i]^=std::byte{0x80};auto out=retained;
        check(preflight_position_blocked_huffman_16m_frame(changed,context,out)!=F::none && out==retained,"all changed frame fields rejected");
    }
    check(preflight_position_blocked_huffman_16m_frame_prefix(frame.first(layout.requirements.prefix_size),context,retained)==F::none,"prefix succeeds before payload arrives");
    const auto saved_layout=layout;
    check(preflight_position_blocked_huffman_16m_frame(std::as_bytes(std::span(&layout,1)),context,layout)==F::overlapping_buffers && layout==saved_layout,"layout/input alias");
    std::vector<LzssTypedToken> token_scratch(6);
    std::vector<std::byte> raw(268,marker),output(269,marker);
    r=decode_position_blocked_huffman_16m_frame(frame,context,*dt,token_scratch,raw,output);
    check(r.error==Error::none && r.serialized_size==frame.size() && r.raw_size==267
        && output[0]==std::byte{65} && output[4]==std::byte{66} && output[267]==marker && raw.back()==marker,"native raw reconstruction and tails");
    auto reject=[&](std::span<const std::byte> input,const Context& c) {
        std::fill(output.begin(),output.end(),marker);
        const auto result=decode_position_blocked_huffman_16m_frame(input,c,*dt,token_scratch,raw,output);
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
    Stream later=stream;later.original_size=16777217;
    Context bad_sequence{later,limits,1,0};(void)reject(frame,bad_sequence);
    const Context bad_committed{later,limits,0,16777216};(void)reject(frame,bad_committed);
    check(decode_position_blocked_huffman_16m_frame(frame,context,*dt,token_scratch,raw,raw).error==Error::overlapping_buffers,"raw scratch/output alias");
    const auto before_input=encoded;
    check(decode_position_blocked_huffman_16m_frame(frame,context,*dt,token_scratch,raw,encoded).error==Error::overlapping_buffers
        && encoded==before_input,"raw output/source alias");
    check(decode_position_blocked_huffman_16m_frame(frame,context,*dt,token_scratch,raw,std::span(output).first(266)).error==Error::output_too_small,"short ordinary raw");
    check(decode_position_blocked_huffman_16m_frame(frame,context,*dt,token_scratch,std::span(raw).first(266),output).error==Error::output_too_small,"short private raw");
    auto quota=limits;quota.max_block_size=267;
    quota.max_internal_buffered_bytes=position_blocked_huffman_16m_decode_frame_workspace+frame.size()+sizeof(*dt)
        +token_scratch.size()*sizeof(LzssTypedToken)+raw.size()+output.size()+sizeof(Context)+sizeof(Stream)+sizeof(quota);
    const Context exact{stream,quota,0,0};
    check(decode_position_blocked_huffman_16m_frame(frame,exact,*dt,token_scratch,raw,output).error==Error::none,"exact frame decode quota");
    --quota.max_internal_buffered_bytes;r=reject(frame,exact);check(r.error==Error::limit_exceeded,"one short frame decode quota");
    quota=limits;quota.max_block_size=267;
    quota.max_internal_buffered_bytes=position_blocked_huffman_16m_encode_frame_workspace+tokens.size()*sizeof(LzssTypedToken)+sizeof(*et)
        +private_payload.size()+serialized.size()+encoded.size()+sizeof(Context)+sizeof(Stream)+sizeof(quota);
    const Context exact_encode{stream,quota,0,0};
    check(encode_position_blocked_huffman_16m_frame(tokens,exact_encode,*et,private_payload,serialized,encoded).error==Error::none && encoded==before_input,"exact frame encode quota and determinism");
    --quota.max_internal_buffered_bytes;
    r=encode_position_blocked_huffman_16m_frame(tokens,exact_encode,*et,private_payload,serialized,encoded);
    check(r.error==Error::limit_exceeded && r.serialized_size==0 && r.raw_size==0 && encoded==before_input,"one short frame encode quota");
    auto invalid=tokens;invalid[1].distance=2;
    r=encode_position_blocked_huffman_16m_frame(invalid,context,*et,private_payload,serialized,encoded);
    check(r.error==Error::token_error && !r.serialized_size && !r.raw_size && encoded==before_input,"invalid history frame encoder retention");
    Stream empty{};const Context empty_context{empty,limits,0,0};
    check(encode_position_blocked_huffman_16m_frame({},empty_context,*et,private_payload,serialized,encoded).error==Error::format_error && encoded==before_input,"empty native frame forbidden");
    Stream partition{};partition.frame_size=4;partition.original_size=9;
    std::array<std::byte,10> partition_output{};partition_output.fill(marker);
    std::uint64_t committed=0;
    for (std::uint64_t sequence=0;sequence<3;++sequence) {
        const auto size=static_cast<std::uint32_t>(std::min(UINT64_C(4),UINT64_C(9)-committed));
        std::vector<LzssTypedToken> literals(size,{LzssTypedTokenKind::literal,static_cast<std::uint8_t>(65+sequence),0,0});
        const Context next{partition,limits,sequence,committed};
        r=encode_position_blocked_huffman_16m_frame(literals,next,*et,private_payload,serialized,encoded);
        check(r.error==Error::none,"partition frame encode");
        const auto decoded=decode_position_blocked_huffman_16m_frame(std::span<const std::byte>(encoded).first(r.serialized_size),next,*dt,token_scratch,raw,
            std::span(partition_output).subspan(static_cast<std::size_t>(committed),size));
        check(decoded.error==Error::none && decoded.raw_size==size,"partition final short frame decode");committed+=size;
    }
    check(partition_output[0]==std::byte{65} && partition_output[4]==std::byte{66}
        && partition_output[8]==std::byte{67} && partition_output[9]==marker,"partition keeps all committed frames and guard");
    // A grammar-valid reference in frame1 must not see frame0's history.
    Stream reset{};reset.frame_size=3;reset.original_size=6;
    const Context first{reset,limits,0,0},second{reset,limits,1,3};
    const std::array<LzssTypedToken,3> first_tokens{{{LzssTypedTokenKind::literal,65,0,0},{LzssTypedTokenKind::literal,65,0,0},{LzssTypedTokenKind::literal,65,0,0}}};
    r=encode_position_blocked_huffman_16m_frame(first_tokens,first,*et,private_payload,serialized,encoded);
    std::array<std::byte,7> combined{};combined.fill(marker);
    check(r.error==Error::none && decode_position_blocked_huffman_16m_frame(std::span<const std::byte>(encoded).first(r.serialized_size),first,*dt,token_scratch,raw,std::span(combined).first(3)).error==Error::none,"first valid reset frame committed");
    const std::array<PositionBlockedHuffmanDecision,4> requests{{{0,1},{20,8},{-1,0},{31,0}}};
    auto builder=std::make_unique<PositionBlockedHuffman16mModelBuilder>();PositionBlockedHuffman16mDescriptor model{};
    for (const auto request:requests) check(builder->add(request.context,request.symbol)==PositionBlockedHuffmanError::none,"reset-invalid model count");
    check(builder->finish(limits,model)==PositionBlockedHuffmanError::none,"reset-invalid model plan");
    std::size_t ds{},ps{};
    check(serialize_position_blocked_huffman_16m_descriptor(model,limits,std::span(serialized).subspan(64),ds)==PositionBlockedHuffmanError::none,"reset-invalid descriptor");
    check(encode_position_blocked_huffman_16m(model,requests,limits,*et,std::span(serialized).subspan(64+ds),ps)==PositionBlockedHuffmanError::none,"reset-invalid payload");
    const PositionBlockedHuffman16mFrameHeader reset_header{0,1,3,1,4,4,static_cast<std::uint32_t>(ps),static_cast<std::uint32_t>(ds),0,0};
    check(serialize_position_blocked_huffman_16m_frame_header(reset_header,second,std::span(serialized).first(64))==F::none,"reset-invalid header");
    r=decode_position_blocked_huffman_16m_frame(std::span<const std::byte>(serialized).first(64+ds+ps),second,*dt,token_scratch,raw,std::span(combined).subspan(3,3));
    check(r.error==Error::token_error && !r.serialized_size && !r.raw_size && r.token_result.token_error==LzssTypedTokenError::invalid_distance
        && combined[0]==std::byte{65} && combined[1]==std::byte{65} && combined[2]==std::byte{65}
        && marked(std::span(combined).subspan(3)),"failed reset frame publishes nothing and preserves prior frame");
    {
        Stream maximum_stream{};maximum_stream.original_size=16777216;
        auto maximum_limits=limits;maximum_limits.max_compressed_payload_size=283115520;
        // Exact private footprint for the maximum metadata envelope only.
        // This does not allocate the maximum payload or choose a public grant.
        maximum_limits.max_internal_buffered_bytes=UINT64_C(283118159)
            +UINT64_C(16777216)*sizeof(LzssTypedToken)+16777216
            +position_blocked_huffman_16m_decode_frame_workspace+61*sizeof(HuffmanDecodeTable);
        const Context maximum_context{maximum_stream,maximum_limits,0,0};
        PositionBlockedHuffman16mFrameHeader maximum{0,0,16777216,16777216,150994944,150994944,283115520,2575,0,0};
        PositionBlockedHuffman16mFrameRequirements required{};
        check(validate_position_blocked_huffman_16m_frame_header(maximum,maximum_context,required)==F::none
            && required.serialized_size==283118159,"maximum frame metadata bounds without payload allocation");
        const auto retained=required;
        auto bad=maximum;++bad.decision_count;++bad.event_count;
        check(validate_position_blocked_huffman_16m_frame_header(bad,maximum_context,required)==F::invalid_counts
            && required==retained,"one excessive nine-times-raw decision count retains requirements");
        bad=maximum;++bad.payload_size;
        check(validate_position_blocked_huffman_16m_frame_header(bad,maximum_context,required)==F::invalid_counts
            && required==retained,"one excessive payload byte refused before allocation");
        bad=maximum;++bad.descriptor_size;
        check(validate_position_blocked_huffman_16m_frame_header(bad,maximum_context,required)==F::invalid_counts
            && required==retained,"one excessive descriptor byte refused");
        bad=maximum;bad.token_count=1;bad.event_count=bad.decision_count=35;bad.payload_size=0;
        check(validate_position_blocked_huffman_16m_frame_header(bad,maximum_context,required)==F::invalid_counts
            && required==retained,"thirty-four-times-token bound enforced");
        --maximum_limits.max_compressed_payload_size;
        check(validate_position_blocked_huffman_16m_frame_header(maximum,maximum_context,required)==F::limit_exceeded
            && required==retained,"configured payload ceiling one short");
    }
    {
        constexpr std::uint32_t full_size=16777216;
        auto full_limits=limits;full_limits.max_block_size=150994944;
        Stream full_stream{};full_stream.original_size=full_size;
        const Context full_context{full_stream,full_limits,0,0};
        std::vector<LzssTypedToken> full_tokens(full_size,{LzssTypedTokenKind::literal,65,0,0});
        std::array<std::byte,99> private_frame{};
        std::array<std::byte,100> full_frame{};full_frame.fill(marker);
        const std::uint64_t full_encode_charge=
            position_blocked_huffman_16m_encode_frame_workspace+full_tokens.size()*sizeof(LzssTypedToken)
            +sizeof(*et)+private_frame.size()+full_frame.size()+sizeof(full_context)+sizeof(full_stream)+sizeof(full_limits);
        // Header admission also promises the minimum private decoder footprint.
        const std::uint64_t full_decode_requirement=99+full_tokens.size()*sizeof(LzssTypedToken)+full_size
            +position_blocked_huffman_16m_decode_frame_workspace+sizeof(*dt);
        full_limits.max_internal_buffered_bytes=std::max({full_limits.max_block_size,
            full_encode_charge,full_decode_requirement});
        const auto full=encode_position_blocked_huffman_16m_frame(full_tokens,full_context,*et,{},private_frame,full_frame);
        check(full.error==Error::none && full.serialized_size==99 && full.raw_size==full_size && full_frame.back()==marker,"full literal frame zero-payload encoding");
        std::array<std::byte,35> hand{};
        (void)marc::core::store_le(hand,0,std::uint32_t{2*full_size});
        hand[9]=std::byte{15};hand[10]=std::byte{5};hand[11]=std::byte{3};
        (void)marc::core::store_le(hand,12,std::uint16_t{56});
        (void)marc::core::store_le(hand,16,std::uint32_t{4658});hand[34]=std::byte{65};
        check(std::equal(hand.begin(),hand.end(),full_frame.begin()+64),"full literal hand descriptor with thirty-two-million decisions");
        std::vector<LzssTypedToken> full_scratch(full_size);
        std::vector<std::byte> full_private(full_size+1,marker),full_output(full_size+1,marker);
        full_limits.max_internal_buffered_bytes=std::max<std::uint64_t>(full_limits.max_block_size,
            position_blocked_huffman_16m_decode_frame_workspace+sizeof(*dt)+full_scratch.size()*sizeof(LzssTypedToken)
            +full_private.size()+full_output.size()+99+sizeof(full_context)+sizeof(full_stream)+sizeof(full_limits));
        const auto decoded=decode_position_blocked_huffman_16m_frame(std::span<const std::byte>(full_frame).first(99),full_context,*dt,full_scratch,full_private,full_output);
        check(decoded.error==Error::none && decoded.raw_size==full_size && full_output.back()==marker && full_private.back()==marker
            && std::all_of(full_output.begin(),full_output.end()-1,[](auto b){return b==std::byte{65};}),"full literal raw restoration and tails");
        std::vector<std::byte> full_wire(112);
        check(serialize_position_blocked_huffman_16m_stream(full_stream,full_limits,full_wire)==F::none,"full literal stream header");
        full_wire.insert(full_wire.end(),full_frame.begin(),full_frame.begin()+99);
        check_stream(full_wire,std::span<const std::byte>(full_output).first(full_size),false,full_size,full_size);
    }
    std::cout<<"native envelope, prefix preflight, private raw restoration and failure publication checked\n";
}
}
int main(int argc,char** argv) { if (argc==2) return fixture(argv[1]);units();return failures ? 1 : 0; }
