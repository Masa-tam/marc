#include "frame/lzss_position_blocked_huffman_1m_owned.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
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
int run(int argc,char** argv) {
    using namespace marc;
    using namespace frame::internal;
    if (argc!=4) return 2;
    const std::string mode=argv[1];
    if (mode!="encode" && mode!="decode") return 2;
    const std::filesystem::path source_path=argv[2],target=argv[3],temporary=target.string()+".tmp";
    if (std::filesystem::exists(target) || std::filesystem::exists(temporary)) return 3;
    const auto size=std::filesystem::file_size(source_path);
    std::ifstream source(source_path,std::ios::binary);
    std::ofstream sink(temporary,std::ios::binary);
    if (!source || !sink) return 3;
    constexpr std::size_t capacity=65536,control=65536;
    constexpr std::uint64_t candidate_grant=UINT64_C(96)<<20;
    core::DecoderLimits limits{};
    limits.max_block_size=8388608;
    limits.max_internal_buffered_bytes=candidate_grant-2*capacity-control;
    PositionBlockedHuffman1mWorkspace requirements{};
    PositionBlockedHuffman1mStreamHeader stream{};stream.original_size=size;
    const auto queried=mode=="encode" ? PositionBlockedHuffman1mOwnedEncoder::requirements(stream,limits,requirements)
        : PositionBlockedHuffman1mOwnedDecoder::requirements(1048576,limits,requirements);
    if (queried!=core::ErrorCode::none) return 4;
    core::ErrorCode error{};
    std::unique_ptr<core::Transform> transform;
    if (mode=="encode") transform=PositionBlockedHuffman1mOwnedEncoder::create(stream,limits,error);
    else transform=PositionBlockedHuffman1mOwnedDecoder::create(1048576,limits,error);
    if (!transform || error!=core::ErrorCode::none) return 4;
    std::array<std::byte,capacity> input{},output{};
    std::size_t present=0,consumed=0;
    std::uint64_t read=0;
    for (;;) {
        if (consumed==present && read<size) {
            present=static_cast<std::size_t>(std::min<std::uint64_t>(capacity,size-read));
            if (!source.read(reinterpret_cast<char*>(input.data()),present)) return 5;
            read+=present;consumed=0;
        }
        const auto r=transform->process(std::span<const std::byte>(input).subspan(consumed,present-consumed),output,
            read==size ? core::flag_value(core::ProcessFlags::end_input) : 0);
        if (r.input_consumed>present-consumed || r.output_produced>output.size() || r.status==core::StreamStatus::error
            || (r.status==core::StreamStatus::progress && !r.input_consumed && !r.output_produced)) return 6;
        consumed+=r.input_consumed;
        sink.write(reinterpret_cast<const char*>(output.data()),r.output_produced);
        if (!sink) return 5;
        if (r.status==core::StreamStatus::end_of_stream) {
            if (read!=size || consumed!=present) return 6;
            break;
        }
        if (!r.input_consumed && !r.output_produced
            && !(r.status==core::StreamStatus::need_input && consumed==present && read<size)) return 6;
    }
    transform.reset();sink.close();
    if (!sink) return 5;
    std::filesystem::rename(temporary,target);
    const auto resident=peak();if (!resident) return 7;
    std::cout<<"{\"transform_completed\":true,\"peak_resident_bytes\":"<<resident
        <<",\"frame_size\":1048576,\"window_size\":1048576,\"driver_io_bytes\":131072,\"control_allowance_bytes\":65536"
        <<",\"queried_owner_aggregate_bytes\":"<<requirements.aggregate_bytes
        <<",\"candidate_logical_grant_bytes\":"<<candidate_grant
        <<",\"owned_buffer_bytes\":"<<requirements.owned_buffer_bytes<<"}\n";
    return 0;
}
int guarded(int argc,char** argv) {
    try { return run(argc,argv); }
    catch (const std::exception& e) { std::cerr<<e.what()<<'\n';return 8; }
}
}
int main(int argc,char** argv) {
#ifdef _WIN32
    return guarded(argc,argv);
#else
    const auto child=fork();if (child<0) return 9;
    if (child==0) { const auto result=guarded(argc,argv);std::cout.flush();std::cerr.flush();_exit(result); }
    int status{};
    while (waitpid(child,&status,0)<0) if (errno!=EINTR) return 9;
    return WIFEXITED(status) ? WEXITSTATUS(status) : 9;
#endif
}
