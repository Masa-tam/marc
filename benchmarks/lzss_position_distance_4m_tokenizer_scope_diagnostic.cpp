#include "lzss_position_distance_4m_tokenizer_scope.hpp"
#include <chrono>
#include <cmath>
#include <iomanip>
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
using marc::dictionary::internal::TokenizerScopeSample;
struct Report {
    std::size_t raw{},tokens{},operations{},serialized{};
    TokenizerScopeSample sample{};double outer_seconds{};
};
}

// Private diagnostic: verify disables clocks; outer measures the tokenizer call;
// timed also enables inner clocks and perturbs each finder call.
// No public codec selection or report publication before complete verification.
// Frozen frames independently supply both token and restored-byte oracles.
int main(int argc,char** argv) {
    if(argc!=4)return 2;
    const std::string mode=argv[3];if(mode!="verify"&&mode!="outer"&&mode!="timed")return 2;
    const bool timed=mode=="timed";
    const bool outer_timed=mode!="verify";
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
    const auto trial=calculate_lzss_position_distance_4m_five_prefix_workspace(frame_size,stream.dictionary,limits);
    if(trial.error!=LzssShortPrefixError::none)return 1;
    const auto view_bytes=required.views_bytes-required.finder_bytes+trial.workspace_size;
    std::size_t budget{};
    if(charge_lzss_position_distance_4m_encode_workspace(limits,direction,0,
        required.raw_bytes,required.serialized_bytes,view_bytes,budget)
        !=LzssPositionDistanceWorkspaceError::none
        ||!marc::core::checked_add(budget,sizeof(LzssPositionDistance4mFivePrefixFinder)+tokenizer_scope_transient_state_bytes,budget)
        ||budget>limits.max_internal_buffered_bytes)return 1;
    // File buffers, frozen token oracle and reports are bounded harness overhead.
    // Codec buffers, admitted finder and staged diagnostic state are charged.
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
        TokenizerScopeSample sample{};
        using Clock=std::chrono::steady_clock;
        const auto before=outer_timed?Clock::now():Clock::time_point{};
        const auto tokenized=tokenize_lzss_position_distance_4m_tokenizer_scope(
            source,stream.dictionary,limits,3,tokens,workspace,sample,timed);
        const auto outer=outer_timed?std::chrono::duration<double>(Clock::now()-before).count():0.0;
        if(tokenized.error!=LzssShortMatchCandidateError::none)return 1;
        const auto count=tokenized.token_count;
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
        if(sample.timed!=timed||sample.raw_bytes!=source.size()||sample.token_count!=count
            ||sample.parse_passes!=1||sample.find_calls!=count||sample.advance_calls!=count
            ||sample.advanced_positions!=source.size())return 1;
        if(!std::isfinite(sample.initialize_seconds)||!std::isfinite(sample.find_seconds)
            ||!std::isfinite(sample.advance_seconds)||sample.initialize_seconds<0
            ||sample.find_seconds<0||sample.advance_seconds<0)return 1;
        const auto inner=sample.initialize_seconds+sample.find_seconds+sample.advance_seconds;
        if(!std::isfinite(outer)||outer<0||inner>outer+std::max(1e-8,outer*1e-9))return 1;
        if((!timed&&inner!=0)||(!outer_timed&&outer!=0))return 1;
        reports.push_back({source.size(),count,encoded.operation_count,encoded.serialized_size,sample,outer});
        cursor+=encoded.serialized_size;
    }
    if(cursor!=archive.size())return 1;
    std::cout<<std::setprecision(12)<<"verified=1\ninput_bytes="<<input.size()<<"\narchive_bytes="<<archive.size()
        <<"\nframes="<<reports.size()<<"\ncharged_workspace_budget="<<budget
        <<"\nfinder_bytes="<<trial.workspace_size
        <<"\ndiagnostic_transient_bytes="<<tokenizer_scope_transient_state_bytes
        <<"\nmode="<<mode<<"\nouter_timed="<<outer_timed<<"\ntimed="<<timed<<'\n';
    for(std::size_t i=0;i<reports.size();++i) {
        const auto& r=reports[i];const auto& c=r.sample;
        const auto prefix="frame_"+std::to_string(i)+"_";
        const auto field=[&](const std::string& name,auto value) {std::cout<<prefix<<name<<'='<<value<<'\n';};
        field("raw_bytes",r.raw);field("tokens",r.tokens);field("operations",r.operations);
        field("serialized_bytes",r.serialized);field("parse_passes",c.parse_passes);
        field("find_calls",c.find_calls);field("advance_calls",c.advance_calls);
        field("advanced_positions",c.advanced_positions);field("initialize_seconds",c.initialize_seconds);
        field("find_seconds",c.find_seconds);field("advance_seconds",c.advance_seconds);
        field("outer_seconds",r.outer_seconds);
    }
}
