#include "frame/lzss_position_distance_4m_owned_encoder.hpp"
#include "frame/lzss_position_distance_4m_owned_decoder.hpp"
#include "marc/marc.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#endif

namespace {
using namespace marc::frame::internal;
using Clock=std::chrono::steady_clock;
constexpr std::size_t chunk=65536,frame=4194304,archive_limit=128U*1024U*1024U;
enum class Profile { reference4m, position1m, contextual4m };
marc::core::DecoderLimits limits() {
    marc::core::DecoderLimits l{};l.max_block_size=frame;
    l.max_compressed_payload_size=75497477;l.max_internal_buffered_bytes=512U*1024U*1024U;return l;
}
TypedContextStreamHeader stream(std::size_t size) {return {frame,size,{frame,3,258,0},32768,46,10,1,11};}
struct Requirements {std::size_t primary{},secondary{},views{},budget{};};
struct Engine {
    Profile profile;bool encode;std::size_t original;
    std::unique_ptr<LzssPositionDistance4mOwnedEncoder> encoder;
    std::unique_ptr<LzssPositionDistance4mOwnedDecoder> decoder;
    std::unique_ptr<std::uint8_t[]> primary,secondary,views;
    marc_transform* handle{};
    marc_lzss_position_distance_dynamic_range_1m_config position{};
    marc_lzss_contextual_dynamic_range_config contextual{};
    Engine(Profile p,bool e,std::size_t n):profile(p),encode(e),original(n) {
        if(p==Profile::position1m) {
            marc_lzss_position_distance_dynamic_range_1m_config_init(e?MARC_DIRECTION_ENCODE:MARC_DIRECTION_DECODE,&position);
            position.original_size=n;position.max_internal_buffered_bytes=512U*1024U*1024U;
        } else if(p==Profile::contextual4m) {
            marc_lzss_contextual_dynamic_range_config_init(e?MARC_DIRECTION_ENCODE:MARC_DIRECTION_DECODE,&contextual);
            marc_lzss_contextual_dynamic_range_config_apply_profile(&contextual,MARC_LZSS_CONTEXTUAL_PROFILE_4M);
            contextual.original_size=n;contextual.max_internal_buffered_bytes=512U*1024U*1024U;
        }
    }
    bool query(std::size_t budget,Requirements& out) {
        if(profile==Profile::reference4m) {
            auto l=limits();l.max_internal_buffered_bytes=budget;
            if(encode) {
                LzssPositionDistanceWorkspaceRequirements r{};
                if(LzssPositionDistance4mOwnedEncoder::requirements(stream(original),l,r)!=marc::core::ErrorCode::none)return false;
                out={r.raw_bytes,r.serialized_bytes,r.views_bytes,r.aggregate_bytes};
            } else {
                LzssPositionDistance4mDecodeWorkspace r{};
                if(LzssPositionDistance4mOwnedDecoder::requirements(frame,l,r)!=marc::core::ErrorCode::none)return false;
                out={r.serialized_bytes,r.raw_bytes,r.token_bytes,r.aggregate_bytes};
            }
            return true;
        }
        marc_workspace_requirements r{};marc_status status{};
        if(profile==Profile::position1m) {
            position.max_internal_buffered_bytes=budget;
            status=marc_lzss_position_distance_dynamic_range_1m_workspace_requirements(&position,&r);
        } else {
            contextual.max_internal_buffered_bytes=budget;
            status=marc_lzss_contextual_dynamic_range_workspace_requirements(&contextual,&r);
        }
        if(status!=MARC_STATUS_OK)return false;
        out={r.primary_bytes,r.secondary_bytes,r.views_bytes,budget};return true;
    }
    bool create() {
        if(profile==Profile::reference4m) {
            marc::core::ErrorCode error{};
            if(encode)encoder=LzssPositionDistance4mOwnedEncoder::create(stream(original),limits(),error);
            else decoder=LzssPositionDistance4mOwnedDecoder::create(frame,limits(),error);
            return error==marc::core::ErrorCode::none&&(encoder||decoder);
        }
        Requirements r{};if(!query(512U*1024U*1024U,r))return false;
        primary.reset(new(std::nothrow) std::uint8_t[r.primary]);
        secondary.reset(new(std::nothrow) std::uint8_t[r.secondary]);
        views.reset(new(std::nothrow) std::uint8_t[r.views]);
        if(!primary||!secondary||!views)return false;
        const marc_buffer a{primary.get(),r.primary},b{secondary.get(),r.secondary},c{views.get(),r.views};
        const auto status=profile==Profile::position1m
            ?marc_lzss_position_distance_dynamic_range_1m_create(&position,a,b,c,&handle)
            :marc_lzss_contextual_dynamic_range_create(&contextual,a,b,c,&handle);
        return status==MARC_STATUS_OK&&handle;
    }
    struct Result {std::size_t consumed{},produced{};bool ended{},valid{};};
    Result process(std::span<const std::byte> in,std::span<std::byte> out,bool end) {
        if(profile==Profile::reference4m) {
            auto* t=encode?static_cast<marc::core::Transform*>(encoder.get()):static_cast<marc::core::Transform*>(decoder.get());
            const auto r=t->process(in,out,end?MARC_PROCESS_END_INPUT:0);
            return {r.input_consumed,r.output_produced,r.status==marc::core::StreamStatus::end_of_stream,
                marc::core::is_valid(r,in.size(),out.size())&&r.status!=marc::core::StreamStatus::error};
        }
        const auto r=marc_transform_process(handle,{reinterpret_cast<const std::uint8_t*>(in.data()),in.size()},
            {reinterpret_cast<std::uint8_t*>(out.data()),out.size()},end?MARC_PROCESS_END_INPUT:0);
        return {r.input_consumed,r.output_produced,r.status==MARC_STATUS_END_OF_STREAM,
            r.input_consumed<=in.size()&&r.output_produced<=out.size()
            &&r.status>=MARC_STATUS_PROGRESS&&r.status<=MARC_STATUS_END_OF_STREAM
            &&(r.status!=MARC_STATUS_PROGRESS||r.input_consumed||r.output_produced)};
    }
    void clear(){encoder.reset();decoder.reset();if(handle)marc_transform_destroy(handle);handle=nullptr;primary.reset();secondary.reset();views.reset();}
    ~Engine(){clear();}
};
struct Memory {std::size_t working{},peak_working{},peak_commit{};bool available{};};
Memory memory() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX p{};
    if(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&p),sizeof(p)))
        return {p.WorkingSetSize,p.PeakWorkingSetSize,p.PeakPagefileUsage,true};
