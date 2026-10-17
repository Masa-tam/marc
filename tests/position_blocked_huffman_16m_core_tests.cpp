#include "entropy/position_blocked_huffman_16m.hpp"
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
using Descriptor=PositionBlockedHuffman16mDescriptor;
int failures{};
void check(bool value,const char* label) { if (!value) { ++failures;std::cerr<<label<<'\n'; } }
std::vector<std::byte> wire(const Descriptor& d) {
    std::vector<std::byte> bytes(2575);std::size_t n=999;
    check(serialize_position_blocked_huffman_16m_descriptor(d,{},bytes,n)==Error::none,"serialize fixture");
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
    if (ds>2576 || ps>150000 || dc>100000 || mode>2 || UINT64_C(16)+ds+ps+UINT64_C(4)*dc!=data.size()) return 3;
    const auto description=data.subspan(16,ds),payload=data.subspan(16+ds,ps);
    Descriptor sentinel{};sentinel.decision_count=777;sentinel.models[0].lengths.fill(7);
    auto d=sentinel;
    const auto parsed=parse_position_blocked_huffman_16m_descriptor(description,dc,ps,{},d);
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
    auto decode_tables=std::make_unique<std::array<HuffmanDecodeTable,61>>();
    PositionBlockedHuffman16mDecoder decoder;
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
    auto encode_tables=std::make_unique<std::array<CanonicalHuffmanTable,61>>();
    std::vector<std::byte> output(ps+2,std::byte{0xa5});std::size_t written=777;
    const auto encoded=encode_position_blocked_huffman_16m(d,decisions,{},*encode_tables,std::span(output).subspan(1,ps),written);
    if (mode==0) {
        check(encoded==Error::none && written==ps,"oracle encode");
        check(std::equal(payload.begin(),payload.end(),output.begin()+1),"oracle payload bytes");
    } else if (encoded!=Error::none) {
        check(written==777 && std::all_of(output.begin(),output.end(),[](auto v){return v==std::byte{0xa5};}),"failed encode retained ordinary destinations");
    }
    check(output.front()==std::byte{0xa5} && output.back()==std::byte{0xa5},"output guards");
    if (mode==0) {
        auto planner=std::make_unique<PositionBlockedHuffman16mModelBuilder>();
        for (const auto request:decisions) check(planner->add(request.context,request.symbol)==Error::none,"fixture planner add");
        Descriptor plan{},again{};
        check(planner->finish({},plan)==Error::none && planner->finish({},again)==Error::none && plan==again,"fixture plan determinism");
        const auto description=wire(plan);
        std::vector<std::byte> planned_payload(plan.payload_size);written=777;
        check(encode_position_blocked_huffman_16m(plan,decisions,{},*encode_tables,planned_payload,written)==Error::none && written==plan.payload_size,"fixture planned encode");
        auto planned_writer=std::make_unique<PositionBlockedHuffman16mPrivateWriter>();
        std::vector<std::byte> planned_private(plan.payload_size+2,std::byte{0xa5});
        check(planned_writer->begin(plan,{},*encode_tables,std::span(planned_private).subspan(1,plan.payload_size))==Error::none,"planned private begin");
        for (const auto request:decisions) check(planned_writer->add(request.context,request.symbol)==Error::none,"planned private add");
        written=777;
        check(planned_writer->finish(written)==Error::none && written==plan.payload_size
            && std::equal(planned_payload.begin(),planned_payload.end(),planned_private.begin()+1)
            && planned_private.front()==std::byte{0xa5} && planned_private.back()==std::byte{0xa5},"planned private/reference equality and guards");
        std::array<std::byte,12> header{};
        check(marc::core::store_le(header,0,static_cast<std::uint32_t>(description.size()))
            && marc::core::store_le(header,4,plan.payload_size)
            && marc::core::store_le(header,8,plan.decision_count),"fixture plan sizes");
        std::ofstream file(std::string(path)+".planned",std::ios::binary);
        for (const auto bytes : {std::span<const std::byte>(header),std::span<const std::byte>(description),std::span<const std::byte>(planned_payload)})
            for (const auto byte:bytes) file.put(static_cast<char>(std::to_integer<unsigned char>(byte)));
        file.close();check(file.good(),"fixture planned artifact persisted");
    }
    // Independently generated codebooks/payloads also exercise the incremental
    // writer. Its private destination may change on failure; only finish's
    // ordinary count and the surrounding guards must remain transactional.
    PositionBlockedHuffman16mPrivateWriter writer;
    std::vector<std::byte> private_output(ps+3,std::byte{0xa5});written=777;
    auto incremental=writer.begin(d,{},*encode_tables,std::span(private_output).subspan(1,ps+1));
    for (const auto request:decisions) {
        if (incremental!=Error::none) break;
        incremental=writer.add(request.context,request.symbol);
    }
    if (incremental==Error::none) incremental=writer.finish(written);
    if (mode==0) {
        check(incremental==Error::none && written==ps,"oracle incremental finish");
        check(std::equal(payload.begin(),payload.end(),private_output.begin()+1),"oracle incremental payload");
        written=888;check(writer.finish(written)==Error::none && written==ps,"incremental repeated finish");
        check(writer.add(0,0)==Error::already_finished,"incremental add after finish");
    } else if (incremental!=Error::none) {
        check(written==777 && writer.finish(written)==incremental && written==777,"incremental failure retained count");
    }
    check(private_output.front()==std::byte{0xa5} && private_output[ps+1]==std::byte{0xa5}
        && private_output.back()==std::byte{0xa5},"incremental guards and unproduced tail");
    return failures ? 1 : 0;
}
void units() {
    Descriptor d{};d.decision_count=4;d.payload_size=1;d.final_valid_bits=2;d.field_mask=3;
    d.models[0].single_symbol=0;d.models[1].lengths[65]=d.models[1].lengths[66]=1;
    const auto expected=wire(d);
    const std::array<PositionBlockedHuffmanDecision,4> decisions{{{0,0},{3,65},{1,0},{8,66}}};
    auto encode_tables=std::make_unique<std::array<CanonicalHuffmanTable,61>>();
    auto decode_tables=std::make_unique<std::array<HuffmanDecodeTable,61>>();
    std::array<std::byte,3> output{};output.fill(std::byte{0xa5});std::size_t written=777;
    const auto unchanged=output;
    auto limits=marc::core::DecoderLimits{};
    limits.max_block_size=d.decision_count;
    const std::uint64_t format_charge=sizeof(d)+2575+expected.size()+d.payload_size;
    limits.max_internal_buffered_bytes=format_charge;
    std::size_t extent=777;
    check(validate_position_blocked_huffman_16m_descriptor(d,limits,extent)==Error::none && extent==expected.size(),"exact format budget");
    --limits.max_internal_buffered_bytes;extent=777;
    check(validate_position_blocked_huffman_16m_descriptor(d,limits,extent)==Error::limit_exceeded && extent==777,"one short format budget");
    Descriptor sentinel{};sentinel.decision_count=777;
    auto parsed=sentinel;
    check(parse_position_blocked_huffman_16m_descriptor(expected,4,1,limits,parsed)==Error::limit_exceeded && parsed==sentinel,"parse budget retention");
    for (std::size_t n=0;n<expected.size();++n) {
        parsed=sentinel;check(parse_position_blocked_huffman_16m_descriptor(std::span(expected).first(n),4,1,{},parsed)!=Error::none && parsed==sentinel,"every descriptor cut retained");
    }
    std::vector<std::byte> small(expected.size()-1,std::byte{0xa5});
    check(serialize_position_blocked_huffman_16m_descriptor(d,{},small,written)==Error::output_too_small && written==777
        && std::all_of(small.begin(),small.end(),[](auto x){return x==std::byte{0xa5};}),"short descriptor output retained");
    const auto saved=d;
    check(serialize_position_blocked_huffman_16m_descriptor(d,{},std::as_writable_bytes(std::span(&d,1)),written)==Error::overlapping_buffers && d==saved,"serializer overlap");
    check(parse_position_blocked_huffman_16m_descriptor(std::as_bytes(std::span(&d,1)).first(30),4,1,{},d)==Error::overlapping_buffers && d==saved,"parser overlap");
    const auto target=std::span(output).subspan(1,1);
    const std::uint64_t encode_charge=16384+sizeof(d)+sizeof(limits)+sizeof(decisions)+sizeof(*encode_tables)+target.size()+sizeof(written);
    limits={};limits.max_block_size=d.decision_count;limits.max_internal_buffered_bytes=encode_charge-1;
    check(encode_position_blocked_huffman_16m(d,decisions,limits,*encode_tables,target,written)==Error::limit_exceeded && written==777 && output==unchanged,"one short encode budget");
    ++limits.max_internal_buffered_bytes;
    check(encode_position_blocked_huffman_16m(d,decisions,limits,*encode_tables,target,written)==Error::none && written==1 && output[1]==std::byte{2},"exact encode budget and AB bits");
    output=unchanged;written=777;
    auto bad=decisions;bad.back().symbol=67;
    check(encode_position_blocked_huffman_16m(d,bad,{},*encode_tables,target,written)==Error::invalid_symbol && output==unchanged && written==777,"late invalid symbol encode retention");
    check(encode_position_blocked_huffman_16m(d,decisions,{},std::span(*encode_tables).first(60),target,written)==Error::scratch_too_small && output==unchanged,"short encode scratch");
    std::array<std::byte,1> payload{std::byte{2}};PositionBlockedHuffman16mDecoder decoder;
    limits={};limits.max_block_size=d.decision_count;limits.max_internal_buffered_bytes=16384+sizeof(decoder)+sizeof(d)+sizeof(limits)+payload.size()+sizeof(*decode_tables)-1;
    check(decoder.begin(d,payload,limits,*decode_tables)==Error::limit_exceeded,"one short decoder budget");
    ++limits.max_internal_buffered_bytes;check(decoder.begin(d,payload,limits,*decode_tables)==Error::none,"exact decoder budget");
    for (const auto request:decisions) { std::uint32_t value=999;check(decoder.read(request.context,value)==Error::none && value==request.symbol,"unit round trip"); }
    check(decoder.finish()==Error::none,"unit decoder finish");
    check(decoder.begin(d,payload,{},std::span(*decode_tables).first(60))==Error::scratch_too_small,"short decode scratch");
    check(decoder.begin(d,payload,{},*decode_tables)==Error::none,"restart");
    std::uint32_t value=999;check(decoder.read(56,value)==Error::invalid_context && value==999,"bad context retains symbol");
    check(decoder.read(0,value)==Error::invalid_context && value==999 && decoder.finish()==Error::invalid_context,"sticky context error");
    check(decoder.begin(d,payload,{},*decode_tables)==Error::none,"restart for unused bits");
    check(decoder.finish()==Error::count_mismatch,"early finish");
    limits={};limits.max_huffman_code_length=14;extent=777;
    check(validate_position_blocked_huffman_16m_descriptor(d,limits,extent)==Error::limit_exceeded && extent==777,"code length limit");
    limits={};limits.max_entropy_table_entries=2*511-1;
    check(validate_position_blocked_huffman_16m_descriptor(d,limits,extent)==Error::limit_exceeded,"model entry limit");
    Descriptor single{};single.decision_count=2;single.field_mask=3;single.models[0].single_symbol=0;single.models[1].single_symbol=65;
    PositionBlockedHuffman16mDecoder empty_bits;
    check(empty_bits.begin(single,{}, {},*decode_tables)==Error::none,"zero-bit singleton begin");
    value=999;check(empty_bits.read(0,value)==Error::none && value==0,"zero-bit kind");
    check(empty_bits.read(3,value)==Error::none && value==65 && empty_bits.finish()==Error::none,"zero-bit literal");
    // Long codes exercise the bounded tree fallback and a non-byte-aligned end.
    Descriptor longest{};longest.decision_count=1;longest.payload_size=2;longest.final_valid_bits=7;longest.field_mask=2;
    for (std::size_t s=0;s<14;++s) longest.models[1].lengths[s]=static_cast<std::uint8_t>(s+1);
    longest.models[1].lengths[14]=longest.models[1].lengths[15]=15;
    const std::array<PositionBlockedHuffmanDecision,1> last{{{3,15}}};
    std::array<std::byte,4> long_output{};long_output.fill(std::byte{0xa5});written=777;
    check(encode_position_blocked_huffman_16m(longest,last,{},*encode_tables,std::span(long_output).subspan(1,2),written)==Error::none
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
    check(encode_position_blocked_huffman_16m(d,decisions,{},*encode_tables,
        std::as_writable_bytes(std::span(&d,1)),written)==Error::overlapping_buffers && d==old_descriptor && written==777,"encoder source/output overlap");
    Descriptor oversized{};oversized.decision_count=UINT32_MAX;oversized.payload_size=UINT32_MAX;oversized.final_valid_bits=8;
    extent=777;check(validate_position_blocked_huffman_16m_descriptor(oversized,{},extent)==Error::limit_exceeded && extent==777,"large counts rejected before allocation");
    // All seven mask bytes are usable; context55 sets the final bit7.
    Descriptor high{};high.decision_count=1;high.field_mask=16;
    high.override_mask=UINT64_C(1)<<55;high.models[4].single_symbol=1;high.models[60].single_symbol=1;
    const auto high_wire=wire(high);
    check(high_wire.size()==35 && high_wire[30]==std::byte{128},"last valid override mask byte");
    parsed=sentinel;check(parse_position_blocked_huffman_16m_descriptor(high_wire,1,0,{},parsed)==Error::none && parsed==high,"last context parsed");
    check(decoder.begin(high,{}, {},*decode_tables)==Error::none,"last context begin");
    value=999;check(decoder.read(55,value)==Error::none && value==1 && decoder.finish()==Error::none,"last context decoded");
    auto out_of_range=high;out_of_range.override_mask|=UINT64_C(1)<<56;extent=777;
    check(validate_position_blocked_huffman_16m_descriptor(out_of_range,{},extent)==Error::invalid_metadata
        && extent==777,"out-of-range mask rejected transactionally");
    Descriptor endpoint{};endpoint.decision_count=1;endpoint.field_mask=8;endpoint.models[3].single_symbol=24;
    check(decoder.begin(endpoint,{}, {},*decode_tables)==Error::none,"distance class24 begin");
    value=999;check(decoder.read(31,value)==Error::none && value==24 && decoder.finish()==Error::none,"distance class24 decoded");
    endpoint.models[3].single_symbol=25;extent=777;
    check(validate_position_blocked_huffman_16m_descriptor(endpoint,{},extent)==Error::invalid_model && extent==777,"distance class25 rejected");
    // Maximum frame decision count is a finite zero-bit scalar block here,
    // not a claim of valid dictionary history or a full public frame test.
    Descriptor maximum{};maximum.decision_count=150994944;maximum.field_mask=16;maximum.models[4].single_symbol=0;
    limits={};limits.max_internal_buffered_bytes=150994944;limits.max_block_size=150994943;
    check(decoder.begin(maximum,{},limits,*decode_tables)==Error::limit_exceeded,"maximum decisions one-short limit");
    ++limits.max_block_size;
    check(decoder.begin(maximum,{},limits,*decode_tables)==Error::none,"maximum finite zero-bit begin");
    for (std::uint32_t i=0;i<150994944;++i) {
        value=999;
        if (decoder.read(55,value)!=Error::none || value!=0) { check(false,"maximum finite read");break; }
    }
    check(decoder.finish()==Error::none,"maximum finite zero-bit finish");
    auto planner=std::make_unique<PositionBlockedHuffman16mModelBuilder>();
    Descriptor plan{},again{};
    check(planner->finish({},plan)==Error::none && plan.decision_count==0 && plan.field_mask==0 && plan.payload_size==0,"empty planner");
    for (int i=0;i<5;++i) check(planner->add(3,65)==Error::none,"weighted A");
    for (int i=0;i<2;++i) check(planner->add(3,66)==Error::none,"weighted B");
    check(planner->add(3,67)==Error::none && planner->finish({},plan)==Error::none,"weighted plan");
    check(plan.models[1].lengths[65]==1 && plan.models[1].lengths[66]==2 && plan.models[1].lengths[67]==2
        && plan.override_mask==0 && plan.field_mask==2 && plan.payload_size==2 && plan.final_valid_bits==3,"hand weighted code lengths");
    const std::array<PositionBlockedHuffmanDecision,8> weighted{{{3,65},{3,65},{3,65},{3,65},{3,65},{3,66},{3,66},{3,67}}};
    std::array<std::byte,2> weighted_bytes{};written=777;
    check(encode_position_blocked_huffman_16m(plan,weighted,{},*encode_tables,weighted_bytes,written)==Error::none
        && weighted_bytes[0]==std::byte{0xa0} && weighted_bytes[1]==std::byte{6},"hand weighted a006 payload");
    check(planner->finish({},again)==Error::none && plan==again,"repeat planner finish determinism");
    limits={};limits.max_block_size=8;
    limits.max_internal_buffered_bytes=sizeof(*planner)+2*sizeof(Descriptor)+131072+2575;
    check(planner->finish(limits,again)==Error::none && plan==again,"exact planner budget");
    const auto saved_plan=again;--limits.max_internal_buffered_bytes;
    check(planner->finish(limits,again)==Error::limit_exceeded && again==saved_plan,"one short planner retains destination");
    for (unsigned count : {16u,17u}) {
        auto threshold=std::make_unique<PositionBlockedHuffman16mModelBuilder>();
        std::vector<PositionBlockedHuffmanDecision> requests;
        for (unsigned i=0;i<count;++i) { check(threshold->add(0,0)==Error::none,"threshold zero");requests.push_back({0,0}); }
        check(threshold->add(1,1)==Error::none,"threshold one");requests.push_back({1,1});
        check(threshold->finish({},plan)==Error::none,"threshold finish");
        check(plan.override_mask==(count==17 ? 1 : 0),"strict override bit cost tie");
        std::vector<std::byte> encoded(plan.payload_size);written=777;
        check(encode_position_blocked_huffman_16m(plan,requests,{},*encode_tables,encoded,written)==Error::none,"threshold encode");
        check(count==17 ? encoded.size()==1 && encoded[0]==std::byte{1}
            : encoded.size()==3 && encoded[0]==std::byte{0} && encoded[1]==std::byte{0} && encoded[2]==std::byte{1},"threshold hand payload");
    }
    auto bounded=std::make_unique<PositionBlockedHuffman16mModelBuilder>(1);
    check(bounded->add(-1,1)==Error::none && bounded->add(-1,0)==Error::limit_exceeded,"planner decision admission");
    again=saved_plan;check(bounded->finish({},again)==Error::limit_exceeded && again==saved_plan,"planner sticky limit retains output");
    auto invalid=std::make_unique<PositionBlockedHuffman16mModelBuilder>();
    check(invalid->add(3,256)==Error::invalid_symbol && invalid->add(3,65)==Error::invalid_symbol,"planner sticky invalid symbol");
    check(invalid->finish({},again)==Error::invalid_symbol && again==saved_plan,"invalid planner retains output");
    auto dense=std::make_unique<PositionBlockedHuffman16mModelBuilder>();
    std::vector<PositionBlockedHuffmanDecision> byte_requests;
    for (unsigned i=0;i<256;++i) { check(dense->add(3,i)==Error::none,"dense planner add");byte_requests.push_back({3,i}); }
    check(dense->finish({},plan)==Error::none && plan.override_mask==0
        && std::all_of(plan.models[1].lengths.begin(),plan.models[1].lengths.end(),[](auto n){return n==8;}),"equal-weight dense eight-bit lengths");
    std::vector<std::byte> encoded(plan.payload_size);written=777;
    check(encode_position_blocked_huffman_16m(plan,byte_requests,{},*encode_tables,encoded,written)==Error::none,"dense planned encode");
    check(decoder.begin(plan,encoded,{},*decode_tables)==Error::none,"dense planned decode begin");
    for (unsigned i=0;i<256;++i) check(decoder.read(3,value)==Error::none && value==i,"dense planned byte");
    check(decoder.finish()==Error::none,"dense planned finish");
    auto skew=std::make_unique<PositionBlockedHuffman16mModelBuilder>();
    std::vector<PositionBlockedHuffmanDecision> skew_requests;
    unsigned first=1,second=1;
    for (unsigned s=0;s<17;++s) {
        const auto count=s<2 ? 1u : first+second;
        if (s>=2) { first=second;second=count; }
        for (unsigned i=0;i<count;++i) { check(skew->add(3,s)==Error::none,"skew count");skew_requests.push_back({3,s}); }
    }
    check(skew->finish({},plan)==Error::none && *std::max_element(plan.models[1].lengths.begin(),plan.models[1].lengths.end())==15
        && validate_code_lengths(plan.models[1].lengths)==HuffmanTableError::none,"length-limited skew planner");
    encoded.assign(plan.payload_size,std::byte{0});written=777;
    check(encode_position_blocked_huffman_16m(plan,skew_requests,{},*encode_tables,encoded,written)==Error::none,"skew planned encode");
    check(decoder.begin(plan,encoded,{},*decode_tables)==Error::none,"skew planned decoder");
    for (const auto request:skew_requests) check(decoder.read(3,value)==Error::none && value==request.symbol,"skew planned symbol");
    check(decoder.finish()==Error::none,"skew planned finish");
    auto all_overrides=std::make_unique<PositionBlockedHuffman16mModelBuilder>();
    std::vector<PositionBlockedHuffmanDecision> only_overrides;
    for (int context : {0,1}) for (unsigned i=0;i<40;++i) {
        check(all_overrides->add(context,static_cast<unsigned>(context))==Error::none,"all overrides add");
        only_overrides.push_back({context,static_cast<unsigned>(context)});
    }
    check(all_overrides->finish({},plan)==Error::none && plan.field_mask==1 && plan.override_mask==3 && plan.payload_size==0,"active field with no pooled lookup");
    written=777;check(encode_position_blocked_huffman_16m(plan,only_overrides,{},*encode_tables,{},written)==Error::none && written==0,"all overrides zero-bit encode");
    check(decoder.begin(plan,{}, {},*decode_tables)==Error::none,"all overrides decoder");
    for (const auto request:only_overrides) check(decoder.read(request.context,value)==Error::none && value==request.symbol,"all overrides symbol");
    check(decoder.finish()==Error::none,"observed field succeeds without pooled lookup");
    auto uniform=std::make_unique<PositionBlockedHuffman16mModelBuilder>();
    for (unsigned bit : {1u,0u,1u}) check(uniform->add(-1,bit)==Error::none,"uniform planner add");
    check(uniform->finish({},plan)==Error::none && plan.field_mask==0 && plan.override_mask==0 && plan.payload_size==1 && plan.final_valid_bits==3,"uniform-only planned metadata");
    const std::array<PositionBlockedHuffmanDecision,3> raw_bits{{{-1,1},{-1,0},{-1,1}}};
    std::array<std::byte,1> uniform_output{};written=777;
    check(encode_position_blocked_huffman_16m(plan,raw_bits,{},*encode_tables,uniform_output,written)==Error::none && uniform_output[0]==std::byte{5},"uniform-only hand payload");
    auto maximum_planner=std::make_unique<PositionBlockedHuffman16mModelBuilder>();
    for (std::size_t i=0;i<150994944;++i) check(maximum_planner->add(0,0)==Error::none,"maximum bounded zero-bit decisions");
    limits={};limits.max_internal_buffered_bytes=150994944;limits.max_block_size=150994944;
    check(maximum_planner->finish(limits,plan)==Error::none && plan.decision_count==150994944 && plan.payload_size==0,"maximum zero-bit plan");
    const auto maximum_plan=plan;
    check(maximum_planner->add(0,0)==Error::limit_exceeded && maximum_planner->finish(limits,plan)==Error::limit_exceeded && plan==maximum_plan,"one excess decision retained maximum plan");
    auto too_large=std::make_unique<PositionBlockedHuffman16mModelBuilder>(150994945);
    check(too_large->add(0,0)==Error::limit_exceeded && too_large->finish({},plan)==Error::limit_exceeded && plan==maximum_plan,"constructor bound retained plan");
    for (int context : {-2,56}) {
        auto bad_context=std::make_unique<PositionBlockedHuffman16mModelBuilder>();
        check(bad_context->add(context,0)==Error::invalid_context && bad_context->finish({},plan)==Error::invalid_context && plan==maximum_plan,"invalid planning context retains plan");
    }
    for (int context : {23,55}) for (unsigned count : {16u,17u}) {
        auto edge=std::make_unique<PositionBlockedHuffman16mModelBuilder>();
        const unsigned symbol=context==23 ? 20u : 0u;
        const int other=context==23 ? 24 : 50;
        for (unsigned i=0;i<count;++i) check(edge->add(context,symbol)==Error::none,"upper context threshold add");
        check(edge->add(other,context==23 ? 19u : 1u)==Error::none && edge->finish({},plan)==Error::none,"upper context threshold finish");
        check(plan.override_mask==(count==17 ? UINT64_C(1)<<context : 0),"upper context strict sixteen-bit override tie");
        const auto edge_wire=wire(plan);parsed=sentinel;
        check(parse_position_blocked_huffman_16m_descriptor(edge_wire,plan.decision_count,plan.payload_size,{},parsed)==Error::none && parsed==plan,"upper context planned descriptor round trip");
    }

    auto writer=std::make_unique<PositionBlockedHuffman16mPrivateWriter>();
    written=777;
    check(writer->add(0,0)==Error::not_started && writer->finish(written)==Error::not_started && written==777,"writer before begin");
    output=unchanged;limits={};limits.max_block_size=4;
    limits.max_internal_buffered_bytes=16384+sizeof(*writer)+sizeof(d)+sizeof(limits)+sizeof(*encode_tables)+target.size()-1;
    check(writer->begin(d,limits,*encode_tables,target)==Error::limit_exceeded && output==unchanged,"one short writer budget");
    ++limits.max_internal_buffered_bytes;
    check(writer->begin(d,limits,*encode_tables,target)==Error::none,"exact writer budget");
    for (const auto request:decisions) check(writer->add(request.context,request.symbol)==Error::none,"writer unit decision");
    check(writer->finish(written)==Error::none && written==1 && output[1]==std::byte{2}
        && output.front()==std::byte{0xa5} && output.back()==std::byte{0xa5},"writer hand AB and guards");
    output=unchanged;written=777;
    check(writer->begin(d,{},std::span(*encode_tables).first(60),target)==Error::scratch_too_small && output==unchanged,"writer short tables");
    check(writer->begin(d,{},*encode_tables,{})==Error::output_too_small && output==unchanged,"writer short private payload");
    check(writer->begin(d,{},*encode_tables,target)==Error::none && writer->add(0,0)==Error::none
        && writer->add(3,65)==Error::none && writer->add(1,0)==Error::none,"writer valid private prefix");
    check(writer->add(8,67)==Error::invalid_symbol && writer->finish(written)==Error::invalid_symbol && written==777
        && writer->add(8,66)==Error::invalid_symbol,"writer late invalid symbol sticky failure");
    check(writer->begin(d,{},*encode_tables,target)==Error::none && writer->add(56,0)==Error::invalid_context
        && writer->finish(written)==Error::invalid_context && written==777,"writer invalid context");
    check(writer->begin(d,{},*encode_tables,target)==Error::none && writer->finish(written)==Error::count_mismatch
        && written==777 && writer->add(0,0)==Error::count_mismatch,"writer early finish sticky");
    Descriptor too_few_bits=d;too_few_bits.final_valid_bits=1;
    check(writer->begin(too_few_bits,{},*encode_tables,target)==Error::none,"writer short bit declaration begin");
    check(writer->add(0,0)==Error::none && writer->add(3,65)==Error::none && writer->add(1,0)==Error::none
        && writer->add(8,66)==Error::count_mismatch && writer->finish(written)==Error::count_mismatch && written==777,"writer prevents bit overrun");
    Descriptor extra_bits=d;extra_bits.final_valid_bits=3;
    check(writer->begin(extra_bits,{},*encode_tables,target)==Error::none,"writer extra bit declaration begin");
    for (const auto request:decisions) check(writer->add(request.context,request.symbol)==Error::none,"writer extra bit decision");
    check(writer->finish(written)==Error::trailing_bits && written==777,"writer rejects unconsumed bits");
    Descriptor unused=d;unused.override_mask=1;unused.models[5].single_symbol=0;
    check(writer->begin(unused,{},*encode_tables,target)==Error::none,"writer unused override begin");
    for (const auto request : std::array<PositionBlockedHuffmanDecision,4>{{{1,0},{3,65},{1,0},{8,66}}})
        check(writer->add(request.context,request.symbol)==Error::none,"writer unused override decision");
    check(writer->finish(written)==Error::unused_model && written==777,"writer unused override retained count");
    const auto saved_d=d;
    check(writer->begin(d,{},*encode_tables,std::as_writable_bytes(std::span(&d,1)))==Error::overlapping_buffers && d==saved_d,"writer descriptor overlap");
    check(writer->begin(d,{},*encode_tables,std::as_writable_bytes(std::span(writer.get(),1)))==Error::overlapping_buffers,"writer self overlap");
    check(writer->begin(d,{},*encode_tables,std::as_writable_bytes(std::span(*encode_tables)))==Error::overlapping_buffers,"writer table overlap");
    std::size_t aliased_count=777;
    auto aliased_storage=std::as_writable_bytes(std::span(&aliased_count,1));
    check(writer->begin(d,{},*encode_tables,aliased_storage)==Error::none,"writer finish alias begin");
    for (const auto request:decisions) check(writer->add(request.context,request.symbol)==Error::none,"writer alias decisions");
    const auto retained_alias=aliased_count;
    check(writer->finish(aliased_count)==Error::overlapping_buffers && aliased_count==retained_alias,"writer finish rejects payload overlap");
    check(writer->finish(written)==Error::none && written==1,"writer alias preflight retained valid state");
    written=777;
    limits={};limits.max_internal_buffered_bytes=150994944;limits.max_block_size=150994944;
    check(writer->begin(maximum_plan,limits,*encode_tables,{})==Error::none,"writer maximum zero-bit begin");
    for (std::size_t i=0;i<150994944;++i)
        if (writer->add(0,0)!=Error::none) { check(false,"writer bounded zero-bit decision");break; }
    check(writer->finish(written)==Error::none && written==0,"writer maximum without decision array");
    check(writer->begin(single,{},*encode_tables,{})==Error::none && writer->add(0,0)==Error::none
        && writer->add(3,65)==Error::none && writer->add(3,65)==Error::count_mismatch,"writer excess zero-bit decision");
    written=777;check(writer->finish(written)==Error::count_mismatch && written==777,"writer excess count retained");
    check(writer->begin(plan,{},*encode_tables,uniform_output)==Error::none && writer->add(-1,2)==Error::invalid_symbol,"writer invalid uniform bit");
    std::cout<<"bounded sixteen-MiB descriptor/scalar invariants checked\n";
}
}
int main(int argc,char** argv) { if (argc==2) return fixture(argv[1]); units(); return failures ? 1 : 0; }
