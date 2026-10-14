#include "frame/lzss_position_tans_64m_stream_decoder.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <vector>

namespace {
using namespace marc;
using namespace frame::internal;
using Token=dictionary::internal::LzssTypedToken;
using Entry=entropy::internal::TansDecodeEntry;
constexpr auto guard=std::byte{0xa5};
constexpr auto end=core::flag_value(core::ProcessFlags::end_input);
void require(bool value) { if (!value) std::abort(); }
core::DecoderLimits limits() {
    core::DecoderLimits value{};
    value.max_total_output_size=65536;
    value.max_frame_size=512;
    value.max_block_size=512;
    value.max_compressed_payload_size=4096;
    value.max_internal_buffered_bytes=UINT64_C(4)<<20;
    value.max_lz_distance=67108864;
    value.max_lz_match_length=258;
    value.max_entropy_table_entries=entropy::internal::position_tans_64m_table_entries;
    return value;
}
struct Observed {
    std::vector<std::byte> raw;
    std::size_t consumed{};
    core::StreamStatus status{};
    core::StreamError error{};
};
Observed scheduled(std::span<const std::byte> input,std::uint8_t selector,bool split) {
    std::array<std::byte,10000> serialized{};
    std::array<Token,512> tokens{};
    std::array<std::byte,512> raw{};
    std::vector<Entry> tables(entropy::internal::position_tans_64m_table_entries);
    PositionTans64mStreamDecoder decoder(limits(),serialized,tokens,raw,tables);
    Observed observed{};
    std::size_t offset{},stop{},steps{};
    while (true) {
        if (offset==stop && stop<input.size())
            stop=std::min(input.size(),stop+(split ? 1+selector%31 : input.size()));
        const auto capacity=split && steps%5==0 ? std::size_t{} : split ? 1+selector%32 : std::size_t{32};
        std::array<std::byte,35> output{};
        output.fill(guard);
        const auto result=decoder.process(input.subspan(offset,stop-offset),std::span(output).first(capacity),
            stop==input.size() ? end : 0);
        require(result.input_consumed<=stop-offset && result.output_produced<=capacity);
        require(result.status!=core::StreamStatus::progress || result.input_consumed || result.output_produced);
        require(std::all_of(output.begin()+result.output_produced,output.end(),[](auto value){ return value==guard; }));
        offset+=result.input_consumed;
        observed.raw.insert(observed.raw.end(),output.begin(),output.begin()+result.output_produced);
        observed.consumed+=result.input_consumed;
        observed.status=result.status;
        observed.error=result.error;
        require(++steps<100000 && observed.raw.size()<=65536);
        if (result.status==core::StreamStatus::error || result.status==core::StreamStatus::end_of_stream) break;
    }
    std::array<std::byte,32> terminal_output{};
    terminal_output.fill(guard);
    const auto terminal=decoder.process({},terminal_output,999);
    require(!terminal.input_consumed && !terminal.output_produced && terminal.status==observed.status);
    require(terminal.error.code==observed.error.code && terminal.error.byte_position==observed.error.byte_position);
    require(std::all_of(terminal_output.begin(),terminal_output.end(),[](auto value){ return value==guard; }));
    return observed;
}
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,std::size_t size) {
    if (!size || size>10000) return 0;
    const auto input=std::as_bytes(std::span(data+1,size-1));
    const auto whole=scheduled(input,data[0],false);
    const auto split=scheduled(input,data[0],true);
    require(whole.raw==split.raw && whole.consumed==split.consumed && whole.status==split.status);
    require(whole.error.code==split.error.code && whole.error.byte_position==split.error.byte_position);
    return 0;
}
