#include "frame/lzss_position_distance_4m_owned_decoder.hpp"
#include "frame/lzss_position_distance_4m_frame.hpp"
#include "core/endian.hpp"
#include "frame/lzss_position_distance_4m_c_adapter.h"
#include <cstring>
#include <vector>
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
class CBoundary final : public marc::core::Transform {
public:
    explicit CBoundary(marc_direction direction= MARC_DIRECTION_DECODE,size_t original=0) {
        if(marc_private_position_distance_4m_config_init(direction,&config)!=MARC_STATUS_OK) std::abort();
        config.original_size=original;config.frame_size=21;
        config.max_frame_size=config.max_block_size=21;config.max_total_output_size=128;
        config.max_internal_buffered_bytes=2U<<20;config.max_compressed_payload_size=383;
        if(marc_private_position_distance_4m_workspace_requirements(&config,&required)!=MARC_STATUS_OK) std::abort();
        primary.resize(required.primary_bytes+32,0xcc);secondary.resize(required.secondary_bytes+32,0xcc);
        aligned.resize((required.views_bytes+32+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
        std::memset(aligned.data(),0xcc,aligned.size()*sizeof(std::max_align_t));
        if(marc_private_position_distance_4m_create(&config,p(),s(),v(),&handle)!=MARC_STATUS_OK || !handle) std::abort();
    }
    ~CBoundary() override {marc_transform_destroy(handle);tails();}
    marc_buffer p(){return {primary.data(),primary.size()};}
    marc_buffer s(){return {secondary.data(),secondary.size()};}
    marc_buffer v(){return {reinterpret_cast<uint8_t*>(aligned.data()),aligned.size()*sizeof(std::max_align_t)};}
    void tails() {
        if(!std::all_of(primary.begin()+required.primary_bytes,primary.end(),[](auto b){return b==0xcc;})
          ||!std::all_of(secondary.begin()+required.secondary_bytes,secondary.end(),[](auto b){return b==0xcc;})
          ||!std::all_of(v().data+required.views_bytes,v().data+v().size,[](auto b){return b==0xcc;}))std::abort();
    }
    marc::core::ProcessResult process(std::span<const std::byte> input,std::span<std::byte> output,uint32_t flags) noexcept override {
        const auto r=marc_transform_process(handle,{reinterpret_cast<const uint8_t*>(input.data()),input.size()},
            {reinterpret_cast<uint8_t*>(output.data()),output.size()},flags);
        using Code=marc::core::ErrorCode;
        const std::array errors{Code::invalid_argument,Code::unsupported,Code::limit_exceeded,
            Code::out_of_memory,Code::malformed_stream,Code::internal_error};
        const std::array statuses{Status::progress,Status::need_input,Status::need_output,Status::end_of_stream};
        if(r.status>=100 && r.status<=105)return {r.input_consumed,r.output_produced,Status::error,
            {errors[r.status-100],r.error_byte_position,r.error_bit_position}};
        if(r.status<1 || r.status>4)std::abort();
        return {r.input_consumed,r.output_produced,statuses[r.status-1],{}};
    }
    marc_private_position_distance_4m_config config{};
    marc_workspace_requirements required{};
    marc_transform* handle{};
private:
    std::vector<uint8_t> primary,secondary;
    std::vector<std::max_align_t> aligned;
};
Result compare(std::span<const std::byte> bytes) {
    marc::core::DecoderLimits limits{};
    limits.max_frame_size=limits.max_block_size=21;limits.max_total_output_size=128;
    limits.max_internal_buffered_bytes=2<<20;limits.max_compressed_payload_size=383;
    std::array<std::byte,463> serialized{};std::array<Token,21> tokens{};std::array<std::byte,21> raw{};
    LzssPositionDistance4mFrameStreamingDecoder reference(limits,serialized,tokens,raw);
    CBoundary adapter;
    const auto a=drive(reference,bytes,1,1),b=drive(adapter,bytes,13,7);
    if(a.raw!=b.raw || a.consumed!=b.consumed || a.produced!=b.produced
        || a.last.status!=b.last.status || a.last.error.code!=b.last.error.code
        || a.last.error.byte_position!=b.last.error.byte_position
        || a.last.error.bit_position!=b.last.error.bit_position) std::abort();
    return a;
}
std::vector<std::byte> encode(std::span<const std::byte> raw,size_t step,size_t capacity) {
    CBoundary adapter(MARC_DIRECTION_ENCODE,raw.size());std::vector<std::byte> result;size_t consumed{};
    for(size_t call=0;call<32768;++call) {
        const auto input=raw.subspan(consumed,std::min(step,raw.size()-consumed));
        std::array<std::byte,9> output;output.fill(sentinel);
        const auto cap=call%7==0?0:capacity;
        const auto r=adapter.process(input,std::span{output}.subspan(1,cap),
            MARC_PROCESS_FLUSH|(consumed+input.size()==raw.size()?MARC_PROCESS_END_INPUT:0u));
        if(!marc::core::is_valid(r,input.size(),cap) || r.status==Status::error || output[0]!=sentinel
          ||!std::all_of(output.begin()+1+r.output_produced,output.end(),[](auto b){return b==sentinel;}))std::abort();
        consumed+=r.input_consumed;result.insert(result.end(),output.begin()+1,output.begin()+1+r.output_produced);
        if(result.size()>2048)std::abort();
        if(r.status==Status::end_of_stream){if(consumed!=raw.size())std::abort();return result;}
    }
    std::abort();
}
void factory_boundaries(uint8_t selector) {
    const auto direction=selector&1?MARC_DIRECTION_ENCODE:MARC_DIRECTION_DECODE;
    CBoundary storage(direction,1);auto c=storage.config;c.frame_size=c.max_frame_size=c.max_block_size=1;
    uint64_t low=1,high=c.max_internal_buffered_bytes;
    while(low<high) {
        const auto middle=low+(high-low)/2;c.max_internal_buffered_bytes=middle;marc_workspace_requirements q{};
        if(marc_private_position_distance_4m_workspace_requirements(&c,&q)==MARC_STATUS_OK)high=middle;else low=middle+1;
    }
    c.max_internal_buffered_bytes=low;marc_workspace_requirements q{};
    if(marc_private_position_distance_4m_workspace_requirements(&c,&q)!=MARC_STATUS_OK)std::abort();
    marc_transform* created=reinterpret_cast<marc_transform*>(1);
    --c.max_internal_buffered_bytes;auto unchanged=q;
    if(marc_private_position_distance_4m_workspace_requirements(&c,&q)!=MARC_STATUS_LIMIT_EXCEEDED
        ||std::memcmp(&q,&unchanged,sizeof(q)))std::abort();
    if(marc_private_position_distance_4m_create(&c,{storage.p().data,q.primary_bytes},
        {storage.s().data,q.secondary_bytes},{storage.v().data,q.views_bytes},&created)!=MARC_STATUS_LIMIT_EXCEEDED || created)std::abort();
    ++c.max_internal_buffered_bytes;
    if(marc_private_position_distance_4m_create(&c,{storage.p().data,q.primary_bytes},
        {storage.s().data,q.secondary_bytes},{storage.v().data,q.views_bytes},&created)!=MARC_STATUS_OK || !created)std::abort();
    marc_transform_destroy(created);
    std::array<uint8_t,sizeof(c)> saved{};std::memcpy(saved.data(),&c,sizeof(c));
    if(marc_private_position_distance_4m_create(&c,storage.p(),storage.s(),storage.v(),
        reinterpret_cast<marc_transform**>(&c))!=MARC_STATUS_INVALID_ARGUMENT
        ||std::memcmp(saved.data(),&c,sizeof(c)))std::abort();
    const unsigned choice=(selector/2)%5;
    uint8_t* pointer=choice<2?reinterpret_cast<uint8_t*>(storage.handle):choice==2?storage.p().data+storage.required.primary_bytes:
        choice==3?storage.s().data+storage.required.secondary_bytes:storage.v().data+storage.required.views_bytes;
    std::array<uint8_t,sizeof(void*)> handle_bytes{};std::memcpy(handle_bytes.data(),storage.handle,handle_bytes.size());
    auto r=marc_transform_process(storage.handle,choice==0?marc_const_buffer{nullptr,0}:marc_const_buffer{pointer,1},
        choice==0?marc_buffer{pointer,sizeof(void*)}:marc_buffer{nullptr,0},0);
    if(r.status!=MARC_STATUS_INVALID_ARGUMENT || r.input_consumed || r.output_produced
        ||std::memcmp(handle_bytes.data(),storage.handle,handle_bytes.size()))std::abort();
    auto again=marc_transform_process(storage.handle,{nullptr,0},{nullptr,0},0);
    if(again.status!=r.status || again.input_consumed || again.output_produced)std::abort();
}
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,std::size_t size) {
    if(size>8192) return 0;
    compare(std::as_bytes(std::span{data,size}));
    const auto raw_size=std::min(size,std::size_t{64});
    factory_boundaries(size?data[0]:0);
    const auto raw=std::as_bytes(std::span{data,raw_size});
    const auto encoded=encode(raw,1,1);
    if(encoded!=encode(raw,13,7))std::abort();
    const auto restored=compare(encoded);
    if(restored.last.status!=Status::end_of_stream || restored.produced!=raw_size
        ||!std::equal(raw.begin(),raw.end(),restored.raw.begin()))std::abort();
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
