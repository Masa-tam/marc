#include <marc/marc.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

namespace {
struct Destroy { void operator()(marc_transform* p) const { marc_transform_destroy(p); } };
std::uint64_t peak() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    if (!GetProcessMemoryInfo(GetCurrentProcess(),&counters,sizeof(counters))) return 0;
    return counters.PeakWorkingSetSize;
#else
    rusage counters{};
    if (getrusage(RUSAGE_SELF,&counters)) return 0;
    return static_cast<std::uint64_t>(counters.ru_maxrss)*1024;
#endif
}
}
int main(int argc,char** argv) {
    if (argc!=5) return 2;
    const std::string profile=argv[1],mode=argv[2];
    if ((profile!="position" && profile!="contextual") || (mode!="encode" && mode!="decode")) return 2;
    const auto direction=mode=="encode" ? MARC_DIRECTION_ENCODE : MARC_DIRECTION_DECODE;
    const std::filesystem::path source_path=argv[3],target=argv[4],temporary=target.string()+".tmp";
    if (std::filesystem::exists(target) || std::filesystem::exists(temporary)) return 3;
    const auto size=std::filesystem::file_size(source_path);
    std::ifstream source(source_path,std::ios::binary);
    std::ofstream sink(temporary,std::ios::binary);
    if (!source || !sink) return 3;
    constexpr std::size_t capacity=65536;
    marc_transform* handle{};
    std::unique_ptr<std::uint8_t[]> primary,secondary;
    std::vector<std::max_align_t> views;
    std::uint64_t queried{};
    marc_status status{};
    const auto start=std::chrono::steady_clock::now();
    if (profile=="position") {
        marc_lzss_position_distance_rans_32m_config config{};
        status=marc_lzss_position_distance_rans_32m_config_init(direction,&config);
        if (status) return 4;
        config.original_size=size;config.external_retained_bytes=65536;
        config.input_capacity_bytes=config.output_capacity_bytes=capacity;
        marc_lzss_position_distance_rans_32m_resources resources{};
        status=marc_lzss_position_distance_rans_32m_resource_requirements(&config,&resources);
        if (status) return 4;
        queried=resources.minimum_aggregate_bytes;
        status=marc_lzss_position_distance_rans_32m_create(&config,&handle);
    } else {
        marc_lzss_contextual_rans_config config{};
        status=marc_lzss_contextual_rans_config_init(direction,&config);
        if (status) return 4;
        status=marc_lzss_contextual_rans_config_apply_profile(&config,MARC_LZSS_CONTEXTUAL_PROFILE_64M);
        if (status) return 4;
        config.frame_size=config.window_size=33554432;
        config.max_frame_size=config.max_lz_distance=33554432;
        config.max_block_size=8*UINT64_C(33554432);
        config.max_compressed_payload_size=16*UINT64_C(33554432)+8;
        config.original_size=size;
        marc_workspace_requirements resources{};
        status=marc_lzss_contextual_rans_workspace_requirements(&config,&resources);
        if (status || resources.views_alignment>alignof(std::max_align_t)) return 4;
        queried=resources.primary_bytes+resources.secondary_bytes+resources.views_bytes;
        primary.reset(new(std::nothrow) std::uint8_t[resources.primary_bytes]);
        secondary.reset(new(std::nothrow) std::uint8_t[resources.secondary_bytes]);
        views.resize((resources.views_bytes+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
        if (!primary || !secondary) return 4;
        status=marc_lzss_contextual_rans_create(&config,{primary.get(),resources.primary_bytes},
            {secondary.get(),resources.secondary_bytes},
            {reinterpret_cast<std::uint8_t*>(views.data()),resources.views_bytes},&handle);
    }
    std::unique_ptr<marc_transform,Destroy> transform(handle);
    if (status || !transform) return 4;
    std::array<std::uint8_t,capacity> input{},output{};
    std::size_t present=0,consumed=0;
    std::uint64_t read=0;
    bool done=false;
    for (;;) {
        if (consumed==present && read<size) {
            present=static_cast<std::size_t>(std::min<std::uint64_t>(capacity,size-read));
            if (!source.read(reinterpret_cast<char*>(input.data()),present)) return 5;
            read+=present;consumed=0;
        }
        const auto r=marc_transform_process(transform.get(),{input.data()+consumed,present-consumed},
            {output.data(),output.size()},read==size ? MARC_PROCESS_END_INPUT : 0);
        if (r.input_consumed>present-consumed || r.output_produced>output.size()
            || r.status>=100 || (r.status==MARC_STATUS_PROGRESS && !r.input_consumed && !r.output_produced)) return 6;
        consumed+=r.input_consumed;
        sink.write(reinterpret_cast<char*>(output.data()),r.output_produced);
        if (!sink) return 5;
        if (r.status==MARC_STATUS_END_OF_STREAM) { done=read==size && consumed==present;break; }
        if (!r.input_consumed && !r.output_produced
            && !(r.status==MARC_STATUS_NEED_INPUT && consumed==present && read<size)) return 6;
    }
    if (!done) return 6;
    transform.reset();sink.close();
    if (!sink) return 5;
    std::filesystem::rename(temporary,target);
    const auto resident=peak();if (!resident) return 7;
    const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"{\"verified\":true,\"seconds\":"<<seconds<<",\"peak_resident_bytes\":"<<resident
        <<",\"queried_bytes\":"<<queried<<",\"query_scope\":\""
        <<(profile=="position" ? "owner_calls_and_controls" : "caller_workspaces")<<"\"}\n";
}
