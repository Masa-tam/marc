#include "dictionary/lzss_encoder.hpp"
#include "dictionary/lzss_binary_tree_match_finder.hpp"
#include "dictionary/lzss_hash_chain_match_finder.hpp"
#include "dictionary/lzss_match_finder.hpp"
#include "dictionary/lzss_typed_encoder.hpp"
#include "core/checked_math.hpp"
#include "frame/lzss_typed_context_frame_encoder.hpp"
#include "frame/lzss_contextual_rans_frame_encoder.hpp"
#include "frame/lzss_contextual_tans_frame_encoder.hpp"
#include "frame/lzss_contextual_blocked_huffman_frame_encoder.hpp"
#include "frame/lzss_contextual_adaptive_huffman_frame_encoder.hpp"
#include "frame/lzss_frame.hpp"
#include "frame/lzss_adaptive_huffman_frame.hpp"
#include "frame/lzss_blocked_huffman_frame.hpp"
#include "frame/lzss_dynamic_range_frame.hpp"
#include "frame/lzss_rans_frame.hpp"
#include "frame/lzss_tans_frame.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

namespace {
using namespace marc::dictionary::internal;

enum class BenchmarkStrategy : std::uint8_t {
    hash_chain_exact,
    binary_tree_exact,
};

[[nodiscard]] bool parse_strategy(
    const std::string_view text, BenchmarkStrategy& strategy) noexcept {
    if (text == "hash-chain-exact") {
        strategy = BenchmarkStrategy::hash_chain_exact;
    } else if (text == "binary-tree-exact") {
        strategy = BenchmarkStrategy::binary_tree_exact;
    } else {
        return false;
    }
    return true;
}

[[nodiscard]] std::string_view strategy_name(
    const BenchmarkStrategy strategy) noexcept {
    return strategy == BenchmarkStrategy::hash_chain_exact
        ? "hash-chain-exact" : "binary-tree-exact";
}

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
    const std::uint64_t input_size, const std::size_t iterations,
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

struct FrameRunResult {
    std::uint64_t input_bytes{};
    std::uint64_t frame_count{};
    std::uint64_t token_count{};
    LzssMatchFinderStatistics statistics{};
    double seconds{};
};

[[nodiscard]] bool add_count(
    std::uint64_t& total, const std::uint64_t value) noexcept {
    return marc::core::checked_add(total, value, total);
}

[[nodiscard]] bool add_statistics(
    LzssMatchFinderStatistics& total,
    const LzssMatchFinderStatistics& frame) noexcept {
    if (frame.overflowed) return false;
    if (!add_count(total.query_count, frame.query_count)
        || !add_count(total.candidate_count, frame.candidate_count)
        || !add_count(total.byte_comparison_count,
                      frame.byte_comparison_count)) {
        return false;
    }
    if (!add_count(total.hash_chain_prefix_match_count,
                   frame.hash_chain_prefix_match_count)
        || !add_count(total.hash_chain_prefix_mismatch_count,
                      frame.hash_chain_prefix_mismatch_count)
        || !add_count(
            total.hash_chain_extension_byte_comparison_count,
            frame.hash_chain_extension_byte_comparison_count)) {
        return false;
    }
    total.hash_chain_maximum_candidates_per_query = std::max(
        total.hash_chain_maximum_candidates_per_query,
        frame.hash_chain_maximum_candidates_per_query);
    for (std::size_t bin = 0;
         bin < total.hash_chain_query_depth_histogram.size(); ++bin) {
        if (!add_count(total.hash_chain_query_depth_histogram[bin],
                       frame.hash_chain_query_depth_histogram[bin])) {
            return false;
        }
    }
    if (!add_count(total.binary_tree_key_comparison_count,
                   frame.binary_tree_key_comparison_count)
        || !add_count(total.binary_tree_key_byte_comparison_count,
                      frame.binary_tree_key_byte_comparison_count)
        || !add_count(total.binary_tree_lcp_byte_comparison_count,
                      frame.binary_tree_lcp_byte_comparison_count)
        || !add_count(total.binary_tree_prefix_range_comparison_count,
                      frame.binary_tree_prefix_range_comparison_count)
        || !add_count(total.binary_tree_rotation_count,
                      frame.binary_tree_rotation_count)
        || !add_count(total.binary_tree_insertion_count,
                      frame.binary_tree_insertion_count)
        || !add_count(total.binary_tree_retirement_count,
                      frame.binary_tree_retirement_count)) {
        return false;
    }
    total.binary_tree_maximum_height = std::max(
        total.binary_tree_maximum_height,
        frame.binary_tree_maximum_height);
    total.binary_tree_maximum_nodes_per_query = std::max(
        total.binary_tree_maximum_nodes_per_query,
        frame.binary_tree_maximum_nodes_per_query);
    for (std::size_t bin = 0;
         bin < total.binary_tree_query_depth_histogram.size(); ++bin) {
        if (!add_count(total.binary_tree_query_depth_histogram[bin],
                       frame.binary_tree_query_depth_histogram[bin])) {
            return false;
        }
    }
    return true;
}

void print_hash_chain_depth_histogram(
    const LzssMatchFinderStatistics& statistics) {
    const auto last_bin = statistics.hash_chain_maximum_candidates_per_query
        == 0 ? 0U
        : std::bit_width(
            statistics.hash_chain_maximum_candidates_per_query);
    std::cout << "hash_chain_query_depth_histogram=";
    for (std::size_t bin = 0; bin <= last_bin; ++bin) {
        if (bin != 0) std::cout << ',';
        std::cout << statistics.hash_chain_query_depth_histogram[bin];
    }
    std::cout << '\n';
}

void print_binary_tree_depth_histogram(
    const LzssMatchFinderStatistics& statistics) {
    const auto last_bin = statistics.binary_tree_maximum_nodes_per_query
        == 0 ? 0U
        : std::bit_width(
            statistics.binary_tree_maximum_nodes_per_query);
    std::cout << "binary_tree_query_depth_histogram=";
    for (std::size_t bin = 0; bin <= last_bin; ++bin) {
        if (bin != 0) std::cout << ',';
        std::cout << statistics.binary_tree_query_depth_histogram[bin];
    }
    std::cout << '\n';
}

[[nodiscard]] bool valid_hash_chain_statistics(
    const LzssMatchFinderStatistics& statistics) noexcept {
    if (statistics.overflowed
        || statistics.hash_chain_extension_byte_comparison_count
            > statistics.byte_comparison_count) {
        return false;
    }
    std::uint64_t classified_candidates{};
    if (!marc::core::checked_add(
            statistics.hash_chain_prefix_match_count,
            statistics.hash_chain_prefix_mismatch_count,
            classified_candidates)
        || classified_candidates != statistics.candidate_count) {
        return false;
    }
    std::uint64_t histogram_queries{};
    for (const auto count : statistics.hash_chain_query_depth_histogram) {
        if (!add_count(histogram_queries, count)) return false;
    }
    return histogram_queries == statistics.query_count;
}

[[nodiscard]] bool valid_binary_tree_statistics(
    const LzssMatchFinderStatistics& statistics) noexcept {
    if (statistics.overflowed
        || statistics.binary_tree_prefix_range_comparison_count
            > statistics.binary_tree_key_comparison_count) {
        return false;
    }
    std::uint64_t histogram_queries{};
    for (const auto count : statistics.binary_tree_query_depth_histogram) {
        if (!add_count(histogram_queries, count)) return false;
    }
    return histogram_queries == statistics.query_count;
}

[[nodiscard]] bool parse_size_argument(
    const std::string_view text, const std::uint64_t maximum,
    std::size_t& value) noexcept {
    std::uint64_t parsed{};
    const auto result = std::from_chars(
        text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc{}
        || result.ptr != text.data() + text.size()
        || parsed == 0 || parsed > maximum
        || parsed > std::numeric_limits<std::size_t>::max()) {
        return false;
    }
    value = static_cast<std::size_t>(parsed);
    return true;
}

enum class SyntheticInputKind : std::uint8_t {
    zeros,
    periodic,
    equal_prefix,
    hash_collision,
    pseudorandom,
};

[[nodiscard]] bool parse_synthetic_input_kind(
    const std::string_view text, SyntheticInputKind& kind) noexcept {
    if (text == "zeros") kind = SyntheticInputKind::zeros;
    else if (text == "periodic") kind = SyntheticInputKind::periodic;
    else if (text == "equal-prefix") kind = SyntheticInputKind::equal_prefix;
    else if (text == "hash-collision") {
        kind = SyntheticInputKind::hash_collision;
    } else if (text == "pseudorandom") {
        kind = SyntheticInputKind::pseudorandom;
    } else {
        return false;
    }
    return true;
}

[[nodiscard]] std::string_view synthetic_input_name(
    const SyntheticInputKind kind) noexcept {
    switch (kind) {
    case SyntheticInputKind::zeros: return "zeros";
    case SyntheticInputKind::periodic: return "periodic";
    case SyntheticInputKind::equal_prefix: return "equal-prefix";
    case SyntheticInputKind::hash_collision: return "hash-collision";
    case SyntheticInputKind::pseudorandom: return "pseudorandom";
    }
    return "unknown";
}

[[nodiscard]] std::uint32_t advance_synthetic_lcg(
    const std::uint64_t steps) noexcept {
    std::uint32_t accumulated_multiplier{1};
    std::uint32_t accumulated_increment{};
    std::uint32_t current_multiplier{UINT32_C(1664525)};
    std::uint32_t current_increment{UINT32_C(1013904223)};
    auto remaining = steps;
    while (remaining != 0) {
        if ((remaining & 1U) != 0) {
            accumulated_multiplier *= current_multiplier;
            accumulated_increment =
                accumulated_increment * current_multiplier
                + current_increment;
        }
        current_increment *= current_multiplier + 1U;
        current_multiplier *= current_multiplier;
        remaining >>= 1U;
    }
    return accumulated_multiplier * UINT32_C(0x13579bdf)
        + accumulated_increment;
}

void fill_synthetic_input(
    const SyntheticInputKind kind, const std::uint64_t absolute_offset,
    const std::span<std::byte> output) noexcept {
    constexpr std::array<std::byte, 5> equal_prefix{
        std::byte{'A'}, std::byte{'B'}, std::byte{'C'}, std::byte{'D'},
        std::byte{'E'}};
    constexpr std::array<std::byte, 5> collision_a{
        std::byte{0x01}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x58}, std::byte{0x59}};
    constexpr std::array<std::byte, 5> collision_b{
        std::byte{0x00}, std::byte{0x20}, std::byte{0x00},
        std::byte{0x58}, std::byte{0x59}};
    if (kind == SyntheticInputKind::pseudorandom) {
        auto state = advance_synthetic_lcg(absolute_offset);
        for (auto& value : output) {
            state = state * UINT32_C(1664525) + UINT32_C(1013904223);
            value = static_cast<std::byte>(state >> 24U);
        }
        return;
    }
    for (std::size_t index = 0; index < output.size(); ++index) {
        const auto position = absolute_offset + index;
        switch (kind) {
        case SyntheticInputKind::zeros:
            output[index] = std::byte{0};
            break;
        case SyntheticInputKind::periodic:
            output[index] = static_cast<std::byte>(position % 251U);
            break;
        case SyntheticInputKind::equal_prefix:
        case SyntheticInputKind::hash_collision: {
            const auto block = position / 8U;
            const auto in_block = static_cast<std::size_t>(position % 8U);
            if (in_block < 5) {
                const auto& prefix = kind == SyntheticInputKind::equal_prefix
                    ? equal_prefix
                    : (block % 2U == 0 ? collision_a : collision_b);
                output[index] = prefix[in_block];
            } else {
                const auto shift = static_cast<unsigned>((in_block - 5U) * 8U);
                output[index] = static_cast<std::byte>(
                    static_cast<std::uint8_t>((block >> shift) & 0xffU));
            }
            break;
        }
        case SyntheticInputKind::pseudorandom: break;
        }
    }
}

