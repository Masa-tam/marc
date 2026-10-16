#include "frame/lzss_position_blocked_huffman_4m_owned.hpp"
#include "core/endian.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <new>
#include <vector>

namespace allocation_test {
std::size_t allocated{},freed{},attempted{};
int remaining=-1;
bool forbidden{};
}
void* operator new(std::size_t size) {
    using namespace allocation_test;
    ++attempted;
    if (forbidden || remaining==0) throw std::bad_alloc{};
    if (remaining>0) --remaining;
    if (auto p=std::malloc(size ? size : 1)) { ++allocated;return p; }
    throw std::bad_alloc{};
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { if (p) { ++allocation_test::freed;std::free(p); } }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p,std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p,std::size_t) noexcept { ::operator delete(p); }
void* operator new(std::size_t size,const std::nothrow_t&) noexcept { try { return ::operator new(size); } catch (...) { return nullptr; } }
void* operator new[](std::size_t size,const std::nothrow_t&) noexcept { return ::operator new(size,std::nothrow); }
void operator delete(void* p,const std::nothrow_t&) noexcept { ::operator delete(p); }
void operator delete[](void* p,const std::nothrow_t&) noexcept { ::operator delete(p); }

namespace {
using namespace marc::frame::internal;
using namespace marc::core;
using namespace marc::entropy::internal;
int failures{};
void check(bool condition,const char* text) { if (!condition) { ++failures;std::cerr<<text<<'\n'; } }
constexpr auto end=flag_value(ProcessFlags::end_input);
std::vector<std::byte> transform(Transform& codec,std::span<const std::byte> input,
                                 std::size_t in_chunk,std::size_t out_chunk,std::size_t first_split=SIZE_MAX) {
    std::vector<std::byte> result,out(out_chunk);result.reserve(112+input.size()*12+20000);
    std::size_t used{};
    for (std::size_t calls=0;calls<12000000;++calls) {
        auto n=std::min(in_chunk,input.size()-used);
        if (used<first_split && first_split<input.size()) n=std::min(n,first_split-used);
        const auto previous=allocation_test::attempted;
        allocation_test::forbidden=true;
        const auto r=codec.process(input.subspan(used,n),out,used+n==input.size() ? end : flag_value(ProcessFlags::flush));
        allocation_test::forbidden=false;
        check(previous==allocation_test::attempted,"process never allocates");
        check(r.input_consumed<=n && r.output_produced<=out.size()
            && !(r.status==StreamStatus::progress && !r.input_consumed && !r.output_produced),"valid process counts");
        used+=r.input_consumed;result.insert(result.end(),out.begin(),out.begin()+r.output_produced);
        if (r.status==StreamStatus::end_of_stream) {
            check(used==input.size(),"all input consumed");
            const auto again=codec.process({},out,end);
            check(again.status==StreamStatus::end_of_stream && !again.input_consumed && !again.output_produced,"repeated owning EOS");
            return result;
        }
        if (r.status==StreamStatus::error) { check(false,"owning transform error");std::cerr<<static_cast<unsigned>(r.error.code)<<'\n';return result; }
    }
    check(false,"bounded transform completed");return result;
}
int fixture(const char* path,bool raw_encode) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if (!file) return 2;
    const auto extent=file.tellg();
    if (extent<120 || extent>static_cast<std::streamoff>(UINT64_C(8)<<20)) return 3;
    file.seekg(0);std::vector<char> chars(static_cast<std::size_t>(extent));
    if (!file.read(chars.data(),static_cast<std::streamsize>(chars.size()))) return 4;
    std::vector<std::byte> data(chars.size());
    std::transform(chars.begin(),chars.end(),data.begin(),[](char c){return static_cast<std::byte>(c);});
    std::uint32_t fs{},rs{};(void)load_le(data,0,fs);(void)load_le(data,4,rs);
    if (rs>4194304 || fs>70781505 || UINT64_C(120)+fs+rs!=data.size()) return 5;
    const auto bytes=std::span<const std::byte>(data);
    const auto archive=bytes.subspan(8,112+fs);const auto expected=bytes.subspan(120+fs,rs);
    PositionBlockedHuffman4mStreamHeader stream{};std::size_t consumed{};DecoderLimits limits{};limits.max_internal_buffered_bytes=UINT64_C(512)<<20;limits.max_compressed_payload_size=70778880;limits.expansion_slack=4194304;limits.max_block_size=37748736;
    if (parse_position_blocked_huffman_4m_stream(archive,limits,stream,consumed)!=PositionBlockedHuffman4mFormatError::none
        || consumed!=112 || stream.original_size!=rs || ((rs==0)!=(fs==0))) return 6;
    ErrorCode error{};
    auto decoder=PositionBlockedHuffman4mOwnedDecoder::create(stream.frame_size,limits,error);
    if (!decoder || error!=ErrorCode::none) return 7;
    const auto raw=transform(*decoder,archive,1,1);
    if (!std::equal(raw.begin(),raw.end(),expected.begin(),expected.end()) || failures) return 8;
    if (raw_encode) {
        auto encoder=PositionBlockedHuffman4mOwnedEncoder::create(stream,limits,error);
        if (!encoder || error!=ErrorCode::none) return 9;
        const auto encoded=transform(*encoder,expected,1,1);
        if (failures) return 10;
        std::ofstream wire(std::string(path)+".encoded",std::ios::binary);
        wire.write(reinterpret_cast<const char*>(encoded.data()),static_cast<std::streamsize>(encoded.size()));
        if (!wire) return 11;
    }
    return 0;
}
}
int main(int argc,char** argv) {
    if (argc==2 && std::string(argv[1])=="--requirements") {
        DecoderLimits limits{};limits.max_internal_buffered_bytes=UINT64_C(512)<<20;limits.max_compressed_payload_size=70778880;limits.expansion_slack=4194304;limits.max_block_size=37748736;
        std::cout<<"[";
        bool first=true;
        for (std::uint32_t size : {64U,4194304U}) for (bool encode : {true,false}) {
            PositionBlockedHuffman4mStreamHeader stream{};stream.frame_size=size;stream.original_size=size;
            PositionBlockedHuffman4mWorkspace w{};
            const auto result=encode ? PositionBlockedHuffman4mOwnedEncoder::requirements(stream,limits,w)
                : PositionBlockedHuffman4mOwnedDecoder::requirements(size,limits,w);
            if (result!=ErrorCode::none) return 20;
            std::cout<<(first ? "" : ",")<<"{\"frame\":"<<size<<",\"encode\":"<<(encode ? "true" : "false")
                <<",\"raw\":"<<w.raw_bytes<<",\"token_count\":"<<w.token_count<<",\"token_bytes\":"<<w.token_bytes
                <<",\"serialized\":"<<w.serialized_bytes<<",\"finder\":"<<w.finder_bytes<<",\"tables\":"<<w.table_bytes
                <<",\"payload\":"<<w.payload_bytes<<",\"private_serialized\":"<<w.private_serialized_bytes
                <<",\"owned\":"<<w.owned_buffer_bytes<<",\"owner\":"<<w.owner_bytes<<",\"working\":"<<w.fixed_working_bytes
                <<",\"metadata_floor\":"<<w.metadata_floor_bytes<<",\"aggregate\":"<<w.aggregate_bytes<<"}";
            first=false;
        }
        std::cout<<"]\n";return 0;
    }
    if (argc==3 && std::string(argv[1])=="--owning-fixture") return fixture(argv[2],false);
    if (argc==3 && std::string(argv[1])=="--owning-raw-fixture") return fixture(argv[2],true);
    DecoderLimits limits{};limits.max_internal_buffered_bytes=UINT64_C(512)<<20;limits.max_compressed_payload_size=70778880;limits.expansion_slack=4194304;limits.max_block_size=37748736;PositionBlockedHuffman4mStreamHeader stream{};stream.frame_size=64;stream.original_size=160;
    PositionBlockedHuffman4mWorkspace enc{},dec{};
    check(PositionBlockedHuffman4mOwnedEncoder::requirements(stream,limits,enc)==ErrorCode::none
        && PositionBlockedHuffman4mOwnedDecoder::requirements(64,limits,dec)==ErrorCode::none,"requirements admitted");
    check(enc.table_bytes==position_blocked_huffman_4m_tables*sizeof(CanonicalHuffmanTable)
        && dec.table_bytes==position_blocked_huffman_4m_tables*sizeof(HuffmanDecodeTable)
        && enc.private_serialized_bytes==enc.serialized_bytes && dec.private_serialized_bytes==0
        && enc.payload_bytes==1080 && dec.payload_bytes==0,"table and private buffer extents");
    for (bool encode : {true,false}) {
        auto tight=limits;tight.max_block_size=512;
        const auto grant=encode ? enc.aggregate_bytes : dec.aggregate_bytes;
        tight.max_internal_buffered_bytes=grant;
        ErrorCode error{};
        if (encode) check(static_cast<bool>(PositionBlockedHuffman4mOwnedEncoder::create(stream,tight,error)) && error==ErrorCode::none,"exact encoder creation");
        else check(static_cast<bool>(PositionBlockedHuffman4mOwnedDecoder::create(64,tight,error)) && error==ErrorCode::none,"exact decoder creation");
        --tight.max_internal_buffered_bytes;PositionBlockedHuffman4mWorkspace marker{};marker.aggregate_bytes=999;
        const auto before=allocation_test::attempted;
        if (encode) {
            check(PositionBlockedHuffman4mOwnedEncoder::requirements(stream,tight,marker)==ErrorCode::limit_exceeded && marker.aggregate_bytes==999,"failed encoder query unchanged");
            check(!PositionBlockedHuffman4mOwnedEncoder::create(stream,tight,error) && error==ErrorCode::limit_exceeded,"below encoder refusal");
        } else {
            check(PositionBlockedHuffman4mOwnedDecoder::requirements(64,tight,marker)==ErrorCode::limit_exceeded && marker.aggregate_bytes==999,"failed decoder query unchanged");
            check(!PositionBlockedHuffman4mOwnedDecoder::create(64,tight,error) && error==ErrorCode::limit_exceeded,"below decoder refusal");
        }
        check(allocation_test::attempted==before,"below grant allocates nothing");
    }
    // Every object/buffer allocation is a separately injected failure.
    for (bool encode : {true,false}) {
        const auto baseline=allocation_test::allocated-allocation_test::freed;
        std::size_t count{};ErrorCode error{};
        const auto before=allocation_test::attempted;
        if (encode) { auto p=PositionBlockedHuffman4mOwnedEncoder::create(stream,limits,error);check(static_cast<bool>(p),"encoder baseline creation"); }
        else { auto p=PositionBlockedHuffman4mOwnedDecoder::create(64,limits,error);check(static_cast<bool>(p),"decoder baseline creation"); }
        count=allocation_test::attempted-before;
        check(count==(encode ? 8U : 5U),"all owning allocation sites counted");
        for (std::size_t i=0;i<count;++i) {
            allocation_test::remaining=static_cast<int>(i);
            if (encode) check(!PositionBlockedHuffman4mOwnedEncoder::create(stream,limits,error) && error==ErrorCode::out_of_memory,"each encoder allocation fails cleanly");
            else check(!PositionBlockedHuffman4mOwnedDecoder::create(64,limits,error) && error==ErrorCode::out_of_memory,"each decoder allocation fails cleanly");
            allocation_test::remaining=-1;
            check(allocation_test::allocated-allocation_test::freed==baseline,"partial allocation rollback releases everything");
        }
    }
    std::vector<std::byte> raw(160);for (std::size_t i=0;i<raw.size();++i) raw[i]=static_cast<std::byte>(i%7);
    ErrorCode error{};auto reference=PositionBlockedHuffman4mOwnedEncoder::create(stream,limits,error);
    const auto expected=transform(*reference,raw,raw.size(),8192);
    for (auto in_chunk : {1U,7U,64U}) for (auto out_chunk : {1U,13U,512U}) {
        auto encoder=PositionBlockedHuffman4mOwnedEncoder::create(stream,limits,error);
        const auto bytes=transform(*encoder,raw,in_chunk,out_chunk);
        check(bytes==expected,"owning encoded bytes independent of chunks");
        auto decoder=PositionBlockedHuffman4mOwnedDecoder::create(64,limits,error);
        check(transform(*decoder,bytes,in_chunk,out_chunk)==raw,"owning raw round trip");
    }
    for (std::size_t split=0;split<=raw.size();++split) {
        auto encoder=PositionBlockedHuffman4mOwnedEncoder::create(stream,limits,error);
        check(transform(*encoder,raw,raw.size(),1,split)==expected,"every small encoder input split with byte output");
    }
    for (auto size : {4194303U,4194304U,4194305U}) {
        auto large_stream=stream;large_stream.frame_size=4194304;large_stream.original_size=size;
        std::vector<std::byte> large_raw(size);for (std::size_t i=0;i<size;++i) large_raw[i]=static_cast<std::byte>(i%256);
        PositionBlockedHuffman4mWorkspace e{},d{};
        check(PositionBlockedHuffman4mOwnedEncoder::requirements(large_stream,limits,e)==ErrorCode::none
            && PositionBlockedHuffman4mOwnedDecoder::requirements(4194304,limits,d)==ErrorCode::none,"full window requirements");
        auto le=limits;le.max_internal_buffered_bytes=e.aggregate_bytes;
        auto ld=limits;ld.max_internal_buffered_bytes=d.aggregate_bytes;
        auto encoder=PositionBlockedHuffman4mOwnedEncoder::create(large_stream,le,error);
        const auto bytes=transform(*encoder,large_raw,997,509);
        auto decoder=PositionBlockedHuffman4mOwnedDecoder::create(4194304,ld,error);
        check(transform(*decoder,bytes,1,1)==large_raw,"full-window exact grants and final partial frame");
    }
    auto pair_stream=stream;pair_stream.frame_size=1;pair_stream.original_size=2;
    const std::array<std::byte,2> pair_raw{std::byte{65},std::byte{66}};
    auto pair_encoder=PositionBlockedHuffman4mOwnedEncoder::create(pair_stream,limits,error);
    auto pair_bytes=transform(*pair_encoder,pair_raw,1,1);pair_bytes[pair_bytes.size()-3]=std::byte{1};
    auto pair_decoder=PositionBlockedHuffman4mOwnedDecoder::create(1,limits,error);
    std::array<std::byte,2> pair_output{std::byte{0x5a},std::byte{0x5a}};
    const auto failed=pair_decoder->process(pair_bytes,pair_output,end);
    check(failed.status==StreamStatus::error && failed.output_produced==1
        && pair_output[0]==std::byte{65} && pair_output[1]==std::byte{0x5a},"owned late second-frame failure publishes only first");
    check(pair_decoder->process({},pair_output,end).output_produced==0,"owned failure remains sticky");
    pair_bytes[pair_bytes.size()-3]=std::byte{0};
    for (std::size_t cut=0;cut<pair_bytes.size();++cut) {
        auto truncated=PositionBlockedHuffman4mOwnedDecoder::create(1,limits,error);
        std::fill(pair_output.begin(),pair_output.end(),std::byte{0x5a});
        const auto r=truncated->process(std::span(pair_bytes).first(cut),pair_output,end);
        const auto committed=cut>=211 ? 1U : 0U;
        check(r.status==StreamStatus::error && r.error.code==ErrorCode::malformed_stream
            && r.output_produced==committed && (committed==0 || pair_output[0]==std::byte{65})
            && pair_output[committed]==std::byte{0x5a},"owned every truncation excludes failed frame");
    }
    auto trailing_bytes=pair_bytes;trailing_bytes.push_back(std::byte{0});
    auto trailing_decoder=PositionBlockedHuffman4mOwnedDecoder::create(1,limits,error);
    const auto trailing=trailing_decoder->process(trailing_bytes,pair_output,end);
    check(trailing.status==StreamStatus::error && trailing.input_consumed==310
        && trailing.output_produced==2,"owned strict trailing refusal after valid frames");
    auto aliased_decoder=PositionBlockedHuffman4mOwnedDecoder::create(1,limits,error);
    const auto pair_before=pair_bytes;
    const auto aliased=aliased_decoder->process(pair_bytes,std::span(pair_bytes).first(2),end);
    check(aliased.status==StreamStatus::error && aliased.error.code==ErrorCode::invalid_argument
        && !aliased.input_consumed && !aliased.output_produced && pair_bytes==pair_before,"owned decoder overlap unchanged");
    auto flagged_encoder=PositionBlockedHuffman4mOwnedEncoder::create(pair_stream,limits,error);
    const auto flag_error=flagged_encoder->process(pair_raw,pair_output,flag_value(ProcessFlags::reset_block));
    check(flag_error.status==StreamStatus::error && flag_error.error.code==ErrorCode::unsupported
        && !flag_error.input_consumed && !flag_error.output_produced,"owned encoder unsupported reset");
    auto early=PositionBlockedHuffman4mOwnedEncoder::create(pair_stream,limits,error);
    std::fill(pair_output.begin(),pair_output.end(),std::byte{0x5a});
    const auto invalid=early->process(std::span(pair_raw).first(1),pair_output,end);
    check(invalid.status==StreamStatus::error && invalid.error.code==ErrorCode::invalid_argument
        && !invalid.input_consumed && !invalid.output_produced && pair_output[0]==std::byte{0x5a},"early known-size EndInput fails before bytes");
    auto empty=stream;empty.original_size=0;auto empty_encoder=PositionBlockedHuffman4mOwnedEncoder::create(empty,limits,error);
    const auto empty_bytes=transform(*empty_encoder,{},1,1);
    auto empty_decoder=PositionBlockedHuffman4mOwnedDecoder::create(64,limits,error);
    check(empty_bytes.size()==112 && transform(*empty_decoder,empty_bytes,1,1).empty(),"empty owning stream");
    std::cout<<"encoder admission "<<enc.aggregate_bytes<<" owned buffers "<<enc.owned_buffer_bytes
        <<" decoder admission "<<dec.aggregate_bytes<<" owned buffers "<<dec.owned_buffer_bytes<<'\n';
    return failures ? 1 : 0;
}

