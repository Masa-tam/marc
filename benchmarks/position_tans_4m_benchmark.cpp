#include <marc/marc.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <vector>

namespace {
using Clock=std::chrono::steady_clock;
constexpr std::size_t chunk=65536;
bool load(const char* path,std::vector<std::uint8_t>& out) {
    std::ifstream file(path,std::ios::binary|std::ios::ate); if (!file) return false;
    const auto n=file.tellg(); if (n<0 || n>256*1024*1024) return false;
    out.resize(static_cast<std::size_t>(n)); file.seekg(0);
    return n==0 || static_cast<bool>(file.read(reinterpret_cast<char*>(out.data()),n));
}
double seconds(Clock::time_point t) { return std::chrono::duration<double>(Clock::now()-t).count(); }
bool run(bool position,marc_direction direction,const std::vector<std::uint8_t>& input,
         const std::vector<std::uint8_t>& expected,double& elapsed,std::uint64_t& queried) {
    marc_transform* raw{};
    std::unique_ptr<std::uint8_t[]> primary,secondary;
    std::vector<std::max_align_t> views;
    marc_status status{};
    marc_lzss_position_distance_tans_4m_config pc{};
    marc_lzss_position_distance_tans_4m_resources pr{};
    marc_lzss_contextual_tans_config cc{};
    marc_workspace_requirements cr{};
    if (position) {
        status=marc_lzss_position_distance_tans_4m_config_init(direction,&pc);
        if (status!=MARC_STATUS_OK) return false;
        pc.original_size=direction==MARC_DIRECTION_ENCODE ? input.size() : expected.size();
        pc.external_retained_bytes=65536+input.capacity()+expected.capacity();
        pc.max_internal_buffered_bytes+=pc.external_retained_bytes;
        status=marc_lzss_position_distance_tans_4m_resource_requirements(&pc,&pr);
        queried=pr.minimum_aggregate_bytes;
    } else {
        status=marc_lzss_contextual_tans_config_init(direction,&cc);
        if (status!=MARC_STATUS_OK) return false;
        status=marc_lzss_contextual_tans_config_apply_profile(&cc,MARC_LZSS_CONTEXTUAL_PROFILE_4M);
        if (status!=MARC_STATUS_OK || cc.frame_size!=4194304 || cc.window_size!=4194304) return false;
        cc.original_size=direction==MARC_DIRECTION_ENCODE ? input.size() : expected.size();
        status=marc_lzss_contextual_tans_workspace_requirements(&cc,&cr);
        queried=cr.primary_bytes+cr.secondary_bytes+cr.views_bytes;
    }
    if (status!=MARC_STATUS_OK) return false;
    auto start=Clock::now();
    if (position) status=marc_lzss_position_distance_tans_4m_create(&pc,&raw);
    else {
        if (cr.views_alignment>alignof(std::max_align_t)) return false;
        primary.reset(new(std::nothrow) std::uint8_t[cr.primary_bytes]);
        secondary.reset(new(std::nothrow) std::uint8_t[cr.secondary_bytes]);
        views.resize((cr.views_bytes+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
        if (!primary || !secondary) return false;
        status=marc_lzss_contextual_tans_create(&cc,{primary.get(),cr.primary_bytes},
            {secondary.get(),cr.secondary_bytes},
            {reinterpret_cast<std::uint8_t*>(views.data()),cr.views_bytes},&raw);
    }
    elapsed=seconds(start); if (status!=MARC_STATUS_OK || !raw) return false;
    std::unique_ptr<marc_transform,void(*)(marc_transform*)> transform(raw,marc_transform_destroy);
    std::array<std::uint8_t,chunk> output{}; std::size_t consumed{},produced{};
    for (std::size_t calls=0;calls<input.size()+expected.size()+1024;++calls) {
        const auto count=std::min(chunk,input.size()-consumed);
        const auto* ptr=input.empty()?nullptr:input.data()+consumed;
        start=Clock::now(); const auto r=marc_transform_process(transform.get(),{ptr,count},
            {output.data(),output.size()},consumed+count==input.size()?MARC_PROCESS_END_INPUT:0);
        elapsed+=seconds(start);
        if (r.input_consumed>count || r.output_produced>output.size() || r.status>=MARC_STATUS_INVALID_ARGUMENT
            || produced>expected.size() || r.output_produced>expected.size()-produced
            || !std::equal(output.begin(),output.begin()+r.output_produced,expected.begin()+produced)) return false;
        consumed+=r.input_consumed; produced+=r.output_produced;
        if (r.status==MARC_STATUS_END_OF_STREAM) return consumed==input.size() && produced==expected.size();
        if (!r.input_consumed && !r.output_produced) return false;
    }
    return false;
}
}
int main(int argc,char** argv) {
    if (argc!=5) return 2;
    const std::string profile=argv[1], mode=argv[2];
    if ((profile!="position" && profile!="contextual") || (mode!="encode" && mode!="decode")) return 2;
    std::vector<std::uint8_t> input,expected; if (!load(argv[3],input)||!load(argv[4],expected)) return 2;
    std::array<double,3> samples{}; std::uint64_t queried{};
    for (int iteration=-1;iteration<3;++iteration) {
        double elapsed{}; if (!run(profile=="position",mode=="encode"?MARC_DIRECTION_ENCODE:MARC_DIRECTION_DECODE,input,expected,elapsed,queried)) return 1;
        if (iteration>=0) samples[static_cast<std::size_t>(iteration)]=elapsed;
    }
    std::cout<<std::setprecision(12)<<"{\"verified\":true,\"profile\":\""<<profile
        <<"\",\"mode\":\""<<mode<<"\",\"frame_size\":4194304,\"window_size\":4194304"
        <<",\"warmups\":1,\"queried_bytes\":"<<queried<<",\"seconds\":["
        <<samples[0]<<','<<samples[1]<<','<<samples[2]<<"]}\n";
}
