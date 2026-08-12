#include "dictionary/lzss_encoder.hpp"
#include "dictionary/lzss_hash_chain_match_finder.hpp"
#include "dictionary/lzss_match_finder.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace {
using namespace marc::dictionary::internal;

struct AlignedWorkspace {
    explicit AlignedWorkspace(const std::size_t size)
        : storage((size + sizeof(std::max_align_t) - 1)
                  / sizeof(std::max_align_t)) {}

    [[nodiscard]] std::span<std::byte> bytes(const std::size_t size) {
        return std::as_writable_bytes(std::span{storage}).first(size);
    }

    std::vector<std::max_align_t> storage;
};

[[nodiscard]] bool read_file(
    const std::filesystem::path& path,
    std::vector<std::byte>& output) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) return false;
    const auto end = stream.tellg();
    if (end < 0) return false;
    const auto size = static_cast<std::uintmax_t>(end);
    if (size > (UINT64_C(1) << 20)) return false;
    output.resize(static_cast<std::size_t>(size));
    stream.seekg(0);
    if (!output.empty()) {
        stream.read(reinterpret_cast<char*>(output.data()),
                    static_cast<std::streamsize>(output.size()));
    }
    return static_cast<bool>(stream);
}

[[nodiscard]] bool parse_iterations(
    const std::string_view text, std::size_t& iterations) noexcept {
    const auto result = std::from_chars(
        text.data(), text.data() + text.size(), iterations);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size()
        && iterations != 0 && iterations <= 1'000'000;
}

template <typename Function>
[[nodiscard]] double measure_seconds(
    const std::size_t iterations, Function&& function) {
    const auto begin = std::chrono::steady_clock::now();
    for (std::size_t iteration = 0; iteration < iterations; ++iteration)
        function();
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(end - begin).count();
}

[[nodiscard]] double throughput(
    const std::size_t input_size, const std::size_t iterations,
    const double seconds) noexcept {
    if (seconds == 0.0) return 0.0;
    return static_cast<double>(input_size) * static_cast<double>(iterations)
        / (1024.0 * 1024.0) / seconds;
}

template <LzssMatchFinder Finder>
[[nodiscard]] std::size_t parse_with_finder(
    const std::span<const std::byte> input, Finder& finder) noexcept {
    std::size_t position{};
    std::size_t token_count{};
    while (position < input.size()) {
        const auto match = finder.find_match(position);
        const auto advance = match.length != 0
                && lzss_match_is_beneficial(match)
            ? static_cast<std::size_t>(match.length) : 1U;
        finder.advance(position, position + advance);
        position += advance;
        ++token_count;
    }
    return token_count;
}

void print_measurement(
    const std::string_view name, const double seconds,
    const std::size_t input_size, const std::size_t iterations) {
    std::cout << name << "_seconds=" << seconds << '\n'
              << name << "_mib_per_second="
              << throughput(input_size, iterations, seconds) << '\n';
}

} // namespace

