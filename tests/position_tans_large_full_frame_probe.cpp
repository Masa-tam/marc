#include <marc/marc.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

#define NAME_IMPL(window, suffix) marc_lzss_position_distance_tans_##window##suffix
#define NAME(window, suffix) NAME_IMPL(window, suffix)
#define API(suffix) NAME(MARC_TANS_WINDOW, suffix)

namespace {
void require(bool ok,const char* label) {
    if (!ok) { std::cerr<<label<<'\n'; std::exit(1); }
}
std::uint64_t peak() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS c{};
    require(GetProcessMemoryInfo(GetCurrentProcess(),&c,sizeof(c))!=0,"peak query");
    return c.PeakWorkingSetSize;
#else
    rusage c{}; require(getrusage(RUSAGE_SELF,&c)==0,"peak query");
    return static_cast<std::uint64_t>(c.ru_maxrss)*1024;
#endif
}
}

int main(int argc,char** argv) {
    require(argc==6,"mode path raw-size input-chunk output-chunk");
    const std::string mode=argv[1];
    const bool encode=mode=="encode";
    const bool failure=mode=="late-expansion" || mode=="bad-tail";
    require(encode || failure || mode=="valid" || mode=="tail","mode");
    const auto raw_size=std::stoull(argv[3]);
    const auto input_chunk=std::stoull(argv[4]), output_chunk=std::stoull(argv[5]);
    require(input_chunk && input_chunk<=65536 && output_chunk && output_chunk<=65536,"chunks");
    std::vector<std::uint8_t> input;
    if (encode) {
        require(raw_size<=67108865,"raw bound");
        input.resize(static_cast<std::size_t>(raw_size));
    } else {
        std::ifstream f(argv[2],std::ios::binary|std::ios::ate);
        require(static_cast<bool>(f),"open"); const auto size=f.tellg();
        require(size>=112 && size<(1<<24),"wire bound");
        input.resize(static_cast<std::size_t>(size));f.seekg(0);
        require(static_cast<bool>(f.read(reinterpret_cast<char*>(input.data()),size)),"read");
    }
    API(_config) config{};
    require(API(_config_init)(encode ? MARC_DIRECTION_ENCODE : MARC_DIRECTION_DECODE,&config)==MARC_STATUS_OK,"init");
    require(raw_size<=static_cast<std::uint64_t>(config.frame_size)+1,"frame boundary");
    if (encode && raw_size>config.frame_size) input.back()=127;
    config.original_size=raw_size;
    config.external_retained_bytes=input.capacity()+65538;
    config.max_internal_buffered_bytes+=config.external_retained_bytes;
    config.input_capacity_bytes=65536; config.output_capacity_bytes=65536;
    API(_resources) resources{};
    require(API(_resource_requirements)(&config,&resources)==MARC_STATUS_OK,"query");
    const auto start=std::chrono::steady_clock::now();
    marc_transform* raw{};
    require(API(_create)(&config,&raw)==MARC_STATUS_OK && raw,"create");
    std::unique_ptr<marc_transform,decltype(&marc_transform_destroy)> transform(raw,marc_transform_destroy);
    std::ofstream archive;
    if (encode) { archive.open(argv[2],std::ios::binary); require(static_cast<bool>(archive),"archive open"); }
    std::vector<std::uint8_t> output(static_cast<std::size_t>(output_chunk)+2,0xa5);
    std::size_t consumed{}, produced{};
    marc_process_result terminal{};
    bool ended{};
    for (std::size_t call=0;call<200000;++call) {
        const auto count=std::min<std::size_t>(static_cast<std::size_t>(input_chunk),input.size()-consumed);
        std::fill(output.begin(),output.end(),0xa5);
        const auto r=marc_transform_process(transform.get(),{input.data()+consumed,count},
            {output.data()+1,static_cast<std::size_t>(output_chunk)},
            consumed+count==input.size() ? MARC_PROCESS_END_INPUT : 0);
        require(r.input_consumed<=count && r.output_produced<=output_chunk,"bounds");
        require(output.front()==0xa5 && std::all_of(output.begin()+1+r.output_produced,output.end(),
            [](auto v){return v==0xa5;}),"output guards");
        consumed+=r.input_consumed;
        if (encode) archive.write(reinterpret_cast<const char*>(output.data()+1),r.output_produced);
        else for (std::size_t i=0;i<r.output_produced;++i) {
            const auto offset=produced+i;
            const auto expected=mode=="tail" && offset==config.frame_size ? 127 : 0;
            require(output[i+1]==expected,"raw mismatch");
        }
        produced+=r.output_produced;
        if (r.status==MARC_STATUS_END_OF_STREAM || r.status>=100) { terminal=r;ended=true;break; }
        require(r.input_consumed || r.output_produced,"no progress");
    }
    require(ended,"call bound");
    require(terminal.status==(failure ? MARC_STATUS_MALFORMED_STREAM : MARC_STATUS_END_OF_STREAM),"terminal");
    require(consumed==input.size(),"complete input");
    if (!encode) require(produced==(mode=="late-expansion" ? 0 : mode=="bad-tail" ? config.frame_size : raw_size),"publication prefix");
    std::fill(output.begin(),output.end(),0xa5);
    const auto again=marc_transform_process(transform.get(),{nullptr,0},{output.data()+1,1},MARC_PROCESS_END_INPUT);
    require(again.status==terminal.status && again.error_byte_position==terminal.error_byte_position
        && again.error_bit_position==terminal.error_bit_position && !again.input_consumed && !again.output_produced
        && std::all_of(output.begin(),output.end(),[](auto v){return v==0xa5;}),"sticky terminal");
    if (encode) { archive.close();require(static_cast<bool>(archive),"archive write"); }
    std::cout<<"{\"verified\":true,\"mode\":\""<<mode<<"\",\"raw_bytes\":"<<raw_size
        <<",\"consumed\":"<<consumed<<",\"produced\":"<<produced
        <<",\"status\":"<<terminal.status<<",\"queried_bytes\":"<<resources.minimum_aggregate_bytes
        <<",\"error_byte\":"<<terminal.error_byte_position<<",\"error_bit\":"<<static_cast<unsigned>(terminal.error_bit_position)
        <<",\"seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()
        <<",\"peak_resident_bytes\":"<<peak()<<"}\n";
}
