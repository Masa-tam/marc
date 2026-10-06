#include "frame/lzss_position_rans_1m_owned.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <vector>

namespace {
using namespace marc::frame::internal;
using Status=marc::core::StreamStatus;
using Code=marc::core::ErrorCode;
void require(bool good) { if (!good) std::abort(); }
marc::core::DecoderLimits limits() {
    marc::core::DecoderLimits l{}; l.max_frame_size=256; l.max_block_size=2304;
    l.max_total_output_size=4096; l.max_internal_buffered_bytes=UINT64_C(4)<<20; return l;
}
struct Run { std::vector<std::byte> output; marc::core::ProcessResult result{}; };
Run run(marc::core::Transform& transform,std::span<const std::byte> input,std::size_t chunk,std::size_t capacity) {
    Run r; std::size_t consumed{};
    for (std::size_t call=0;call<input.size()*3+16384;++call) {
        const auto count=std::min(chunk,input.size()-consumed);
        std::array<std::byte,514> output{}; output.fill(std::byte{0xa5});
        const auto available=call%3 ? capacity : 0;
        const auto flags=(consumed+count==input.size() ? marc::core::flag_value(marc::core::ProcessFlags::end_input) : 0)
            | (call%2 ? marc::core::flag_value(marc::core::ProcessFlags::flush) : 0);
        r.result=transform.process(input.subspan(consumed,count),std::span(output).subspan(1,available),flags);
        require(r.result.input_consumed<=count && r.result.output_produced<=available);
        require(r.result.status!=Status::progress || r.result.input_consumed || r.result.output_produced);
        require(output.front()==std::byte{0xa5} && std::all_of(output.begin()+1+r.result.output_produced,output.end(),[](auto b) { return b==std::byte{0xa5}; }));
        consumed+=r.result.input_consumed;
        r.output.insert(r.output.end(),output.begin()+1,output.begin()+1+r.result.output_produced);
        if (r.result.status==Status::error || r.result.status==Status::end_of_stream) {
            const auto repeated=transform.process({}, {},0);
            require(repeated.status==r.result.status && !repeated.input_consumed && !repeated.output_produced);
            require(repeated.error.code==r.result.error.code && repeated.error.byte_position==r.result.error.byte_position);
            return r;
        }
    }
    std::abort();
}
Run decode(std::span<const std::byte> bytes,std::uint32_t fs,std::size_t chunk,std::size_t capacity) {
    Code error{}; auto p=PositionRans1mOwnedDecoder::create(fs,limits(),error);
    require(p && error==Code::none); return run(*p,bytes,chunk,capacity);
}
}
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,std::size_t size) {
    if (size>512) return 0;
    const std::span<const std::byte> raw(reinterpret_cast<const std::byte*>(data),size);
    const std::uint32_t fs=size ? 1+data[0]%64 : 1;
    const std::uint32_t eligibility=size && (data[size-1]&1) ? 5 : 3;
    const PositionRans1mStreamHeader stream{fs,size};
    Code error{};
    auto a=PositionRans1mOwnedEncoder::create(stream,limits(),error,eligibility);
    require(a && error==Code::none);
    auto b=PositionRans1mOwnedEncoder::create(stream,limits(),error,eligibility);
    require(b && error==Code::none);
    const auto whole=run(*a,raw,size+1,512),split=run(*b,raw,1,1);
    require(whole.result.status==Status::end_of_stream && split.result.status==Status::end_of_stream && whole.output==split.output);
    const auto decoded=decode(whole.output,fs,7,1);
    require(decoded.result.status==Status::end_of_stream && decoded.output.size()==size
        && std::equal(decoded.output.begin(),decoded.output.end(),raw.begin()));
    auto mutated=whole.output;
    if (size) mutated[(size+data[0]*17)%mutated.size()]^=static_cast<std::byte>(data[size-1] | 1);
    const auto x=decode(mutated,fs,mutated.size()+1,512),y=decode(mutated,fs,7,1);
    require(x.output==y.output && x.result.status==y.result.status
        && x.result.error.code==y.result.error.code && x.result.error.byte_position==y.result.error.byte_position);
    return 0;
}