int main(const int argc, const char* const argv[]) {
    if (argc < 2 || argc > 3) {
        std::cerr << "usage: marc_lzss_match_finder_benchmark "
                     "<input-file> [iterations]\n";
        return 2;
    }
    std::size_t iterations{1};
    if (argc == 3 && !parse_iterations(argv[2], iterations)) {
        std::cerr << "invalid iteration count\n";
        return 2;
    }
    std::vector<std::byte> input;
    if (!read_file(argv[1], input)) {
        std::cerr << "cannot read input or input exceeds one MiB\n";
        return 2;
    }

    const LzssParameters parameters{};
    const marc::core::DecoderLimits limits{};
    const auto requirements = calculate_lzss_hash_chain_workspace(
        input.size(), parameters, limits);
    if (requirements.error != LzssHashChainError::none) {
        std::cerr << "cannot calculate HashChain workspace\n";
        return 1;
    }
    AlignedWorkspace workspace_owner(requirements.workspace_size);
    auto workspace = workspace_owner.bytes(requirements.workspace_size);

    const auto exhaustive_plan = plan_lzss_token_stream(
        input, parameters, limits);
    const auto hash_plan = plan_lzss_token_stream_hash_chain(
        input, parameters, limits, workspace);
    if (exhaustive_plan.error != LzssEncodeError::none
        || hash_plan.error != LzssEncodeError::none
        || exhaustive_plan.output_size != hash_plan.output_size
        || exhaustive_plan.token_count != hash_plan.token_count) {
        std::cerr << "planning equivalence failed\n";
        return 1;
    }
    std::vector<std::byte> exhaustive_output(exhaustive_plan.output_size);
    std::vector<std::byte> hash_output(hash_plan.output_size);
    if (encode_lzss_token_stream(
            input, parameters, limits, exhaustive_output).error
            != LzssEncodeError::none
        || encode_lzss_token_stream_hash_chain(
            input, parameters, limits, hash_output, workspace).error
            != LzssEncodeError::none
        || exhaustive_output != hash_output) {
        std::cerr << "encoded equivalence failed\n";
        return 1;
    }

    LzssMatchFinderStatistics exhaustive_statistics{};
    LzssExhaustiveMatchFinder exhaustive_finder{
        input, parameters, &exhaustive_statistics};
    const auto exhaustive_tokens = parse_with_finder(input, exhaustive_finder);
    LzssMatchFinderStatistics hash_statistics{};
    LzssHashChainMatchFinder hash_finder{};
    if (initialize_lzss_hash_chain_match_finder(
            input, parameters, limits, workspace, hash_finder,
            &hash_statistics) != LzssHashChainError::none) {
        std::cerr << "HashChain initialization failed\n";
        return 1;
    }
    const auto hash_tokens = parse_with_finder(input, hash_finder);
    if (exhaustive_tokens != hash_tokens
        || hash_tokens != hash_plan.token_count) {
        std::cerr << "finder equivalence failed\n";
        return 1;
    }

    bool timing_ok{true};
    const auto exhaustive_plan_seconds = measure_seconds(iterations, [&] {
        timing_ok = timing_ok && plan_lzss_token_stream(
            input, parameters, limits).error == LzssEncodeError::none;
    });
    const auto hash_plan_seconds = measure_seconds(iterations, [&] {
        timing_ok = timing_ok && plan_lzss_token_stream_hash_chain(
            input, parameters, limits, workspace).error
            == LzssEncodeError::none;
    });
    const auto exhaustive_encode_seconds = measure_seconds(iterations, [&] {
        timing_ok = timing_ok && encode_lzss_token_stream(
            input, parameters, limits, exhaustive_output).error
            == LzssEncodeError::none;
    });
    const auto hash_encode_seconds = measure_seconds(iterations, [&] {
        timing_ok = timing_ok && encode_lzss_token_stream_hash_chain(
            input, parameters, limits, hash_output, workspace).error
            == LzssEncodeError::none;
    });
    if (!timing_ok || exhaustive_output != hash_output) {
        std::cerr << "timed encoding failed\n";
        return 1;
    }

    std::cout << std::fixed << std::setprecision(6)
              << "input_bytes=" << input.size() << '\n'
              << "output_bytes=" << hash_output.size() << '\n'
              << "token_count=" << hash_plan.token_count << '\n'
              << "iterations=" << iterations << '\n'
              << "hash_workspace_bytes=" << requirements.workspace_size
              << '\n'
              << "exhaustive_queries=" << exhaustive_statistics.query_count
              << '\n'
              << "exhaustive_candidates="
              << exhaustive_statistics.candidate_count << '\n'
              << "exhaustive_byte_comparisons="
              << exhaustive_statistics.byte_comparison_count << '\n'
              << "hash_chain_queries=" << hash_statistics.query_count << '\n'
              << "hash_chain_candidates=" << hash_statistics.candidate_count
              << '\n'
              << "hash_chain_byte_comparisons="
              << hash_statistics.byte_comparison_count << '\n';
    print_measurement("exhaustive_plan", exhaustive_plan_seconds,
                      input.size(), iterations);
    print_measurement("hash_chain_plan", hash_plan_seconds,
                      input.size(), iterations);
    print_measurement("exhaustive_encode", exhaustive_encode_seconds,
                      input.size(), iterations);
    print_measurement("hash_chain_encode", hash_encode_seconds,
                      input.size(), iterations);
    return 0;
}
