// Instrument the actual CLI path without adding a production output option.
#include <chrono>
#define main marc_cli_entry
#include "../tools/marc_cli.cpp"
#undef main
#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif
int main(int argc,char** argv) {
    const auto begin=std::chrono::steady_clock::now();
    const int code=marc_cli_entry(argc,argv);
    const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
    std::uint64_t peak{};
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS counters{};
    if (!GetProcessMemoryInfo(GetCurrentProcess(),&counters,sizeof(counters))) return 3;
    peak=static_cast<std::uint64_t>(counters.PeakWorkingSetSize);
#else
    rusage usage{}; if (getrusage(RUSAGE_SELF,&usage)) return 3;
    peak=static_cast<std::uint64_t>(usage.ru_maxrss)*1024;
#endif
    std::cout<<"{\"cli_exit\":"<<code<<",\"seconds\":"<<seconds<<",\"peak_resident_bytes\":"<<peak<<"}\n";
    return code;
}
