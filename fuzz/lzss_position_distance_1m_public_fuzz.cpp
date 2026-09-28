#include "marc/marc.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <vector>

namespace {
void check(bool x) {if(!x) std::abort();}
struct Result {
    std::vector<uint8_t> bytes;
    marc_status status{};
    uint64_t position{};
    bool operator==(const Result&) const = default;
};
Result run(marc_direction direction,const uint8_t* input,size_t size,size_t in_chunk,size_t out_chunk) {
    marc_lzss_position_distance_dynamic_range_1m_config c{};
    check(marc_lzss_position_distance_dynamic_range_1m_config_init(direction,&c)==MARC_STATUS_OK);
    c.original_size=size;c.frame_size=32;c.max_frame_size=c.max_block_size=32;
    c.max_total_output_size=256;c.max_compressed_payload_size=18*32+5;
    marc_workspace_requirements r{};
    check(marc_lzss_position_distance_dynamic_range_1m_workspace_requirements(&c,&r)==MARC_STATUS_OK);
    std::vector<uint8_t> p(r.primary_bytes),s(r.secondary_bytes);
    std::vector<std::max_align_t> v((r.views_bytes+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
    marc_transform* t{};
    check(marc_lzss_position_distance_dynamic_range_1m_create(&c,{p.data(),p.size()},{s.data(),s.size()},
        {reinterpret_cast<uint8_t*>(v.data()),r.views_bytes},&t)==MARC_STATUS_OK && t);
    Result result{};size_t consumed=0;
    for(unsigned call=0;call<100000;++call) {
        const auto n=std::min(in_chunk,size-consumed);const auto cap=call%7==0?0:out_chunk;
        std::array<uint8_t,32> out;out.fill(0xcd);
        const auto q=marc_transform_process(t,{n?input+consumed:nullptr,n},{out.data()+1,cap},
            (consumed+n==size?MARC_PROCESS_END_INPUT:0)|MARC_PROCESS_FLUSH);
        check(q.input_consumed<=n && q.output_produced<=cap);
        check(q.status!=MARC_STATUS_PROGRESS || q.input_consumed || q.output_produced);
        check(out[0]==0xcd && std::all_of(out.begin()+1+q.output_produced,out.end(),[](auto b){return b==0xcd;}));
        consumed+=q.input_consumed;result.bytes.insert(result.bytes.end(),out.begin()+1,out.begin()+1+q.output_produced);
        check(result.bytes.size()<=8192);
        if(q.status==MARC_STATUS_END_OF_STREAM || q.status>=100) {
            result.status=q.status;result.position=q.error_byte_position;
            const auto again=marc_transform_process(t,{nullptr,0},{nullptr,0},0);
            check(again.status==q.status && again.error_byte_position==q.error_byte_position
                && !again.input_consumed && !again.output_produced);
            marc_transform_destroy(t);return result;
        }
    }
    std::abort();
}
}
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data,size_t size) {
    if(size>128) return 0;
    const auto encoded=run(MARC_DIRECTION_ENCODE,data,size,1,1);
    check(encoded.status==MARC_STATUS_END_OF_STREAM);
    check(encoded==run(MARC_DIRECTION_ENCODE,data,size,13,7));
    auto decode=[&](const uint8_t* bytes,size_t count) {
        auto a=run(MARC_DIRECTION_DECODE,bytes,count,1,1);
        check(a==run(MARC_DIRECTION_DECODE,bytes,count,11,7));return a;
    };
    const auto decoded=decode(encoded.bytes.data(),encoded.bytes.size());
    check(decoded.status==MARC_STATUS_END_OF_STREAM && decoded.bytes.size()==size
        && std::equal(decoded.bytes.begin(),decoded.bytes.end(),data));
    decode(data,size);
    if(size) {
        auto mutated=encoded.bytes;mutated[data[0]%mutated.size()]^=data[size-1]|1;
        decode(mutated.data(),mutated.size());
        decode(encoded.bytes.data(),data[0]%encoded.bytes.size());
    }
    return 0;
}
