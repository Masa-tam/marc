#include "frame/lzss_position_distance_4m_finder_scratch_owned_encoder.hpp"
#include "frame/lzss_position_distance_4m_frame_streaming_encoder.hpp"
#include "frame/lzss_position_distance_4m_owned_decoder.hpp"
#include <algorithm>
#include <cstdlib>
#include <vector>

namespace {
void check(bool value) {if(!value) std::abort();}
std::vector<std::byte> run(marc::core::Transform& transform,std::span<const std::byte> input,
    std::size_t in_chunk,std::size_t out_chunk) {
    using namespace marc::core;
    constexpr std::byte sentinel{0xcc};
    std::vector<std::byte> result,buffer(out_chunk+2);std::size_t position{};
    for(unsigned call=0;call<100000;++call) {
        const auto in=input.subspan(position,std::min(in_chunk,input.size()-position));
        const auto capacity=call%7==0?0:out_chunk;
        buffer.assign(buffer.size(),sentinel);
        const auto flags=(position+in.size()==input.size()?flag_value(ProcessFlags::end_input):0)
            |(call%2?flag_value(ProcessFlags::flush):0);
        const auto q=transform.process(in,std::span{buffer}.subspan(1,capacity),flags);
        check(is_valid(q,in.size(),capacity) && q.status!=StreamStatus::error);
        check(buffer.front()==sentinel && std::all_of(buffer.begin()+1+q.output_produced,
            buffer.end(),[&](auto b){return b==sentinel;}));
        position+=q.input_consumed;
        check(result.size()+q.output_produced<=16384);
        result.insert(result.end(),buffer.begin()+1,buffer.begin()+1+q.output_produced);
        if(q.status==StreamStatus::end_of_stream) {
            check(position==input.size());
            const auto ended=transform.process({}, {},0);
            check(ended.status==StreamStatus::end_of_stream && !ended.output_produced && !ended.input_consumed);
            return result;
        }
    }
    std::abort();
}
}
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,std::size_t size) {
    if(size>128) return 0;
    using namespace marc::frame::internal;
    using Search=marc::dictionary::internal::LzssPositionDistance4mSearch;
    const auto frame=static_cast<std::uint32_t>(size?1+data[0]%64:1);
    const auto policy=size?3+data[size-1]%3:3;
    const TypedContextStreamHeader stream{frame,size,{4194304,3,258,0},32768,46,10,1,11};
    marc::core::DecoderLimits limits{};
    limits.max_frame_size=limits.max_block_size=64;limits.max_total_output_size=128;
    limits.max_compressed_payload_size=18*64+5;limits.max_internal_buffered_bytes=2U<<20;
    LzssPositionDistanceWorkspaceRequirements r{};
    check(calculate_lzss_position_distance_4m_encode_workspace(stream,limits,
        LzssPositionDistanceWorkspaceDirection::encode,sizeof(LzssPositionDistance4mFrameStreamingEncoder),r,
        Search::exhaustive)==LzssPositionDistanceWorkspaceError::none);
    std::vector<std::byte> raw(r.raw_bytes),serialized(r.serialized_bytes);
    std::vector<std::max_align_t> aligned((r.views_bytes+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
    LzssPositionDistance4mFrameStreamingEncoder reference(stream,limits,raw,serialized,
        std::as_writable_bytes(std::span{aligned}).first(r.views_bytes),policy,Search::exhaustive);
    const auto input=std::as_bytes(std::span{data,size});
    const auto expected=run(reference,input,13,7);
    marc::core::ErrorCode error{};
    auto owner=LzssPositionDistance4mFinderScratchOwnedEncoder::create(stream,limits,error,policy);
    check(owner && error==marc::core::ErrorCode::none);
    const auto actual=run(*owner,input,1,1);check(actual==expected);
    auto decoder=LzssPositionDistance4mOwnedDecoder::create(frame,limits,error);
    check(decoder && error==marc::core::ErrorCode::none);
    const auto decoded=run(*decoder,actual,11,3);
    check(std::ranges::equal(decoded,input));
    return 0;
}