[[nodiscard]] bool process_frame(
    const BenchmarkStrategy strategy,
    const std::span<const std::byte> frame,
    const LzssParameters& parameters,
    const marc::core::DecoderLimits& limits,
    const std::span<std::byte> workspace, const bool collect_statistics,
    const bool measure, FrameRunResult& result) noexcept {
    LzssMatchFinderStatistics frame_statistics{};
    const auto begin = std::chrono::steady_clock::now();
    std::size_t frame_tokens{};
    if (strategy == BenchmarkStrategy::hash_chain_exact) {
        LzssHashChainMatchFinder finder{};
        if (initialize_lzss_hash_chain_match_finder(
                frame, parameters, limits, workspace, finder,
                collect_statistics ? &frame_statistics : nullptr)
            != LzssHashChainError::none) {
            return false;
        }
        frame_tokens = parse_with_finder(frame, finder);
    } else {
        LzssBinaryTreeMatchFinder finder{};
        if (initialize_lzss_binary_tree_match_finder(
                frame, parameters, limits, workspace, finder,
                collect_statistics ? &frame_statistics : nullptr)
            != LzssBinaryTreeError::none) {
            return false;
        }
        frame_tokens = parse_with_finder(frame, finder);
    }
    const auto end = std::chrono::steady_clock::now();
    if (measure) {
        result.seconds += std::chrono::duration<double>(end - begin).count();
    }
    return add_count(result.input_bytes, frame.size())
        && add_count(result.frame_count, 1)
        && add_count(result.token_count, frame_tokens)
        && (!collect_statistics
            || add_statistics(result.statistics, frame_statistics));
}

[[nodiscard]] bool process_frames(
    const BenchmarkStrategy strategy,
    const std::filesystem::path& path, const std::uint64_t expected_file_size,
    const std::size_t frame_size, const LzssParameters& parameters,
    const marc::core::DecoderLimits& limits,
    const std::span<std::byte> workspace, const bool collect_statistics,
    const bool measure, FrameRunResult& result) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    std::vector<std::byte> input(frame_size);
    std::uint64_t remaining = expected_file_size;
    while (remaining != 0) {
        const auto current_size = static_cast<std::size_t>(
            std::min<std::uint64_t>(remaining, frame_size));
        stream.read(reinterpret_cast<char*>(input.data()),
                    static_cast<std::streamsize>(current_size));
        if (stream.gcount() != static_cast<std::streamsize>(current_size)) {
            return false;
        }

        const auto frame = std::span<const std::byte>{input}.first(current_size);
        if (!process_frame(
                strategy, frame, parameters, limits, workspace,
                collect_statistics, measure, result)) {
            return false;
        }
        remaining -= current_size;
    }

    char extra{};
    if (stream.read(&extra, 1)) return false;
    return stream.eof();
}

[[nodiscard]] bool process_synthetic_frames(
    const BenchmarkStrategy strategy,
    const SyntheticInputKind kind, const std::uint64_t input_size,
    const std::size_t frame_size, const LzssParameters& parameters,
    const marc::core::DecoderLimits& limits,
    const std::span<std::byte> workspace, const bool collect_statistics,
    const bool measure, FrameRunResult& result) {
    std::vector<std::byte> input(frame_size);
    std::uint64_t offset{};
    while (offset < input_size) {
        const auto current_size = static_cast<std::size_t>(
            std::min<std::uint64_t>(input_size - offset, frame_size));
        const auto frame = std::span<std::byte>{input}.first(current_size);
        fill_synthetic_input(kind, offset, frame);
        if (!process_frame(
                strategy, frame, parameters, limits, workspace,
                collect_statistics, measure, result)) {
            return false;
        }
        offset += current_size;
    }
    return true;
}

