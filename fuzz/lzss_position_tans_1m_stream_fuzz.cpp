#include "frame/lzss_position_tans_1m_stream_decoder.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <vector>

namespace {
using namespace marc::frame::internal;
using Token=marc::dictionary::internal::LzssTypedToken;
using Entry=marc::entropy::internal::TansDecodeEntry;
using Status=marc::core::StreamStatus;
constexpr std::size_t entries=marc::entropy::internal::position_tans_1m_table_entries;
constexpr std::size_t serialized_capacity=12*512+9273;
void require(bool value) { if (!value) std::abort(); }
marc::core::DecoderLimits limits() {
    marc::core::DecoderLimits l{};
    l.max_lz_distance=1048576;l.max_frame_size=512;l.max_block_size=4096;
    l.max_total_output_size=4096;l.max_internal_buffered_bytes=UINT64_C(4)<<20;
    return l;
}
struct Result { std::vector<std::byte> raw;marc::core::ProcessResult last{}; };
Result decode(std::span<const std::byte> wire,std::size_t chunk,std::size_t capacity) {
    std::array<std::byte,serialized_capacity> serialized{};
    std::array<std::byte,512> raw{};std::array<Token,512> tokens{};
    std::vector<Entry> tables(entries);
    PositionTans1mStreamDecoder decoder(limits(),serialized,tokens,raw,tables);
    std::size_t consumed{};Result r;
    for (std::size_t call=0;call<wire.size()*2+16384;++call) {
        const auto count=std::min(chunk,wire.size()-consumed);
        std::array<std::byte,514> output{};output.fill(std::byte{0xa5});
        const auto available=call%3 ? capacity : 0;
        const auto flags=(consumed+count==wire.size() ? marc::core::flag_value(marc::core::ProcessFlags::end_input) : 0)
            | (call%2 ? marc::core::flag_value(marc::core::ProcessFlags::flush) : 0);
        r.last=decoder.process(wire.subspan(consumed,count),std::span(output).subspan(1,available),flags);
        require(r.last.input_consumed<=count && r.last.output_produced<=available);
        require(r.last.status!=Status::progress || r.last.input_consumed || r.last.output_produced);
        require(output.front()==std::byte{0xa5});
        require(std::all_of(output.begin()+1+r.last.output_produced,output.end(),[](auto b){return b==std::byte{0xa5};}));
        consumed+=r.last.input_consumed;
        r.raw.insert(r.raw.end(),output.begin()+1,output.begin()+1+r.last.output_produced);
        require(r.raw.size()<=4096);
        if (r.last.status==Status::error || r.last.status==Status::end_of_stream) {
            const auto again=decoder.process({},{},0);
            require(again.status==r.last.status && !again.input_consumed && !again.output_produced
                && again.error.code==r.last.error.code && again.error.byte_position==r.last.error.byte_position);
            return r;
        }
    }
    std::abort();
}
void differential(std::span<const std::byte> wire) {
    const auto whole=decode(wire,wire.size()+1,512),split=decode(wire,7,1);
    require(whole.raw==split.raw && whole.last.status==split.last.status
        && whole.last.error.code==split.last.error.code && whole.last.error.byte_position==split.last.error.byte_position);
}
}
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,std::size_t size) {
    if (size>4096) return 0;
    differential(std::as_bytes(std::span(data,size)));
    const auto count=std::min<std::size_t>(size,128)+1;
    std::vector<Token> tokens(count);
    for (std::size_t i=0;i<count;++i)
        tokens[i]={marc::dictionary::internal::LzssTypedTokenKind::literal,size ? data[i%size] : std::uint8_t{0},0,0};
    const auto l=limits();PositionTans1mStreamHeader stream{static_cast<std::uint32_t>(count),2*count};
    std::vector<std::byte> wire(112);
    require(serialize_position_tans_1m_stream(stream,l,wire)==PositionTans1mFormatError::none);
    std::vector<std::uint16_t> tables(entries);
    std::array<std::byte,4096> payload{};
    std::array<std::byte,serialized_capacity> scratch{},frame{};
    for (unsigned i=0;i<2;++i) {
        const auto encoded=encode_position_tans_1m_frame(tokens,{stream,l,i,i*count},tables,payload,scratch,frame);
        require(encoded.error==PositionTans1mFrameError::none);
        wire.insert(wire.end(),frame.begin(),frame.begin()+encoded.serialized_size);
    }
    differential(wire);
    if (size) {
        wire[(static_cast<std::size_t>(data[0])*17+size)%wire.size()]^=static_cast<std::byte>(data[size-1]|1);
        differential(wire);
        wire.resize((static_cast<std::size_t>(data[0])*31+size)%wire.size());differential(wire);
    }
    return 0;
}
