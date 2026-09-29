#include "context/lzss_position_distance_1m_prepared_model.hpp"
#include "dictionary/lzss_position_distance_1m_five_prefix_finder.hpp"
#include "frame/lzss_position_distance_1m_raw_frame_encoder.hpp"
#include "entropy/lzss_position_distance_1m_range_encoder.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <vector>
using namespace marc::context::internal;
using namespace marc::dictionary::internal;
using namespace marc::frame::internal;
using namespace marc::entropy::internal;
using Clock=std::chrono::steady_clock;
#if defined(_MSC_VER)
#define PRIVATE_NOINLINE __declspec(noinline)
#else
#define PRIVATE_NOINLINE __attribute__((noinline))
#endif
PRIVATE_NOINLINE LzssFieldContextResult baseline(std::span<const LzssTypedToken> tokens,
    const LzssParameters& parameters,const LzssTypedFrameValidationContext& context,
    const marc::core::DecoderLimits& limits,std::span<ModeledOperation> output) {
    const auto plan=plan_lzss_position_distance_1m_operations(tokens,parameters,context,limits);
    if(plan.error!=LzssFieldContextError::none) return plan;
    return model_lzss_position_distance_1m_tokens(tokens,parameters,context,limits,output);
}
PRIVATE_NOINLINE LzssFieldContextResult trial(std::span<const LzssTypedToken> tokens,
    const LzssParameters& parameters,const LzssTypedFrameValidationContext& context,
    const marc::core::DecoderLimits& limits,std::span<ModeledOperation> output) {
    PreparedLzssPositionDistance1mModel prepared;
    const auto plan=prepared.prepare(tokens,parameters,context,limits);
    if(plan.error!=LzssFieldContextError::none) return plan;
    return prepared.write(output);
}
bool same(const ModeledOperation& a,const ModeledOperation& b) {
    return a.kind==b.kind && a.context_id==b.context_id && a.alphabet_size==b.alphabet_size
        && a.value==b.value && a.bit_count==b.bit_count;
}
int main(int argc,char** argv) {
    if(argc!=3 || (std::string_view(argv[2])!="forward" && std::string_view(argv[2])!="reverse")) return 2;
    const bool reverse=std::string_view(argv[2])=="reverse";
    std::ifstream file(argv[1],std::ios::binary|std::ios::ate);if(!file) return 2;
    const auto size=file.tellg();if(size<=0 || size>64*1024*1024) return 2;
    std::vector<std::byte> input(static_cast<std::size_t>(size));file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(input.data()),size)) return 2;
    constexpr std::size_t capacity=1048576;
    const TypedContextStreamHeader stream{capacity,input.size(),{capacity,3,258,0},32768,44,9,1,10};
    const marc::core::DecoderLimits limits{};
    const auto q=calculate_lzss_position_distance_1m_five_prefix_workspace(capacity,stream.dictionary,limits);
    if(q.error!=LzssShortPrefixError::none)return 1;
    std::vector<std::uint32_t> storage(q.workspace_size/4);
    std::vector<LzssTypedToken> tokens(capacity),decoded(capacity);
    std::vector<ModeledOperation> expected(2*capacity),actual(2*capacity);
    std::vector<std::byte> frame(18*capacity+85),payload(frame.size()),raw_output(capacity);
    std::array<std::array<double,3>,3> totals{};
    std::uint64_t token_count{},operation_count{},frame_bytes{};
    std::cout<<std::setprecision(12);
    for(std::size_t offset=0;offset<input.size();offset+=capacity) {
        const auto raw=std::span{input}.subspan(offset,std::min(capacity,input.size()-offset));
        const auto index=offset/capacity;
        const auto reference=encode_lzss_position_distance_1m_raw_frame(stream,limits,index,offset,raw,3,
            LzssPositionDistance1mSearch::indexed_five_prefix,tokens,expected,
            std::as_writable_bytes(std::span{storage}),frame);
        if(reference.error!=LzssPositionDistanceRawFrameError::none)return 1;
        const auto selected=std::span{tokens}.first(reference.candidate.token_count);
        const LzssTypedFrameValidationContext context{static_cast<std::uint32_t>(selected.size()),static_cast<std::uint32_t>(raw.size()),offset};
        const auto operations=std::span{expected}.first(reference.frame.operation_count);
        token_count+=selected.size();operation_count+=operations.size();frame_bytes+=reference.frame.serialized_size;
        for(int iteration=-1;iteration<3;++iteration) for(std::size_t rank=0;rank<3;++rank) {
            const auto rotated=(rank+static_cast<std::size_t>(iteration+1)+index)%3;
            const auto slot=reverse?2-rotated:rotated;
            const auto function=slot==2?trial:baseline;
            const auto begin=Clock::now();
            const auto mapped=function(selected,stream.dictionary,context,limits,actual);
            const auto seconds=std::chrono::duration<double>(Clock::now()-begin).count();
            if(mapped.error!=LzssFieldContextError::none || mapped.operation_count!=operations.size()
                || mapped.decision_count!=reference.frame.decision_count || mapped.token_count!=selected.size()
                || mapped.raw_size!=raw.size() || !std::equal(operations.begin(),operations.end(),actual.begin(),same)) return 1;
            if(iteration>=0) {
                totals[iteration][slot]+=seconds;
                std::cout<<"frame_"<<index<<"_iteration_"<<iteration<<"_slot_"<<slot<<"_rank="<<rank<<'\n'
                         <<"frame_"<<index<<"_iteration_"<<iteration<<"_slot_"<<slot<<"_seconds="<<seconds<<'\n';
            }
        }
        ContextualDynamicRangeDescriptor descriptor{};
        const auto entropy=encode_lzss_position_distance_1m_range_operations_scratch(
            std::span{actual}.first(operations.size()),limits,payload,descriptor);
        if(entropy.error!=ContextualDynamicRangeEncodeError::none || entropy.payload_size!=reference.frame.payload_size
            || !std::equal(payload.begin(),payload.begin()+entropy.payload_size,frame.begin()+80)) return 1;
        const auto restored=decode_lzss_position_distance_1m_frame_scratch(
            std::span{frame}.first(reference.frame.serialized_size),{stream,limits,index,offset},decoded,raw_output);
        if(restored.error!=LzssShortMatchFrameDecodeError::none || !std::equal(raw.begin(),raw.end(),raw_output.begin()))return 1;
    }
    std::cout<<"input_bytes="<<input.size()<<"\ntokens="<<token_count<<"\noperations="<<operation_count
        <<"\nframe_bytes="<<frame_bytes<<"\nprepared_state_bytes="<<sizeof(PreparedLzssPositionDistance1mModel)
        <<"\noperation_identity=1\npayload_identity=1\nround_trip=1\nverified_iterations=3\nexecution_reverse="<<reverse<<'\n';
    for(std::size_t i=0;i<3;++i)for(std::size_t slot=0;slot<3;++slot)
        std::cout<<"iteration_"<<i<<"_slot_"<<slot<<"_seconds="<<totals[i][slot]<<'\n';
}
