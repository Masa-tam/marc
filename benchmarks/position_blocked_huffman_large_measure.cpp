#include <marc/marc.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

namespace {
using Clock=std::chrono::steady_clock;
constexpr std::size_t capacity=65536;
void check(bool ok,const char* what) { if (!ok) throw std::runtime_error(what); }
double elapsed(Clock::time_point start) { return std::chrono::duration<double>(Clock::now()-start).count(); }
std::uint64_t peak() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    check(GetProcessMemoryInfo(GetCurrentProcess(),&counters,sizeof(counters))!=0,"peak query");
    return counters.PeakWorkingSetSize;
#else
    rusage counters{}; check(getrusage(RUSAGE_SELF,&counters)==0,"peak query");
    return static_cast<std::uint64_t>(counters.ru_maxrss)*1024;
#endif
}
struct Destroy { void operator()(marc_transform* p) const { marc_transform_destroy(p); } };
template<class Config,class Resources,class Init,class Query,class Create>
marc_transform* position(marc_direction direction,std::uint64_t size,std::uint64_t& queried,
    Init init,Query query,Create create) {
    Config config{}; Resources resources{};
    check(init(direction,&config)==MARC_STATUS_OK,"position init");
    config.original_size=size; config.external_retained_bytes=65536;
    config.input_capacity_bytes=config.output_capacity_bytes=capacity;
    check(query(&config,&resources)==MARC_STATUS_OK,"position query");
    queried=resources.minimum_aggregate_bytes;
    marc_transform* handle{};
    check(create(&config,&handle)==MARC_STATUS_OK && handle,"position create");
    return handle;
}
}