void print_frame_report(
    const BenchmarkStrategy strategy,
    const std::string_view mode, const std::string_view synthetic_case,
    const std::size_t frame_size, const std::size_t window_size,
    const std::size_t iterations, const std::size_t workspace_size,
    const FrameRunResult& verified, const double measured_seconds) {
    std::cout << std::fixed << std::setprecision(6)
              << "mode=" << mode << '\n'
              << "strategy=" << strategy_name(strategy) << '\n';
    if (!synthetic_case.empty()) {
        std::cout << "synthetic_case=" << synthetic_case << '\n';
    }
    std::cout << "input_bytes=" << verified.input_bytes << '\n'
              << "frame_bytes=" << frame_size << '\n'
              << "window_bytes=" << window_size << '\n'
              << "frame_count=" << verified.frame_count << '\n'
              << "token_count=" << verified.token_count << '\n'
              << "iterations=" << iterations << '\n';
    if (strategy == BenchmarkStrategy::hash_chain_exact) {
        std::cout << "hash_workspace_bytes=" << workspace_size << '\n'
                  << "hash_chain_queries="
                  << verified.statistics.query_count << '\n'
                  << "hash_chain_candidates="
                  << verified.statistics.candidate_count << '\n'
                  << "hash_chain_byte_comparisons="
                  << verified.statistics.byte_comparison_count << '\n'
                  << "hash_chain_prefix_matches="
                  << verified.statistics.hash_chain_prefix_match_count << '\n'
                  << "hash_chain_prefix_mismatches="
                  << verified.statistics.hash_chain_prefix_mismatch_count
                  << '\n'
                  << "hash_chain_extension_byte_comparisons="
                  << verified.statistics
                         .hash_chain_extension_byte_comparison_count
                  << '\n'
                  << "hash_chain_max_candidates_per_query="
                  << verified.statistics
                         .hash_chain_maximum_candidates_per_query
                  << '\n'
                  << "hash_chain_frame_seconds=" << measured_seconds << '\n'
                  << "hash_chain_frame_mib_per_second="
                  << throughput(
                         verified.input_bytes, iterations, measured_seconds)
                  << '\n';
        print_hash_chain_depth_histogram(verified.statistics);
        return;
    }
    std::cout << "binary_tree_workspace_bytes=" << workspace_size << '\n'
              << "binary_tree_queries="
              << verified.statistics.query_count << '\n'
              << "binary_tree_key_comparisons="
              << verified.statistics.binary_tree_key_comparison_count << '\n'
              << "binary_tree_key_byte_comparisons="
              << verified.statistics.binary_tree_key_byte_comparison_count
              << '\n'
              << "binary_tree_lcp_byte_comparisons="
              << verified.statistics.binary_tree_lcp_byte_comparison_count
              << '\n'
              << "binary_tree_prefix_range_comparisons="
              << verified.statistics.binary_tree_prefix_range_comparison_count
              << '\n'
              << "binary_tree_rotations="
              << verified.statistics.binary_tree_rotation_count << '\n'
              << "binary_tree_insertions="
              << verified.statistics.binary_tree_insertion_count << '\n'
              << "binary_tree_retirements="
              << verified.statistics.binary_tree_retirement_count << '\n'
              << "binary_tree_maximum_height="
              << verified.statistics.binary_tree_maximum_height << '\n'
              << "binary_tree_max_nodes_per_query="
              << verified.statistics.binary_tree_maximum_nodes_per_query
              << '\n'
              << "binary_tree_frame_seconds=" << measured_seconds << '\n'
              << "binary_tree_frame_mib_per_second="
              << throughput(
                     verified.input_bytes, iterations, measured_seconds)
              << '\n';
    print_binary_tree_depth_histogram(verified.statistics);
}

void print_usage() {
    std::cerr
        << "usage: marc_lzss_match_finder_benchmark "
           "<input-file> [iterations]\n"
        << "       marc_lzss_match_finder_benchmark --frames "
           "<hash-chain-exact|binary-tree-exact> <input-file> [iterations] "
           "[frame-bytes] [window-bytes]\n"
        << "       marc_lzss_match_finder_benchmark --synthetic "
           "<hash-chain-exact|binary-tree-exact> <case> "
           "[input-bytes] [iterations] "
           "[frame-bytes] [window-bytes]\n";
}

[[nodiscard]] int run_frame_benchmark(
    const int argc, const char* const argv[]) {
    BenchmarkStrategy strategy{};
    if (argc < 4 || argc > 7 || !parse_strategy(argv[2], strategy)) {
        print_usage();
        return 2;
    }

    std::size_t iterations{1};
    std::size_t frame_size{UINT64_C(1) << 20};
    std::size_t window_size{UINT64_C(1) << 16};
    const marc::core::DecoderLimits limits{};
    if ((argc >= 5 && !parse_iterations(argv[4], iterations))
        || (argc >= 6 && !parse_size_argument(
                argv[5], limits.max_frame_size, frame_size))
        || (argc >= 7 && !parse_size_argument(
                argv[6], limits.max_lz_distance, window_size))
        || window_size > std::numeric_limits<std::uint32_t>::max()) {
        std::cerr << "invalid frame benchmark argument\n";
        return 2;
    }

    std::error_code file_error{};
    const auto raw_file_size = std::filesystem::file_size(argv[3], file_error);
    if (file_error
        || raw_file_size > std::numeric_limits<std::uint64_t>::max()) {
        std::cerr << "cannot determine input size\n";
        return 2;
    }
    const auto file_size = static_cast<std::uint64_t>(raw_file_size);

    LzssParameters parameters{};
    parameters.window_size = static_cast<std::uint32_t>(window_size);
    std::size_t workspace_size{};
    if (strategy == BenchmarkStrategy::hash_chain_exact) {
        const auto requirements = calculate_lzss_hash_chain_workspace(
            frame_size, parameters, limits);
        if (requirements.error != LzssHashChainError::none) {
            std::cerr << "cannot calculate frame HashChain workspace\n";
            return 1;
        }
        workspace_size = requirements.workspace_size;
    } else {
        const auto requirements = calculate_lzss_binary_tree_workspace(
            frame_size, parameters, limits);
        if (requirements.error != LzssBinaryTreeError::none) {
            std::cerr << "cannot calculate frame BinaryTree workspace\n";
            return 1;
        }
        workspace_size = requirements.workspace_size;
    }
    AlignedWorkspace workspace_owner(workspace_size);
    const auto workspace = workspace_owner.bytes(workspace_size);

    FrameRunResult verified{};
    const auto valid_statistics =
        strategy == BenchmarkStrategy::hash_chain_exact
        ? valid_hash_chain_statistics : valid_binary_tree_statistics;
    if (!process_frames(
            strategy, argv[3], file_size, frame_size, parameters, limits,
            workspace, true, false, verified)
        || verified.input_bytes != file_size
        || verified.statistics.query_count != verified.token_count
        || !valid_statistics(verified.statistics)) {
        std::cerr << "match-finder frame verification failed\n";
        return 1;
    }

    double measured_seconds{};
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
        FrameRunResult measured{};
        if (!process_frames(
                strategy, argv[3], file_size, frame_size, parameters, limits,
                workspace, false, true, measured)
            || measured.input_bytes != verified.input_bytes
            || measured.frame_count != verified.frame_count
            || measured.token_count != verified.token_count) {
            std::cerr << "timed match-finder frame processing failed\n";
            return 1;
        }
        measured_seconds += measured.seconds;
    }

    print_frame_report(
        strategy, "frames", {}, frame_size, window_size, iterations,
        workspace_size, verified, measured_seconds);
    return 0;
}

