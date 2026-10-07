#include "frame/lzss_position_rans_4m_full_literal_stream_decoder.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <vector>

namespace {
using namespace marc::frame::internal;
using Token=marc::dictionary::internal::LzssTypedToken;
using Status=marc::core::StreamStatus;
void require(bool value) { if (!value) std::abort(); }
marc::core::DecoderLimits limits() {
    marc::core::DecoderLimits l{};
    l.max_frame_size=512; l.max_block_size=4608;
    l.max_total_output_size=4096; l.max_internal_buffered_bytes=UINT64_C(1)<<20;
    return l;
}
struct Result {
    std::vector<std::byte> raw;
    marc::core::ProcessResult result{};
};
Result decode(std::span<const std::byte> wire,std::size_t chunk,std::size_t capacity) {
    std::array<std::byte,18*512+9313> serialized{};
    std::array<std::byte,512> raw{};
    std::array<Token,512> tokens{};
    PositionRans4mFullLiteralStreamDecoder decoder(limits(),serialized,tokens,raw);
    std::size_t consumed{};
    Result r;
    for (std::size_t call=0;call<wire.size()*2+16384;++call) {
        const auto count=std::min(chunk,wire.size()-consumed);
        std::array<std::byte,514> output{};
        output.fill(std::byte{0xa5});
        const auto available=call%3==0 ? 0 : capacity;
        const auto flags=(consumed+count==wire.size() ? marc::core::flag_value(marc::core::ProcessFlags::end_input) : 0)
            | (call%2 ? marc::core::flag_value(marc::core::ProcessFlags::flush) : 0);
        r.result=decoder.process(wire.subspan(consumed,count),std::span(output).subspan(1,available),flags);
        require(r.result.input_consumed<=count && r.result.output_produced<=available);
        require(r.result.status!=Status::progress || r.result.input_consumed || r.result.output_produced);
        require(output.front()==std::byte{0xa5});
        require(std::all_of(output.begin()+1+r.result.output_produced,output.end(),[](auto b) { return b==std::byte{0xa5}; }));
        consumed+=r.result.input_consumed;
        r.raw.insert(r.raw.end(),output.begin()+1,output.begin()+1+r.result.output_produced);
        require(r.raw.size()<=4096);
        if (r.result.status==Status::error || r.result.status==Status::end_of_stream) {
            const auto repeated=decoder.process({}, {},0);
            require(repeated.status==r.result.status && !repeated.input_consumed && !repeated.output_produced);
            require(repeated.error.code==r.result.error.code && repeated.error.byte_position==r.result.error.byte_position);
            return r;
        }
    }
    std::abort();
}
void differential(std::span<const std::byte> wire) {
    const auto a=decode(wire,wire.size()+1,512),b=decode(wire,7,1);
    require(a.raw==b.raw && a.result.status==b.result.status);
    require(a.result.error.code==b.result.error.code && a.result.error.byte_position==b.result.error.byte_position);
}
}
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,std::size_t size) {
    if (size>4096) return 0;
    differential({reinterpret_cast<const std::byte*>(data),size});
    // Valid seeds drive the model, frame validator, reconstruction and drain path.
    const auto count=std::min<std::size_t>(size,128)+1;
    std::vector<Token> tokens(count);
    for (std::size_t i=0;i<count;++i)
        tokens[i]={marc::dictionary::internal::LzssTypedTokenKind::literal,size ? data[i%size] : std::uint8_t{0},0,0};
    const auto l=limits();
    PositionRans4mFullLiteralStreamHeader stream{static_cast<std::uint32_t>(count),2*count};
    std::vector<std::byte> wire(112);
    require(serialize_position_rans_4m_full_literal_stream(stream,l,wire)==PositionRans4mFullLiteralFormatError::none);
    for (unsigned i=0;i<2;++i) {
        std::array<std::byte,8192> frame{};
        const auto r=encode_position_rans_4m_full_literal_frame(tokens,{stream,l,i,i*count},frame);
        require(r.error==PositionRans4mFullLiteralFrameError::none);
        wire.insert(wire.end(),frame.begin(),frame.begin()+r.serialized_size);
    }
    differential(wire);
    if (size) {
        const auto offset=(static_cast<std::size_t>(data[0])*17+size)%wire.size();
        wire[offset]^=static_cast<std::byte>(data[size-1] | 1);
        differential(wire);
        wire.resize((static_cast<std::size_t>(data[0])*31+size)%wire.size());
        differential(wire);
    }
    return 0;
}
