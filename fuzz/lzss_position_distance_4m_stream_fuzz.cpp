#include "frame/lzss_position_distance_4m_owned_decoder.hpp"
#include "frame/lzss_position_distance_4m_frame.hpp"
#include "core/endian.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <tuple>

namespace {
using namespace marc::frame::internal;
using Token=marc::dictionary::internal::LzssTypedToken;
using Status=marc::core::StreamStatus;
constexpr auto end=marc::core::flag_value(marc::core::ProcessFlags::end_input);
constexpr std::byte sentinel{0xcc};
struct Result {
    std::array<std::byte,128> raw{};
    std::size_t consumed{},produced{};
    marc::core::ProcessResult last{};
};
Result drive(marc::core::Transform& decoder,std::span<const std::byte> input,std::size_t step,std::size_t capacity) {
    Result r{};
    for(std::size_t call=0;call<32768;++call) {
        const auto chunk=input.subspan(r.consumed,std::min(step,input.size()-r.consumed));
        std::array<std::byte,9> output;output.fill(sentinel);
        const auto count=call%7==0?0:capacity;
        const auto q=decoder.process(chunk,std::span{output}.subspan(1,count),
            marc::core::flag_value(marc::core::ProcessFlags::flush)|(r.consumed+chunk.size()==input.size()?end:0));
        if(!marc::core::is_valid(q,chunk.size(),count) || output.front()!=sentinel
            || !std::all_of(output.begin()+1+q.output_produced,output.end(),[](auto b){return b==sentinel;})
            || q.output_produced>r.raw.size()-r.produced) std::abort();
        r.consumed+=q.input_consumed;
        std::copy_n(output.begin()+1,q.output_produced,r.raw.begin()+r.produced);
        r.produced+=q.output_produced;r.last=q;
        if(q.status==Status::error || q.status==Status::end_of_stream) {
            const auto again=decoder.process({}, {},0);
            if(again.status!=q.status || again.input_consumed || again.output_produced
                || again.error.code!=q.error.code || again.error.byte_position!=q.error.byte_position) std::abort();
            return r;
        }
    }
    std::abort();
}
Result compare(std::span<const std::byte> bytes) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size=limits.max_block_size=21;limits.max_total_output_size=128;
    limits.max_internal_buffered_bytes=1<<20;limits.max_compressed_payload_size=383;
    std::array<std::byte,463> serialized{};std::array<Token,21> tokens{};std::array<std::byte,21> raw{};
    LzssPositionDistance4mFrameStreamingDecoder borrowed(limits,serialized,tokens,raw);
    marc::core::ErrorCode error{};
    auto owned=LzssPositionDistance4mOwnedDecoder::create(21,limits,error);
    if(!owned || error!=marc::core::ErrorCode::none) std::abort();
    const auto a=drive(borrowed,bytes,1,1),b=drive(*owned,bytes,13,7);
    if(a.raw!=b.raw || a.consumed!=b.consumed || a.produced!=b.produced
        || a.last.status!=b.last.status || a.last.error.code!=b.last.error.code
        || a.last.error.byte_position!=b.last.error.byte_position) std::abort();
    return a;
}
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,std::size_t size) {
    if(size>8192) return 0;
    compare(std::as_bytes(std::span{data,size}));
    const auto raw_size=std::min(size,std::size_t{64});
    const TypedContextStreamHeader stream{21,raw_size,{4194304,3,258,0},32768,46,10,1,11};
    std::array<std::byte,2048> storage{};
    storage[0]=std::byte{'M'};storage[1]=std::byte{'A'};storage[2]=std::byte{'R'};storage[3]=std::byte{'C'};
    const auto put=[&](std::size_t offset,auto value){if(!marc::core::store_le(std::span{storage},offset,value)) std::abort();};
    for(const auto pair:std::array<std::array<std::uint16_t,2>,10>{
        {{4,2},{8,64},{10,1},{12,2},{14,10},{16,3},{18,2},{84,46},{96,1},{98,11}}}) put(pair[0],pair[1]);
    put(20,UINT32_C(21));put(28,UINT32_C(16));put(32,UINT32_C(16));put(40,static_cast<std::uint64_t>(raw_size));
    put(48,UINT32_C(16));put(64,UINT32_C(4194304));put(68,UINT32_C(3));put(72,UINT32_C(258));put(80,UINT32_C(32768));
    std::size_t written=112;
    std::array<std::size_t,4> frame_ends{};
    std::array<std::size_t,4> raw_ends{};
    std::size_t frame_count{};
    for(std::size_t offset=0;offset<raw_size;offset+=21) {
        const auto n=std::min(std::size_t{21},raw_size-offset);
        std::array<Token,21> tokens{};
        for(std::size_t i=0;i<n;++i) tokens[i]={marc::dictionary::internal::LzssTypedTokenKind::literal,data[offset+i],0,0};
        std::array<marc::context::internal::ModeledOperation,105> operations{};
        const auto e=encode_lzss_position_distance_4m_frame(stream,{},offset/21,offset,
            std::span{tokens}.first(n),operations,std::span{storage}.subspan(written));
        if(e.error!=LzssShortMatchFrameEncodeError::none) std::abort();
        written+=e.serialized_size;
        frame_ends[frame_count]=written;raw_ends[frame_count++]=offset+n;
    }
    auto bytes=std::span{storage}.first(written);
    const auto valid=compare(bytes);
    if(valid.last.status!=Status::end_of_stream || valid.produced!=raw_size) std::abort();
    for(std::size_t i=0;i<raw_size;++i) if(valid.raw[i]!=static_cast<std::byte>(data[i])) std::abort();
    if(size) {
        const auto cut=static_cast<std::size_t>(data[0])%written;
        const auto truncated=compare(bytes.first(cut));
        std::size_t frontier{};
        for(std::size_t i=0;i<frame_count;++i) if(frame_ends[i]<=cut) frontier=raw_ends[i];
        if(truncated.produced!=frontier) std::abort();
        for(std::size_t i=0;i<frontier;++i) if(truncated.raw[i]!=static_cast<std::byte>(data[i])) std::abort();
        if(frame_count>1) {
            // A reserved byte in the second frame prefix must be rejected.
            const auto position=frame_ends[0]+6;
            const auto saved=storage[position];storage[position]=std::byte{1};
            const auto failed=compare(bytes);storage[position]=saved;
            if(failed.last.status!=Status::error || failed.produced!=raw_ends[0]) std::abort();
            for(std::size_t i=0;i<failed.produced;++i) if(failed.raw[i]!=static_cast<std::byte>(data[i])) std::abort();
        }
        const auto pos=(static_cast<std::size_t>(data[0])*257+data[size-1])%written;
        storage[pos]^=static_cast<std::byte>(data[size/2]|1);
        compare(bytes);
        compare(bytes.first(data[0]%written));
    }
    return 0;
}
