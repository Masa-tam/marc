#include "lzss_position_distance_4m_end_probe_finder.hpp"
#include "core/checked_math.hpp"
#include "frame/lzss_position_distance_4m_encode_workspace.hpp"
#include "frame/lzss_position_distance_4m_frame_streaming_encoder.hpp"
#include "frame/lzss_position_distance_4m_frame.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
constexpr std::size_t frame_size=4194304;
bool read(const char* path,std::vector<std::byte>& bytes,std::size_t maximum) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file)return false;
    const auto n=file.tellg();if(n<0||static_cast<std::uint64_t>(n)>maximum)return false;
    bytes.resize(static_cast<std::size_t>(n));file.seekg(0);
    return n==0||static_cast<bool>(file.read(reinterpret_cast<char*>(bytes.data()),n));
}
using marc::dictionary::internal::LzssPositionDistance4mEndProbeCounters;
struct Report {
    std::size_t raw{},tokens{},operations{},serialized{};
    LzssPositionDistance4mEndProbeCounters counters{};
};
}

// Diagnostic only: no clocks, no public codec selection, no failed report output.
// Frozen frames independently supply both token and restored-byte oracles.
int main(int argc,char** argv) {
    if(argc!=3)return 2;
    std::vector<std::byte> input,archive;
    if(!read(argv[1],input,64U*1024U*1024U)||!read(argv[2],archive,128U*1024U*1024U))return 2;
    using namespace marc::frame::internal;
    using namespace marc::dictionary::internal;
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
    const auto trial=calculate_lzss_position_distance_4m_end_probe_workspace(frame_size,stream.dictionary,limits);
    if(trial.error!=LzssShortPrefixError::none)return 1;
    const auto view_bytes=required.views_bytes-required.finder_bytes+trial.workspace_size;
    std::size_t budget{};
    if(charge_lzss_position_distance_4m_encode_workspace(limits,direction,0,
        required.raw_bytes,required.serialized_bytes,view_bytes,budget)
        !=LzssPositionDistanceWorkspaceError::none
        ||!marc::core::checked_add(budget,sizeof(LzssPositionDistance4mEndProbeFinder),budget)
        ||budget>limits.max_internal_buffered_bytes)return 1;
    // File buffers, frozen token oracle and reports are bounded harness overhead.
    // Codec buffers and the whole enlarged finder instance are charged above.
    std::vector<std::byte> raw(required.raw_bytes),serialized(required.serialized_bytes);
    std::vector<LzssTypedToken> tokens(required.token_count),oracle(required.token_count);
    std::vector<marc::context::internal::ModeledOperation> operations(required.operation_count);
    std::vector<std::uint32_t> storage(trial.workspace_size/sizeof(std::uint32_t));
    auto workspace=std::as_writable_bytes(std::span{storage});
    std::vector<Report> reports;reports.reserve((input.size()+frame_size-1)/frame_size);
    std::size_t cursor=header.size();
    for(std::size_t offset=0;offset<input.size();offset+=frame_size) {
        const auto source=std::span<const std::byte>{input}.subspan(offset,std::min(frame_size,input.size()-offset));
        const auto sequence=offset/frame_size;
        const auto decoded=decode_lzss_position_distance_4m_frame_scratch(
            std::span<const std::byte>{archive}.subspan(cursor),{stream,limits,sequence,offset},oracle,raw);
        if(decoded.error!=LzssShortMatchFrameDecodeError::none||decoded.required_raw_size!=source.size()
            ||!std::equal(source.begin(),source.end(),raw.begin()))return 1;
        LzssPositionDistance4mEndProbeFinder finder;
        if(initialize_lzss_position_distance_4m_end_probe_finder(source,stream.dictionary,limits,workspace,finder)
            !=LzssShortPrefixError::none)return 1;
        std::size_t count{};
        for(std::size_t position=0;position<source.size();) {
            const auto match=finder.find_match(position);
            if(count>=tokens.size())return 1;
            auto& token=tokens[count++];token={};
            const auto step=match.length>=3?match.length:1U;
            if(match.length>=3)token={LzssTypedTokenKind::match,0,match.distance,match.length};
            else token.literal=std::to_integer<std::uint8_t>(source[position]);
            finder.advance(position,position+step);position+=step;
        }
        if(count!=decoded.required_token_count)return 1;
        for(std::size_t i=0;i<count;++i) {
            const auto& x=tokens[i];const auto& y=oracle[i];
            if(x.kind!=y.kind||x.literal!=y.literal||x.length!=y.length||x.distance!=y.distance)return 1;
        }
        const auto encoded=encode_lzss_position_distance_4m_frame(stream,limits,sequence,offset,
            std::span<const LzssTypedToken>{tokens}.first(count),operations,serialized);
        if(encoded.error!=LzssShortMatchFrameEncodeError::none||encoded.raw_size!=source.size()
            ||encoded.token_count!=count||encoded.serialized_size>archive.size()-cursor
            ||encoded.serialized_size!=decoded.serialized_consumed)return 1;
        if(!std::equal(serialized.begin(),serialized.begin()+encoded.serialized_size,archive.begin()+cursor))return 1;
        const auto c=finder.counters();
        if(c.overflow||c.invalid_find_calls||c.invalid_advance_calls||c.find_calls!=count
            ||c.advance_calls!=count||c.advanced_positions!=source.size()
            ||c.chain_visits[2]!=c.five_out_of_window+c.candidate_filter_comparisons
            ||c.candidate_filter_comparisons!=c.best_length_rejections+c.end_probe_rejections+c.five_prefix_rejections+c.extension_attempts
            ||c.extension_attempts!=c.improved_candidates+c.equal_candidates+c.shorter_candidates
            ||c.extension_attempts!=c.extension_limit_stops+c.extension_mismatch_stops
            ||c.extension_comparisons!=c.improving_extension_comparisons+c.nonimproving_extension_comparisons
            ||c.extension_equal_bytes!=c.improving_extension_equal_bytes+c.nonimproving_extension_equal_bytes
            ||c.maximum_length_updates!=c.extension_limit_stops||c.equal_candidates)return 1;
        reports.push_back({source.size(),count,encoded.operation_count,encoded.serialized_size,c});
        cursor+=encoded.serialized_size;
    }
    if(cursor!=archive.size())return 1;
    std::cout<<"verified=1\ninput_bytes="<<input.size()<<"\narchive_bytes="<<archive.size()
        <<"\nframes="<<reports.size()<<"\nquery_budget="<<budget
        <<"\nfinder_bytes="<<trial.workspace_size<<"\nfinder_state_bytes="<<sizeof(LzssPositionDistance4mEndProbeFinder)<<'\n';
    for(std::size_t i=0;i<reports.size();++i) {
        const auto& r=reports[i];const auto& c=r.counters;
        const auto prefix="frame_"+std::to_string(i)+"_";
        const auto field=[&](const std::string& name,std::uint64_t value) {
            std::cout<<prefix<<name<<'='<<value<<'\n';
        };
        field("raw_bytes",r.raw);field("tokens",r.tokens);field("operations",r.operations);
        field("serialized_bytes",r.serialized);field("initialized_words",c.initialized_words);
        field("find_calls",c.find_calls);field("advance_calls",c.advance_calls);
        field("advanced_positions",c.advanced_positions);
        field("fast_path_comparisons",c.fast_path_comparisons);
        field("candidate_filter_comparisons",c.candidate_filter_comparisons);
        field("extension_comparisons",c.extension_comparisons);
        field("extension_equal_bytes",c.extension_equal_bytes);
        field("end_probe_comparisons",c.end_probe_comparisons);
        field("end_probe_rejections",c.end_probe_rejections);
        field("five_out_of_window",c.five_out_of_window);
        field("best_length_rejections",c.best_length_rejections);
        field("five_prefix_rejections",c.five_prefix_rejections);
        field("extension_attempts",c.extension_attempts);
        field("improved_candidates",c.improved_candidates);
        field("equal_candidates",c.equal_candidates);
        field("shorter_candidates",c.shorter_candidates);
        field("improving_extension_comparisons",c.improving_extension_comparisons);
        field("improving_extension_equal_bytes",c.improving_extension_equal_bytes);
        field("nonimproving_extension_comparisons",c.nonimproving_extension_comparisons);
        field("nonimproving_extension_equal_bytes",c.nonimproving_extension_equal_bytes);
        field("extension_limit_stops",c.extension_limit_stops);
        field("extension_mismatch_stops",c.extension_mismatch_stops);
        field("maximum_length_updates",c.maximum_length_updates);
        for(std::size_t j=0;j<3;++j) {
            const auto suffix=std::to_string(j+3);
            field("chain_visits_"+suffix,c.chain_visits[j]);
            field("prefix_comparisons_"+suffix,c.prefix_comparisons[j]);
            field("insertions_"+suffix,c.insertions[j]);
        }
    }
}
