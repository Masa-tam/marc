#include "dictionary/lzss_encoder.hpp"
#include "dictionary/lzss_binary_tree_match_finder.hpp"
#include "dictionary/lzss_hash_chain_match_finder.hpp"
#include "dictionary/lzss_hash_tree_match_finder.hpp"
#include "dictionary/lzss_match_finder.hpp"
#include "dictionary/lzss_red_black_tree_match_finder.hpp"
#include "dictionary/lzss_scapegoat_tree_match_finder.hpp"
#include "dictionary/lzss_sparse_hash_tree_match_finder.hpp"
#include "dictionary/lzss_typed_encoder.hpp"
#include "dictionary/lzss_wavl_tree_match_finder.hpp"
#include "core/checked_math.hpp"
#include "core/sha256.hpp"
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
    hash_chain_legacy_65536_exact,
    hash_chain_mnemonic_mixer_v1_exact,
    hash_chain_best_length_probe_exact,
    hash_chain_buckets_262144_exact,
    hash_chain_buckets_1048576_exact,
    hash_chain_buckets_4194304_exact,
    binary_tree_exact,
    wavl_tree_exact,
    red_black_tree_exact,
    scapegoat_tree_exact,
    hash_tree_exact,
    sparse_hash_tree_exact,
    sparse_hash_tree_reuse_gated_exact,
    sparse_hash_tree_immutable_snapshot_exact,
    sparse_hash_tree_snapshot_delta_budget_exact,
};

[[nodiscard]] bool parse_strategy(
    const std::string_view text, BenchmarkStrategy& strategy) noexcept {
    if (text == "hash-chain-exact") {
        strategy = BenchmarkStrategy::hash_chain_exact;
    } else if (text == "hash-chain-legacy-65536-exact") {
        strategy = BenchmarkStrategy::hash_chain_legacy_65536_exact;
    } else if (text == "hash-chain-best-length-probe-exact") {
        strategy = BenchmarkStrategy::hash_chain_best_length_probe_exact;
    } else if (text == "hash-chain-mnemonic-mixer-v1-exact") {
        strategy = BenchmarkStrategy::hash_chain_mnemonic_mixer_v1_exact;
    } else if (text == "hash-chain-buckets-262144-exact") {
        strategy = BenchmarkStrategy::hash_chain_buckets_262144_exact;
    } else if (text == "hash-chain-buckets-1048576-exact") {
        strategy = BenchmarkStrategy::hash_chain_buckets_1048576_exact;
    } else if (text == "hash-chain-buckets-4194304-exact") {
        strategy = BenchmarkStrategy::hash_chain_buckets_4194304_exact;
    } else if (text == "binary-tree-exact") {
        strategy = BenchmarkStrategy::binary_tree_exact;
    } else if (text == "wavl-tree-exact") {
        strategy = BenchmarkStrategy::wavl_tree_exact;
    } else if (text == "red-black-tree-exact") {
        strategy = BenchmarkStrategy::red_black_tree_exact;
    } else if (text == "scapegoat-tree-exact") {
        strategy = BenchmarkStrategy::scapegoat_tree_exact;
    } else if (text == "hash-tree-exact") {
        strategy = BenchmarkStrategy::hash_tree_exact;
    } else if (text == "sparse-hash-tree-exact") {
        strategy = BenchmarkStrategy::sparse_hash_tree_exact;
    } else if (text == "sparse-hash-tree-reuse-gated-exact") {
        strategy = BenchmarkStrategy::sparse_hash_tree_reuse_gated_exact;
    } else if (text == "sparse-hash-tree-immutable-snapshot-exact") {
        strategy =
            BenchmarkStrategy::sparse_hash_tree_immutable_snapshot_exact;
    } else if (text == "sparse-hash-tree-snapshot-delta-budget-exact") {
        strategy =
            BenchmarkStrategy::sparse_hash_tree_snapshot_delta_budget_exact;
    } else {
        return false;
    }
    return true;
}

[[nodiscard]] std::string_view strategy_name(
    const BenchmarkStrategy strategy) noexcept {
    switch (strategy) {
    case BenchmarkStrategy::hash_chain_exact: return "hash-chain-exact";
    case BenchmarkStrategy::hash_chain_best_length_probe_exact:
        return "hash-chain-best-length-probe-exact";
    case BenchmarkStrategy::hash_chain_legacy_65536_exact:
        return "hash-chain-legacy-65536-exact";
    case BenchmarkStrategy::hash_chain_mnemonic_mixer_v1_exact:
        return "hash-chain-mnemonic-mixer-v1-exact";
    case BenchmarkStrategy::hash_chain_buckets_262144_exact:
        return "hash-chain-buckets-262144-exact";
    case BenchmarkStrategy::hash_chain_buckets_1048576_exact:
        return "hash-chain-buckets-1048576-exact";
    case BenchmarkStrategy::hash_chain_buckets_4194304_exact:
        return "hash-chain-buckets-4194304-exact";
    case BenchmarkStrategy::binary_tree_exact: return "binary-tree-exact";
    case BenchmarkStrategy::wavl_tree_exact: return "wavl-tree-exact";
    case BenchmarkStrategy::red_black_tree_exact:
        return "red-black-tree-exact";
    case BenchmarkStrategy::scapegoat_tree_exact:
        return "scapegoat-tree-exact";
    case BenchmarkStrategy::hash_tree_exact: return "hash-tree-exact";
    case BenchmarkStrategy::sparse_hash_tree_exact:
        return "sparse-hash-tree-exact";
    case BenchmarkStrategy::sparse_hash_tree_reuse_gated_exact:
        return "sparse-hash-tree-reuse-gated-exact";
    case BenchmarkStrategy::sparse_hash_tree_immutable_snapshot_exact:
        return "sparse-hash-tree-immutable-snapshot-exact";
    case BenchmarkStrategy::sparse_hash_tree_snapshot_delta_budget_exact:
        return "sparse-hash-tree-snapshot-delta-budget-exact";
    }
    return "unknown";
}

[[nodiscard]] bool is_hash_chain_strategy(
    const BenchmarkStrategy strategy) noexcept {
    return strategy == BenchmarkStrategy::hash_chain_exact
        || strategy == BenchmarkStrategy::hash_chain_best_length_probe_exact
        || strategy == BenchmarkStrategy::hash_chain_legacy_65536_exact
        || strategy
            == BenchmarkStrategy::hash_chain_mnemonic_mixer_v1_exact
        || strategy == BenchmarkStrategy::hash_chain_buckets_262144_exact
        || strategy == BenchmarkStrategy::hash_chain_buckets_1048576_exact
        || strategy == BenchmarkStrategy::hash_chain_buckets_4194304_exact;
}

[[nodiscard]] std::size_t configured_hash_chain_bucket_cap(
    const BenchmarkStrategy strategy) noexcept {
    switch (strategy) {
    case BenchmarkStrategy::hash_chain_exact:
        return lzss_hash_chain_production_bucket_cap;
    case BenchmarkStrategy::hash_chain_best_length_probe_exact:
        return lzss_hash_chain_production_bucket_cap;
    case BenchmarkStrategy::hash_chain_legacy_65536_exact:
    case BenchmarkStrategy::hash_chain_mnemonic_mixer_v1_exact:
        return lzss_hash_chain_legacy_bucket_cap;
    case BenchmarkStrategy::hash_chain_buckets_262144_exact:
        return lzss_hash_chain_bucket_cap_262144;
    case BenchmarkStrategy::hash_chain_buckets_1048576_exact:
        return lzss_hash_chain_bucket_cap_1048576;
    case BenchmarkStrategy::hash_chain_buckets_4194304_exact:
        return lzss_hash_chain_bucket_cap_4194304;
    default: return 0;
    }
}

[[nodiscard]] LzssHashChainWorkspaceRequirements
calculate_hash_chain_workspace_for_strategy(
    const BenchmarkStrategy strategy, const std::size_t input_size,
    const LzssParameters& parameters,
    const marc::core::DecoderLimits& limits) noexcept {
    if (strategy == BenchmarkStrategy::hash_chain_exact
        || strategy == BenchmarkStrategy::hash_chain_best_length_probe_exact) {
        return calculate_lzss_hash_chain_workspace(
            input_size, parameters, limits);
    }
    const auto bucket_cap = configured_hash_chain_bucket_cap(strategy);
    return calculate_lzss_hash_chain_workspace_with_private_bucket_cap(
        input_size, parameters, limits, bucket_cap);
}

[[nodiscard]] bool is_sparse_hash_tree_strategy(
    const BenchmarkStrategy strategy) noexcept {
    return strategy == BenchmarkStrategy::sparse_hash_tree_exact
        || strategy == BenchmarkStrategy::sparse_hash_tree_reuse_gated_exact
        || strategy
            == BenchmarkStrategy::sparse_hash_tree_immutable_snapshot_exact
        || strategy
            == BenchmarkStrategy::sparse_hash_tree_snapshot_delta_budget_exact;
}

[[nodiscard]] bool uses_sparse_reuse_argument(
    const BenchmarkStrategy strategy) noexcept {
    return strategy == BenchmarkStrategy::sparse_hash_tree_reuse_gated_exact
        || strategy
            == BenchmarkStrategy::sparse_hash_tree_immutable_snapshot_exact
        || strategy
            == BenchmarkStrategy::sparse_hash_tree_snapshot_delta_budget_exact;
}

[[nodiscard]] bool uses_immutable_snapshot_lifecycle(
    const BenchmarkStrategy strategy) noexcept {
    return strategy
            == BenchmarkStrategy::sparse_hash_tree_immutable_snapshot_exact
        || strategy
            == BenchmarkStrategy::sparse_hash_tree_snapshot_delta_budget_exact;
}

