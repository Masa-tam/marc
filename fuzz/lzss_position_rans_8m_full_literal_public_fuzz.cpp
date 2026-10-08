#include <marc/marc.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <vector>

namespace {
void require(bool ok) { if (!ok) std::abort(); }
struct Result { std::vector<std::uint8_t> bytes; marc_process_result last{}; };
Result run(marc_direction direction,std::span<const std::uint8_t> input,
    std::uint32_t frame,std::size_t chunk,std::size_t capacity) {
    marc_lzss_position_distance_rans_8m_config config{};
    require(marc_lzss_position_distance_rans_8m_config_init(direction,&config)==MARC_STATUS_OK);
    config.frame_size=frame; config.max_frame_size=frame; config.max_block_size=9*frame;
    config.max_total_output_size=4096; config.max_internal_buffered_bytes=UINT64_C(4)<<20;
    config.original_size=input.size(); config.input_capacity_bytes=65536; config.output_capacity_bytes=512;
    marc_transform* transform{};
    require(marc_lzss_position_distance_rans_8m_create(&config,&transform)==MARC_STATUS_OK && transform);
    Result r; std::size_t consumed{};
    // One-byte frames and one-byte output expand calls through outer headers.
    for (std::size_t call=0;call<input.size()*3+131072;++call) {
        const auto count=std::min(chunk,input.size()-consumed);
        const auto available=call%3 ? capacity : 0;
        std::array<std::uint8_t,514> output{}; output.fill(0xa5);
        const auto flags=(consumed+count==input.size() ? MARC_PROCESS_END_INPUT : 0)
            | (call%2 ? MARC_PROCESS_FLUSH : 0);
        r.last=marc_transform_process(transform,{input.empty() ? nullptr : input.data()+consumed,count},
            {output.data()+1,available},flags);
        require(r.last.input_consumed<=count && r.last.output_produced<=available);
        require(r.last.status!=MARC_STATUS_PROGRESS || r.last.input_consumed || r.last.output_produced);
        require(output.front()==0xa5 && std::all_of(output.begin()+1+r.last.output_produced,output.end(),[](auto b) { return b==0xa5; }));
        consumed+=r.last.input_consumed;
        r.bytes.insert(r.bytes.end(),output.begin()+1,output.begin()+1+r.last.output_produced);
        if (r.last.status==MARC_STATUS_END_OF_STREAM || r.last.status>=100) {
            const auto again=marc_transform_process(transform,{nullptr,0},{nullptr,0},0);
            require(again.status==r.last.status && again.error_byte_position==r.last.error_byte_position
                && !again.input_consumed && !again.output_produced);
            marc_transform_destroy(transform); return r;
        }
    }
    std::abort();
}
void compare(std::span<const std::uint8_t> wire,std::uint32_t frame) {
    const auto whole=run(MARC_DIRECTION_DECODE,wire,frame,wire.size()+1,512);
    const auto split=run(MARC_DIRECTION_DECODE,wire,frame,7,1);
    require(whole.bytes==split.bytes && whole.last.status==split.last.status
        && whole.last.error_byte_position==split.last.error_byte_position);
}
}
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,std::size_t size) {
    if (size>512) return 0;
    const std::span<const std::uint8_t> raw(data,size);
    const std::uint32_t frame=size ? 1+data[0]%64 : 1;
    const auto whole=run(MARC_DIRECTION_ENCODE,raw,frame,size+1,512);
    const auto split=run(MARC_DIRECTION_ENCODE,raw,frame,1,1);
    require(whole.last.status==MARC_STATUS_END_OF_STREAM && split.last.status==MARC_STATUS_END_OF_STREAM
        && whole.bytes==split.bytes);
    const auto restored=run(MARC_DIRECTION_DECODE,whole.bytes,frame,7,1);
    require(restored.last.status==MARC_STATUS_END_OF_STREAM && restored.bytes.size()==size
        && std::equal(restored.bytes.begin(),restored.bytes.end(),raw.begin()));
    auto damaged=whole.bytes;
    if (size) damaged[(size+data[0]*17)%damaged.size()]^=static_cast<std::uint8_t>(data[size-1] | 1);
    compare(damaged,frame);
    compare(raw,frame);
    compare(std::span(whole.bytes).first(size ? data[size-1]%whole.bytes.size() : 0),frame);
    return 0;
}
