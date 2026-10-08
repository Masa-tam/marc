#include <marc/marc.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <new>
#include <string>
#include <vector>

namespace {
using Clock=std::chrono::steady_clock;
constexpr std::size_t chunk=65536;
double duration(Clock::time_point start) { return std::chrono::duration<double>(Clock::now()-start).count(); }
bool load(const char* path,std::vector<std::uint8_t>& out) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if (!file) return false;
    const auto n=file.tellg(); if (n<0 || n>128*1024*1024) return false;
    out.resize(static_cast<std::size_t>(n)); file.seekg(0);
    return !n || static_cast<bool>(file.read(reinterpret_cast<char*>(out.data()),n));
}
bool measure(bool position,marc_direction direction,const std::vector<std::uint8_t>& input,
    const std::vector<std::uint8_t>& expected,double& elapsed,std::uint64_t& retained) {
    marc_transform* transform{};
    std::unique_ptr<std::uint8_t[]> primary,secondary,views;
    marc_lzss_position_distance_rans_32m_config candidate{};
    marc_lzss_contextual_rans_config control{};
    marc_workspace_requirements workspace{};
    marc_lzss_position_distance_rans_32m_resources resources{};
    auto s=position ? marc_lzss_position_distance_rans_32m_config_init(direction,&candidate)
        : marc_lzss_contextual_rans_config_init(direction,&control);
    if (s!=MARC_STATUS_OK) return false;
    if (position) {
        candidate.original_size=input.size();
        s=marc_lzss_position_distance_rans_32m_resource_requirements(&candidate,&resources);
        retained=resources.minimum_aggregate_bytes;
    } else {
        s=marc_lzss_contextual_rans_config_apply_profile(&control,MARC_LZSS_CONTEXTUAL_PROFILE_64M);
        if (s!=MARC_STATUS_OK) return false;
        control.frame_size=control.window_size=33554432;
        control.max_frame_size=control.max_lz_distance=33554432;
        control.max_block_size=7*UINT64_C(33554432);
        control.max_compressed_payload_size=14*UINT64_C(33554432)+8;
        control.original_size=input.size();
        s=marc_lzss_contextual_rans_workspace_requirements(&control,&workspace);
        retained=workspace.primary_bytes+workspace.secondary_bytes+workspace.views_bytes;
    }
    if (s!=MARC_STATUS_OK) return false;
    auto start=Clock::now();
    if (position) s=marc_lzss_position_distance_rans_32m_create(&candidate,&transform);
    else {
        if (workspace.views_alignment>alignof(std::max_align_t)) return false;
        primary.reset(new(std::nothrow) std::uint8_t[workspace.primary_bytes]);
        secondary.reset(new(std::nothrow) std::uint8_t[workspace.secondary_bytes]);
        views.reset(new(std::nothrow) std::uint8_t[workspace.views_bytes]);
        if (!primary || !secondary || !views) return false;
        s=marc_lzss_contextual_rans_create(&control,{primary.get(),workspace.primary_bytes},
            {secondary.get(),workspace.secondary_bytes},{views.get(),workspace.views_bytes},&transform);
    }
    elapsed=duration(start); if (s!=MARC_STATUS_OK || !transform) return false;
    std::array<std::uint8_t,chunk> output{};
    std::size_t consumed{},produced{};
    bool good=false;
    for (std::size_t calls=0;calls<input.size()+expected.size()+1024;++calls) {
        const auto count=std::min(chunk,input.size()-consumed);
        const auto* pointer=input.empty() ? nullptr : input.data()+consumed;
        start=Clock::now();
        const auto r=marc_transform_process(transform,{pointer,count},{output.data(),output.size()},
            consumed+count==input.size() ? MARC_PROCESS_END_INPUT : 0);
        elapsed+=duration(start);
        if (r.input_consumed>count || r.output_produced>output.size() || r.status>=100
            || produced>expected.size() || r.output_produced>expected.size()-produced
            || !std::equal(output.begin(),output.begin()+r.output_produced,expected.begin()+produced)) break;
        consumed+=r.input_consumed; produced+=r.output_produced;
        if (r.status==MARC_STATUS_END_OF_STREAM) { good=consumed==input.size() && produced==expected.size(); break; }
        if (!r.input_consumed && !r.output_produced) break;
    }
    start=Clock::now(); marc_transform_destroy(transform);
    primary.reset(); secondary.reset(); views.reset();
    elapsed+=duration(start); return good;
}
}
int main(int argc,char** argv) {
    if (argc!=5) return 2;
    const std::string profile=argv[1],mode=argv[2];
    if ((profile!="position" && profile!="contextual") || (mode!="encode" && mode!="decode")) return 2;
    std::vector<std::uint8_t> input,expected;
    if (!load(argv[3],input) || !load(argv[4],expected)) return 2;
    std::array<double,3> samples{}; std::uint64_t retained{};
    for (int iteration=-1;iteration<3;++iteration) {
        double elapsed{};
        if (!measure(profile=="position",mode=="encode" ? MARC_DIRECTION_ENCODE : MARC_DIRECTION_DECODE,
            input,expected,elapsed,retained)) return 1;
        if (iteration>=0) samples[static_cast<std::size_t>(iteration)]=elapsed;
    }
    std::cout<<std::setprecision(12)<<"{\"verified\":true,\"query_scope\":\""
        <<(profile=="position" ? "owner_and_declared_calls" : "caller_workspaces")
        <<"\",\"queried_bytes\":"<<retained<<",\"seconds\":["
        <<samples[0]<<','<<samples[1]<<','<<samples[2]<<"]}\n"; return 0;
}
