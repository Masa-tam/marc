#include "dictionary/lzss_encoder.hpp"
#include "dictionary/lzss_hash_chain_match_finder.hpp"
#include "dictionary/lzss_match_finder.hpp"
#include "dictionary/lzss_typed_encoder.hpp"
#include "frame/lzss_typed_context_frame_encoder.hpp"
#include "frame/lzss_contextual_rans_frame_encoder.hpp"
#include "frame/lzss_contextual_tans_frame_encoder.hpp"
#include "frame/lzss_contextual_blocked_huffman_frame_encoder.hpp"
#include "frame/lzss_contextual_adaptive_huffman_frame_encoder.hpp"
#include "frame/lzss_frame.hpp"

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
