#include <marc/marc.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
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
#include <cerrno>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
namespace {
using Clock=std::chrono::steady_clock;
void check(bool b,const char* what) {if (!b) throw std::runtime_error(what);}
double seconds(Clock::time_point start) {return std::chrono::duration<double>(Clock::now()-start).count();}
struct Destroy {void operator()(marc_transform* p) const noexcept {marc_transform_destroy(p);}};
std::uint64_t peak() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS c{};check(GetProcessMemoryInfo(GetCurrentProcess(),&c,sizeof(c))!=0,"peak query");return c.PeakWorkingSetSize;
#else
    rusage c{};check(getrusage(RUSAGE_SELF,&c)==0,"peak query");return static_cast<std::uint64_t>(c.ru_maxrss)*1024;
#endif
}
struct Selection {std::string label;std::uint32_t extent,position,contextual;};
Selection select(const std::string& label) {
    const std::array<Selection,7> profiles{{
        {"64k",65536,MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_64K,MARC_LZSS_CONTEXTUAL_PROFILE_64K},
        {"1m",1048576,MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_1M,MARC_LZSS_CONTEXTUAL_PROFILE_1M},
        {"4m",4194304,MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_4M,MARC_LZSS_CONTEXTUAL_PROFILE_4M},
        {"8m",8388608,MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_8M,MARC_LZSS_CONTEXTUAL_PROFILE_16M},
        {"16m",16777216,MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_16M,MARC_LZSS_CONTEXTUAL_PROFILE_16M},
        {"32m",33554432,MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_32M,MARC_LZSS_CONTEXTUAL_PROFILE_64M},
        {"64m",67108864,MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_64M,MARC_LZSS_CONTEXTUAL_PROFILE_64M}}};
    for (const auto& p:profiles) if (p.label==label) return p;
    throw std::runtime_error("unknown window");
}
int run(int argc,char** argv) {
    check(argc==6,"usage: window position|contextual encode|decode source target");
    const auto selected=select(argv[1]);const std::string profile=argv[2],mode=argv[3];
    check(profile=="position" || profile=="contextual","profile");check(mode=="encode" || mode=="decode","mode");
    const auto direction=mode=="encode" ? MARC_DIRECTION_ENCODE : MARC_DIRECTION_DECODE;
    const std::filesystem::path source_path=argv[4],target=argv[5],temporary=target.string()+".tmp";
    check(!std::filesystem::exists(target) && !std::filesystem::exists(temporary),"existing output");
    const auto size=std::filesystem::file_size(source_path);std::ifstream source(source_path,std::ios::binary);
    check(static_cast<bool>(source),"open input");
    std::unique_ptr<std::uint8_t[]> primary,secondary;std::vector<std::max_align_t> views;
    marc_transform* handle{};std::uint64_t queried{},allocated{},budget{},max_block{},max_payload{};
    double codec_seconds{};const auto wall_start=Clock::now();auto start=Clock::now();marc_status status{};
    if (profile=="position") {
        marc_lzss_position_distance_adaptive_huffman_config c{};
        check(marc_lzss_position_distance_adaptive_huffman_config_init(direction,&c)==MARC_STATUS_OK,"position init");
        status=marc_lzss_position_distance_adaptive_huffman_config_apply_profile(&c,selected.position);
        if (status!=MARC_STATUS_OK) {
            std::cout<<"{\"configuration_accepted\":false,\"profile\":\"position\",\"apply_status\":"<<status<<"}\n";return 4;
        }
        check(c.profile==selected.position && c.frame_size==selected.extent,"position selection");
        c.original_size=direction==MARC_DIRECTION_ENCODE ? size : 0;c.external_retained_bytes=65536;
        c.input_capacity_bytes=c.output_capacity_bytes=65536;budget=c.max_internal_buffered_bytes;
        max_block=c.max_block_size;max_payload=c.max_compressed_payload_size;
        marc_lzss_position_distance_adaptive_huffman_resources r{};
        check(marc_lzss_position_distance_adaptive_huffman_resource_requirements(&c,&r)==MARC_STATUS_OK,"position query");
        queried=r.minimum_aggregate_bytes;allocated=r.owned_buffer_bytes;
        status=marc_lzss_position_distance_adaptive_huffman_create(&c,&handle);
    } else {
        marc_lzss_contextual_adaptive_huffman_config c{};
        check(marc_lzss_contextual_adaptive_huffman_config_init(direction,&c)==MARC_STATUS_OK,"context init");
        check(marc_lzss_contextual_adaptive_huffman_config_apply_profile(&c,selected.contextual)==MARC_STATUS_OK,"context profile");
        // For8/32MiB explicitly test the larger public identity with matching
        // actual window/frame. A query refusal is evidence, not a comparison.
        c.frame_size=c.window_size=selected.extent;c.max_frame_size=c.max_lz_distance=selected.extent;
        c.max_block_size=selected.extent;c.max_compressed_payload_size=(UINT64_C(267)*selected.extent+7)/8;
        c.original_size=direction==MARC_DIRECTION_ENCODE ? size : 0;
        budget=c.max_internal_buffered_bytes;max_block=c.max_block_size;max_payload=c.max_compressed_payload_size;
        marc_workspace_requirements r{};status=marc_lzss_contextual_adaptive_huffman_workspace_requirements(&c,&r);
        if (status!=MARC_STATUS_OK) {
            std::cout<<"{\"configuration_accepted\":false,\"profile\":\"contextual\",\"query_status\":"<<status
                <<",\"contextual_profile\":"<<c.profile<<",\"window_size\":"<<c.window_size<<",\"frame_size\":"<<c.frame_size
                <<",\"max_block_size\":"<<max_block<<",\"max_compressed_payload_size\":"<<max_payload
                <<",\"configured_internal_bytes\":"<<budget<<"}\n";return 4;
        }
        check(r.views_alignment<=alignof(std::max_align_t),"alignment");queried=r.primary_bytes+r.secondary_bytes+r.views_bytes;
        primary.reset(new std::uint8_t[r.primary_bytes]);secondary.reset(new std::uint8_t[r.secondary_bytes]);
        views.resize((r.views_bytes+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
        allocated=r.primary_bytes+r.secondary_bytes+views.size()*sizeof(std::max_align_t);
        status=marc_lzss_contextual_adaptive_huffman_create(&c,{primary.get(),r.primary_bytes},{secondary.get(),r.secondary_bytes},
            {reinterpret_cast<std::uint8_t*>(views.data()),r.views_bytes},&handle);
    }
    std::unique_ptr<marc_transform,Destroy> transform(handle);check(status==MARC_STATUS_OK && transform,"create transform");
    codec_seconds+=seconds(start);std::ofstream sink(temporary,std::ios::binary);check(static_cast<bool>(sink),"open output");
    std::array<std::uint8_t,65536> input{},output{};std::size_t present{},used{};std::uint64_t read{},written{};
    for (;;) {
        if (present==used && read<size) {
            present=static_cast<std::size_t>(std::min<std::uint64_t>(input.size(),size-read));used=0;
            check(static_cast<bool>(source.read(reinterpret_cast<char*>(input.data()),present)),"read input");read+=present;
        }
        start=Clock::now();const auto r=marc_transform_process(transform.get(),{input.data()+used,present-used},{output.data(),output.size()},
            read==size ? MARC_PROCESS_END_INPUT : 0);codec_seconds+=seconds(start);
        check(r.status<MARC_STATUS_INVALID_ARGUMENT && r.input_consumed<=present-used && r.output_produced<=output.size(),"process");
        check(r.status!=MARC_STATUS_PROGRESS || r.input_consumed || r.output_produced,"empty progress");
        used+=r.input_consumed;written+=r.output_produced;sink.write(reinterpret_cast<const char*>(output.data()),r.output_produced);
        check(static_cast<bool>(sink),"write output");
        if (r.status==MARC_STATUS_END_OF_STREAM) {check(read==size && used==present,"early completion");break;}
        check(r.input_consumed || r.output_produced || (r.status==MARC_STATUS_NEED_INPUT && used==present && read<size),"stalled transform");
    }
    start=Clock::now();transform.reset();primary.reset();secondary.reset();std::vector<std::max_align_t>().swap(views);codec_seconds+=seconds(start);
    sink.close();check(static_cast<bool>(sink),"close output");std::filesystem::rename(temporary,target);const auto resident=peak();check(resident>0,"missing peak");
    std::cout<<std::setprecision(12)<<"{\"transform_completed\":true,\"public_api\":true,\"profile\":\""<<profile<<"\",\"mode\":\""<<mode
        <<"\",\"window_size\":"<<selected.extent<<",\"frame_size\":"<<selected.extent<<",\"input_bytes\":"<<size<<",\"output_bytes\":"<<written
        <<",\"codec_seconds\":"<<codec_seconds<<",\"wall_seconds\":"<<seconds(wall_start)<<",\"peak_resident_bytes\":"<<resident
        <<",\"queried_bytes\":"<<queried<<",\"allocated_workspace_bytes\":"<<allocated<<",\"configured_internal_bytes\":"<<budget
        <<",\"max_block_size\":"<<max_block<<",\"max_compressed_payload_size\":"<<max_payload<<",\"driver_io_bytes\":131072,\"control_allowance_bytes\":65536"
        <<",\"query_scope\":\""<<(profile=="position" ? "public_owner_calls_and_controls" : "caller_workspaces")<<"\"}\n";return 0;
}
int guarded(int argc,char** argv) {try {return run(argc,argv);}catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
}
int main(int argc,char** argv) {
#ifdef _WIN32
    return guarded(argc,argv);
#else
    const auto child=fork();if (child<0) return 2;
    if (child==0) {const auto code=guarded(argc,argv);std::cout.flush();std::cerr.flush();_exit(code);}
    int status{};while (waitpid(child,&status,0)<0) if (errno!=EINTR) return 2;
    return WIFEXITED(status) ? WEXITSTATUS(status) : 2;
#endif
}
