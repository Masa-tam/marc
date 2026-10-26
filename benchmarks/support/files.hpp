#ifndef MARC_BENCHMARK_FILES_HPP
#define MARC_BENCHMARK_FILES_HPP

#include "stream_pump.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>

namespace marc::benchmark {

class TemporaryFiles {
public:
    TemporaryFiles() {
        const auto root = std::filesystem::temp_directory_path();
        std::random_device random;
        for (unsigned attempt = 0; attempt < 256; ++attempt) {
            const auto candidate = root / ("marc-benchmark-"
                + std::to_string(random()) + "-" + std::to_string(attempt));
            std::error_code error;
            if (std::filesystem::create_directory(candidate, error)) {
                directory_ = candidate;
                return;
            }
            if (error && error != std::errc::file_exists)
                throw std::filesystem::filesystem_error(
                    "benchmark temporary directory", candidate, error);
        }
        throw std::runtime_error("benchmark temporary directory unavailable");
    }

    TemporaryFiles(const TemporaryFiles&) = delete;
    TemporaryFiles& operator=(const TemporaryFiles&) = delete;

    ~TemporaryFiles() {
        std::error_code ignored;
        for (const auto* name : {"archive", "candidate", "restored"})
            std::filesystem::remove(directory_ / name, ignored);
        std::filesystem::remove(directory_, ignored);
    }

    [[nodiscard]] std::filesystem::path archive() const {
        return directory_ / "archive";
    }
    [[nodiscard]] std::filesystem::path candidate() const {
        return directory_ / "candidate";
    }
    [[nodiscard]] std::filesystem::path restored() const {
        return directory_ / "restored";
    }

private:
    std::filesystem::path directory_;
};

[[nodiscard]] inline bool equal_files(const std::filesystem::path& first,
                                      const std::filesystem::path& second,
                                      const std::uint64_t expected_bytes) {
    std::ifstream left(first, std::ios::binary);
    std::ifstream right(second, std::ios::binary);
    if (!left || !right) return false;
    std::array<char, io_capacity> a{};
    std::array<char, io_capacity> b{};
    auto remaining = expected_bytes;
    while (remaining != 0) {
        const auto count = static_cast<std::streamsize>(
            std::min<std::uint64_t>(remaining, io_capacity));
        left.read(a.data(), count);
        right.read(b.data(), count);
        if (left.gcount() != count || right.gcount() != count
            || left.bad() || right.bad()
            || !std::equal(a.begin(), a.begin() + count, b.begin()))
            return false;
        remaining -= static_cast<std::uint64_t>(count);
    }
    return left.peek() == std::char_traits<char>::eof() && !left.bad()
        && right.peek() == std::char_traits<char>::eof() && !right.bad();
}

} // namespace marc::benchmark

#endif