[[nodiscard]] bool uses_snapshot_delta_budget_argument(
    const BenchmarkStrategy strategy) noexcept {
    return strategy
        == BenchmarkStrategy::sparse_hash_tree_snapshot_delta_budget_exact;
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

[[nodiscard]] bool parse_promotion_threshold(
    const std::string_view text, std::uint64_t& threshold) noexcept {
    const auto result = std::from_chars(
        text.data(), text.data() + text.size(), threshold);
    return result.ec == std::errc{}
        && result.ptr == text.data() + text.size()
        && threshold != std::numeric_limits<std::uint64_t>::max();
}

[[nodiscard]] bool parse_promotion_reuse_threshold(
    const std::string_view text, std::uint8_t& threshold) noexcept {
    std::uint64_t parsed{};
    const auto result = std::from_chars(
        text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc{}
        || result.ptr != text.data() + text.size()
        || parsed == 0
        || parsed > std::numeric_limits<std::uint8_t>::max()) {
        return false;
    }
    threshold = static_cast<std::uint8_t>(parsed);
    return true;
}

[[nodiscard]] bool parse_pool_node_capacity(
    const std::string_view text, const std::size_t maximum,
    std::size_t& capacity) noexcept {
    std::uint64_t parsed{};
    const auto result = std::from_chars(
        text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc{}
        || result.ptr != text.data() + text.size()
        || parsed > maximum
        || parsed > std::numeric_limits<std::size_t>::max()) {
        return false;
    }
    capacity = static_cast<std::size_t>(parsed);
    return true;
}

[[nodiscard]] bool parse_delta_candidate_budget(
    const std::string_view text, std::size_t& budget) noexcept {
    std::uint64_t parsed{};
    const auto result = std::from_chars(
        text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc{}
        || result.ptr != text.data() + text.size()
        || parsed == 0
        || parsed > std::numeric_limits<std::size_t>::max()) {
        return false;
    }
    budget = static_cast<std::size_t>(parsed);
    return true;
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

struct TokenSummary {
    std::uint64_t literal_count{};
    std::uint64_t match_count{};
    std::uint64_t matched_bytes{};
    marc::core::Sha256 fingerprint{};
    bool valid{true};

    void begin_frame(const std::size_t frame_size) noexcept {
        std::array<std::byte, 9> record{};
        record[0] = std::byte{0xf0};
        auto value = static_cast<std::uint64_t>(frame_size);
        for (std::size_t index = 0; index < 8; ++index) {
            record[index + 1] = static_cast<std::byte>(value & 0xffU);
            value >>= 8U;
        }
        valid = valid && fingerprint.update(record);
    }

    void add_literal(const std::byte literal) noexcept {
        std::array<std::byte, 9> record{};
        record[0] = std::byte{0};
        record[1] = literal;
        valid = valid
            && marc::core::checked_add(
                literal_count, UINT64_C(1), literal_count)
            && fingerprint.update(record);
    }

    void add_match(const LzssMatch match) noexcept {
        std::array<std::byte, 9> record{};
        record[0] = std::byte{1};
        auto length = match.length;
        auto distance = match.distance;
        for (std::size_t index = 0; index < 4; ++index) {
            record[index + 1] = static_cast<std::byte>(length & 0xffU);
            record[index + 5] = static_cast<std::byte>(distance & 0xffU);
            length >>= 8U;
            distance >>= 8U;
        }
        valid = valid
            && marc::core::checked_add(
                match_count, UINT64_C(1), match_count)
            && marc::core::checked_add(
                matched_bytes, static_cast<std::uint64_t>(match.length),
                matched_bytes)
            && fingerprint.update(record);
    }
};

template <LzssMatchFinder Finder>
[[nodiscard]] std::size_t parse_with_finder(
    const std::span<const std::byte> input, Finder& finder,
    TokenSummary* const summary = nullptr) noexcept {
    std::size_t position{};
    std::size_t token_count{};
    while (position < input.size()) {
        const auto match = finder.find_match(position);
        const auto use_match = match.length != 0
            && lzss_match_is_beneficial(match);
        const auto advance = use_match
            ? static_cast<std::size_t>(match.length) : 1U;
        if (summary != nullptr) {
            if (use_match) {
                summary->add_match(match);
            } else {
                summary->add_literal(input[position]);
            }
        }
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
    TokenSummary token_summary{};
    LzssMatchFinderStatistics statistics{};
    std::uint64_t wavl_tree_maximum_final_height{};
    std::uint64_t red_black_tree_maximum_final_height{};
    std::uint64_t scapegoat_tree_maximum_final_height{};
    double seconds{};
};

[[nodiscard]] bool add_count(
    std::uint64_t& total, const std::uint64_t value) noexcept {
    return marc::core::checked_add(total, value, total);
}

[[nodiscard]] bool valid_token_summary(
    const FrameRunResult& result) noexcept {
    std::uint64_t tokens{};
    std::uint64_t reconstructed_bytes{};
    return result.token_summary.valid
        && add_count(tokens, result.token_summary.literal_count)
        && add_count(tokens, result.token_summary.match_count)
        && tokens == result.token_count
        && add_count(
            reconstructed_bytes, result.token_summary.literal_count)
        && add_count(
            reconstructed_bytes, result.token_summary.matched_bytes)
        && reconstructed_bytes == result.input_bytes;
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
        || !add_count(total.hash_chain_best_length_probe_comparison_count,
                      frame.hash_chain_best_length_probe_comparison_count)
        || !add_count(total.hash_chain_best_length_probe_pruned_candidate_count,
                      frame.hash_chain_best_length_probe_pruned_candidate_count)
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
    if (!add_count(total.wavl_tree_key_comparison_count,
                   frame.wavl_tree_key_comparison_count)
        || !add_count(total.wavl_tree_key_byte_comparison_count,
                      frame.wavl_tree_key_byte_comparison_count)
        || !add_count(total.wavl_tree_lcp_byte_comparison_count,
                      frame.wavl_tree_lcp_byte_comparison_count)
        || !add_count(total.wavl_tree_prefix_range_comparison_count,
                      frame.wavl_tree_prefix_range_comparison_count)
        || !add_count(total.wavl_tree_insertion_promotion_count,
                      frame.wavl_tree_insertion_promotion_count)
        || !add_count(total.wavl_tree_insertion_single_rotation_count,
                      frame.wavl_tree_insertion_single_rotation_count)
        || !add_count(total.wavl_tree_insertion_double_rotation_count,
                      frame.wavl_tree_insertion_double_rotation_count)
        || !add_count(total.wavl_tree_insertion_fixup_step_count,
                      frame.wavl_tree_insertion_fixup_step_count)
        || !add_count(total.wavl_tree_insertion_count,
                      frame.wavl_tree_insertion_count)
        || !add_count(total.wavl_tree_removal_demotion_count,
                      frame.wavl_tree_removal_demotion_count)
        || !add_count(total.wavl_tree_removal_single_rotation_count,
                      frame.wavl_tree_removal_single_rotation_count)
        || !add_count(total.wavl_tree_removal_double_rotation_count,
                      frame.wavl_tree_removal_double_rotation_count)
        || !add_count(total.wavl_tree_removal_fixup_step_count,
                      frame.wavl_tree_removal_fixup_step_count)
        || !add_count(total.wavl_tree_removal_preflight_node_count,
                      frame.wavl_tree_removal_preflight_node_count)
        || !add_count(total.wavl_tree_retirement_count,
                      frame.wavl_tree_retirement_count)) {
        return false;
    }
    total.wavl_tree_maximum_nodes_per_query = std::max(
        total.wavl_tree_maximum_nodes_per_query,
        frame.wavl_tree_maximum_nodes_per_query);
    total.wavl_tree_maximum_insertion_fixup_steps = std::max(
        total.wavl_tree_maximum_insertion_fixup_steps,
        frame.wavl_tree_maximum_insertion_fixup_steps);
    total.wavl_tree_maximum_removal_fixup_steps = std::max(
        total.wavl_tree_maximum_removal_fixup_steps,
        frame.wavl_tree_maximum_removal_fixup_steps);
    total.wavl_tree_maximum_removal_preflight_nodes = std::max(
        total.wavl_tree_maximum_removal_preflight_nodes,
        frame.wavl_tree_maximum_removal_preflight_nodes);
    for (std::size_t bin = 0;
         bin < total.wavl_tree_query_depth_histogram.size(); ++bin) {
        if (!add_count(total.wavl_tree_query_depth_histogram[bin],
                       frame.wavl_tree_query_depth_histogram[bin])) {
            return false;
        }
    }
    if (!add_count(total.red_black_tree_key_comparison_count,
                   frame.red_black_tree_key_comparison_count)
        || !add_count(total.red_black_tree_key_byte_comparison_count,
                      frame.red_black_tree_key_byte_comparison_count)
        || !add_count(total.red_black_tree_lcp_byte_comparison_count,
                      frame.red_black_tree_lcp_byte_comparison_count)
        || !add_count(total.red_black_tree_prefix_range_comparison_count,
                      frame.red_black_tree_prefix_range_comparison_count)
        || !add_count(total.red_black_tree_rotation_count,
                      frame.red_black_tree_rotation_count)
        || !add_count(total.red_black_tree_recoloring_count,
                      frame.red_black_tree_recoloring_count)
        || !add_count(total.red_black_tree_insertion_fixup_step_count,
                      frame.red_black_tree_insertion_fixup_step_count)
        || !add_count(total.red_black_tree_removal_fixup_step_count,
                      frame.red_black_tree_removal_fixup_step_count)
        || !add_count(total.red_black_tree_insertion_count,
                      frame.red_black_tree_insertion_count)
        || !add_count(total.red_black_tree_retirement_count,
                      frame.red_black_tree_retirement_count)) {
        return false;
    }
    total.red_black_tree_maximum_fixup_steps = std::max(
        total.red_black_tree_maximum_fixup_steps,
        frame.red_black_tree_maximum_fixup_steps);
    total.red_black_tree_maximum_nodes_per_query = std::max(
        total.red_black_tree_maximum_nodes_per_query,
        frame.red_black_tree_maximum_nodes_per_query);
    for (std::size_t bin = 0;
         bin < total.red_black_tree_query_depth_histogram.size(); ++bin) {
        if (!add_count(total.red_black_tree_query_depth_histogram[bin],
                       frame.red_black_tree_query_depth_histogram[bin])) {
            return false;
        }
    }
    if (!add_count(total.scapegoat_tree_key_comparison_count,
                   frame.scapegoat_tree_key_comparison_count)
        || !add_count(total.scapegoat_tree_key_byte_comparison_count,
                      frame.scapegoat_tree_key_byte_comparison_count)
        || !add_count(total.scapegoat_tree_lcp_byte_comparison_count,
                      frame.scapegoat_tree_lcp_byte_comparison_count)
        || !add_count(total.scapegoat_tree_prefix_range_comparison_count,
                      frame.scapegoat_tree_prefix_range_comparison_count)
        || !add_count(total.scapegoat_tree_insertion_count,
                      frame.scapegoat_tree_insertion_count)
        || !add_count(total.scapegoat_tree_retirement_count,
                      frame.scapegoat_tree_retirement_count)
        || !add_count(total.scapegoat_tree_depth_violation_count,
                      frame.scapegoat_tree_depth_violation_count)
        || !add_count(total.scapegoat_tree_ancestor_step_count,
                      frame.scapegoat_tree_ancestor_step_count)
        || !add_count(total.scapegoat_tree_subtree_rebuild_count,
                      frame.scapegoat_tree_subtree_rebuild_count)
        || !add_count(total.scapegoat_tree_whole_tree_rebuild_count,
                      frame.scapegoat_tree_whole_tree_rebuild_count)
        || !add_count(total.scapegoat_tree_rebuilt_node_count,
                      frame.scapegoat_tree_rebuilt_node_count)) {
        return false;
    }
    total.scapegoat_tree_maximum_rebuilt_nodes = std::max(
        total.scapegoat_tree_maximum_rebuilt_nodes,
        frame.scapegoat_tree_maximum_rebuilt_nodes);
    total.scapegoat_tree_maximum_structural_nodes_per_update = std::max(
        total.scapegoat_tree_maximum_structural_nodes_per_update,
        frame.scapegoat_tree_maximum_structural_nodes_per_update);
    total.scapegoat_tree_maximum_nodes_per_query = std::max(
        total.scapegoat_tree_maximum_nodes_per_query,
        frame.scapegoat_tree_maximum_nodes_per_query);
    for (std::size_t bin = 0;
         bin < total.scapegoat_tree_query_depth_histogram.size(); ++bin) {
        if (!add_count(total.scapegoat_tree_query_depth_histogram[bin],
                       frame.scapegoat_tree_query_depth_histogram[bin])) {
            return false;
        }
    }
    if (!add_count(total.hash_tree_chain_query_count,
                   frame.hash_tree_chain_query_count)
        || !add_count(total.hash_tree_chain_candidate_count,
                      frame.hash_tree_chain_candidate_count)
        || !add_count(total.hash_tree_trigger_query_count,
                      frame.hash_tree_trigger_query_count)
        || !add_count(total.hash_tree_tree_query_count,
                      frame.hash_tree_tree_query_count)
        || !add_count(total.hash_tree_promotion_count,
                      frame.hash_tree_promotion_count)
        || !add_count(total.hash_tree_pool_rejection_count,
                      frame.hash_tree_pool_rejection_count)
        || !add_count(total.hash_tree_promotion_trigger_candidate_count,
                      frame.hash_tree_promotion_trigger_candidate_count)
        || !add_count(total.hash_tree_promotion_build_node_count,
                      frame.hash_tree_promotion_build_node_count)
        || !add_count(total.hash_tree_tree_query_node_count,
                      frame.hash_tree_tree_query_node_count)
        || !add_count(total.hash_tree_insertion_count,
                      frame.hash_tree_insertion_count)
        || !add_count(total.hash_tree_retirement_count,
                      frame.hash_tree_retirement_count)
        || !add_count(
            total.hash_tree_promotion_build_key_comparison_count,
            frame.hash_tree_promotion_build_key_comparison_count)
        || !add_count(
            total.hash_tree_promotion_build_key_byte_comparison_count,
            frame.hash_tree_promotion_build_key_byte_comparison_count)
        || !add_count(total.hash_tree_promotion_build_rotation_count,
                      frame.hash_tree_promotion_build_rotation_count)
        || !add_count(total.hash_tree_tree_query_key_comparison_count,
                      frame.hash_tree_tree_query_key_comparison_count)
        || !add_count(total.hash_tree_tree_query_key_byte_comparison_count,
                      frame.hash_tree_tree_query_key_byte_comparison_count)
        || !add_count(total.hash_tree_tree_query_lcp_byte_comparison_count,
                      frame.hash_tree_tree_query_lcp_byte_comparison_count)
        || !add_count(
            total.hash_tree_tree_query_prefix_range_comparison_count,
            frame.hash_tree_tree_query_prefix_range_comparison_count)
        || !add_count(
            total.hash_tree_tree_query_prefix_range_byte_comparison_count,
            frame.hash_tree_tree_query_prefix_range_byte_comparison_count)
        || !add_count(total.hash_tree_tree_query_lcp_skipped_byte_count,
                      frame.hash_tree_tree_query_lcp_skipped_byte_count)
        || !add_count(total.hash_tree_maintenance_key_comparison_count,
                      frame.hash_tree_maintenance_key_comparison_count)
        || !add_count(total.hash_tree_maintenance_key_byte_comparison_count,
                      frame.hash_tree_maintenance_key_byte_comparison_count)
        || !add_count(total.hash_tree_rotation_count,
                      frame.hash_tree_rotation_count)
        || !add_count(total.hash_tree_snapshot_query_count,
                      frame.hash_tree_snapshot_query_count)
        || !add_count(total.hash_tree_snapshot_query_node_count,
                      frame.hash_tree_snapshot_query_node_count)
        || !add_count(
            total.hash_tree_snapshot_stale_subtree_prune_count,
            frame.hash_tree_snapshot_stale_subtree_prune_count)
        || !add_count(total.hash_tree_snapshot_delta_query_count,
                      frame.hash_tree_snapshot_delta_query_count)
        || !add_count(total.hash_tree_snapshot_delta_candidate_count,
                      frame.hash_tree_snapshot_delta_candidate_count)
        || !add_count(
            total.hash_tree_snapshot_delta_budget_query_count,
            frame.hash_tree_snapshot_delta_budget_query_count)
        || !add_count(
            total.hash_tree_snapshot_delta_budget_breach_count,
            frame.hash_tree_snapshot_delta_budget_breach_count)
        || !add_count(
            total.hash_tree_snapshot_delta_budget_demotion_count,
            frame.hash_tree_snapshot_delta_budget_demotion_count)
        || !add_count(total.hash_tree_snapshot_promotion_count,
                      frame.hash_tree_snapshot_promotion_count)
        || !add_count(total.hash_tree_snapshot_expiration_count,
                      frame.hash_tree_snapshot_expiration_count)
        || !add_count(total.hash_tree_snapshot_bulk_release_count,
                      frame.hash_tree_snapshot_bulk_release_count)) {
        return false;
    }
    total.hash_tree_promotion_maximum_trigger_candidates = std::max(
        total.hash_tree_promotion_maximum_trigger_candidates,
        frame.hash_tree_promotion_maximum_trigger_candidates);
    total.hash_tree_maximum_nodes_per_query = std::max(
        total.hash_tree_maximum_nodes_per_query,
        frame.hash_tree_maximum_nodes_per_query);
    total.hash_tree_maximum_promoted_buckets = std::max(
        total.hash_tree_maximum_promoted_buckets,
        frame.hash_tree_maximum_promoted_buckets);
    total.hash_tree_maximum_promoted_nodes = std::max(
        total.hash_tree_maximum_promoted_nodes,
        frame.hash_tree_maximum_promoted_nodes);
    total.hash_tree_maximum_height = std::max(
        total.hash_tree_maximum_height, frame.hash_tree_maximum_height);
    total.hash_tree_snapshot_delta_maximum_candidates_per_query = std::max(
        total.hash_tree_snapshot_delta_maximum_candidates_per_query,
        frame.hash_tree_snapshot_delta_maximum_candidates_per_query);
    total.hash_tree_snapshot_delta_budget_maximum_candidates_at_breach =
        std::max(
            total.hash_tree_snapshot_delta_budget_maximum_candidates_at_breach,
            frame.hash_tree_snapshot_delta_budget_maximum_candidates_at_breach);
    for (std::size_t bin = 0;
         bin < total.hash_tree_chain_query_depth_histogram.size(); ++bin) {
        if (!add_count(total.hash_tree_chain_query_depth_histogram[bin],
                       frame.hash_tree_chain_query_depth_histogram[bin])
            || !add_count(total.hash_tree_tree_query_depth_histogram[bin],
                          frame.hash_tree_tree_query_depth_histogram[bin])) {
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

void print_wavl_tree_depth_histogram(
    const LzssMatchFinderStatistics& statistics) {
    const auto last_bin = statistics.wavl_tree_maximum_nodes_per_query
        == 0 ? 0U
        : std::bit_width(statistics.wavl_tree_maximum_nodes_per_query);
    std::cout << "wavl_tree_query_depth_histogram=";
    for (std::size_t bin = 0; bin <= last_bin; ++bin) {
        if (bin != 0) std::cout << ',';
        std::cout << statistics.wavl_tree_query_depth_histogram[bin];
    }
    std::cout << '\n';
}

void print_red_black_tree_depth_histogram(
    const LzssMatchFinderStatistics& statistics) {
    const auto last_bin = statistics.red_black_tree_maximum_nodes_per_query
        == 0 ? 0U
        : std::bit_width(
            statistics.red_black_tree_maximum_nodes_per_query);
    std::cout << "red_black_tree_query_depth_histogram=";
    for (std::size_t bin = 0; bin <= last_bin; ++bin) {
        if (bin != 0) std::cout << ',';
        std::cout << statistics.red_black_tree_query_depth_histogram[bin];
    }
    std::cout << '\n';
}

void print_scapegoat_tree_depth_histogram(
    const LzssMatchFinderStatistics& statistics) {
    const auto last_bin = statistics.scapegoat_tree_maximum_nodes_per_query
        == 0 ? 0U
        : std::bit_width(
            statistics.scapegoat_tree_maximum_nodes_per_query);
    std::cout << "scapegoat_tree_query_depth_histogram=";
    for (std::size_t bin = 0; bin <= last_bin; ++bin) {
        if (bin != 0) std::cout << ',';
        std::cout << statistics.scapegoat_tree_query_depth_histogram[bin];
    }
    std::cout << '\n';
}

void print_hash_tree_depth_histograms(
    const LzssMatchFinderStatistics& statistics) {
    const auto print = [](const std::string_view name,
                          const auto& histogram) {
        std::size_t last_bin{};
        for (std::size_t bin = 1; bin < histogram.size(); ++bin) {
            if (histogram[bin] != 0) last_bin = bin;
        }
        std::cout << name << '=';
        for (std::size_t bin = 0; bin <= last_bin; ++bin) {
            if (bin != 0) std::cout << ',';
            std::cout << histogram[bin];
        }
        std::cout << '\n';
    };
    print("hash_tree_chain_query_depth_histogram",
          statistics.hash_tree_chain_query_depth_histogram);
    print("hash_tree_tree_query_depth_histogram",
          statistics.hash_tree_tree_query_depth_histogram);
}

[[nodiscard]] bool valid_hash_chain_statistics(
    const LzssMatchFinderStatistics& statistics,
    const bool uses_best_length_probe = false) noexcept {
    const auto probes = statistics.hash_chain_best_length_probe_comparison_count;
    const auto pruned = statistics.hash_chain_best_length_probe_pruned_candidate_count;
    if ((!uses_best_length_probe && (probes != 0 || pruned != 0))
        || pruned > probes || probes > statistics.candidate_count
        || probes > statistics.byte_comparison_count) return false;
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
        || !add_count(classified_candidates, pruned)
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

[[nodiscard]] bool valid_red_black_tree_statistics(
    const LzssMatchFinderStatistics& statistics) noexcept {
    if (statistics.overflowed
        || statistics.red_black_tree_prefix_range_comparison_count
            > statistics.red_black_tree_key_comparison_count) {
        return false;
    }
    std::uint64_t histogram_queries{};
    for (const auto count : statistics.red_black_tree_query_depth_histogram) {
        if (!add_count(histogram_queries, count)) return false;
    }
    return histogram_queries == statistics.query_count;
}

[[nodiscard]] bool valid_wavl_tree_statistics(
    const LzssMatchFinderStatistics& statistics) noexcept {
    if (statistics.overflowed
        || statistics.wavl_tree_prefix_range_comparison_count
            > statistics.wavl_tree_key_comparison_count) {
        return false;
    }
    std::uint64_t histogram_queries{};
    for (const auto count : statistics.wavl_tree_query_depth_histogram) {
        if (!add_count(histogram_queries, count)) return false;
    }
    return histogram_queries == statistics.query_count;
}

[[nodiscard]] bool valid_scapegoat_tree_statistics(
    const LzssMatchFinderStatistics& statistics) noexcept {
    if (statistics.overflowed
        || statistics.scapegoat_tree_prefix_range_comparison_count
            > statistics.scapegoat_tree_key_comparison_count
        || statistics.scapegoat_tree_whole_tree_rebuild_count
            > statistics.scapegoat_tree_subtree_rebuild_count
        || statistics.scapegoat_tree_maximum_rebuilt_nodes
            > statistics.scapegoat_tree_rebuilt_node_count) {
        return false;
    }
    std::uint64_t histogram_queries{};
    for (const auto count : statistics.scapegoat_tree_query_depth_histogram) {
        if (!add_count(histogram_queries, count)) return false;
    }
    return histogram_queries == statistics.query_count;
}

[[nodiscard]] bool valid_hash_tree_statistics(
    const LzssMatchFinderStatistics& statistics,
    const bool require_every_trigger_promoted = true) noexcept {
    std::uint64_t accounted_triggers{};
    if (!marc::core::checked_add(
            statistics.hash_tree_promotion_count,
            statistics.hash_tree_pool_rejection_count,
            accounted_triggers)) {
        return false;
    }
    if (statistics.overflowed
        || statistics.hash_tree_promotion_count
            > statistics.hash_tree_trigger_query_count
        || statistics.hash_tree_pool_rejection_count
            > statistics.hash_tree_trigger_query_count
        || (require_every_trigger_promoted
            && statistics.hash_tree_trigger_query_count
                != statistics.hash_tree_promotion_count)
        || (!require_every_trigger_promoted
            && statistics.hash_tree_trigger_query_count
                != accounted_triggers)) {
        return false;
    }
    std::uint64_t route_queries{};
    if (!marc::core::checked_add(
            statistics.hash_tree_chain_query_count,
            statistics.hash_tree_tree_query_count, route_queries)
        || route_queries != statistics.query_count) {
        return false;
    }
    std::uint64_t chain_histogram_queries{};
    std::uint64_t tree_histogram_queries{};
    for (std::size_t bin = 0;
         bin < statistics.hash_tree_chain_query_depth_histogram.size();
         ++bin) {
        if (!add_count(
                chain_histogram_queries,
                statistics.hash_tree_chain_query_depth_histogram[bin])
            || !add_count(
                tree_histogram_queries,
                statistics.hash_tree_tree_query_depth_histogram[bin])) {
            return false;
        }
    }
    return chain_histogram_queries == statistics.hash_tree_chain_query_count
        && tree_histogram_queries == statistics.hash_tree_tree_query_count;
}

[[nodiscard]] bool valid_statistics(
    const BenchmarkStrategy strategy,
    const LzssMatchFinderStatistics& statistics,
    const std::size_t delta_candidate_budget = 0) noexcept {
    switch (strategy) {
    case BenchmarkStrategy::hash_chain_best_length_probe_exact:
        return valid_hash_chain_statistics(statistics, true);
    case BenchmarkStrategy::hash_chain_exact:
    case BenchmarkStrategy::hash_chain_legacy_65536_exact:
    case BenchmarkStrategy::hash_chain_mnemonic_mixer_v1_exact:
    case BenchmarkStrategy::hash_chain_buckets_262144_exact:
    case BenchmarkStrategy::hash_chain_buckets_1048576_exact:
    case BenchmarkStrategy::hash_chain_buckets_4194304_exact:
        return valid_hash_chain_statistics(statistics);
    case BenchmarkStrategy::binary_tree_exact:
        return valid_binary_tree_statistics(statistics);
    case BenchmarkStrategy::wavl_tree_exact:
        return valid_wavl_tree_statistics(statistics);
    case BenchmarkStrategy::red_black_tree_exact:
        return valid_red_black_tree_statistics(statistics);
    case BenchmarkStrategy::scapegoat_tree_exact:
        return valid_scapegoat_tree_statistics(statistics);
    case BenchmarkStrategy::hash_tree_exact:
        return valid_hash_tree_statistics(statistics);
    case BenchmarkStrategy::sparse_hash_tree_exact:
    case BenchmarkStrategy::sparse_hash_tree_reuse_gated_exact:
        return valid_hash_tree_statistics(statistics, false);
    case BenchmarkStrategy::sparse_hash_tree_immutable_snapshot_exact: {
        std::uint64_t routed_queries{};
        return marc::core::checked_add(
                   statistics.hash_tree_chain_query_count,
                   statistics.hash_tree_snapshot_query_count,
                   routed_queries)
            && routed_queries == statistics.query_count
            && statistics.hash_tree_tree_query_count == 0
            && statistics.hash_tree_insertion_count == 0
            && statistics.hash_tree_retirement_count == 0
            && statistics.hash_tree_snapshot_delta_query_count
                == statistics.hash_tree_snapshot_query_count
            && statistics.hash_tree_snapshot_promotion_count
                == statistics.hash_tree_promotion_count
            && statistics.hash_tree_snapshot_bulk_release_count
                <= statistics.hash_tree_snapshot_expiration_count
            && statistics.hash_tree_snapshot_delta_budget_query_count == 0
            && statistics.hash_tree_snapshot_delta_budget_breach_count == 0
            && statistics.hash_tree_snapshot_delta_budget_demotion_count == 0
            && statistics
                    .hash_tree_snapshot_delta_budget_maximum_candidates_at_breach
                == 0
            && !statistics.overflowed;
    }
    case BenchmarkStrategy::sparse_hash_tree_snapshot_delta_budget_exact: {
        std::uint64_t routed_queries{};
        std::uint64_t accounted_releases{};
        const auto breaches =
            statistics.hash_tree_snapshot_delta_budget_breach_count;
        return delta_candidate_budget != 0
            && marc::core::checked_add(
                statistics.hash_tree_chain_query_count,
                statistics.hash_tree_snapshot_query_count,
                routed_queries)
            && routed_queries == statistics.query_count
            && statistics.hash_tree_tree_query_count == 0
            && statistics.hash_tree_insertion_count == 0
            && statistics.hash_tree_retirement_count == 0
            && statistics.hash_tree_snapshot_delta_query_count
                == statistics.hash_tree_snapshot_query_count
            && statistics.hash_tree_snapshot_delta_budget_query_count
                == statistics.hash_tree_snapshot_query_count
            && statistics.hash_tree_snapshot_promotion_count
                == statistics.hash_tree_promotion_count
            && breaches
                == statistics.hash_tree_snapshot_delta_budget_demotion_count
            && marc::core::checked_add(
                statistics.hash_tree_snapshot_expiration_count,
                statistics.hash_tree_snapshot_delta_budget_demotion_count,
                accounted_releases)
            && accounted_releases
                == statistics.hash_tree_snapshot_bulk_release_count
            && ((breaches == 0
                    && statistics
                           .hash_tree_snapshot_delta_budget_maximum_candidates_at_breach
                        == 0)
                || (breaches != 0
                    && statistics
                           .hash_tree_snapshot_delta_budget_maximum_candidates_at_breach
                        > delta_candidate_budget))
            && !statistics.overflowed;
    }
    }
    return false;
}

[[nodiscard]] bool measure_wavl_tree_final_height(
    const LzssWavlTreeMatchFinder& finder,
    std::uint64_t& height) noexcept {
    height = 0;
    if (finder.empty()) return true;
    auto current = finder.root_index();
    auto previous = lzss_wavl_tree_null_node;
    std::uint64_t depth{1};
    std::uint64_t traversed{};
    height = 1;
    const auto traversal_limit =
        static_cast<std::uint64_t>(finder.active_node_count()) * 2U + 1U;
    while (current != lzss_wavl_tree_null_node) {
        if (++traversed > traversal_limit) return false;
        const auto node = inspect_lzss_wavl_tree_node(finder, current);
        std::uint32_t next{};
        if (previous == node.parent) {
            if (node.left != lzss_wavl_tree_null_node) {
                next = node.left;
                ++depth;
                height = std::max(height, depth);
            } else if (node.right != lzss_wavl_tree_null_node) {
                next = node.right;
                ++depth;
                height = std::max(height, depth);
            } else {
                next = node.parent;
                --depth;
            }
        } else if (previous == node.left) {
            if (node.right != lzss_wavl_tree_null_node) {
                next = node.right;
                ++depth;
                height = std::max(height, depth);
            } else {
                next = node.parent;
                --depth;
            }
        } else if (previous == node.right) {
            next = node.parent;
            --depth;
        } else {
            return false;
        }
        previous = current;
        current = next;
    }
    return depth == 0;
}

[[nodiscard]] bool measure_red_black_tree_final_height(
    const LzssRedBlackTreeMatchFinder& finder,
    std::uint64_t& height) noexcept {
    height = 0;
    if (finder.empty()) return true;
    auto current = finder.root_index();
    auto previous = lzss_red_black_tree_null_node;
    std::uint64_t depth{1};
    std::uint64_t traversed{};
    height = 1;
    const auto traversal_limit =
        static_cast<std::uint64_t>(finder.active_node_count()) * 2U + 1U;
    while (current != lzss_red_black_tree_null_node) {
        if (++traversed > traversal_limit) return false;
        const auto node = inspect_lzss_red_black_tree_node(finder, current);
        std::uint32_t next{};
        if (previous == node.parent) {
            if (node.left != lzss_red_black_tree_null_node) {
                next = node.left;
                ++depth;
                height = std::max(height, depth);
            } else if (node.right != lzss_red_black_tree_null_node) {
                next = node.right;
                ++depth;
                height = std::max(height, depth);
            } else {
                next = node.parent;
                --depth;
            }
        } else if (previous == node.left) {
            if (node.right != lzss_red_black_tree_null_node) {
                next = node.right;
                ++depth;
                height = std::max(height, depth);
            } else {
                next = node.parent;
                --depth;
            }
        } else if (previous == node.right) {
            next = node.parent;
            --depth;
        } else {
            return false;
        }
        previous = current;
        current = next;
    }
    return depth == 0;
}

[[nodiscard]] bool measure_scapegoat_tree_final_height(
    const LzssScapegoatTreeMatchFinder& finder,
    std::uint64_t& height) noexcept {
    height = 0;
    if (finder.empty()) return true;
    auto current = finder.root_index();
    auto previous = lzss_scapegoat_tree_null_node;
    std::uint64_t depth{1};
    std::uint64_t traversed{};
    height = 1;
    const auto traversal_limit =
        static_cast<std::uint64_t>(finder.active_node_count()) * 2U + 1U;
    while (current != lzss_scapegoat_tree_null_node) {
        if (++traversed > traversal_limit) return false;
        const auto node = inspect_lzss_scapegoat_tree_node(finder, current);
        std::uint32_t next{};
        if (previous == node.parent) {
            if (node.left != lzss_scapegoat_tree_null_node) {
                next = node.left;
                ++depth;
                height = std::max(height, depth);
            } else if (node.right != lzss_scapegoat_tree_null_node) {
                next = node.right;
                ++depth;
                height = std::max(height, depth);
            } else {
                next = node.parent;
                --depth;
            }
        } else if (previous == node.left) {
            if (node.right != lzss_scapegoat_tree_null_node) {
                next = node.right;
                ++depth;
                height = std::max(height, depth);
            } else {
                next = node.parent;
                --depth;
            }
        } else if (previous == node.right) {
            next = node.parent;
            --depth;
        } else {
            return false;
        }
        previous = current;
        current = next;
    }
    return depth == 0;
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

[[nodiscard]] bool parse_positive_u64(
    const std::string_view text, std::uint64_t& value) noexcept {
    const auto result = std::from_chars(
        text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{}
        && result.ptr == text.data() + text.size() && value != 0;
}

enum class SyntheticInputKind : std::uint8_t {
    zeros,
    periodic,
    equal_prefix,
    hash_collision,
    pseudorandom,
    deletion_heavy,
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
    } else if (text == "deletion-heavy") {
        kind = SyntheticInputKind::deletion_heavy;
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
    case SyntheticInputKind::deletion_heavy: return "deletion-heavy";
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
    if (kind == SyntheticInputKind::pseudorandom
        || kind == SyntheticInputKind::deletion_heavy) {
        auto state = advance_synthetic_lcg(absolute_offset);
        for (std::size_t index = 0; index < output.size(); ++index) {
            state = state * UINT32_C(1664525) + UINT32_C(1013904223);
            auto value = static_cast<std::uint8_t>(state >> 24U);
            if (kind == SyntheticInputKind::deletion_heavy) {
                const auto position = absolute_offset + index;
                value ^= static_cast<std::uint8_t>(position);
                value ^= static_cast<std::uint8_t>(position >> 11U);
            }
            output[index] = static_cast<std::byte>(value);
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
        case SyntheticInputKind::pseudorandom:
        case SyntheticInputKind::deletion_heavy: break;
        }
    }
}

[[nodiscard]] bool process_frame(
    const BenchmarkStrategy strategy,
    const std::span<const std::byte> frame,
    const LzssParameters& parameters,
    const marc::core::DecoderLimits& limits,
    const std::span<std::byte> workspace, const bool collect_statistics,
    const bool measure, const std::size_t pool_node_capacity,
    const std::uint64_t promotion_threshold,
    const std::uint8_t promotion_reuse_threshold,
    const std::size_t delta_candidate_budget,
    FrameRunResult& result) noexcept {
    LzssMatchFinderStatistics frame_statistics{};
    auto* const token_summary = collect_statistics
        ? &result.token_summary : nullptr;
    if (token_summary != nullptr) token_summary->begin_frame(frame.size());
    const auto begin = std::chrono::steady_clock::now();
    std::size_t frame_tokens{};
    if (strategy == BenchmarkStrategy::hash_chain_exact) {
        LzssHashChainNoProbeMatchFinder finder{};
        if (initialize_lzss_hash_chain_no_probe_match_finder(
                frame, parameters, limits, workspace, finder,
                collect_statistics ? &frame_statistics : nullptr)
            != LzssHashChainError::none) {
            return false;
        }
        frame_tokens = parse_with_finder(frame, finder, token_summary);
    } else if (strategy
               == BenchmarkStrategy::hash_chain_best_length_probe_exact) {
        LzssHashChainBestLengthProbeMatchFinder finder{};
        if (initialize_lzss_hash_chain_best_length_probe_match_finder(
                frame, parameters, limits, workspace, finder,
                collect_statistics ? &frame_statistics : nullptr)
            != LzssHashChainError::none) {
            return false;
        }
        frame_tokens = parse_with_finder(frame, finder, token_summary);
    } else if (strategy
               == BenchmarkStrategy::hash_chain_mnemonic_mixer_v1_exact) {
        LzssHashChainMnemonicMixerV1MatchFinder finder{};
        if (initialize_lzss_hash_chain_mnemonic_mixer_v1_match_finder(
                frame, parameters, limits, workspace, finder,
                collect_statistics ? &frame_statistics : nullptr)
            != LzssHashChainError::none) {
            return false;
        }
        frame_tokens = parse_with_finder(frame, finder, token_summary);
    } else if (strategy
               == BenchmarkStrategy::hash_chain_legacy_65536_exact) {
        LzssHashChainBuckets65536MatchFinder finder{};
        if (initialize_lzss_hash_chain_bucket_scaled_match_finder(
                frame, parameters, limits, workspace, finder,
                collect_statistics ? &frame_statistics : nullptr)
            != LzssHashChainError::none) {
            return false;
        }
        frame_tokens = parse_with_finder(frame, finder, token_summary);
    } else if (strategy
               == BenchmarkStrategy::hash_chain_buckets_262144_exact) {
        LzssHashChainBuckets262144MatchFinder finder{};
        if (initialize_lzss_hash_chain_bucket_scaled_match_finder(
                frame, parameters, limits, workspace, finder,
                collect_statistics ? &frame_statistics : nullptr)
            != LzssHashChainError::none) {
            return false;
        }
        frame_tokens = parse_with_finder(frame, finder, token_summary);
    } else if (strategy
               == BenchmarkStrategy::hash_chain_buckets_1048576_exact) {
        LzssHashChainBuckets1048576MatchFinder finder{};
        if (initialize_lzss_hash_chain_bucket_scaled_match_finder(
                frame, parameters, limits, workspace, finder,
                collect_statistics ? &frame_statistics : nullptr)
            != LzssHashChainError::none) {
            return false;
        }
        frame_tokens = parse_with_finder(frame, finder, token_summary);
    } else if (strategy
               == BenchmarkStrategy::hash_chain_buckets_4194304_exact) {
        LzssHashChainBuckets4194304MatchFinder finder{};
        if (initialize_lzss_hash_chain_bucket_scaled_match_finder(
                frame, parameters, limits, workspace, finder,
                collect_statistics ? &frame_statistics : nullptr)
            != LzssHashChainError::none) {
            return false;
        }
        frame_tokens = parse_with_finder(frame, finder, token_summary);
    } else if (strategy == BenchmarkStrategy::binary_tree_exact) {
        LzssBinaryTreeMatchFinder finder{};
        if (initialize_lzss_binary_tree_match_finder(
                frame, parameters, limits, workspace, finder,
                collect_statistics ? &frame_statistics : nullptr)
            != LzssBinaryTreeError::none) {
            return false;
        }
        frame_tokens = parse_with_finder(frame, finder, token_summary);
    } else if (strategy == BenchmarkStrategy::wavl_tree_exact) {
        LzssWavlTreeMatchFinder finder{};
        if (initialize_lzss_wavl_tree_match_finder(
                frame, parameters, limits, workspace, finder,
                collect_statistics ? &frame_statistics : nullptr)
            != LzssWavlTreeError::none) {
            return false;
        }
        frame_tokens = parse_with_finder(frame, finder, token_summary);
        if (!finder.state_valid()) return false;
        if (collect_statistics) {
            if (validate_lzss_wavl_tree(finder)
                != LzssWavlTreeValidationError::none) {
                return false;
            }
            std::uint64_t final_height{};
            if (!measure_wavl_tree_final_height(finder, final_height)) {
                return false;
            }
            result.wavl_tree_maximum_final_height = std::max(
                result.wavl_tree_maximum_final_height, final_height);
        }
    } else if (strategy == BenchmarkStrategy::red_black_tree_exact) {
        LzssRedBlackTreeMatchFinder finder{};
        if (initialize_lzss_red_black_tree_match_finder(
                frame, parameters, limits, workspace, finder,
                collect_statistics ? &frame_statistics : nullptr)
            != LzssRedBlackTreeError::none) {
            return false;
        }
        frame_tokens = parse_with_finder(frame, finder, token_summary);
        if (!finder.state_valid()) {
            return false;
        }
        if (collect_statistics) {
            if (validate_lzss_red_black_tree(finder)
                != LzssRedBlackTreeValidationError::none) {
                return false;
            }
            std::uint64_t final_height{};
            if (!measure_red_black_tree_final_height(finder, final_height)) {
                return false;
            }
            result.red_black_tree_maximum_final_height = std::max(
                result.red_black_tree_maximum_final_height, final_height);
        }
    } else if (strategy == BenchmarkStrategy::scapegoat_tree_exact) {
        LzssScapegoatTreeMatchFinder finder{};
        if (initialize_lzss_scapegoat_tree_match_finder(
                frame, parameters, limits, workspace, finder,
                collect_statistics ? &frame_statistics : nullptr)
            != LzssScapegoatTreeError::none) {
            return false;
        }
        frame_tokens = parse_with_finder(frame, finder, token_summary);
        if (!finder.state_valid()) return false;
        if (collect_statistics) {
            if (validate_lzss_scapegoat_tree(finder)
                != LzssScapegoatTreeValidationError::none) {
                return false;
            }
            std::uint64_t final_height{};
            if (!measure_scapegoat_tree_final_height(finder, final_height)) {
                return false;
            }
            result.scapegoat_tree_maximum_final_height = std::max(
                result.scapegoat_tree_maximum_final_height, final_height);
        }
    } else if (strategy == BenchmarkStrategy::hash_tree_exact) {
        LzssHashTreeMatchFinder finder{};
        if (initialize_lzss_hash_tree_match_finder(
                frame, parameters, limits, workspace, finder,
                collect_statistics ? &frame_statistics : nullptr,
                LzssHashTreeOptions{promotion_threshold})
            != LzssHashTreeError::none) {
            return false;
        }
        frame_tokens = parse_with_finder(frame, finder, token_summary);
        if (!finder.state_valid()) return false;
    } else {
        const auto effective_pool_capacity = frame.size()
                < lzss_match_finder_prefix_size
            ? 0U
            : std::min({pool_node_capacity, frame.size(),
                        static_cast<std::size_t>(parameters.window_size)});
        LzssSparseHashTreeMatchFinder finder{};
        if (initialize_lzss_sparse_hash_tree_match_finder(
                frame, parameters, limits, workspace, finder,
                collect_statistics ? &frame_statistics : nullptr,
                {effective_pool_capacity, promotion_threshold,
                 promotion_reuse_threshold,
                 uses_immutable_snapshot_lifecycle(strategy)
                     ? LzssSparseHashTreeLifecycleMode::immutable_snapshot
                     : LzssSparseHashTreeLifecycleMode::mutable_tree,
                 delta_candidate_budget})
            != LzssSparseHashTreeMatchFinderError::none) {
            return false;
        }
        frame_tokens = parse_with_finder(frame, finder, token_summary);
        if (!finder.state_valid()) return false;
    }
    const auto end = std::chrono::steady_clock::now();
    if (measure) {
        result.seconds += std::chrono::duration<double>(end - begin).count();
    }
    return (token_summary == nullptr || token_summary->valid)
        && add_count(result.input_bytes, frame.size())
        && add_count(result.frame_count, 1)
        && add_count(result.token_count, frame_tokens)
        && (!collect_statistics
            || add_statistics(result.statistics, frame_statistics));
}

[[nodiscard]] std::array<char, marc::core::sha256_digest_size * 2 + 1>
token_fingerprint_hex(const TokenSummary& summary) noexcept {
    std::array<std::byte, marc::core::sha256_digest_size> digest{};
    auto snapshot = summary.fingerprint;
    const auto finalized = snapshot.finalize(digest);
    constexpr std::array<char, 16> hex{
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    std::array<char, marc::core::sha256_digest_size * 2 + 1> result{};
    if (!finalized) return result;
    for (std::size_t index = 0; index < digest.size(); ++index) {
        const auto value = std::to_integer<unsigned>(digest[index]);
        result[index * 2] = hex[value >> 4U];
        result[index * 2 + 1] = hex[value & 0x0fU];
    }
    return result;
}

[[nodiscard]] bool process_frames(
    const BenchmarkStrategy strategy,
    const std::filesystem::path& path, const std::uint64_t expected_file_size,
    const std::size_t frame_size, const LzssParameters& parameters,
    const marc::core::DecoderLimits& limits,
    const std::span<std::byte> workspace, const bool collect_statistics,
    const bool measure, const std::size_t pool_node_capacity,
    const std::uint64_t promotion_threshold,
    const std::uint8_t promotion_reuse_threshold,
    const std::size_t delta_candidate_budget,
    FrameRunResult& result) {
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
                collect_statistics, measure, pool_node_capacity,
                promotion_threshold, promotion_reuse_threshold,
                delta_candidate_budget, result)) {
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
    const bool measure, const std::size_t pool_node_capacity,
    const std::uint64_t promotion_threshold,
    const std::uint8_t promotion_reuse_threshold,
    const std::size_t delta_candidate_budget,
    FrameRunResult& result) {
    std::vector<std::byte> input(frame_size);
    std::uint64_t offset{};
    while (offset < input_size) {
        const auto current_size = static_cast<std::size_t>(
            std::min<std::uint64_t>(input_size - offset, frame_size));
        const auto frame = std::span<std::byte>{input}.first(current_size);
        fill_synthetic_input(kind, offset, frame);
        if (!process_frame(
                strategy, frame, parameters, limits, workspace,
                collect_statistics, measure, pool_node_capacity,
                promotion_threshold, promotion_reuse_threshold,
                delta_candidate_budget, result)) {
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
    const std::size_t hash_chain_bucket_count,
    const std::uint64_t max_internal_buffered_bytes,
    const std::size_t pool_node_capacity,
    const std::uint64_t promotion_threshold,
    const std::uint8_t promotion_reuse_threshold,
    const std::size_t delta_candidate_budget,
    const FrameRunResult& verified, const double measured_seconds) {
    const auto fingerprint = token_fingerprint_hex(verified.token_summary);
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
              << "literal_count=" << verified.token_summary.literal_count
              << '\n'
              << "match_count=" << verified.token_summary.match_count << '\n'
              << "matched_bytes=" << verified.token_summary.matched_bytes
              << '\n'
              << "token_fingerprint_sha256=" << fingerprint.data() << '\n'
              << "iterations=" << iterations << '\n';
    if (mode == "frames-limited") {
        std::cout << "max_internal_buffered_bytes="
                  << max_internal_buffered_bytes << '\n'
                  << "workspace_bytes=" << workspace_size << '\n';
    }
    if (is_hash_chain_strategy(strategy)) {
        std::cout << "hash_chain_configured_bucket_cap="
                  << configured_hash_chain_bucket_cap(strategy) << '\n'
                  << "hash_chain_bucket_count=" << hash_chain_bucket_count
                  << '\n'
                  << "hash_workspace_bytes=" << workspace_size << '\n'
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
        std::cout << "hash_chain_best_length_probe_comparisons="
                  << verified.statistics.hash_chain_best_length_probe_comparison_count
                  << '\n'
                  << "hash_chain_best_length_probe_pruned_candidates="
                  << verified.statistics.hash_chain_best_length_probe_pruned_candidate_count
                  << '\n';
        print_hash_chain_depth_histogram(verified.statistics);
        return;
    }
    if (strategy == BenchmarkStrategy::binary_tree_exact) {
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
        return;
    }
    if (strategy == BenchmarkStrategy::wavl_tree_exact) {
        const auto& statistics = verified.statistics;
        std::cout << "wavl_tree_workspace_bytes=" << workspace_size << '\n'
              << "wavl_tree_queries=" << statistics.query_count << '\n'
              << "wavl_tree_key_comparisons="
              << statistics.wavl_tree_key_comparison_count << '\n'
              << "wavl_tree_key_byte_comparisons="
              << statistics.wavl_tree_key_byte_comparison_count << '\n'
              << "wavl_tree_lcp_byte_comparisons="
              << statistics.wavl_tree_lcp_byte_comparison_count << '\n'
              << "wavl_tree_prefix_range_comparisons="
              << statistics.wavl_tree_prefix_range_comparison_count << '\n'
              << "wavl_tree_insertion_promotions="
              << statistics.wavl_tree_insertion_promotion_count << '\n'
              << "wavl_tree_insertion_single_rotations="
              << statistics.wavl_tree_insertion_single_rotation_count << '\n'
              << "wavl_tree_insertion_double_rotations="
              << statistics.wavl_tree_insertion_double_rotation_count << '\n'
              << "wavl_tree_insertion_fixup_steps="
              << statistics.wavl_tree_insertion_fixup_step_count << '\n'
              << "wavl_tree_maximum_insertion_fixup_steps="
              << statistics.wavl_tree_maximum_insertion_fixup_steps << '\n'
              << "wavl_tree_insertions="
              << statistics.wavl_tree_insertion_count << '\n'
              << "wavl_tree_removal_demotions="
              << statistics.wavl_tree_removal_demotion_count << '\n'
              << "wavl_tree_removal_single_rotations="
              << statistics.wavl_tree_removal_single_rotation_count << '\n'
              << "wavl_tree_removal_double_rotations="
              << statistics.wavl_tree_removal_double_rotation_count << '\n'
              << "wavl_tree_removal_fixup_steps="
              << statistics.wavl_tree_removal_fixup_step_count << '\n'
              << "wavl_tree_maximum_removal_fixup_steps="
              << statistics.wavl_tree_maximum_removal_fixup_steps << '\n'
              << "wavl_tree_removal_preflight_nodes="
              << statistics.wavl_tree_removal_preflight_node_count << '\n'
              << "wavl_tree_maximum_removal_preflight_nodes="
              << statistics.wavl_tree_maximum_removal_preflight_nodes << '\n'
              << "wavl_tree_retirements="
              << statistics.wavl_tree_retirement_count << '\n'
              << "wavl_tree_maximum_final_height="
              << verified.wavl_tree_maximum_final_height << '\n'
              << "wavl_tree_max_nodes_per_query="
              << statistics.wavl_tree_maximum_nodes_per_query << '\n'
              << "wavl_tree_frame_seconds=" << measured_seconds << '\n'
              << "wavl_tree_frame_mib_per_second="
              << throughput(
                     verified.input_bytes, iterations, measured_seconds)
              << '\n';
        print_wavl_tree_depth_histogram(statistics);
        return;
    }
    if (strategy == BenchmarkStrategy::red_black_tree_exact) {
        const auto& statistics = verified.statistics;
        std::cout << "red_black_tree_workspace_bytes=" << workspace_size
              << '\n'
              << "red_black_tree_queries=" << statistics.query_count << '\n'
              << "red_black_tree_key_comparisons="
              << statistics.red_black_tree_key_comparison_count << '\n'
              << "red_black_tree_key_byte_comparisons="
              << statistics.red_black_tree_key_byte_comparison_count << '\n'
              << "red_black_tree_lcp_byte_comparisons="
              << statistics.red_black_tree_lcp_byte_comparison_count << '\n'
              << "red_black_tree_prefix_range_comparisons="
              << statistics.red_black_tree_prefix_range_comparison_count
              << '\n'
              << "red_black_tree_rotations="
              << statistics.red_black_tree_rotation_count << '\n'
              << "red_black_tree_recolorings="
              << statistics.red_black_tree_recoloring_count << '\n'
              << "red_black_tree_insertion_fixup_steps="
              << statistics.red_black_tree_insertion_fixup_step_count << '\n'
              << "red_black_tree_removal_fixup_steps="
              << statistics.red_black_tree_removal_fixup_step_count << '\n'
              << "red_black_tree_maximum_fixup_steps="
              << statistics.red_black_tree_maximum_fixup_steps << '\n'
              << "red_black_tree_insertions="
              << statistics.red_black_tree_insertion_count << '\n'
              << "red_black_tree_retirements="
              << statistics.red_black_tree_retirement_count << '\n'
              << "red_black_tree_maximum_final_height="
              << verified.red_black_tree_maximum_final_height << '\n'
              << "red_black_tree_max_nodes_per_query="
              << statistics.red_black_tree_maximum_nodes_per_query << '\n'
              << "red_black_tree_frame_seconds=" << measured_seconds << '\n'
              << "red_black_tree_frame_mib_per_second="
              << throughput(
                     verified.input_bytes, iterations, measured_seconds)
              << '\n';
        print_red_black_tree_depth_histogram(statistics);
        return;
    }
    if (strategy == BenchmarkStrategy::scapegoat_tree_exact) {
        const auto& statistics = verified.statistics;
        std::cout << "scapegoat_tree_workspace_bytes=" << workspace_size
              << '\n'
              << "scapegoat_tree_queries=" << statistics.query_count << '\n'
              << "scapegoat_tree_key_comparisons="
              << statistics.scapegoat_tree_key_comparison_count << '\n'
              << "scapegoat_tree_key_byte_comparisons="
              << statistics.scapegoat_tree_key_byte_comparison_count << '\n'
              << "scapegoat_tree_lcp_byte_comparisons="
              << statistics.scapegoat_tree_lcp_byte_comparison_count << '\n'
              << "scapegoat_tree_prefix_range_comparisons="
              << statistics.scapegoat_tree_prefix_range_comparison_count
              << '\n'
              << "scapegoat_tree_insertions="
              << statistics.scapegoat_tree_insertion_count << '\n'
              << "scapegoat_tree_retirements="
              << statistics.scapegoat_tree_retirement_count << '\n'
              << "scapegoat_tree_depth_violations="
              << statistics.scapegoat_tree_depth_violation_count << '\n'
              << "scapegoat_tree_ancestor_steps="
              << statistics.scapegoat_tree_ancestor_step_count << '\n'
              << "scapegoat_tree_subtree_rebuilds="
              << statistics.scapegoat_tree_subtree_rebuild_count << '\n'
              << "scapegoat_tree_whole_tree_rebuilds="
              << statistics.scapegoat_tree_whole_tree_rebuild_count << '\n'
              << "scapegoat_tree_rebuilt_nodes="
              << statistics.scapegoat_tree_rebuilt_node_count << '\n'
              << "scapegoat_tree_maximum_rebuilt_nodes="
              << statistics.scapegoat_tree_maximum_rebuilt_nodes << '\n'
              << "scapegoat_tree_maximum_structural_nodes_per_update="
              << statistics
                     .scapegoat_tree_maximum_structural_nodes_per_update
              << '\n'
              << "scapegoat_tree_maximum_final_height="
              << verified.scapegoat_tree_maximum_final_height << '\n'
              << "scapegoat_tree_max_nodes_per_query="
              << statistics.scapegoat_tree_maximum_nodes_per_query << '\n'
              << "scapegoat_tree_frame_seconds=" << measured_seconds << '\n'
              << "scapegoat_tree_frame_mib_per_second="
              << throughput(
                     verified.input_bytes, iterations, measured_seconds)
              << '\n';
        print_scapegoat_tree_depth_histogram(statistics);
        return;
    }
    const auto& statistics = verified.statistics;
    if (is_sparse_hash_tree_strategy(strategy)) {
        std::cout << "sparse_hash_tree_pool_node_capacity="
                  << pool_node_capacity << '\n'
                  << "sparse_hash_tree_promotion_candidate_threshold="
                  << promotion_threshold << '\n'
                  << "sparse_hash_tree_workspace_bytes=" << workspace_size
                  << '\n';
        if (uses_sparse_reuse_argument(strategy)) {
            std::cout << "sparse_hash_tree_promotion_reuse_threshold="
                      << static_cast<unsigned>(promotion_reuse_threshold)
                      << '\n';
        }
        if (uses_immutable_snapshot_lifecycle(strategy)) {
            std::cout
                << "sparse_hash_tree_lifecycle=immutable-snapshot\n"
                << "hash_tree_snapshot_queries="
                << statistics.hash_tree_snapshot_query_count << '\n'
                << "hash_tree_snapshot_query_nodes="
                << statistics.hash_tree_snapshot_query_node_count << '\n'
                << "hash_tree_snapshot_stale_subtree_prunes="
                << statistics.hash_tree_snapshot_stale_subtree_prune_count
                << '\n'
                << "hash_tree_snapshot_delta_queries="
                << statistics.hash_tree_snapshot_delta_query_count << '\n'
                << "hash_tree_snapshot_delta_candidates="
                << statistics.hash_tree_snapshot_delta_candidate_count
                << '\n'
                << "hash_tree_snapshot_delta_max_candidates_per_query="
                << statistics
                       .hash_tree_snapshot_delta_maximum_candidates_per_query
                << '\n'
                << "hash_tree_snapshot_promotions="
                << statistics.hash_tree_snapshot_promotion_count << '\n'
                << "hash_tree_snapshot_expirations="
                << statistics.hash_tree_snapshot_expiration_count << '\n'
                << "hash_tree_snapshot_bulk_releases="
                << statistics.hash_tree_snapshot_bulk_release_count << '\n';
            if (uses_snapshot_delta_budget_argument(strategy)) {
                std::cout
                    << "hash_tree_snapshot_delta_candidate_budget="
                    << delta_candidate_budget << '\n'
                    << "hash_tree_snapshot_delta_budget_queries="
                    << statistics
                           .hash_tree_snapshot_delta_budget_query_count
                    << '\n'
                    << "hash_tree_snapshot_delta_budget_breaches="
                    << statistics
                           .hash_tree_snapshot_delta_budget_breach_count
                    << '\n'
                    << "hash_tree_snapshot_delta_budget_demotions="
                    << statistics
                           .hash_tree_snapshot_delta_budget_demotion_count
                    << '\n'
                    << "hash_tree_snapshot_delta_budget_max_candidates_at_breach="
                    << statistics
                           .hash_tree_snapshot_delta_budget_maximum_candidates_at_breach
                    << '\n';
            }
        }
    }
    std::cout << "hash_tree_promotion_candidate_threshold="
              << promotion_threshold << '\n'
              << "hash_tree_workspace_bytes=" << workspace_size << '\n'
              << "hash_tree_queries=" << statistics.query_count << '\n'
              << "hash_tree_chain_queries="
              << statistics.hash_tree_chain_query_count << '\n'
              << "hash_tree_chain_candidates="
              << statistics.hash_tree_chain_candidate_count << '\n'
              << "hash_tree_trigger_queries="
              << statistics.hash_tree_trigger_query_count << '\n'
              << "hash_tree_tree_queries="
              << statistics.hash_tree_tree_query_count << '\n'
              << "hash_tree_promotions="
              << statistics.hash_tree_promotion_count << '\n'
              << "hash_tree_pool_rejections="
              << statistics.hash_tree_pool_rejection_count << '\n'
              << "hash_tree_promotion_trigger_candidates="
              << statistics.hash_tree_promotion_trigger_candidate_count
              << '\n'
              << "hash_tree_promotion_max_trigger_candidates="
              << statistics.hash_tree_promotion_maximum_trigger_candidates
              << '\n'
              << "hash_tree_promotion_build_nodes="
              << statistics.hash_tree_promotion_build_node_count << '\n'
              << "hash_tree_promotion_build_key_comparisons="
              << statistics.hash_tree_promotion_build_key_comparison_count
              << '\n'
              << "hash_tree_promotion_build_key_byte_comparisons="
              << statistics
                     .hash_tree_promotion_build_key_byte_comparison_count
              << '\n'
              << "hash_tree_promotion_build_rotations="
              << statistics.hash_tree_promotion_build_rotation_count << '\n'
              << "hash_tree_tree_query_nodes="
              << statistics.hash_tree_tree_query_node_count << '\n'
              << "hash_tree_tree_query_key_comparisons="
              << statistics.hash_tree_tree_query_key_comparison_count << '\n'
              << "hash_tree_tree_query_key_byte_comparisons="
              << statistics.hash_tree_tree_query_key_byte_comparison_count
              << '\n'
              << "hash_tree_tree_query_lcp_byte_comparisons="
              << statistics.hash_tree_tree_query_lcp_byte_comparison_count
              << '\n'
              << "hash_tree_tree_query_prefix_range_comparisons="
              << statistics
                     .hash_tree_tree_query_prefix_range_comparison_count
              << '\n'
              << "hash_tree_tree_query_prefix_range_byte_comparisons="
              << statistics
                     .hash_tree_tree_query_prefix_range_byte_comparison_count
              << '\n'
              << "hash_tree_tree_query_lcp_skipped_bytes="
              << statistics.hash_tree_tree_query_lcp_skipped_byte_count
              << '\n'
              << "hash_tree_insertions="
              << statistics.hash_tree_insertion_count << '\n'
              << "hash_tree_retirements="
              << statistics.hash_tree_retirement_count << '\n'
              << "hash_tree_maintenance_key_comparisons="
              << statistics.hash_tree_maintenance_key_comparison_count << '\n'
              << "hash_tree_maintenance_key_byte_comparisons="
              << statistics.hash_tree_maintenance_key_byte_comparison_count
              << '\n'
              << "hash_tree_rotations="
              << statistics.hash_tree_rotation_count << '\n'
              << "hash_tree_maximum_height="
              << statistics.hash_tree_maximum_height << '\n'
              << "hash_tree_max_nodes_per_query="
              << statistics.hash_tree_maximum_nodes_per_query << '\n'
              << "hash_tree_max_promoted_buckets="
              << statistics.hash_tree_maximum_promoted_buckets << '\n'
              << "hash_tree_max_promoted_nodes="
              << statistics.hash_tree_maximum_promoted_nodes << '\n'
              << "hash_tree_frame_seconds=" << measured_seconds << '\n'
              << "hash_tree_frame_mib_per_second="
              << throughput(
                     verified.input_bytes, iterations, measured_seconds)
              << '\n';
    if (is_sparse_hash_tree_strategy(strategy)) {
        std::cout << "sparse_hash_tree_frame_seconds=" << measured_seconds
                  << '\n'
                  << "sparse_hash_tree_frame_mib_per_second="
                  << throughput(
                         verified.input_bytes, iterations, measured_seconds)
                  << '\n';
    }
    print_hash_tree_depth_histograms(statistics);
}

void print_usage() {
    std::cerr
        << "usage: marc_lzss_match_finder_benchmark "
           "<input-file> [iterations]\n"
        << "       marc_lzss_match_finder_benchmark --frames "
           "<hash-chain-exact|hash-chain-legacy-65536-exact|"
           "hash-chain-best-length-probe-exact|"
           "hash-chain-mnemonic-mixer-v1-exact|"
           "hash-chain-buckets-262144-exact|"
           "hash-chain-buckets-1048576-exact|"
           "hash-chain-buckets-4194304-exact|"
           "binary-tree-exact|wavl-tree-exact|"
           "red-black-tree-exact|"
           "scapegoat-tree-exact> "
           "<input-file> [iterations] "
           "[frame-bytes] [window-bytes]\n"
        << "       marc_lzss_match_finder_benchmark --frames "
           "hash-tree-exact <input-file> <iterations> <frame-bytes> "
           "<window-bytes> <promotion-candidates>\n"
        << "       marc_lzss_match_finder_benchmark --frames "
           "sparse-hash-tree-exact <input-file> <iterations> <frame-bytes> "
           "<window-bytes> <pool-nodes> <promotion-candidates>\n"
        << "       marc_lzss_match_finder_benchmark --frames-limited "
           "<hash-chain-exact|hash-chain-legacy-65536-exact|"
           "hash-chain-best-length-probe-exact|"
           "hash-chain-mnemonic-mixer-v1-exact|"
           "hash-chain-buckets-262144-exact|"
           "hash-chain-buckets-1048576-exact|"
           "hash-chain-buckets-4194304-exact|"
           "binary-tree-exact> <input-file> "
           "<iterations> <frame-bytes> <window-bytes> "
           "<max-internal-buffered-bytes>\n"
        << "       marc_lzss_match_finder_benchmark --frames-limited "
           "sparse-hash-tree-exact <input-file> <iterations> "
           "<frame-bytes> <window-bytes> <pool-nodes> "
           "<promotion-candidates> <max-internal-buffered-bytes>\n"
        << "       marc_lzss_match_finder_benchmark --frames-limited "
           "sparse-hash-tree-reuse-gated-exact <input-file> <iterations> "
           "<frame-bytes> <window-bytes> <pool-nodes> "
           "<promotion-candidates> <promotion-reuse-threshold> "
           "<max-internal-buffered-bytes>\n"
        << "       marc_lzss_match_finder_benchmark --frames-limited "
           "sparse-hash-tree-immutable-snapshot-exact <input-file> "
           "<iterations> <frame-bytes> <window-bytes> <pool-nodes> "
           "<promotion-candidates> <promotion-reuse-threshold> "
           "<max-internal-buffered-bytes>\n"
        << "       marc_lzss_match_finder_benchmark --frames-limited "
           "sparse-hash-tree-snapshot-delta-budget-exact <input-file> "
           "<iterations> <frame-bytes> <window-bytes> <pool-nodes> "
           "<promotion-candidates> <promotion-reuse-threshold> "
           "<delta-candidate-budget> <max-internal-buffered-bytes>\n"
        << "       marc_lzss_match_finder_benchmark --synthetic "
           "<hash-chain-exact|hash-chain-legacy-65536-exact|"
           "hash-chain-best-length-probe-exact|"
           "hash-chain-mnemonic-mixer-v1-exact|"
           "hash-chain-buckets-262144-exact|"
           "hash-chain-buckets-1048576-exact|"
           "hash-chain-buckets-4194304-exact|"
           "binary-tree-exact|wavl-tree-exact|"
           "red-black-tree-exact|"
           "scapegoat-tree-exact> "
           "<case> "
           "[input-bytes] [iterations] "
           "[frame-bytes] [window-bytes]\n"
        << "       marc_lzss_match_finder_benchmark --synthetic "
           "hash-tree-exact <case> <input-bytes> <iterations> "
           "<frame-bytes> <window-bytes> <promotion-candidates>\n"
        << "       marc_lzss_match_finder_benchmark --synthetic "
           "sparse-hash-tree-exact <case> <input-bytes> <iterations> "
           "<frame-bytes> <window-bytes> <pool-nodes> "
           "<promotion-candidates>\n";
}

[[nodiscard]] int run_frame_benchmark(
    const int argc, const char* const argv[], const bool explicit_limit) {
    BenchmarkStrategy strategy{};
    if (argc < 3
        || (!explicit_limit && (argc < 4 || argc > 9))
        || !parse_strategy(argv[2], strategy)
        || (explicit_limit
            && ((strategy == BenchmarkStrategy::sparse_hash_tree_exact
                    && argc != 10)
                || (strategy
                        == BenchmarkStrategy::sparse_hash_tree_reuse_gated_exact
                    && argc != 11)
                || (strategy == BenchmarkStrategy::
                        sparse_hash_tree_immutable_snapshot_exact
                    && argc != 11)
                || (strategy == BenchmarkStrategy::
                        sparse_hash_tree_snapshot_delta_budget_exact
                    && argc != 12)
                || ((is_hash_chain_strategy(strategy)
                         || strategy == BenchmarkStrategy::binary_tree_exact)
                    && argc != 8)
                || (!is_sparse_hash_tree_strategy(strategy)
                    && !is_hash_chain_strategy(strategy)
                    && strategy != BenchmarkStrategy::binary_tree_exact)))
        || (!explicit_limit
            && strategy == BenchmarkStrategy::hash_tree_exact && argc != 8)
        || (strategy == BenchmarkStrategy::sparse_hash_tree_exact
            && !explicit_limit && argc != 9)
        || (strategy
                == BenchmarkStrategy::sparse_hash_tree_reuse_gated_exact
            && !explicit_limit)
        || (strategy == BenchmarkStrategy::
                sparse_hash_tree_immutable_snapshot_exact
            && !explicit_limit)
        || (strategy == BenchmarkStrategy::
                sparse_hash_tree_snapshot_delta_budget_exact
            && !explicit_limit)
        || (strategy != BenchmarkStrategy::hash_tree_exact
            && !is_sparse_hash_tree_strategy(strategy)
            && !explicit_limit && argc > 7)) {
        print_usage();
        return 2;
    }

    std::size_t iterations{1};
    std::size_t frame_size{UINT64_C(1) << 20};
    std::size_t window_size{UINT64_C(1) << 16};
    std::size_t pool_node_capacity{};
    std::uint64_t promotion_threshold{
        std::numeric_limits<std::uint64_t>::max()};
    std::uint8_t promotion_reuse_threshold{1};
    std::size_t delta_candidate_budget{};
    auto limits = marc::core::DecoderLimits{};
    if (explicit_limit) {
        limits.max_frame_size = std::numeric_limits<std::uint32_t>::max();
        limits.max_lz_distance = std::numeric_limits<std::uint32_t>::max();
    }
    if ((argc >= 5 && !parse_iterations(argv[4], iterations))
        || (argc >= 6 && !parse_size_argument(
                argv[5], limits.max_frame_size, frame_size))
        || (argc >= 7 && !parse_size_argument(
                argv[6], limits.max_lz_distance, window_size))
        || (strategy == BenchmarkStrategy::hash_tree_exact
            && !parse_promotion_threshold(argv[7], promotion_threshold))
        || (is_sparse_hash_tree_strategy(strategy)
            && (!parse_pool_node_capacity(
                    argv[7], std::min(frame_size, window_size),
                    pool_node_capacity)
                || !parse_promotion_threshold(
                    argv[8], promotion_threshold)))
        || (uses_sparse_reuse_argument(strategy)
            && !parse_promotion_reuse_threshold(
                argv[9], promotion_reuse_threshold))
        || (uses_snapshot_delta_budget_argument(strategy)
            && !parse_delta_candidate_budget(
                argv[10], delta_candidate_budget))
        || (explicit_limit
            && !parse_positive_u64(
                argv[uses_snapshot_delta_budget_argument(strategy)
                        ? 11
                        : (uses_sparse_reuse_argument(strategy)
                               ? 10
                        : (strategy == BenchmarkStrategy::sparse_hash_tree_exact
                               ? 9
                               : 7))],
                limits.max_internal_buffered_bytes))
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
    std::size_t hash_chain_bucket_count{};
    if (is_hash_chain_strategy(strategy)) {
        const auto requirements = calculate_hash_chain_workspace_for_strategy(
            strategy, frame_size, parameters, limits);
        if (requirements.error != LzssHashChainError::none) {
            std::cerr << "cannot calculate frame HashChain workspace\n";
            return 1;
        }
        workspace_size = requirements.workspace_size;
        hash_chain_bucket_count = requirements.bucket_count;
    } else if (strategy == BenchmarkStrategy::binary_tree_exact) {
        const auto requirements = calculate_lzss_binary_tree_workspace(
            frame_size, parameters, limits);
        if (requirements.error != LzssBinaryTreeError::none) {
            std::cerr << "cannot calculate frame BinaryTree workspace\n";
            return 1;
        }
        workspace_size = requirements.workspace_size;
    } else if (strategy == BenchmarkStrategy::wavl_tree_exact) {
        const auto requirements = calculate_lzss_wavl_tree_workspace(
            frame_size, parameters, limits);
        if (requirements.error != LzssWavlTreeError::none) {
            std::cerr << "cannot calculate frame WAVL workspace\n";
            return 1;
        }
        workspace_size = requirements.workspace_size;
    } else if (strategy == BenchmarkStrategy::red_black_tree_exact) {
        const auto requirements = calculate_lzss_red_black_tree_workspace(
            frame_size, parameters, limits);
        if (requirements.error != LzssRedBlackTreeError::none) {
            std::cerr << "cannot calculate frame RedBlack workspace\n";
            return 1;
        }
        workspace_size = requirements.workspace_size;
    } else if (strategy == BenchmarkStrategy::scapegoat_tree_exact) {
        const auto requirements = calculate_lzss_scapegoat_tree_workspace(
            frame_size, parameters, limits);
        if (requirements.error != LzssScapegoatTreeError::none) {
            std::cerr << "cannot calculate frame Scapegoat workspace\n";
            return 1;
        }
        workspace_size = requirements.workspace_size;
    } else if (strategy == BenchmarkStrategy::hash_tree_exact) {
        const auto requirements = calculate_lzss_hash_tree_workspace(
            frame_size, parameters, limits);
        if (requirements.error != LzssHashTreeError::none) {
            std::cerr << "cannot calculate frame HashTree workspace\n";
            return 1;
        }
        workspace_size = requirements.workspace_size;
    } else {
        const auto requirements = calculate_lzss_sparse_hash_tree_workspace(
            frame_size, parameters, limits, pool_node_capacity,
            promotion_reuse_threshold);
        if (requirements.error != LzssSparseHashTreeError::none) {
            std::cerr << "cannot calculate frame sparse HashTree workspace\n";
            return 1;
        }
        workspace_size = requirements.workspace_size;
    }
    AlignedWorkspace workspace_owner(workspace_size);
    const auto workspace = workspace_owner.bytes(workspace_size);

    FrameRunResult verified{};
    if (!process_frames(
            strategy, argv[3], file_size, frame_size, parameters, limits,
            workspace, true, false, pool_node_capacity,
            promotion_threshold, promotion_reuse_threshold,
            delta_candidate_budget, verified)
        || verified.input_bytes != file_size
        || verified.statistics.query_count != verified.token_count
        || !valid_token_summary(verified)
        || !valid_statistics(
            strategy, verified.statistics, delta_candidate_budget)) {
        std::cerr << "match-finder frame verification failed\n";
        return 1;
    }

    if (strategy == BenchmarkStrategy::hash_chain_best_length_probe_exact) {
        FrameRunResult baseline{};
        if (!process_frames(
                BenchmarkStrategy::hash_chain_exact, argv[3], file_size,
                frame_size, parameters, limits, workspace, true, false,
                pool_node_capacity, promotion_threshold,
                promotion_reuse_threshold, delta_candidate_budget, baseline)
            || !valid_token_summary(baseline)
            || !valid_hash_chain_statistics(baseline.statistics)
            || baseline.token_count != verified.token_count
            || baseline.statistics.candidate_count != verified.statistics.candidate_count
            || token_fingerprint_hex(baseline.token_summary)
                != token_fingerprint_hex(verified.token_summary)) {
            std::cerr << "best-length probe baseline identity check failed\n";
            return 1;
        }
    }

    double measured_seconds{};
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
        FrameRunResult measured{};
        if (!process_frames(
                strategy, argv[3], file_size, frame_size, parameters, limits,
                workspace, false, true, pool_node_capacity,
                promotion_threshold, promotion_reuse_threshold,
                delta_candidate_budget, measured)
            || measured.input_bytes != verified.input_bytes
            || measured.frame_count != verified.frame_count
            || measured.token_count != verified.token_count) {
            std::cerr << "timed match-finder frame processing failed\n";
            return 1;
        }
        measured_seconds += measured.seconds;
    }

    print_frame_report(
        strategy, explicit_limit ? "frames-limited" : "frames", {},
        frame_size, window_size, iterations, workspace_size,
        hash_chain_bucket_count,
        limits.max_internal_buffered_bytes, pool_node_capacity,
        promotion_threshold, promotion_reuse_threshold,
        delta_candidate_budget, verified,
        measured_seconds);
    return 0;
}

[[nodiscard]] int run_synthetic_benchmark(
    const int argc, const char* const argv[]) {
    BenchmarkStrategy strategy{};
    if (argc < 4 || argc > 10 || !parse_strategy(argv[2], strategy)
        || strategy
            == BenchmarkStrategy::sparse_hash_tree_reuse_gated_exact
        || strategy
            == BenchmarkStrategy::sparse_hash_tree_immutable_snapshot_exact
        || strategy
            == BenchmarkStrategy::sparse_hash_tree_snapshot_delta_budget_exact
        || (strategy == BenchmarkStrategy::hash_tree_exact && argc != 9)
        || (strategy == BenchmarkStrategy::sparse_hash_tree_exact
            && argc != 10)
        || (strategy != BenchmarkStrategy::hash_tree_exact
            && strategy != BenchmarkStrategy::sparse_hash_tree_exact
            && argc > 8)) {
        print_usage();
        return 2;
    }
    SyntheticInputKind kind{};
    std::size_t input_size{UINT64_C(1) << 20};
    std::size_t iterations{1};
    std::size_t frame_size{UINT64_C(1) << 20};
    std::size_t window_size{UINT64_C(1) << 20};
    std::size_t pool_node_capacity{};
    std::uint64_t promotion_threshold{
        std::numeric_limits<std::uint64_t>::max()};
    constexpr std::uint8_t promotion_reuse_threshold{1};
    const marc::core::DecoderLimits limits{};
    if (!parse_synthetic_input_kind(argv[3], kind)
        || (argc >= 5 && !parse_size_argument(
                argv[4], limits.max_total_output_size, input_size))
        || (argc >= 6 && !parse_iterations(argv[5], iterations))
        || (argc >= 7 && !parse_size_argument(
                argv[6], limits.max_frame_size, frame_size))
        || (argc >= 8 && !parse_size_argument(
                argv[7], limits.max_lz_distance, window_size))
        || (strategy == BenchmarkStrategy::hash_tree_exact
            && !parse_promotion_threshold(argv[8], promotion_threshold))
        || (strategy == BenchmarkStrategy::sparse_hash_tree_exact
            && (!parse_pool_node_capacity(
                    argv[8], std::min(frame_size, window_size),
                    pool_node_capacity)
                || !parse_promotion_threshold(
                    argv[9], promotion_threshold)))
        || window_size > std::numeric_limits<std::uint32_t>::max()) {
        std::cerr << "invalid synthetic benchmark argument\n";
        return 2;
    }

    LzssParameters parameters{};
    parameters.window_size = static_cast<std::uint32_t>(window_size);
    std::size_t workspace_size{};
    std::size_t hash_chain_bucket_count{};
    if (is_hash_chain_strategy(strategy)) {
        const auto requirements = calculate_hash_chain_workspace_for_strategy(
            strategy, frame_size, parameters, limits);
        if (requirements.error != LzssHashChainError::none) {
            std::cerr << "cannot calculate synthetic HashChain workspace\n";
            return 1;
        }
        workspace_size = requirements.workspace_size;
        hash_chain_bucket_count = requirements.bucket_count;
    } else if (strategy == BenchmarkStrategy::binary_tree_exact) {
        const auto requirements = calculate_lzss_binary_tree_workspace(
            frame_size, parameters, limits);
        if (requirements.error != LzssBinaryTreeError::none) {
            std::cerr << "cannot calculate synthetic BinaryTree workspace\n";
            return 1;
        }
        workspace_size = requirements.workspace_size;
    } else if (strategy == BenchmarkStrategy::wavl_tree_exact) {
        const auto requirements = calculate_lzss_wavl_tree_workspace(
            frame_size, parameters, limits);
        if (requirements.error != LzssWavlTreeError::none) {
            std::cerr << "cannot calculate synthetic WAVL workspace\n";
            return 1;
        }
        workspace_size = requirements.workspace_size;
    } else if (strategy == BenchmarkStrategy::red_black_tree_exact) {
        const auto requirements = calculate_lzss_red_black_tree_workspace(
            frame_size, parameters, limits);
        if (requirements.error != LzssRedBlackTreeError::none) {
            std::cerr << "cannot calculate synthetic RedBlack workspace\n";
            return 1;
        }
        workspace_size = requirements.workspace_size;
    } else if (strategy == BenchmarkStrategy::scapegoat_tree_exact) {
        const auto requirements = calculate_lzss_scapegoat_tree_workspace(
            frame_size, parameters, limits);
        if (requirements.error != LzssScapegoatTreeError::none) {
            std::cerr << "cannot calculate synthetic Scapegoat workspace\n";
            return 1;
        }
        workspace_size = requirements.workspace_size;
    } else if (strategy == BenchmarkStrategy::hash_tree_exact) {
        const auto requirements = calculate_lzss_hash_tree_workspace(
            frame_size, parameters, limits);
        if (requirements.error != LzssHashTreeError::none) {
            std::cerr << "cannot calculate synthetic HashTree workspace\n";
            return 1;
        }
        workspace_size = requirements.workspace_size;
    } else {
        const auto requirements = calculate_lzss_sparse_hash_tree_workspace(
            frame_size, parameters, limits, pool_node_capacity);
        if (requirements.error != LzssSparseHashTreeError::none) {
            std::cerr
                << "cannot calculate synthetic sparse HashTree workspace\n";
            return 1;
        }
        workspace_size = requirements.workspace_size;
    }
    AlignedWorkspace workspace_owner(workspace_size);
    const auto workspace = workspace_owner.bytes(workspace_size);

    FrameRunResult verified{};
    if (!process_synthetic_frames(
            strategy, kind, input_size, frame_size, parameters, limits,
            workspace, true, false, pool_node_capacity,
            promotion_threshold, promotion_reuse_threshold, 0, verified)
        || verified.input_bytes != input_size
        || verified.statistics.query_count != verified.token_count
        || !valid_token_summary(verified)
        || !valid_statistics(strategy, verified.statistics, 0)) {
        std::cerr << "synthetic match-finder verification failed\n";
        return 1;
    }

    if (strategy == BenchmarkStrategy::hash_chain_best_length_probe_exact) {
        FrameRunResult baseline{};
        if (!process_synthetic_frames(
                BenchmarkStrategy::hash_chain_exact, kind, input_size,
                frame_size, parameters, limits, workspace, true, false,
                pool_node_capacity, promotion_threshold,
                promotion_reuse_threshold, 0, baseline)
            || !valid_token_summary(baseline)
            || !valid_hash_chain_statistics(baseline.statistics)
            || baseline.token_count != verified.token_count
            || baseline.statistics.candidate_count != verified.statistics.candidate_count
            || token_fingerprint_hex(baseline.token_summary)
                != token_fingerprint_hex(verified.token_summary)) {
            std::cerr << "synthetic best-length probe baseline identity check failed\n";
            return 1;
        }
    }

    double measured_seconds{};
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
        FrameRunResult measured{};
        if (!process_synthetic_frames(
                strategy, kind, input_size, frame_size, parameters, limits,
                workspace, false, true, pool_node_capacity,
                promotion_threshold, promotion_reuse_threshold, 0, measured)
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
        window_size, iterations, workspace_size,
        hash_chain_bucket_count,
        limits.max_internal_buffered_bytes, pool_node_capacity,
        promotion_threshold, promotion_reuse_threshold, 0, verified,
        measured_seconds);
    return 0;
}

} // namespace

int main(const int argc, const char* const argv[]) {
    if (argc >= 2 && std::string_view{argv[1]} == "--frames") {
        return run_frame_benchmark(argc, argv, false);
    }
    if (argc >= 2 && std::string_view{argv[1]} == "--frames-limited") {
        return run_frame_benchmark(argc, argv, true);
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
