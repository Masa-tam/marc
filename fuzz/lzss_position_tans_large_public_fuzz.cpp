// Bounded public-lifecycle extension of the repository-owned 4 MiB harness.
#include <marc/marc.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <span>
#include <vector>

#ifndef MARC_TANS_WINDOW
#error Define MARC_TANS_WINDOW as the selected public API suffix.
#endif
#define MARC_TANS_NAME_IMPL(window, suffix) marc_lzss_position_distance_tans_##window##suffix
#define MARC_TANS_NAME(window, suffix) MARC_TANS_NAME_IMPL(window, suffix)
#define MARC_TANS_API(suffix) MARC_TANS_NAME(MARC_TANS_WINDOW, suffix)

namespace {
using Config=MARC_TANS_API(_config);
using Resources=MARC_TANS_API(_resources);
constexpr std::uint64_t budget=UINT64_C(8)<<20;
constexpr std::uint8_t guard=0xa5;
void require(bool ok,std::source_location where=std::source_location::current()) {
    if (!ok) {
        std::fprintf(stderr,"public lifecycle invariant failed at %s:%u\n",
            where.file_name(),static_cast<unsigned>(where.line()));
        std::abort();
    }
}
struct Result { std::vector<std::uint8_t> bytes; marc_process_result last{}; };

Result run(marc_direction direction,std::span<const std::uint8_t> input,
           std::uint32_t frame,std::size_t chunk,std::size_t capacity) {
    Config config{};
    require(MARC_TANS_API(_config_init)(direction,&config)==MARC_STATUS_OK);
    config.frame_size=frame;
    config.max_frame_size=frame;
    config.max_block_size=9*frame;
    config.max_compressed_payload_size=16384;
    config.max_total_output_size=4096;
    config.max_internal_buffered_bytes=budget;
    config.external_retained_bytes=65536;
    config.input_capacity_bytes=65536;
    config.output_capacity_bytes=512;
    config.original_size=direction==MARC_DIRECTION_ENCODE ? input.size() : 0;
    Resources resources{};
    require(MARC_TANS_API(_resource_requirements)(&config,&resources)==MARC_STATUS_OK
        && resources.minimum_aggregate_bytes<=budget);
    marc_transform* transform{};
    require(MARC_TANS_API(_create)(&config,&transform)==MARC_STATUS_OK && transform);
    Result result;
    std::size_t consumed{};
    for (std::size_t call=0;call<input.size()*3+131072;++call) {
        const auto count=std::min(chunk,input.size()-consumed);
        const auto available=call%3 ? capacity : 0;
        std::array<std::uint8_t,514> output{};
        output.fill(guard);
        const auto flags=(consumed+count==input.size() ? MARC_PROCESS_END_INPUT : 0)
            | (call%2 ? MARC_PROCESS_FLUSH : 0);
        result.last=marc_transform_process(transform,
            {input.empty() ? nullptr : input.data()+consumed,count},
            {output.data()+1,available},flags);
        require(result.last.input_consumed<=count && result.last.output_produced<=available);
        require(result.last.status!=MARC_STATUS_PROGRESS
            || result.last.input_consumed || result.last.output_produced);
        require(output.front()==guard
            && std::all_of(output.begin()+1+result.last.output_produced,output.end(),
                [](auto byte){ return byte==guard; }));
        consumed+=result.last.input_consumed;
        result.bytes.insert(result.bytes.end(),output.begin()+1,
                            output.begin()+1+result.last.output_produced);
        require(result.bytes.size()<=65536);
        if (result.last.status==MARC_STATUS_END_OF_STREAM || result.last.status>=100) {
            require(result.last.status!=MARC_STATUS_INTERNAL_ERROR);
            const auto terminal=marc_transform_process(transform,{nullptr,0},{nullptr,0},0);
            require(terminal.status==result.last.status
                && terminal.error_byte_position==result.last.error_byte_position
                && terminal.error_bit_position==result.last.error_bit_position
                && !terminal.input_consumed && !terminal.output_produced);
            marc_transform_destroy(transform);
            return result;
        }
    }
    std::abort();
}

Result compare(std::span<const std::uint8_t> wire,std::uint32_t frame) {
    const auto whole=run(MARC_DIRECTION_DECODE,wire,frame,wire.size()+1,512);
    const auto split=run(MARC_DIRECTION_DECODE,wire,frame,7,1);
    require(whole.bytes==split.bytes && whole.last.status==split.last.status
        && whole.last.error_byte_position==split.last.error_byte_position
        && whole.last.error_bit_position==split.last.error_bit_position);
    return whole;
}
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,std::size_t size) {
    if (size>128) return 0;
    const std::span<const std::uint8_t> raw(data,size);
    const std::uint32_t frame=size ? 32+data[0]%33 : 32;
    const auto whole=run(MARC_DIRECTION_ENCODE,raw,frame,size+1,512);
    const auto split=run(MARC_DIRECTION_ENCODE,raw,frame,1,1);
    require(whole.last.status==MARC_STATUS_END_OF_STREAM
        && split.last.status==MARC_STATUS_END_OF_STREAM && whole.bytes==split.bytes);
    const auto restored=run(MARC_DIRECTION_DECODE,whole.bytes,frame,7,1);
    require(restored.last.status==MARC_STATUS_END_OF_STREAM
        && restored.bytes.size()==size && std::equal(restored.bytes.begin(),restored.bytes.end(),raw.begin()));
    // Truncation removes the last frame's final byte; only preceding frames
    // may be published, including when the last raw frame was full-sized.
    require(!whole.bytes.empty());
    const auto truncated=compare(std::span(whole.bytes).first(whole.bytes.size()-1),frame);
    const auto prefix=size ? ((size-1)/frame)*frame : 0;
    require(truncated.last.status>=100 && truncated.bytes.size()==prefix
        && std::equal(truncated.bytes.begin(),truncated.bytes.end(),raw.begin()));
    auto damaged=whole.bytes;
    if (size) damaged[(size+data[0]*17)%damaged.size()]^=static_cast<std::uint8_t>(data[size-1]|1);
    (void)compare(damaged,frame);
    (void)compare(raw,frame);
    (void)compare(std::span(whole.bytes).first(size ? data[size-1]%whole.bytes.size() : 0),frame);
    return 0;
}
