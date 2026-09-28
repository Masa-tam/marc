#include "frame/lzss_position_distance_1m_owned_encoder.hpp"
#include "frame/lzss_position_distance_1m_owned_decoder.hpp"
#include <algorithm>
#include <cstdlib>
#include <vector>

namespace {
void check(bool value) {if(!value) std::abort();}
std::vector<std::byte> run(marc::core::Transform& transform,std::span<const std::byte> input,
    std::size_t in_chunk,std::size_t out_chunk) {
    using namespace marc::core;
    std::vector<std::byte> result,buffer(out_chunk);std::size_t position=0;
    for(unsigned call=0;call<100000;++call) {
        const auto in=input.subspan(position,std::min(in_chunk,input.size()-position));
        const auto capacity=call%7==0?0:out_chunk;
        const auto flags=(position+in.size()==input.size()?flag_value(ProcessFlags::end_input):0)
            |(call%2?flag_value(ProcessFlags::flush):0);
        const auto q=transform.process(in,std::span{buffer}.first(capacity),flags);
        check(is_valid(q,in.size(),capacity) && q.status!=StreamStatus::error);
        position+=q.input_consumed;result.insert(result.end(),buffer.begin(),buffer.begin()+q.output_produced);
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
    using Search=marc::dictionary::internal::LzssPositionDistance1mSearch;
    const auto frame=static_cast<std::uint32_t>(size?1+data[0]%64:1);
    const auto policy=size?3+data[size-1]%3:3;
    const TypedContextStreamHeader stream{frame,size,{1048576,3,258,0},32768,44,9,1,10};
    marc::core::DecoderLimits limits{};
    LzssPositionDistanceWorkspaceRequirements r{};
    check(calculate_lzss_position_distance_1m_encode_workspace(stream,limits,
        LzssPositionDistanceWorkspaceDirection::encode,sizeof(LzssPositionDistance1mFrameStreamingEncoder),r)
        ==LzssPositionDistanceWorkspaceError::none);
    std::vector<std::byte> raw(r.raw_bytes),serialized(r.serialized_bytes);
    std::vector<std::max_align_t> aligned((r.views_bytes+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
    LzssPositionDistance1mFrameStreamingEncoder reference(stream,limits,raw,serialized,
        std::as_writable_bytes(std::span{aligned}).first(r.views_bytes),policy,Search::exhaustive);
    const auto input=std::as_bytes(std::span{data,size});
    const auto expected=run(reference,input,13,7);
    marc::core::ErrorCode error{};
    auto owner=LzssPositionDistance1mOwnedEncoder::create(stream,limits,error,policy);
    check(owner && error==marc::core::ErrorCode::none);
    const auto actual=run(*owner,input,1,1);check(actual==expected);
    auto decoder=LzssPositionDistance1mOwnedDecoder::create(frame,limits,error);
    check(decoder && error==marc::core::ErrorCode::none);
    const auto decoded=run(*decoder,actual,11,3);
    check(std::ranges::equal(decoded,input));
    return 0;
}