#endif
    return {};
}
bool read(const char* path,std::vector<std::byte>& bytes,std::size_t maximum) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file)return false;
    const auto n=file.tellg();if(n<0||static_cast<std::uint64_t>(n)>maximum)return false;
    bytes.resize(static_cast<std::size_t>(n));file.seekg(0);
    return n==0||static_cast<bool>(file.read(reinterpret_cast<char*>(bytes.data()),n));
}
// Sink copies/comparisons stay outside all timed intervals.
bool run(Engine& engine,std::span<const std::byte> source,std::span<const std::byte> expected,
    std::vector<std::byte>* capture,bool timed,double& elapsed) {
    std::array<std::byte,chunk> out{};std::size_t consumed{},produced{};
    const auto measure=[&](auto operation){
        const auto before=timed?Clock::now():Clock::time_point{};
        const auto result=operation();
        if(timed)elapsed+=std::chrono::duration<double>(Clock::now()-before).count();return result;
    };
    if(!measure([&]{return engine.create();}))return false;
    for(std::size_t calls=0;calls<source.size()+archive_limit+1024;++calls) {
        const auto in=source.subspan(consumed,std::min(chunk,source.size()-consumed));
        const auto r=measure([&]{return engine.process(in,out,consumed+in.size()==source.size());});
        if(!r.valid||r.produced>archive_limit-produced)return false;
        if(capture)capture->insert(capture->end(),out.begin(),out.begin()+r.produced);
        else if(produced>expected.size()||r.produced>expected.size()-produced
            ||!std::equal(out.begin(),out.begin()+r.produced,expected.begin()+produced))return false;
        consumed+=r.consumed;produced+=r.produced;
        if(r.ended) {
            measure([&]{engine.clear();return true;});
            return consumed==source.size()&&(capture||produced==expected.size());
        }
    }
    return false;
}
}
int main(int argc,char** argv) {
    if(argc!=5)return 2;
    const std::string_view profile_name=argv[2],mode=argv[3];
    Profile profile{};
    if(profile_name=="position4m-reference")profile=Profile::reference4m;
    else if(profile_name=="position1m-public")profile=Profile::position1m;
    else if(profile_name=="contextual4m-public")profile=Profile::contextual4m;
    else return 2;
    if(mode!="verify"&&mode!="encode"&&mode!="decode")return 2;
    std::vector<std::byte> input,archive;
    if(!read(argv[1],input,64U*1024U*1024U))return 2;
    const bool encode=mode!="decode";
    if(mode!="verify"&&!read(argv[4],archive,archive_limit))return 2;
    Engine engine(profile,encode,input.size());Requirements required{};
    if(!engine.query(512U*1024U*1024U,required))return 1;
    std::size_t low=1,high=512U*1024U*1024U;
    while(low<high){const auto mid=low+(high-low)/2;Requirements ignored{};if(engine.query(mid,ignored))high=mid;else low=mid+1;}
    if(!engine.query(512U*1024U*1024U,required))return 1;
    const auto before=memory();double elapsed{};
    if(!run(engine,encode?std::span<const std::byte>{input}:std::span<const std::byte>{archive},
        encode?std::span<const std::byte>{archive}:std::span<const std::byte>{input},mode=="verify"?&archive:nullptr,
        mode!="verify",elapsed))return 1;
    const auto after=memory();
    if(mode=="verify") {
        Engine decoder(profile,false,input.size());double ignored{};
        if(!run(decoder,archive,input,nullptr,false,ignored))return 1;
        std::ofstream file(argv[4],std::ios::binary);if(!file)return 2;
        if(!archive.empty())file.write(reinterpret_cast<const char*>(archive.data()),archive.size());if(!file)return 2;
    }
    std::cout<<std::setprecision(12)<<"profile="<<profile_name<<"\nmode="<<mode<<"\ninput_bytes="<<input.size()
        <<"\narchive_bytes="<<archive.size()<<"\nverified=1\ninput_chunk=65536\noutput_chunk=65536\nseconds="<<elapsed
        <<"\nprimary_bytes="<<required.primary<<"\nsecondary_bytes="<<required.secondary<<"\nviews_bytes="<<required.views
        <<"\nminimum_query_budget="<<low<<"\nmemory_available="<<after.available
        <<"\npre_codec_working_set_bytes="<<before.working<<"\nprocess_peak_working_set_bytes="<<after.peak_working
        <<"\nprocess_peak_pagefile_bytes="<<after.peak_commit<<'\n';
}
