#ifndef MARC_BENCHMARK_RESIDENT_MEMORY_HPP
#define MARC_BENCHMARK_RESIDENT_MEMORY_HPP

#include <cstdint>
#include <iostream>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#else
#include <cerrno>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace marc::benchmark {

[[nodiscard]] inline std::uint64_t peak_resident_bytes() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
        throw std::runtime_error("peak resident query failed");
    return counters.PeakWorkingSetSize;
#else
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0 || usage.ru_maxrss < 0)
        throw std::runtime_error("peak resident query failed");
#ifdef __APPLE__
    return static_cast<std::uint64_t>(usage.ru_maxrss);
#else
    return static_cast<std::uint64_t>(usage.ru_maxrss) * UINT64_C(1024);
#endif
#endif
}

template<class Run>
int fresh_worker(Run&& run) {
#ifdef _WIN32
    return run();
#else
    // Start a child after exec and before codec allocation so the measurement
    // does not inherit a Python/shell launcher's historical RSS high-water mark.
    std::cout.flush();
    std::cerr.flush();
    const auto child = fork();
    if (child < 0) return 2;
    if (child == 0) {
        const auto code = run();
        std::cout.flush();
        std::cerr.flush();
        _exit(code);
    }
    int status{};
    while (waitpid(child, &status, 0) < 0)
        if (errno != EINTR) return 2;
    return WIFEXITED(status) ? WEXITSTATUS(status) : 2;
#endif
}

} // namespace marc::benchmark

#endif
