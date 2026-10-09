#include "frame/lzss_position_tans_64k_owned.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
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
    using namespace marc::frame::internal;
    using namespace marc::core;
    if (argc!=4) return 2;
    const std::string mode=argv[1];
    if (mode!="encode" && mode!="decode") return 2;
    const std::filesystem::path source_path=argv[2],target=argv[3];
    const std::filesystem::path temporary=target.string()+".tmp";
    if (std::filesystem::exists(target) || std::filesystem::exists(temporary)) return 3;
    const auto size=std::filesystem::file_size(source_path);
    std::ifstream source(source_path,std::ios::binary);
    if (!source) return 3;
    PositionTans64kStreamHeader stream{};
    DecoderLimits limits{};
    stream.original_size=size;
    if (mode=="decode") {
        std::array<std::byte,112> header{};
        if (!source.read(reinterpret_cast<char*>(header.data()),header.size())) return 4;
        std::size_t consumed{};
        if (parse_position_tans_64k_stream(header,limits,stream,consumed)!=PositionTans64kFormatError::none
            || consumed!=112 || stream.frame_size!=65536) return 4;
        source.seekg(0);
        if (!source) return 3;
    }
    const auto query=[&](const DecoderLimits& grant,PositionTans64kWorkspace& result) {
        return mode=="encode" ? PositionTans64kOwnedEncoder::requirements(stream,grant,result)
            : PositionTans64kOwnedDecoder::requirements(stream.frame_size,grant,result);
    };
    PositionTans64kWorkspace resources{};
    if (query(limits,resources)!=ErrorCode::none || !resources.aggregate_bytes) return 4;
    limits.max_internal_buffered_bytes=resources.aggregate_bytes;
    PositionTans64kWorkspace exact{};
    if (query(limits,exact)!=ErrorCode::none || exact!=resources) return 4;
    auto below=limits; --below.max_internal_buffered_bytes;
    auto untouched=resources;
    if (query(below,untouched)!=ErrorCode::limit_exceeded || untouched!=resources) return 4;
    ErrorCode error{};
    // A rejected grant must fail before allocating owner buffers. Allocation
    // rollback itself is qualified separately in the owning unit executable.
    if (mode=="encode") {
        if (PositionTans64kOwnedEncoder::create(stream,below,error) || error!=ErrorCode::limit_exceeded) return 4;
    } else if (PositionTans64kOwnedDecoder::create(stream.frame_size,below,error) || error!=ErrorCode::limit_exceeded) return 4;
    const auto start=std::chrono::steady_clock::now();
    std::unique_ptr<Transform> transform;
    if (mode=="encode") transform=PositionTans64kOwnedEncoder::create(stream,limits,error);
    else transform=PositionTans64kOwnedDecoder::create(stream.frame_size,limits,error);
    if (!transform || error!=ErrorCode::none) return 4;
    std::ofstream sink(temporary,std::ios::binary);
    if (!sink) return 3;
    constexpr std::size_t capacity=65536;
    std::array<std::byte,capacity> input{},output{};
    std::size_t present{},consumed{};
    std::uint64_t read{},produced{};
    for (;;) {
        if (consumed==present && read<size) {
            present=static_cast<std::size_t>(std::min<std::uint64_t>(capacity,size-read));
            if (!source.read(reinterpret_cast<char*>(input.data()),static_cast<std::streamsize>(present))) return 5;
            read+=present; consumed=0;
        }
        const auto result=transform->process(std::span<const std::byte>(input).subspan(consumed,present-consumed),
            output,read==size ? flag_value(ProcessFlags::end_input) : 0);
        if (result.input_consumed>present-consumed || result.output_produced>output.size()
            || result.status==StreamStatus::error
            || (result.status==StreamStatus::progress && !result.input_consumed && !result.output_produced)) return 6;
        consumed+=result.input_consumed;
        if (produced>UINT64_MAX-result.output_produced) return 6;
        produced+=result.output_produced;
        sink.write(reinterpret_cast<const char*>(output.data()),static_cast<std::streamsize>(result.output_produced));
        if (!sink) return 5;
        if (result.status==StreamStatus::end_of_stream) {
            if (read!=size || consumed!=present || (mode=="decode" && produced!=stream.original_size)) return 6;
            break;
        }
        if (!result.input_consumed && !result.output_produced
            && !(result.status==StreamStatus::need_input && consumed==present && read<size)) return 6;
    }
    transform.reset(); sink.close();
    if (!sink) return 5;
    std::filesystem::rename(temporary,target);
    const auto resident=peak(); if (!resident) return 7;
    const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"{\"transform_completed\":true,\"seconds\":"<<seconds<<",\"peak_resident_bytes\":"<<resident
        <<",\"query_scope\":\"private_owner_buffers_controls_and_working_admission\""
        <<",\"frame_size\":"<<stream.frame_size<<",\"window_size\":"<<stream.dictionary.window_size
        <<",\"original_size\":"<<stream.original_size<<",\"input_bytes\":"<<size<<",\"output_bytes\":"<<produced
        <<",\"exact_grant_checked\":true,\"one_below_rejected\":true"
        <<",\"aggregate_bytes\":"<<resources.aggregate_bytes<<",\"owned_buffer_bytes\":"<<resources.owned_buffer_bytes
        <<",\"owner_bytes\":"<<resources.owner_bytes<<",\"fixed_working_bytes\":"<<resources.fixed_working_bytes
        <<",\"metadata_floor_bytes\":"<<resources.metadata_floor_bytes<<",\"raw_bytes\":"<<resources.raw_bytes
        <<",\"token_bytes\":"<<resources.token_bytes<<",\"serialized_bytes\":"<<resources.serialized_bytes
        <<",\"finder_bytes\":"<<resources.finder_bytes<<",\"table_bytes\":"<<resources.table_bytes
        <<",\"payload_bytes\":"<<resources.payload_bytes<<",\"private_serialized_bytes\":"<<resources.private_serialized_bytes
        <<",\"driver_io_bytes_excluded_from_query\":"<<2*capacity<<"}\n";
    return 0;
}
}
int guarded_run(int argc,char** argv) {
    try { return run(argc,argv); }
    catch (const std::exception& error) { std::cerr<<error.what()<<'\n';return 8; }
}
int main(int argc,char** argv) {
#ifdef _WIN32
    return guarded_run(argc,argv);
#else
    // Linux retains a launcher's pre-exec high-water RSS. Fork from the small
    // normal executable before allocating owner buffers so getrusage measures
    // this diagnostic child, rather than an inherited Python-launcher peak.
    const auto child=fork();
    if (child<0) return 9;
    if (child==0) {
        const auto status=guarded_run(argc,argv);
        std::cout.flush();std::cerr.flush();
        _exit(status);
    }
    int status{};
    while (waitpid(child,&status,0)<0) if (errno!=EINTR) return 9;
    return WIFEXITED(status) ? WEXITSTATUS(status) : 9;
#endif
}
