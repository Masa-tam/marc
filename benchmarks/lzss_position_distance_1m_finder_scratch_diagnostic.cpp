#include "frame/lzss_position_distance_1m_finder_scratch_raw_frame_encoder.hpp"
#include "frame/lzss_position_distance_1m_prepared_raw_frame_encoder.hpp"
#include "dictionary/lzss_position_distance_1m_five_prefix_finder.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    std::ifstream file(argv[1],std::ios::binary|std::ios::ate);if(!file)return 2;
    const auto size=file.tellg();if(size<=0 || size>64*1024*1024)return 2;
    std::vector<std::byte> input(static_cast<std::size_t>(size));file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(input.data()),size))return 2;
    using namespace marc::frame::internal;using namespace marc::dictionary::internal;
    constexpr unsigned frame_size=1048576;
    const TypedContextStreamHeader stream{frame_size,input.size(),{frame_size,3,258,0},32768,44,9,1,10};
    const auto requirement=calculate_lzss_position_distance_1m_five_prefix_workspace(frame_size,stream.dictionary,{});
    if(requirement.error!=LzssShortPrefixError::none)return 1;
    std::vector<std::max_align_t> sa((requirement.workspace_size+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t)),sb(sa.size());
    auto fa=std::as_writable_bytes(std::span{sa}).first(requirement.workspace_size);
    auto fb=std::as_writable_bytes(std::span{sb}).first(requirement.workspace_size);
    std::vector<LzssTypedToken> ta(frame_size),tb(frame_size),decoded(frame_size);
    std::vector<marc::context::internal::ModeledOperation> oa(2*frame_size),ob(2*frame_size);
    std::vector<std::byte> a(18*frame_size+85),b(a.size()),restored(frame_size);
    std::uint64_t frames{},reused{},raw_reused{},tokens{},operations{},bytes{},max_bound{};
    for(std::size_t pos=0;pos<input.size();pos+=frame_size) {
        const auto raw=std::span{input}.subspan(pos,std::min<std::size_t>(frame_size,input.size()-pos));
        const auto x=encode_lzss_position_distance_1m_prepared_raw_frame(stream,{},pos/frame_size,pos,raw,3,LzssPositionDistance1mSearch::indexed_five_prefix,ta,oa,fa,a);
        const auto y=encode_lzss_position_distance_1m_finder_scratch_raw_frame(stream,{},pos/frame_size,pos,raw,3,LzssPositionDistance1mSearch::indexed_five_prefix,tb,ob,fb,b);
        if(x.error!=LzssPositionDistanceRawFrameError::none || y.error!=x.error
            || x.frame.serialized_size!=y.frame.serialized_size
            || !std::equal(a.begin(),a.begin()+x.frame.serialized_size,b.begin()))return 1;
        const auto q=decode_lzss_position_distance_1m_frame(std::span{b}.first(y.frame.serialized_size),{stream,{},pos/frame_size,pos},decoded,restored);
        if(q.error!=LzssShortMatchFrameDecodeError::none || !std::equal(raw.begin(),raw.end(),restored.begin()))return 1;
        ++frames;reused+=y.used_finder_scratch;if(y.used_finder_scratch)raw_reused+=raw.size();
        tokens+=y.candidate.token_count;operations+=y.frame.operation_count;bytes+=y.frame.serialized_size;
        max_bound=std::max(max_bound,2*static_cast<std::uint64_t>(y.frame.decision_count)+5);
    }
    std::cout<<"input_bytes="<<input.size()<<"\nframes="<<frames<<"\nreused_frames="<<reused
        <<"\nfallback_frames="<<frames-reused<<"\nreused_raw_bytes="<<raw_reused
        <<"\ntokens="<<tokens<<"\noperations="<<operations<<"\nframe_bytes="<<bytes
        <<"\nfinder_bytes="<<requirement.workspace_size<<"\nmax_payload_bound="<<max_bound
        <<"\nidentity=1\nround_trip=1\n";
}
