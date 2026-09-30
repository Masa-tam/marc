#include "frame/lzss_position_distance_4m_encode_workspace.hpp"
#include "frame/lzss_position_distance_4m_frame_streaming_encoder.hpp"
#include "frame/lzss_position_distance_4m_frame.hpp"
#include "dictionary/lzss_position_distance_4m_five_prefix_candidate.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {
constexpr std::size_t frame_size=4194304;
bool read(const char* path,std::vector<std::byte>& bytes,std::size_t maximum) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file)return false;
    const auto n=file.tellg();if(n<0||static_cast<std::uint64_t>(n)>maximum)return false;
    bytes.resize(static_cast<std::size_t>(n));file.seekg(0);
    return n==0||static_cast<bool>(file.read(reinterpret_cast<char*>(bytes.data()),n));
}
struct FrameReport {
    std::size_t raw{},tokens{},operations{},serialized{};
    double selection{},coding{};
};
}
// Diagnostic-only split path: original codec functions contain no timers.
// Comparison and restoration occur outside both additive measured intervals.
int main(int argc,char** argv) {
    if(argc!=5)return 2;
    const std::string_view finder=argv[4];
    if(finder!="reference-0"&&finder!="reference-1"&&finder!="five-prefix")return 2;
    const bool trial=finder=="five-prefix";
    const std::string_view mode=argv[3];
    if(mode!="verify"&&mode!="measure")return 2;
    const bool timed=mode=="measure";
    std::vector<std::byte> input,archive;
    if(!read(argv[1],input,64U*1024U*1024U)||!read(argv[2],archive,128U*1024U*1024U))return 2;
    using namespace marc::frame::internal;
    using namespace marc::dictionary::internal;
    using Clock=std::chrono::steady_clock;
    const TypedContextStreamHeader stream{frame_size,input.size(),{frame_size,3,258,0},32768,46,10,1,11};
    marc::core::DecoderLimits limits{};
    limits.max_block_size=frame_size;limits.max_compressed_payload_size=75497477;
    limits.max_internal_buffered_bytes=512U*1024U*1024U;
    std::array<std::byte,typed_context_stream_header_size> header{};
    if(!serialize_lzss_position_distance_4m_stream_header(stream,limits,header)
        ||archive.size()<header.size()||!std::equal(header.begin(),header.end(),archive.begin()))return 1;
    LzssPositionDistanceWorkspaceRequirements required{};
    constexpr auto direction=LzssPositionDistanceWorkspaceDirection::encode;
    if(calculate_lzss_position_distance_4m_encode_workspace(stream,limits,direction,0,required)
        !=LzssPositionDistanceWorkspaceError::none)return 1;
    const auto trial_required=calculate_lzss_position_distance_4m_five_prefix_workspace(
        frame_size,stream.dictionary,limits);
    if(trial_required.error!=LzssShortPrefixError::none)return 1;
    const auto finder_bytes=trial?trial_required.workspace_size:required.finder_bytes;
    std::size_t budget{};
    const auto view_bytes=required.views_bytes-required.finder_bytes+finder_bytes;
    if(charge_lzss_position_distance_4m_encode_workspace(limits,direction,0,
        required.raw_bytes,required.serialized_bytes,view_bytes,budget)
        !=LzssPositionDistanceWorkspaceError::none)return 1;
    // Independent, query-sized finder storage in every fresh process. The
    // untimed token oracle and input/archive buffers are diagnostic overhead.
    std::vector<std::byte> raw(required.raw_bytes),serialized(required.serialized_bytes);
    std::vector<LzssTypedToken> tokens_buffer(required.token_count),oracle(required.token_count);
    std::vector<marc::context::internal::ModeledOperation> operations_buffer(required.operation_count);
    std::vector<std::uint32_t> finder_buffer(finder_bytes/sizeof(std::uint32_t));
    LzssPositionDistanceWorkspaceViews views{raw,serialized,
        std::as_writable_bytes(std::span{finder_buffer}),tokens_buffer,operations_buffer};
    std::vector<FrameReport> reports;reports.reserve((input.size()+frame_size-1)/frame_size);
    std::size_t cursor=header.size(),tokens{},operations{};double selection{},coding{};
    for(std::size_t offset=0;offset<input.size();offset+=frame_size) {
        const auto source=std::span<const std::byte>{input}.subspan(offset,std::min(frame_size,input.size()-offset));
        const auto sequence=offset/frame_size;
        // Independently decode the frozen frame before either measured call.
        const auto decoded=decode_lzss_position_distance_4m_frame_scratch(
            std::span<const std::byte>{archive}.subspan(cursor),
            {stream,limits,sequence,offset},oracle,views.raw);
        if(decoded.error!=LzssShortMatchFrameDecodeError::none
            ||decoded.required_raw_size!=source.size()
            ||!std::equal(source.begin(),source.end(),views.raw.begin()))return 1;
        const auto begin=timed?Clock::now():Clock::time_point{};
        const auto candidate=trial
            ?tokenize_lzss_position_distance_4m_five_prefix_candidate(
                source,stream.dictionary,limits,3,views.tokens,views.finder)
            :tokenize_lzss_position_distance_4m_candidate(source,stream.dictionary,limits,
                3,LzssPositionDistance4mSearch::indexed_reference,views.tokens,views.finder);
        const double selected_seconds=timed?std::chrono::duration<double>(Clock::now()-begin).count():0;
        if(candidate.error!=LzssShortMatchCandidateError::none
            ||candidate.token_count!=decoded.required_token_count)return 1;
        const auto selected=views.tokens.first(candidate.token_count);
        for(std::size_t i=0;i<selected.size();++i) {
            const auto& x=selected[i];const auto& y=oracle[i];
            if(x.kind!=y.kind||x.literal!=y.literal||x.length!=y.length||x.distance!=y.distance)return 1;
        }
        const auto frame_begin=timed?Clock::now():Clock::time_point{};
        const auto encoded=encode_lzss_position_distance_4m_frame(stream,limits,sequence,offset,
            selected,views.operations,views.serialized);
        const double coded_seconds=timed?std::chrono::duration<double>(Clock::now()-frame_begin).count():0;
        if(encoded.error!=LzssShortMatchFrameEncodeError::none||encoded.raw_size!=source.size()
            ||encoded.token_count!=candidate.token_count||encoded.serialized_size>archive.size()-cursor)return 1;
        const auto bytes=std::span<const std::byte>{views.serialized}.first(encoded.serialized_size);
        if(!std::equal(bytes.begin(),bytes.end(),archive.begin()+cursor))return 1;
        if(encoded.serialized_size!=decoded.serialized_consumed)return 1;
        reports.push_back({source.size(),candidate.token_count,encoded.operation_count,bytes.size(),selected_seconds,coded_seconds});
        cursor+=bytes.size();tokens+=candidate.token_count;operations+=encoded.operation_count;
        selection+=selected_seconds;coding+=coded_seconds;
    }
    if(cursor!=archive.size())return 1;
    std::cout<<std::setprecision(12)<<"verified=1\nmode="<<mode<<"\ninput_bytes="<<input.size()
        <<"\narchive_bytes="<<archive.size()<<"\nframes="<<reports.size()<<"\ntokens="<<tokens
        <<"\noperations="<<operations<<"\nselection_seconds="<<selection<<"\nframe_seconds="<<coding
        <<"\nfinder="<<finder<<"\nquery_budget="<<budget<<"\nfinder_bytes="<<finder_bytes
        <<"\noracle_bytes="<<oracle.size()*sizeof(LzssTypedToken)<<'\n';
    for(std::size_t i=0;i<reports.size();++i) {
        const auto& r=reports[i];const auto prefix="frame_"+std::to_string(i)+"_";
        std::cout<<prefix<<"raw_bytes="<<r.raw<<'\n'<<prefix<<"tokens="<<r.tokens<<'\n'
            <<prefix<<"operations="<<r.operations<<'\n'<<prefix<<"serialized_bytes="<<r.serialized<<'\n'
            <<prefix<<"selection_seconds="<<r.selection<<'\n'<<prefix<<"coding_seconds="<<r.coding<<'\n';
    }
}