int main(int argc,char** argv) {
    try {
        if (argc!=6) return 2;
        const auto window=std::stoul(argv[1]);
        check(window==16 || window==32 || window==64,"window");
        const auto extent=static_cast<std::uint32_t>(window*1048576);
        const std::string profile=argv[2],mode=argv[3];
        check(profile=="position" || profile=="contextual","profile");
        check(mode=="encode" || mode=="decode","mode");
        const auto direction=mode=="encode" ? MARC_DIRECTION_ENCODE : MARC_DIRECTION_DECODE;
        const std::filesystem::path source_path=argv[4],target=argv[5],temporary=target.string()+".tmp";
        check(!std::filesystem::exists(target) && !std::filesystem::exists(temporary),"existing target");
        const auto size=std::filesystem::file_size(source_path);
        std::ifstream source(source_path,std::ios::binary);
        std::ofstream sink(temporary,std::ios::binary);
        check(source && sink,"file open");
        std::unique_ptr<std::uint8_t[]> primary,secondary;
        std::vector<std::max_align_t> views;
        std::uint64_t queried{},profile_id{},min_match{},max_block{},max_payload{},budget{};
        marc_transform* handle{};
        auto start=Clock::now();
        if (profile=="position") {
#define POSITION_CASE(N) case N: handle=position<marc_lzss_position_distance_blocked_huffman_##N##m_config,marc_lzss_position_distance_blocked_huffman_##N##m_resources>(direction,size,queried,marc_lzss_position_distance_blocked_huffman_##N##m_config_init,marc_lzss_position_distance_blocked_huffman_##N##m_resource_requirements,marc_lzss_position_distance_blocked_huffman_##N##m_create); break
            switch (window) { POSITION_CASE(16); POSITION_CASE(32); POSITION_CASE(64); }
#undef POSITION_CASE
            min_match=3;
        } else {
            marc_lzss_contextual_blocked_huffman_config config{};
            check(marc_lzss_contextual_blocked_huffman_config_init(direction,&config)==MARC_STATUS_OK,"context init");
            check(marc_lzss_contextual_blocked_huffman_config_apply_profile(&config,
                window==16 ? MARC_LZSS_CONTEXTUAL_PROFILE_16M : MARC_LZSS_CONTEXTUAL_PROFILE_64M)==MARC_STATUS_OK,"context profile");
            // The 64-MiB format allows the explicitly requested 32-MiB window/frame.
            config.frame_size=config.window_size=extent;
            config.max_frame_size=config.max_lz_distance=extent;
            config.max_block_size=static_cast<std::uint64_t>(extent)*(window==16 ? 7 : 8);
            config.max_compressed_payload_size=static_cast<std::uint64_t>(extent)*(window==16 ? 105 : 120)/8;
            config.original_size=size;
            profile_id=config.profile; min_match=config.min_match_length;
            max_block=config.max_block_size; max_payload=config.max_compressed_payload_size;
            budget=config.max_internal_buffered_bytes;
            marc_workspace_requirements r{};
            const auto status=marc_lzss_contextual_blocked_huffman_workspace_requirements(&config,&r);
            if (status!=MARC_STATUS_OK) {
                std::cout<<"{\"configuration_accepted\":false,\"query_status\":"<<status
                    <<",\"frame_size\":"<<extent<<",\"window_size\":"<<extent
                    <<",\"profile_id\":"<<profile_id<<"}\n";
                return 4;
            }
            check(r.views_alignment<=alignof(std::max_align_t),"views alignment");
            queried=r.primary_bytes+r.secondary_bytes+r.views_bytes;
            primary.reset(new std::uint8_t[r.primary_bytes]);
            secondary.reset(new std::uint8_t[r.secondary_bytes]);
            views.resize((r.views_bytes+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
            check(marc_lzss_contextual_blocked_huffman_create(&config,{primary.get(),r.primary_bytes},
                {secondary.get(),r.secondary_bytes},{reinterpret_cast<std::uint8_t*>(views.data()),r.views_bytes},
                &handle)==MARC_STATUS_OK && handle,"context create");
        }
        double codec_seconds=elapsed(start);
        std::unique_ptr<marc_transform,Destroy> transform(handle);
        std::array<std::uint8_t,capacity> input{},output{};
        std::size_t present{},consumed{};
        std::uint64_t read{},produced{};
        for (;;) {
            if (consumed==present && read<size) {
                present=static_cast<std::size_t>(std::min<std::uint64_t>(capacity,size-read));
                check(static_cast<bool>(source.read(reinterpret_cast<char*>(input.data()),present)),"read");
                read+=present; consumed=0;
            }
            start=Clock::now();
            const auto r=marc_transform_process(transform.get(),{input.data()+consumed,present-consumed},
                {output.data(),output.size()},read==size ? MARC_PROCESS_END_INPUT : 0);
            codec_seconds+=elapsed(start);
            check(r.input_consumed<=present-consumed && r.output_produced<=output.size(),"counts");
            check(r.status<100,"process error");
            check(r.status!=MARC_STATUS_PROGRESS || r.input_consumed || r.output_produced,"no progress");
            consumed+=r.input_consumed; produced+=r.output_produced;
            sink.write(reinterpret_cast<char*>(output.data()),r.output_produced);
            check(static_cast<bool>(sink),"write");
            if (r.status==MARC_STATUS_END_OF_STREAM) { check(read==size && consumed==present,"completion"); break; }
            check(r.input_consumed || r.output_produced
                || (r.status==MARC_STATUS_NEED_INPUT && consumed==present && read<size),"stalled");
        }
        start=Clock::now(); transform.reset(); primary.reset(); secondary.reset();
        std::vector<std::max_align_t>().swap(views);
        codec_seconds+=elapsed(start);
        sink.close(); check(static_cast<bool>(sink),"close");
        std::filesystem::rename(temporary,target);
        std::cout<<std::setprecision(12)<<"{\"configuration_accepted\":true,\"transform_completed\":true"
            <<",\"frame_size\":"<<extent<<",\"window_size\":"<<extent<<",\"profile_id\":"<<profile_id
            <<",\"min_match_length\":"<<min_match<<",\"max_block_size\":"<<max_block
            <<",\"max_payload_size\":"<<max_payload<<",\"contextual_budget\":"<<budget
            <<",\"input_bytes\":"<<size<<",\"output_bytes\":"<<produced
            <<",\"codec_seconds\":"<<codec_seconds<<",\"peak_resident_bytes\":"<<peak()
            <<",\"queried_bytes\":"<<queried
            <<",\"timer_scope\":\"allocation_create_process_destroy_excludes_file_io\""
            <<",\"peak_scope\":\"fresh_process_including_streaming_driver\",\"io_bytes\":131072}\n";
        return 0;
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 8; }
}
