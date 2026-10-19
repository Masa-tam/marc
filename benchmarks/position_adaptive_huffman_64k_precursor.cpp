#include "frame/lzss_position_adaptive_huffman_owned.hpp"
#include <marc/marc.h>
#include <algorithm>
#include <array>
#include <chrono>
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
using namespace marc::frame::internal;
using Clock=std::chrono::steady_clock;
void check(bool b,const char* message) {if (!b) throw std::runtime_error(message);}
double elapsed(Clock::time_point t) {return std::chrono::duration<double>(Clock::now()-t).count();}
std::uint64_t peak() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS c{};check(GetProcessMemoryInfo(GetCurrentProcess(),&c,sizeof(c))!=0,"peak query");return c.PeakWorkingSetSize;
#else
    rusage r{};check(getrusage(RUSAGE_SELF,&r)==0,"peak query");return static_cast<std::uint64_t>(r.ru_maxrss)*1024;
#endif
}
class Context final : public marc::core::Transform {
public:
    explicit Context(marc_transform* p):p_(p) {}
    ~Context() override {marc_transform_destroy(p_);}
    marc::core::ProcessResult process(std::span<const std::byte> in,std::span<std::byte> out,std::uint32_t flags) noexcept override {
        const auto r=marc_transform_process(p_,{reinterpret_cast<const std::uint8_t*>(in.data()),in.size()},
            {reinterpret_cast<std::uint8_t*>(out.data()),out.size()},flags);
        using S=marc::core::StreamStatus;
        const auto s=r.status==MARC_STATUS_PROGRESS ? S::progress : r.status==MARC_STATUS_NEED_INPUT ? S::need_input
            : r.status==MARC_STATUS_NEED_OUTPUT ? S::need_output : r.status==MARC_STATUS_END_OF_STREAM ? S::end_of_stream : S::error;
        return {r.input_consumed,r.output_produced,s,{s==S::error ? marc::core::ErrorCode::malformed_stream : marc::core::ErrorCode::none,0,0}};
    }
private:marc_transform* p_;
};
int run(int argc,char** argv) {
    check(argc==5,"usage: position|contextual encode|decode source target");
    const std::string kind=argv[1],mode=argv[2];check(kind=="position" || kind=="contextual","profile");
    check(mode=="encode" || mode=="decode","direction");const bool encode=mode=="encode";
    const auto direction=encode ? MARC_DIRECTION_ENCODE : MARC_DIRECTION_DECODE;
    const std::filesystem::path source=argv[3],target=argv[4],temp=target.string()+".tmp";
    check(!std::filesystem::exists(target) && !std::filesystem::exists(temp),"existing output");
    const auto size=std::filesystem::file_size(source);std::ifstream input(source,std::ios::binary);std::ofstream output(temp,std::ios::binary);
    check(input && output,"open files");
    std::unique_ptr<marc::core::Transform> transform;
    std::unique_ptr<std::uint8_t[]> primary,secondary;std::vector<std::max_align_t> views;
    std::uint64_t queried{};double codec_seconds{};const auto wall_start=Clock::now();auto start=Clock::now();
    if (kind=="position") {
        PositionAdaptiveHuffmanStreamHeader h{};h.original_size=encode ? size : 0;
        marc::core::DecoderLimits limits;PositionAdaptiveHuffmanWorkspace resources{};marc::core::ErrorCode error{};
        if (encode) {
            check(PositionAdaptiveHuffmanOwnedEncoder::requirements(h,limits,resources)==marc::core::ErrorCode::none,"position query");
            transform=PositionAdaptiveHuffmanOwnedEncoder::create(h,limits,error);
        } else {
            check(PositionAdaptiveHuffmanOwnedDecoder::requirements(h.profile,65536,limits,resources)==marc::core::ErrorCode::none,"position query");
            transform=PositionAdaptiveHuffmanOwnedDecoder::create(h.profile,65536,limits,error);
        }
        queried=resources.aggregate_bytes;check(error==marc::core::ErrorCode::none && transform,"position create");
    } else {
        marc_lzss_contextual_adaptive_huffman_config c{};
        check(marc_lzss_contextual_adaptive_huffman_config_init(direction,&c)==MARC_STATUS_OK,"context init");
        check(marc_lzss_contextual_adaptive_huffman_config_apply_profile(&c,MARC_LZSS_CONTEXTUAL_PROFILE_64K)==MARC_STATUS_OK,"context profile");
        check(c.frame_size==65536 && c.window_size==65536,"context configuration");c.original_size=encode ? size : 0;
        marc_workspace_requirements r{};check(marc_lzss_contextual_adaptive_huffman_workspace_requirements(&c,&r)==MARC_STATUS_OK,"context query");
        check(r.views_alignment<=alignof(std::max_align_t),"alignment");
        primary.reset(new std::uint8_t[r.primary_bytes]);secondary.reset(new std::uint8_t[r.secondary_bytes]);
        views.resize((r.views_bytes+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
        queried=r.primary_bytes+r.secondary_bytes+r.views_bytes;marc_transform* handle{};
        check(marc_lzss_contextual_adaptive_huffman_create(&c,{primary.get(),r.primary_bytes},{secondary.get(),r.secondary_bytes},
            {reinterpret_cast<std::uint8_t*>(views.data()),r.views_bytes},&handle)==MARC_STATUS_OK,"context create");
        try {transform=std::make_unique<Context>(handle);} catch (...) {marc_transform_destroy(handle);throw;}
    }
    codec_seconds+=elapsed(start);
    std::array<std::byte,65536> in{},out{};std::size_t present{},used{};std::uint64_t read{},written{};
    for (;;) {
        if (used==present && read<size) {
            present=static_cast<std::size_t>(std::min<std::uint64_t>(in.size(),size-read));
            check(static_cast<bool>(input.read(reinterpret_cast<char*>(in.data()),present)),"read input");used=0;read+=present;
        }
        start=Clock::now();const auto r=transform->process(std::span(in).subspan(used,present-used),out,
            read==size ? marc::core::flag_value(marc::core::ProcessFlags::end_input) : 0);codec_seconds+=elapsed(start);
        check(marc::core::is_valid(r,present-used,out.size()) && r.status!=marc::core::StreamStatus::error,"process");
        used+=r.input_consumed;written+=r.output_produced;
        output.write(reinterpret_cast<const char*>(out.data()),r.output_produced);check(static_cast<bool>(output),"write output");
        if (r.status==marc::core::StreamStatus::end_of_stream) {check(read==size && used==present,"early completion");break;}
        check(r.input_consumed || r.output_produced || (r.status==marc::core::StreamStatus::need_input && used==present && read<size),"stalled transform");
    }
    start=Clock::now();transform.reset();std::vector<std::max_align_t>().swap(views);primary.reset();secondary.reset();codec_seconds+=elapsed(start);
    output.close();check(static_cast<bool>(output),"close output");std::filesystem::rename(temp,target);
    const auto resident=peak();check(resident>0,"missing peak");
    std::cout<<std::setprecision(12)<<"{\"transform_completed\":true,\"prototype_owner\":"<<(kind=="position" ? "true" : "false")
        <<",\"profile\":\""<<kind<<"\",\"mode\":\""<<mode<<"\",\"frame_size\":65536,\"window_size\":65536"
        <<",\"input_bytes\":"<<size<<",\"output_bytes\":"<<written<<",\"codec_seconds\":"<<codec_seconds
        <<",\"wall_seconds\":"<<elapsed(wall_start)<<",\"peak_resident_bytes\":"<<resident<<",\"driver_io_bytes\":131072,\"queried_bytes\":"<<queried
        <<",\"query_scope\":\""<<(kind=="position" ? "private_owner_and_controls" : "caller_workspaces")<<"\"}\n";
    return 0;
}
int guarded(int argc,char** argv) {try {return run(argc,argv);} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
}
int main(int argc,char** argv) {
#ifdef _WIN32
    return guarded(argc,argv);
#else
    const auto child=fork();if (child<0) return 2;
    if (child==0) {const auto status=guarded(argc,argv);std::cout.flush();std::cerr.flush();_exit(status);}
    int status{};while (waitpid(child,&status,0)<0) if (errno!=EINTR) return 2;
    return WIFEXITED(status) ? WEXITSTATUS(status) : 2;
#endif
}