[[nodiscard]] int run_synthetic_benchmark(
    const int argc, const char* const argv[]) {
    BenchmarkStrategy strategy{};
    if (argc < 4 || argc > 8 || !parse_strategy(argv[2], strategy)) {
        print_usage();
        return 2;
    }
    SyntheticInputKind kind{};
    std::size_t input_size{UINT64_C(1) << 20};
    std::size_t iterations{1};
    std::size_t frame_size{UINT64_C(1) << 20};
    std::size_t window_size{UINT64_C(1) << 20};
    const marc::core::DecoderLimits limits{};
    if (!parse_synthetic_input_kind(argv[3], kind)
        || (argc >= 5 && !parse_size_argument(
                argv[4], limits.max_total_output_size, input_size))
        || (argc >= 6 && !parse_iterations(argv[5], iterations))
        || (argc >= 7 && !parse_size_argument(
                argv[6], limits.max_frame_size, frame_size))
        || (argc >= 8 && !parse_size_argument(
                argv[7], limits.max_lz_distance, window_size))
        || window_size > std::numeric_limits<std::uint32_t>::max()) {
        std::cerr << "invalid synthetic benchmark argument\n";
        return 2;
    }

    LzssParameters parameters{};
    parameters.window_size = static_cast<std::uint32_t>(window_size);
    std::size_t workspace_size{};
    if (strategy == BenchmarkStrategy::hash_chain_exact) {
        const auto requirements = calculate_lzss_hash_chain_workspace(
            frame_size, parameters, limits);
        if (requirements.error != LzssHashChainError::none) {
            std::cerr << "cannot calculate synthetic HashChain workspace\n";
            return 1;
        }
        workspace_size = requirements.workspace_size;
    } else {
        const auto requirements = calculate_lzss_binary_tree_workspace(
            frame_size, parameters, limits);
        if (requirements.error != LzssBinaryTreeError::none) {
            std::cerr << "cannot calculate synthetic BinaryTree workspace\n";
            return 1;
        }
        workspace_size = requirements.workspace_size;
    }
    AlignedWorkspace workspace_owner(workspace_size);
    const auto workspace = workspace_owner.bytes(workspace_size);

    FrameRunResult verified{};
    const auto valid_statistics =
        strategy == BenchmarkStrategy::hash_chain_exact
        ? valid_hash_chain_statistics : valid_binary_tree_statistics;
    if (!process_synthetic_frames(
            strategy, kind, input_size, frame_size, parameters, limits,
            workspace, true, false, verified)
        || verified.input_bytes != input_size
        || verified.statistics.query_count != verified.token_count
        || !valid_statistics(verified.statistics)) {
        std::cerr << "synthetic match-finder verification failed\n";
        return 1;
    }

    double measured_seconds{};
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
        FrameRunResult measured{};
        if (!process_synthetic_frames(
                strategy, kind, input_size, frame_size, parameters, limits,
                workspace, false, true, measured)
            || measured.input_bytes != verified.input_bytes
            || measured.frame_count != verified.frame_count
            || measured.token_count != verified.token_count) {
            std::cerr << "timed synthetic match-finder processing failed\n";
            return 1;
        }
        measured_seconds += measured.seconds;
    }

    print_frame_report(
        strategy, "synthetic", synthetic_input_name(kind), frame_size,
        window_size, iterations, workspace_size, verified, measured_seconds);
    return 0;
}

} // namespace

int main(const int argc, const char* const argv[]) {
    if (argc >= 2 && std::string_view{argv[1]} == "--frames") {
        return run_frame_benchmark(argc, argv);
    }
    if (argc >= 2 && std::string_view{argv[1]} == "--synthetic") {
        return run_synthetic_benchmark(argc, argv);
    }
    if (argc < 2 || argc > 3) {
        print_usage();
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

    marc::frame::StreamHeader lzss_frame_stream{};
    lzss_frame_stream.dictionary_algorithm =
        marc::frame::DictionaryAlgorithm::lzss;
    lzss_frame_stream.dictionary_variant = 1;
    lzss_frame_stream.frame_size = static_cast<std::uint32_t>(input.size());
    lzss_frame_stream.dictionary_parameters_size = lzss_parameter_size;
    lzss_frame_stream.original_size = input.size();
    const auto lzss_frame_plan = marc::frame::plan_lzss_frame(
        lzss_frame_stream, parameters, limits, 0, 0, input);
    if (lzss_frame_plan.error != marc::frame::LzssFrameCodecError::none) {
        std::cerr << "LZSS frame planning failed\n";
        return 1;
    }
    std::vector<std::byte> lzss_frame_exhaustive(
        lzss_frame_plan.serialized_size);
    std::vector<std::byte> lzss_frame_hash_chain(
        lzss_frame_plan.serialized_size);
    if (marc::frame::encode_lzss_frame(
            lzss_frame_stream, parameters, limits, 0, 0, input,
            lzss_frame_exhaustive).error
            != marc::frame::LzssFrameCodecError::none
        || marc::frame::encode_lzss_frame_hash_chain(
            lzss_frame_stream, parameters, limits, 0, 0, input, workspace,
            lzss_frame_hash_chain).error
            != marc::frame::LzssFrameCodecError::none
        || lzss_frame_exhaustive != lzss_frame_hash_chain) {
        std::cerr << "LZSS frame equivalence failed\n";
        return 1;
    }

    auto byte_blocked_stream = lzss_frame_stream;
    byte_blocked_stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::blocked_huffman;
    byte_blocked_stream.entropy_variant = 1;
    byte_blocked_stream.entropy_block_size = 65'536;
    std::vector<std::byte> byte_blocked_exhaustive_staging(
        exhaustive_plan.output_size);
    std::vector<std::byte> byte_blocked_hash_staging(
        exhaustive_plan.output_size);
    const auto byte_blocked_plan = marc::frame::
        plan_lzss_blocked_huffman_frame(
            byte_blocked_stream, parameters, limits, 0, 0, input,
            byte_blocked_exhaustive_staging);
    if (byte_blocked_plan.error != marc::frame::
            LzssBlockedHuffmanFrameValidationError::none) {
        std::cerr << "LZSS Blocked Huffman frame planning failed\n";
        return 1;
    }
    std::vector<std::byte> byte_blocked_exhaustive(
        byte_blocked_plan.serialized_size);
    std::vector<std::byte> byte_blocked_hash_chain(
        byte_blocked_plan.serialized_size);
    if (marc::frame::encode_lzss_blocked_huffman_frame(
            byte_blocked_stream, parameters, limits, 0, 0, input,
            byte_blocked_exhaustive_staging, byte_blocked_exhaustive).error
            != marc::frame::LzssBlockedHuffmanFrameValidationError::none
        || marc::frame::encode_lzss_blocked_huffman_frame_hash_chain(
            byte_blocked_stream, parameters, limits, 0, 0, input,
            byte_blocked_hash_staging, workspace,
            byte_blocked_hash_chain).error
            != marc::frame::LzssBlockedHuffmanFrameValidationError::none
        || byte_blocked_exhaustive != byte_blocked_hash_chain) {
        std::cerr << "LZSS Blocked Huffman frame equivalence failed\n";
        return 1;
    }

    auto byte_adaptive_stream = lzss_frame_stream;
    byte_adaptive_stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::adaptive_huffman;
    byte_adaptive_stream.entropy_variant = 1;
    std::vector<std::byte> byte_adaptive_exhaustive_staging(
        exhaustive_plan.output_size);
    std::vector<std::byte> byte_adaptive_hash_staging(
        exhaustive_plan.output_size);
    const auto byte_adaptive_plan = marc::frame::
        plan_lzss_adaptive_huffman_frame(
            byte_adaptive_stream, parameters, limits, 0, 0, input,
            byte_adaptive_exhaustive_staging);
    if (byte_adaptive_plan.error != marc::frame::
            LzssAdaptiveHuffmanFrameValidationError::none) {
        std::cerr << "LZSS Adaptive Huffman frame planning failed\n";
        return 1;
    }
    std::vector<std::byte> byte_adaptive_exhaustive(
        byte_adaptive_plan.serialized_size);
    std::vector<std::byte> byte_adaptive_hash_chain(
        byte_adaptive_plan.serialized_size);
    if (marc::frame::encode_lzss_adaptive_huffman_frame(
            byte_adaptive_stream, parameters, limits, 0, 0, input,
            byte_adaptive_exhaustive_staging, byte_adaptive_exhaustive).error
            != marc::frame::LzssAdaptiveHuffmanFrameValidationError::none
        || marc::frame::encode_lzss_adaptive_huffman_frame_hash_chain(
            byte_adaptive_stream, parameters, limits, 0, 0, input,
            byte_adaptive_hash_staging, workspace,
            byte_adaptive_hash_chain).error
            != marc::frame::LzssAdaptiveHuffmanFrameValidationError::none
        || byte_adaptive_exhaustive != byte_adaptive_hash_chain) {
        std::cerr << "LZSS Adaptive Huffman frame equivalence failed\n";
        return 1;
    }

    auto byte_range_stream = lzss_frame_stream;
    byte_range_stream.entropy_algorithm =
        marc::frame::EntropyAlgorithm::dynamic_range;
    byte_range_stream.entropy_variant = 1;
    std::vector<std::byte> byte_range_exhaustive_staging(
        exhaustive_plan.output_size);
    std::vector<std::byte> byte_range_hash_staging(
        exhaustive_plan.output_size);
    const auto byte_range_plan = marc::frame::plan_lzss_dynamic_range_frame(
        byte_range_stream, parameters, limits, 0, 0, input,
        byte_range_exhaustive_staging);
    if (byte_range_plan.error
        != marc::frame::LzssDynamicRangeFrameValidationError::none) {
        std::cerr << "LZSS Dynamic Range frame planning failed\n";
        return 1;
    }
    std::vector<std::byte> byte_range_exhaustive(
        byte_range_plan.serialized_size);
    std::vector<std::byte> byte_range_hash_chain(
        byte_range_plan.serialized_size);
    if (marc::frame::encode_lzss_dynamic_range_frame(
            byte_range_stream, parameters, limits, 0, 0, input,
            byte_range_exhaustive_staging, byte_range_exhaustive).error
            != marc::frame::LzssDynamicRangeFrameValidationError::none
        || marc::frame::encode_lzss_dynamic_range_frame_hash_chain(
            byte_range_stream, parameters, limits, 0, 0, input,
            byte_range_hash_staging, workspace, byte_range_hash_chain).error
            != marc::frame::LzssDynamicRangeFrameValidationError::none
        || byte_range_exhaustive != byte_range_hash_chain) {
        std::cerr << "LZSS Dynamic Range frame equivalence failed\n";
        return 1;
    }

    auto byte_rans_stream = lzss_frame_stream;
    byte_rans_stream.entropy_algorithm = marc::frame::EntropyAlgorithm::rans;
    byte_rans_stream.entropy_variant = 1;
    byte_rans_stream.entropy_block_size = 65'536;
    std::vector<std::byte> byte_rans_exhaustive_staging(
        exhaustive_plan.output_size);
    std::vector<std::byte> byte_rans_hash_staging(
        exhaustive_plan.output_size);
    const auto byte_rans_plan = marc::frame::plan_lzss_rans_frame(
        byte_rans_stream, parameters, limits, 0, 0, input,
        byte_rans_exhaustive_staging);
    if (byte_rans_plan.error
        != marc::frame::LzssRansFrameValidationError::none) {
        std::cerr << "LZSS rANS frame planning failed\n";
        return 1;
    }
    std::vector<std::byte> byte_rans_exhaustive(
        byte_rans_plan.serialized_size);
    std::vector<std::byte> byte_rans_hash_chain(
        byte_rans_plan.serialized_size);
    if (marc::frame::encode_lzss_rans_frame(
            byte_rans_stream, parameters, limits, 0, 0, input,
            byte_rans_exhaustive_staging, byte_rans_exhaustive).error
            != marc::frame::LzssRansFrameValidationError::none
        || marc::frame::encode_lzss_rans_frame_hash_chain(
            byte_rans_stream, parameters, limits, 0, 0, input,
            byte_rans_hash_staging, workspace, byte_rans_hash_chain).error
            != marc::frame::LzssRansFrameValidationError::none
        || byte_rans_exhaustive != byte_rans_hash_chain) {
        std::cerr << "LZSS rANS frame equivalence failed\n";
        return 1;
    }

    auto byte_tans_stream = lzss_frame_stream;
    byte_tans_stream.entropy_algorithm = marc::frame::EntropyAlgorithm::tans;
    byte_tans_stream.entropy_variant = 1;
    byte_tans_stream.entropy_block_size = 65'536;
    std::vector<std::byte> byte_tans_exhaustive_staging(
        exhaustive_plan.output_size);
    std::vector<std::byte> byte_tans_hash_staging(
        exhaustive_plan.output_size);
    const auto byte_tans_plan = marc::frame::plan_lzss_tans_frame(
        byte_tans_stream, parameters, limits, 0, 0, input,
        byte_tans_exhaustive_staging);
    if (byte_tans_plan.error
        != marc::frame::LzssTansFrameValidationError::none) {
        std::cerr << "LZSS tANS frame planning failed\n";
        return 1;
    }
    std::vector<std::byte> byte_tans_exhaustive(
        byte_tans_plan.serialized_size);
    std::vector<std::byte> byte_tans_hash_chain(
        byte_tans_plan.serialized_size);
    if (marc::frame::encode_lzss_tans_frame(
            byte_tans_stream, parameters, limits, 0, 0, input,
            byte_tans_exhaustive_staging, byte_tans_exhaustive).error
            != marc::frame::LzssTansFrameValidationError::none
        || marc::frame::encode_lzss_tans_frame_hash_chain(
            byte_tans_stream, parameters, limits, 0, 0, input,
            byte_tans_hash_staging, workspace, byte_tans_hash_chain).error
            != marc::frame::LzssTansFrameValidationError::none
        || byte_tans_exhaustive != byte_tans_hash_chain) {
        std::cerr << "LZSS tANS frame equivalence failed\n";
        return 1;
    }

    const auto typed_plan = plan_lzss_typed_tokens_hash_chain(
        input, parameters, limits, workspace);
    if (typed_plan.error != LzssTypedEncodeError::none
        || typed_plan.token_count != hash_plan.token_count) {
        std::cerr << "typed planning equivalence failed\n";
        return 1;
    }
    std::vector<LzssTypedToken> typed_two_pass(input.size());
    std::vector<LzssTypedToken> typed_single_pass(input.size());
    const auto typed_two_pass_result = encode_lzss_typed_tokens_hash_chain(
        input, parameters, limits, typed_two_pass, workspace);
    const auto typed_single_pass_result =
        encode_lzss_typed_tokens_hash_chain_single_pass(
            input, parameters, limits, typed_single_pass, workspace);
    if (typed_two_pass_result.error != LzssTypedEncodeError::none
        || typed_single_pass_result.error != LzssTypedEncodeError::none
        || typed_two_pass_result.token_count
            != typed_single_pass_result.token_count
        || !std::equal(
            typed_two_pass.begin(),
            typed_two_pass.begin() + typed_two_pass_result.token_count,
            typed_single_pass.begin(),
            [](const LzssTypedToken& left,
               const LzssTypedToken& right) noexcept {
                return left.kind == right.kind
                    && left.literal == right.literal
                    && left.distance == right.distance
                    && left.length == right.length;
            })) {
        std::cerr << "typed single-pass equivalence failed\n";
        return 1;
    }

    marc::frame::internal::TypedContextStreamHeader frame_stream{};
    frame_stream.frame_size = static_cast<std::uint32_t>(input.size());
    frame_stream.original_size = input.size();
    frame_stream.range_model_total =
        marc::frame::internal::typed_context_model_total;
    frame_stream.context_count =
        marc::frame::internal::typed_context_count;
    std::vector<marc::context::internal::ModeledOperation> operations(
        input.size() * 5U);
    const auto frame_plan =
        marc::frame::internal::plan_lzss_typed_context_frame(
            frame_stream, limits, 0, 0, input, typed_two_pass, operations);
    if (frame_plan.error
        != marc::frame::internal::LzssTypedContextFrameEncodeError::none) {
        std::cerr << "contextual frame planning failed\n";
        return 1;
    }
    std::vector<std::byte> frame_exhaustive(frame_plan.serialized_size);
    std::vector<std::byte> frame_hash_chain(frame_plan.serialized_size);
    if (marc::frame::internal::encode_lzss_typed_context_frame(
            frame_stream, limits, 0, 0, input, typed_two_pass, operations,
            frame_exhaustive).error
            != marc::frame::internal::LzssTypedContextFrameEncodeError::none
        || marc::frame::internal::encode_lzss_typed_context_frame_hash_chain(
            frame_stream, limits, 0, 0, input, typed_single_pass, operations,
            workspace, frame_hash_chain).error
            != marc::frame::internal::LzssTypedContextFrameEncodeError::none
        || frame_exhaustive != frame_hash_chain) {
        std::cerr << "contextual frame equivalence failed\n";
        return 1;
    }

    marc::frame::internal::LzssContextualRansStreamHeader rans_stream{};
    rans_stream.frame_size = static_cast<std::uint32_t>(input.size());
    rans_stream.original_size = input.size();
    const auto rans_plan =
        marc::frame::internal::plan_lzss_contextual_rans_frame(
            rans_stream, limits, 0, 0, input, typed_two_pass);
    if (rans_plan.error
        != marc::frame::internal::LzssContextualRansFrameEncodeError::none) {
        std::cerr << "contextual rANS frame planning failed\n";
        return 1;
    }
    std::vector<std::byte> rans_exhaustive(rans_plan.serialized_size);
    std::vector<std::byte> rans_hash_chain(rans_plan.serialized_size);
    if (marc::frame::internal::encode_lzss_contextual_rans_frame(
            rans_stream, limits, 0, 0, input, typed_two_pass,
            rans_exhaustive).error
            != marc::frame::internal::
                LzssContextualRansFrameEncodeError::none
        || marc::frame::internal::
            encode_lzss_contextual_rans_frame_hash_chain(
                rans_stream, limits, 0, 0, input, typed_single_pass,
                workspace, rans_hash_chain).error
            != marc::frame::internal::
                LzssContextualRansFrameEncodeError::none
        || rans_exhaustive != rans_hash_chain) {
        std::cerr << "contextual rANS frame equivalence failed\n";
        return 1;
    }

    marc::frame::internal::LzssContextualTansStreamHeader tans_stream{};
    tans_stream.frame_size = static_cast<std::uint32_t>(input.size());
    tans_stream.original_size = input.size();
    std::vector<std::uint16_t> tans_tables(
        marc::entropy::internal::contextual_tans_encode_table_entries);
    const auto tans_plan =
        marc::frame::internal::plan_lzss_contextual_tans_frame(
            tans_stream, limits, 0, 0, input, typed_two_pass, tans_tables);
    if (tans_plan.error
        != marc::frame::internal::LzssContextualTansFrameEncodeError::none) {
        std::cerr << "contextual tANS frame planning failed\n";
        return 1;
    }
    std::vector<std::byte> tans_exhaustive(tans_plan.serialized_size);
    std::vector<std::byte> tans_hash_chain(tans_plan.serialized_size);
    if (marc::frame::internal::encode_lzss_contextual_tans_frame(
            tans_stream, limits, 0, 0, input, typed_two_pass, tans_tables,
            tans_exhaustive).error
            != marc::frame::internal::
                LzssContextualTansFrameEncodeError::none
        || marc::frame::internal::
            encode_lzss_contextual_tans_frame_hash_chain(
                tans_stream, limits, 0, 0, input, typed_single_pass,
                tans_tables, workspace, tans_hash_chain).error
            != marc::frame::internal::
                LzssContextualTansFrameEncodeError::none
        || tans_exhaustive != tans_hash_chain) {
        std::cerr << "contextual tANS frame equivalence failed\n";
        return 1;
    }

    marc::frame::internal::LzssContextualBlockedHuffmanStreamHeader
        blocked_huffman_stream{};
    blocked_huffman_stream.frame_size =
        static_cast<std::uint32_t>(input.size());
    blocked_huffman_stream.original_size = input.size();
    const auto blocked_huffman_plan = marc::frame::internal::
        plan_lzss_contextual_blocked_huffman_frame(
            blocked_huffman_stream, limits, 0, 0, input, typed_two_pass);
    if (blocked_huffman_plan.error != marc::frame::internal::
            LzssContextualBlockedHuffmanFrameEncodeError::none) {
        std::cerr << "Contextual Blocked Huffman frame planning failed\n";
        return 1;
    }
    std::vector<std::byte> blocked_huffman_exhaustive(
        blocked_huffman_plan.serialized_size);
    std::vector<std::byte> blocked_huffman_hash_chain(
        blocked_huffman_plan.serialized_size);
    if (marc::frame::internal::
            encode_lzss_contextual_blocked_huffman_frame(
                blocked_huffman_stream, limits, 0, 0, input,
                typed_two_pass, blocked_huffman_exhaustive).error
            != marc::frame::internal::
                LzssContextualBlockedHuffmanFrameEncodeError::none
        || marc::frame::internal::
            encode_lzss_contextual_blocked_huffman_frame_hash_chain(
                blocked_huffman_stream, limits, 0, 0, input,
                typed_single_pass, workspace,
                blocked_huffman_hash_chain).error
            != marc::frame::internal::
                LzssContextualBlockedHuffmanFrameEncodeError::none
        || blocked_huffman_exhaustive != blocked_huffman_hash_chain) {
        std::cerr << "Contextual Blocked Huffman frame equivalence failed\n";
        return 1;
    }

    marc::frame::internal::LzssContextualAdaptiveHuffmanStreamHeader
        adaptive_huffman_stream{};
    adaptive_huffman_stream.frame_size =
        static_cast<std::uint32_t>(input.size());
    adaptive_huffman_stream.original_size = input.size();
    std::vector<marc::entropy::internal::AdaptiveHuffmanNode> adaptive_nodes(
        marc::entropy::internal::
            contextual_adaptive_huffman_node_entries);
    std::vector<std::uint16_t> adaptive_symbols(
        marc::entropy::internal::
            contextual_adaptive_huffman_symbol_entries);
    const auto adaptive_huffman_plan = marc::frame::internal::
        plan_lzss_contextual_adaptive_huffman_frame(
            adaptive_huffman_stream, limits, 0, 0, input, typed_two_pass,
            adaptive_nodes, adaptive_symbols);
    if (adaptive_huffman_plan.error != marc::frame::internal::
            LzssContextualAdaptiveHuffmanFrameEncodeError::none) {
        std::cerr << "Contextual Adaptive Huffman frame planning failed\n";
        return 1;
    }
    std::vector<std::byte> adaptive_huffman_exhaustive(
        adaptive_huffman_plan.serialized_size);
    std::vector<std::byte> adaptive_huffman_hash_chain(
        adaptive_huffman_plan.serialized_size);
    if (marc::frame::internal::
            encode_lzss_contextual_adaptive_huffman_frame(
                adaptive_huffman_stream, limits, 0, 0, input,
                typed_two_pass, adaptive_nodes, adaptive_symbols,
                adaptive_huffman_exhaustive).error
            != marc::frame::internal::
                LzssContextualAdaptiveHuffmanFrameEncodeError::none
        || marc::frame::internal::
            encode_lzss_contextual_adaptive_huffman_frame_hash_chain(
                adaptive_huffman_stream, limits, 0, 0, input,
                typed_single_pass, adaptive_nodes, adaptive_symbols,
                workspace, adaptive_huffman_hash_chain).error
            != marc::frame::internal::
                LzssContextualAdaptiveHuffmanFrameEncodeError::none
        || adaptive_huffman_exhaustive != adaptive_huffman_hash_chain) {
        std::cerr << "Contextual Adaptive Huffman frame equivalence failed\n";
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
    const auto lzss_frame_exhaustive_seconds = measure_seconds(
        iterations, [&] {
            timing_ok = timing_ok && marc::frame::encode_lzss_frame(
                lzss_frame_stream, parameters, limits, 0, 0, input,
                lzss_frame_exhaustive).error
                == marc::frame::LzssFrameCodecError::none;
        });
    const auto lzss_frame_hash_chain_seconds = measure_seconds(
        iterations, [&] {
            timing_ok = timing_ok
                && marc::frame::encode_lzss_frame_hash_chain(
                    lzss_frame_stream, parameters, limits, 0, 0, input,
                    workspace, lzss_frame_hash_chain).error
                    == marc::frame::LzssFrameCodecError::none;
        });
    const auto byte_blocked_exhaustive_seconds = measure_seconds(
        iterations, [&] {
            timing_ok = timing_ok
                && marc::frame::encode_lzss_blocked_huffman_frame(
                    byte_blocked_stream, parameters, limits, 0, 0, input,
                    byte_blocked_exhaustive_staging,
                    byte_blocked_exhaustive).error
                    == marc::frame::
                        LzssBlockedHuffmanFrameValidationError::none;
        });
    const auto byte_blocked_hash_chain_seconds = measure_seconds(
        iterations, [&] {
            timing_ok = timing_ok
                && marc::frame::
                    encode_lzss_blocked_huffman_frame_hash_chain(
                        byte_blocked_stream, parameters, limits, 0, 0, input,
                        byte_blocked_hash_staging, workspace,
                        byte_blocked_hash_chain).error
                    == marc::frame::
                        LzssBlockedHuffmanFrameValidationError::none;
        });
    const auto byte_adaptive_exhaustive_seconds = measure_seconds(
        iterations, [&] {
            timing_ok = timing_ok
                && marc::frame::encode_lzss_adaptive_huffman_frame(
                    byte_adaptive_stream, parameters, limits, 0, 0, input,
                    byte_adaptive_exhaustive_staging,
                    byte_adaptive_exhaustive).error
                    == marc::frame::
                        LzssAdaptiveHuffmanFrameValidationError::none;
        });
    const auto byte_adaptive_hash_chain_seconds = measure_seconds(
        iterations, [&] {
            timing_ok = timing_ok
                && marc::frame::encode_lzss_adaptive_huffman_frame_hash_chain(
                    byte_adaptive_stream, parameters, limits, 0, 0, input,
                    byte_adaptive_hash_staging, workspace,
                    byte_adaptive_hash_chain).error
                    == marc::frame::
                        LzssAdaptiveHuffmanFrameValidationError::none;
        });
    const auto byte_range_exhaustive_seconds = measure_seconds(
        iterations, [&] {
            timing_ok = timing_ok
                && marc::frame::encode_lzss_dynamic_range_frame(
                    byte_range_stream, parameters, limits, 0, 0, input,
                    byte_range_exhaustive_staging,
                    byte_range_exhaustive).error
                    == marc::frame::
                        LzssDynamicRangeFrameValidationError::none;
        });
    const auto byte_range_hash_chain_seconds = measure_seconds(
        iterations, [&] {
            timing_ok = timing_ok
                && marc::frame::encode_lzss_dynamic_range_frame_hash_chain(
                    byte_range_stream, parameters, limits, 0, 0, input,
                    byte_range_hash_staging, workspace,
                    byte_range_hash_chain).error
                    == marc::frame::
                        LzssDynamicRangeFrameValidationError::none;
        });
    const auto byte_rans_exhaustive_seconds = measure_seconds(
        iterations, [&] {
            timing_ok = timing_ok
                && marc::frame::encode_lzss_rans_frame(
                    byte_rans_stream, parameters, limits, 0, 0, input,
                    byte_rans_exhaustive_staging,
                    byte_rans_exhaustive).error
                    == marc::frame::LzssRansFrameValidationError::none;
        });
    const auto byte_rans_hash_chain_seconds = measure_seconds(
        iterations, [&] {
            timing_ok = timing_ok
                && marc::frame::encode_lzss_rans_frame_hash_chain(
                    byte_rans_stream, parameters, limits, 0, 0, input,
                    byte_rans_hash_staging, workspace,
                    byte_rans_hash_chain).error
                    == marc::frame::LzssRansFrameValidationError::none;
        });
    const auto byte_tans_exhaustive_seconds = measure_seconds(
        iterations, [&] {
            timing_ok = timing_ok
                && marc::frame::encode_lzss_tans_frame(
                    byte_tans_stream, parameters, limits, 0, 0, input,
                    byte_tans_exhaustive_staging,
                    byte_tans_exhaustive).error
                    == marc::frame::LzssTansFrameValidationError::none;
        });
    const auto byte_tans_hash_chain_seconds = measure_seconds(
        iterations, [&] {
            timing_ok = timing_ok
                && marc::frame::encode_lzss_tans_frame_hash_chain(
                    byte_tans_stream, parameters, limits, 0, 0, input,
                    byte_tans_hash_staging, workspace,
                    byte_tans_hash_chain).error
                    == marc::frame::LzssTansFrameValidationError::none;
        });
    const auto typed_two_pass_seconds = measure_seconds(iterations, [&] {
        timing_ok = timing_ok && encode_lzss_typed_tokens_hash_chain(
            input, parameters, limits, typed_two_pass, workspace).error
            == LzssTypedEncodeError::none;
    });
    const auto typed_single_pass_seconds = measure_seconds(iterations, [&] {
        timing_ok = timing_ok
            && encode_lzss_typed_tokens_hash_chain_single_pass(
                input, parameters, limits, typed_single_pass, workspace).error
                == LzssTypedEncodeError::none;
    });
    const auto frame_exhaustive_seconds = measure_seconds(iterations, [&] {
        timing_ok = timing_ok
            && marc::frame::internal::encode_lzss_typed_context_frame(
                frame_stream, limits, 0, 0, input, typed_two_pass, operations,
                frame_exhaustive).error
                == marc::frame::internal::
                    LzssTypedContextFrameEncodeError::none;
    });
    const auto frame_hash_chain_seconds = measure_seconds(iterations, [&] {
        timing_ok = timing_ok
            && marc::frame::internal::
                encode_lzss_typed_context_frame_hash_chain(
                    frame_stream, limits, 0, 0, input, typed_single_pass,
                    operations, workspace, frame_hash_chain).error
                == marc::frame::internal::
                    LzssTypedContextFrameEncodeError::none;
    });
    const auto rans_exhaustive_seconds = measure_seconds(iterations, [&] {
        timing_ok = timing_ok
            && marc::frame::internal::encode_lzss_contextual_rans_frame(
                rans_stream, limits, 0, 0, input, typed_two_pass,
                rans_exhaustive).error
                == marc::frame::internal::
                    LzssContextualRansFrameEncodeError::none;
    });
    const auto rans_hash_chain_seconds = measure_seconds(iterations, [&] {
        timing_ok = timing_ok
            && marc::frame::internal::
                encode_lzss_contextual_rans_frame_hash_chain(
                    rans_stream, limits, 0, 0, input, typed_single_pass,
                    workspace, rans_hash_chain).error
                == marc::frame::internal::
                    LzssContextualRansFrameEncodeError::none;
    });
    const auto tans_exhaustive_seconds = measure_seconds(iterations, [&] {
        timing_ok = timing_ok
            && marc::frame::internal::encode_lzss_contextual_tans_frame(
                tans_stream, limits, 0, 0, input, typed_two_pass,
                tans_tables, tans_exhaustive).error
                == marc::frame::internal::
                    LzssContextualTansFrameEncodeError::none;
    });
    const auto tans_hash_chain_seconds = measure_seconds(iterations, [&] {
        timing_ok = timing_ok
            && marc::frame::internal::
                encode_lzss_contextual_tans_frame_hash_chain(
                    tans_stream, limits, 0, 0, input, typed_single_pass,
                    tans_tables, workspace, tans_hash_chain).error
                == marc::frame::internal::
                    LzssContextualTansFrameEncodeError::none;
    });
    const auto blocked_huffman_exhaustive_seconds = measure_seconds(
        iterations, [&] {
            timing_ok = timing_ok && marc::frame::internal::
                encode_lzss_contextual_blocked_huffman_frame(
                    blocked_huffman_stream, limits, 0, 0, input,
                    typed_two_pass, blocked_huffman_exhaustive).error
                == marc::frame::internal::
                    LzssContextualBlockedHuffmanFrameEncodeError::none;
        });
    const auto blocked_huffman_hash_chain_seconds = measure_seconds(
        iterations, [&] {
            timing_ok = timing_ok && marc::frame::internal::
                encode_lzss_contextual_blocked_huffman_frame_hash_chain(
                    blocked_huffman_stream, limits, 0, 0, input,
                    typed_single_pass, workspace,
                    blocked_huffman_hash_chain).error
                == marc::frame::internal::
                    LzssContextualBlockedHuffmanFrameEncodeError::none;
        });
    const auto adaptive_huffman_exhaustive_seconds = measure_seconds(
        iterations, [&] {
            timing_ok = timing_ok && marc::frame::internal::
                encode_lzss_contextual_adaptive_huffman_frame(
                    adaptive_huffman_stream, limits, 0, 0, input,
                    typed_two_pass, adaptive_nodes, adaptive_symbols,
                    adaptive_huffman_exhaustive).error
                == marc::frame::internal::
                    LzssContextualAdaptiveHuffmanFrameEncodeError::none;
        });
    const auto adaptive_huffman_hash_chain_seconds = measure_seconds(
        iterations, [&] {
            timing_ok = timing_ok && marc::frame::internal::
                encode_lzss_contextual_adaptive_huffman_frame_hash_chain(
                    adaptive_huffman_stream, limits, 0, 0, input,
                    typed_single_pass, adaptive_nodes, adaptive_symbols,
                    workspace, adaptive_huffman_hash_chain).error
                == marc::frame::internal::
                    LzssContextualAdaptiveHuffmanFrameEncodeError::none;
        });
    if (!timing_ok || exhaustive_output != hash_output
        || lzss_frame_exhaustive != lzss_frame_hash_chain
        || byte_blocked_exhaustive != byte_blocked_hash_chain
        || byte_adaptive_exhaustive != byte_adaptive_hash_chain
        || byte_range_exhaustive != byte_range_hash_chain
        || byte_rans_exhaustive != byte_rans_hash_chain
        || byte_tans_exhaustive != byte_tans_hash_chain
        || frame_exhaustive != frame_hash_chain
        || rans_exhaustive != rans_hash_chain
        || tans_exhaustive != tans_hash_chain
        || blocked_huffman_exhaustive != blocked_huffman_hash_chain
        || adaptive_huffman_exhaustive != adaptive_huffman_hash_chain) {
        std::cerr << "timed encoding failed\n";
        return 1;
    }

    std::cout << std::fixed << std::setprecision(6)
              << "input_bytes=" << input.size() << '\n'
              << "output_bytes=" << hash_output.size() << '\n'
              << "lzss_frame_bytes=" << lzss_frame_hash_chain.size() << '\n'
              << "lzss_blocked_huffman_frame_bytes="
              << byte_blocked_hash_chain.size() << '\n'
              << "lzss_adaptive_huffman_frame_bytes="
              << byte_adaptive_hash_chain.size() << '\n'
              << "lzss_dynamic_range_frame_bytes="
              << byte_range_hash_chain.size() << '\n'
              << "lzss_rans_frame_bytes=" << byte_rans_hash_chain.size()
              << '\n'
              << "lzss_tans_frame_bytes=" << byte_tans_hash_chain.size()
              << '\n'
              << "token_count=" << hash_plan.token_count << '\n'
              << "contextual_frame_bytes=" << frame_hash_chain.size() << '\n'
              << "contextual_rans_frame_bytes=" << rans_hash_chain.size()
              << '\n'
              << "contextual_tans_frame_bytes=" << tans_hash_chain.size()
              << '\n'
              << "contextual_blocked_huffman_frame_bytes="
              << blocked_huffman_hash_chain.size() << '\n'
              << "contextual_adaptive_huffman_frame_bytes="
              << adaptive_huffman_hash_chain.size() << '\n'
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
    print_measurement("lzss_frame_exhaustive",
                      lzss_frame_exhaustive_seconds, input.size(), iterations);
    print_measurement("lzss_frame_hash_chain",
                      lzss_frame_hash_chain_seconds, input.size(), iterations);
    print_measurement("lzss_blocked_huffman_frame_exhaustive",
                      byte_blocked_exhaustive_seconds, input.size(),
                      iterations);
    print_measurement("lzss_blocked_huffman_frame_hash_chain",
                      byte_blocked_hash_chain_seconds, input.size(),
                      iterations);
    print_measurement("lzss_adaptive_huffman_frame_exhaustive",
                      byte_adaptive_exhaustive_seconds, input.size(),
                      iterations);
    print_measurement("lzss_adaptive_huffman_frame_hash_chain",
                      byte_adaptive_hash_chain_seconds, input.size(),
                      iterations);
    print_measurement("lzss_dynamic_range_frame_exhaustive",
                      byte_range_exhaustive_seconds, input.size(), iterations);
    print_measurement("lzss_dynamic_range_frame_hash_chain",
                      byte_range_hash_chain_seconds, input.size(), iterations);
    print_measurement("lzss_rans_frame_exhaustive",
                      byte_rans_exhaustive_seconds, input.size(), iterations);
    print_measurement("lzss_rans_frame_hash_chain",
                      byte_rans_hash_chain_seconds, input.size(), iterations);
    print_measurement("lzss_tans_frame_exhaustive",
                      byte_tans_exhaustive_seconds, input.size(), iterations);
    print_measurement("lzss_tans_frame_hash_chain",
                      byte_tans_hash_chain_seconds, input.size(), iterations);
    print_measurement("hash_chain_typed_two_pass", typed_two_pass_seconds,
                      input.size(), iterations);
    print_measurement("hash_chain_typed_single_pass",
                      typed_single_pass_seconds, input.size(), iterations);
    print_measurement("contextual_frame_exhaustive",
                      frame_exhaustive_seconds, input.size(), iterations);
    print_measurement("contextual_frame_hash_chain",
                      frame_hash_chain_seconds, input.size(), iterations);
    print_measurement("contextual_rans_frame_exhaustive",
                      rans_exhaustive_seconds, input.size(), iterations);
    print_measurement("contextual_rans_frame_hash_chain",
                      rans_hash_chain_seconds, input.size(), iterations);
    print_measurement("contextual_tans_frame_exhaustive",
                      tans_exhaustive_seconds, input.size(), iterations);
    print_measurement("contextual_tans_frame_hash_chain",
                      tans_hash_chain_seconds, input.size(), iterations);
    print_measurement("contextual_blocked_huffman_frame_exhaustive",
                      blocked_huffman_exhaustive_seconds, input.size(),
                      iterations);
    print_measurement("contextual_blocked_huffman_frame_hash_chain",
                      blocked_huffman_hash_chain_seconds, input.size(),
                      iterations);
    print_measurement("contextual_adaptive_huffman_frame_exhaustive",
                      adaptive_huffman_exhaustive_seconds, input.size(),
                      iterations);
    print_measurement("contextual_adaptive_huffman_frame_hash_chain",
                      adaptive_huffman_hash_chain_seconds, input.size(),
                      iterations);
    return 0;
}
