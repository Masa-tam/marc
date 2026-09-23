#include "marc/marc.h"

#include "context/lzss_contextual_rans_encode_phase_timing.hpp"
#include "core/checked_math.hpp"
#include "core/sha256.hpp"
#include "dictionary/lzss_typed_tokenize_timing.hpp"
#include "frame/lzss_contextual_rans_frame_streaming_encoder.hpp"
#include "frame/lzss_contextual_rans_profile.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using marc::context::internal::LzssContextualRansEncodePhaseSummary;
using marc::context::internal::LzssContextualRansEncodePhaseTiming;
using marc::dictionary::internal::LzssTypedTokenizeSummary;
using marc::dictionary::internal::LzssTypedTokenizeTiming;
using marc::frame::internal::LzssContextualRansEncoderViews;
using marc::frame::internal::LzssContextualRansEncoderWorkspaceRequirements;
using marc::frame::internal::LzssContextualRansFrameStreamingEncoder;
using marc::frame::internal::LzssContextualRansStreamHeader;

constexpr std::size_t maximum_output_capacity = std::size_t{1} << 30;

enum class ReportMode : std::uint8_t {
    phases,
    tokenize_breakdown,
    probe_baseline,
    probe_candidate,
};

struct TransformDeleter {
    void operator()(marc_transform* value) const noexcept {
        marc_transform_destroy(value);
    }
};

struct AlignedStorage {
    std::vector<std::byte> storage{};
    std::span<std::byte> view{};

    [[nodiscard]] bool reset(const std::size_t size,
                             const std::size_t alignment) {
        if (alignment == 0 || size > std::numeric_limits<std::size_t>::max()
                                         - (alignment - 1)) {
            return false;
        }
        storage.resize(size + alignment - 1);
        if (size == 0) {
            view = {};
            return true;
        }
        const auto address = reinterpret_cast<std::uintptr_t>(storage.data());
        const auto remainder = address % alignment;
        const auto offset = remainder == 0 ? 0 : alignment - remainder;
        view = {storage.data() + offset, size};
        return true;
    }
};

[[nodiscard]] marc_buffer c_buffer(const std::span<std::byte> value) noexcept {
    return {reinterpret_cast<std::uint8_t*>(value.data()), value.size()};
}

[[nodiscard]] marc_const_buffer c_buffer(
    const std::span<const std::byte> value) noexcept {
    return {reinterpret_cast<const std::uint8_t*>(value.data()),
            value.size()};
}

[[nodiscard]] bool read_file(const std::filesystem::path& path,
                             std::vector<std::byte>& bytes) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size > std::numeric_limits<std::size_t>::max()
        || size > static_cast<std::uintmax_t>(
                       std::numeric_limits<std::streamsize>::max())) {
        return false;
    }
    bytes.resize(static_cast<std::size_t>(size));
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (input.gcount() != static_cast<std::streamsize>(bytes.size())) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool output_capacity(const std::size_t input_size,
                                   const std::uint32_t frame_size,
                                   std::size_t& capacity) noexcept {
    if (frame_size == 0) return false;
    const auto frames = input_size == 0 ? std::size_t{0}
        : std::size_t{1} + (input_size - 1) / frame_size;
    std::size_t payload{};
    std::size_t frame_overhead{};
    return marc::core::checked_multiply(input_size, std::size_t{14}, payload)
        && marc::core::checked_multiply(
            frames, std::size_t{9193}, frame_overhead)
        && marc::core::checked_add(
            std::size_t{112}, payload, capacity)
        && marc::core::checked_add(capacity, frame_overhead, capacity)
        && capacity <= maximum_output_capacity;
}

[[nodiscard]] bool digest_hex(const std::span<const std::byte> bytes,
                              std::string& result) {
    marc::core::Sha256 hash{};
    std::array<std::byte, marc::core::sha256_digest_size> digest{};
    if (!hash.update(bytes) || !hash.finalize(digest)) return false;
    constexpr std::string_view alphabet = "0123456789abcdef";
    result.clear();
    result.reserve(digest.size() * 2);
    for (const auto byte : digest) {
        const auto value = std::to_integer<unsigned>(byte);
        result.push_back(alphabet[value >> 4]);
        result.push_back(alphabet[value & 15U]);
    }
    return true;
}

[[nodiscard]] bool configure(const marc_direction direction,
                             const std::size_t original_size,
                             marc_lzss_contextual_rans_config& config) {
    if (marc_lzss_contextual_rans_config_init(direction, &config)
            != MARC_STATUS_OK
        || marc_lzss_contextual_rans_config_apply_profile(
               &config, MARC_LZSS_CONTEXTUAL_PROFILE_4M) != MARC_STATUS_OK) {
        return false;
    }
    config.original_size = original_size;
    return true;
}

[[nodiscard]] bool run_public(
    const marc_lzss_contextual_rans_config& config,
    const std::span<const std::byte> input,
    const std::span<std::byte> output, std::size_t& produced,
    marc_workspace_requirements& requirements,
    std::chrono::nanoseconds* const elapsed = nullptr) {
    if (marc_lzss_contextual_rans_workspace_requirements(
            &config, &requirements) != MARC_STATUS_OK) {
        return false;
    }
    std::vector<std::byte> primary(requirements.primary_bytes);
    std::vector<std::byte> secondary(requirements.secondary_bytes);
    AlignedStorage views{};
    if (!views.reset(requirements.views_bytes,
                     requirements.views_alignment)) {
        return false;
    }
    marc_transform* raw{};
    if (marc_lzss_contextual_rans_create(
            &config, c_buffer(std::span<std::byte>{primary}),
            c_buffer(std::span<std::byte>{secondary}),
            c_buffer(views.view), &raw) != MARC_STATUS_OK) {
        return false;
    }
    const std::unique_ptr<marc_transform, TransformDeleter> transform{raw};
    const auto start = std::chrono::steady_clock::now();
    const auto processed = marc_transform_process(
        transform.get(), c_buffer(input), c_buffer(output),
        MARC_PROCESS_END_INPUT);
    if (elapsed != nullptr) {
        *elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - start);
    }
    produced = processed.output_produced;
    return processed.status == MARC_STATUS_END_OF_STREAM
        && processed.input_consumed == input.size();
}

[[nodiscard]] bool private_profile(
    const marc_lzss_contextual_rans_config& config,
    const marc_workspace_requirements& public_requirements,
    marc::core::DecoderLimits& limits,
    LzssContextualRansStreamHeader& stream,
    LzssContextualRansEncoderWorkspaceRequirements& requirements) noexcept {
    limits.max_total_output_size = config.max_total_output_size;
    limits.max_frame_size = config.max_frame_size;
    limits.max_block_size = config.max_block_size;
    limits.max_compressed_payload_size = config.max_compressed_payload_size;
    limits.max_internal_buffered_bytes = config.max_internal_buffered_bytes;
    limits.max_lz_distance = config.max_lz_distance;
    limits.max_lz_match_length = config.max_lz_match_length;
    limits.max_entropy_table_entries = config.max_entropy_table_entries;
    const marc::dictionary::internal::LzssParameters dictionary{
        config.window_size, config.min_match_length,
        config.max_match_length, 0};
    if (marc::frame::internal::make_lzss_contextual_rans_profile(
            {config.original_size, config.frame_size, dictionary,
             marc::frame::internal::LzssContextualRansProfileVariant::
                 field_context_4m,
             marc::dictionary::internal::LzssMatchFinderStrategy::
                 hash_chain_exact},
            limits, stream, requirements)
        != marc::frame::internal::LzssContextualRansProfileError::none) {
        return false;
    }
    return config.profile == MARC_LZSS_CONTEXTUAL_PROFILE_4M
        && config.match_finder_strategy
            == MARC_LZSS_MATCH_FINDER_HASH_CHAIN_EXACT
        && requirements.frame_input_bytes == public_requirements.primary_bytes
        && requirements.frame_encoded_bytes
            == public_requirements.secondary_bytes
        && requirements.views_bytes == public_requirements.views_bytes
        && requirements.views_alignment
            == public_requirements.views_alignment;
}

[[nodiscard]] bool run_private(
    const LzssContextualRansStreamHeader& stream,
    const marc::core::DecoderLimits& limits,
    const LzssContextualRansEncoderWorkspaceRequirements& requirements,
    const std::span<const std::byte> input,
    const std::span<std::byte> output,
    LzssContextualRansEncodePhaseTiming* const timing,
    LzssTypedTokenizeTiming* const tokenize_timing,
    std::size_t& produced,
    std::uint64_t& token_count,
    std::chrono::nanoseconds& elapsed,
    const bool private_best_length_probe = false) {
    std::vector<std::byte> primary(requirements.frame_input_bytes);
    std::vector<std::byte> secondary(requirements.frame_encoded_bytes);
    AlignedStorage views_storage{};
    if (!views_storage.reset(requirements.views_bytes,
                             requirements.views_alignment)) {
        return false;
    }
    LzssContextualRansEncoderViews views{};
    if (marc::frame::internal::partition_lzss_contextual_rans_encoder_views(
            requirements, views_storage.view, views)
        != marc::frame::internal::LzssContextualRansWorkspaceError::none) {
        return false;
    }
    LzssContextualRansFrameStreamingEncoder encoder{
        stream, limits, primary, views.tokens, views.match_finder, secondary,
        requirements.match_finder_strategy, timing, tokenize_timing,
        private_best_length_probe};
    const auto start = std::chrono::steady_clock::now();
    const auto result = encoder.process(
        input, output, marc::core::flag_value(marc::core::ProcessFlags::end_input));
    elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - start);
    produced = result.output_produced;
    token_count = encoder.diagnostic_token_count();
    return result.status == marc::core::StreamStatus::end_of_stream
        && result.input_consumed == input.size();
}

[[nodiscard]] bool parse_iterations(const std::string_view text,
                                    std::uint32_t& iterations) noexcept {
    iterations = 0;
    const auto parsed = std::from_chars(
        text.data(), text.data() + text.size(), iterations);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size()
        && iterations != 0;
}

[[nodiscard]] int run(const std::filesystem::path& path,
                      const std::uint32_t iterations,
                      const ReportMode mode) {
    std::vector<std::byte> input{};
    if (!read_file(path, input)) {
        std::cerr << "input read failed\n";
        return 1;
    }
    marc_lzss_contextual_rans_config encoder_config{};
    marc_lzss_contextual_rans_config decoder_config{};
    if (!configure(MARC_DIRECTION_ENCODE, input.size(), encoder_config)
        || !configure(MARC_DIRECTION_DECODE, input.size(), decoder_config)) {
        std::cerr << "configuration failed\n";
        return 1;
    }
    std::size_t capacity{};
    if (!output_capacity(input.size(), encoder_config.frame_size, capacity)) {
        std::cerr << "output capacity exceeds diagnostic limit\n";
        return 1;
    }
    std::vector<std::byte> archive(capacity);
    marc_workspace_requirements public_requirements{};
    std::size_t public_size{};
    if (!run_public(encoder_config, input, archive, public_size,
                    public_requirements)) {
        std::cerr << "public encode failed\n";
        return 1;
    }
    std::string expected_digest{};
    if (!digest_hex(std::span<const std::byte>{archive}.first(public_size),
                    expected_digest)) {
        return 1;
    }
    std::vector<std::byte> decoded(input.size());
    marc_workspace_requirements decoder_requirements{};
    std::size_t decoded_size{};
    if (!run_public(decoder_config,
                    std::span<const std::byte>{archive}.first(public_size),
                    decoded, decoded_size, decoder_requirements)
        || decoded_size != input.size() || decoded != input) {
        std::cerr << "public round trip failed\n";
        return 1;
    }
    marc::core::DecoderLimits limits{};
    LzssContextualRansStreamHeader stream{};
    LzssContextualRansEncoderWorkspaceRequirements private_requirements{};
    if (!private_profile(encoder_config, public_requirements, limits, stream,
                         private_requirements)) {
        std::cerr << "public/private configuration mismatch\n";
        return 1;
    }
    std::size_t workspace_bytes{};
    if (!marc::core::checked_add(public_requirements.primary_bytes,
                                 public_requirements.secondary_bytes,
                                 workspace_bytes)
        || !marc::core::checked_add(workspace_bytes,
                                    public_requirements.views_bytes,
                                    workspace_bytes)) {
        std::cerr << "workspace size overflow\n";
        return 1;
    }
    std::string input_digest{};
    if (!digest_hex(input, input_digest)) return 1;
    const bool probe_comparison = mode == ReportMode::probe_baseline
        || mode == ReportMode::probe_candidate;
    const bool use_probe = mode == ReportMode::probe_candidate;
    std::vector<std::byte> expected_archive;
    if (probe_comparison) {
        expected_archive.assign(archive.begin(), archive.begin() + public_size);
    }
    std::size_t actual_size{};
    std::uint64_t token_count{};
    std::chrono::nanoseconds elapsed{};
    if (!run_private(stream, limits, private_requirements, input, archive,
                     nullptr, nullptr, actual_size, token_count, elapsed,
                     use_probe)
        || actual_size != public_size) {
        std::cerr << "untimed private encode mismatch\n";
        return 1;
    }

    if (probe_comparison) {
        const auto matches = [&]() {
            return actual_size == expected_archive.size()
                && std::equal(expected_archive.begin(), expected_archive.end(),
                              archive.begin());
        };
        if (!matches()) {
            std::cerr << "private archive byte mismatch\n";
            return 1;
        }
        std::size_t decoder_workspace_bytes{};
        if (!marc::core::checked_add(decoder_requirements.primary_bytes,
                                    decoder_requirements.secondary_bytes,
                                    decoder_workspace_bytes)
            || !marc::core::checked_add(decoder_workspace_bytes,
                                      decoder_requirements.views_bytes,
                                      decoder_workspace_bytes)) return 1;
        std::cout << std::setprecision(17)
                  << "report_schema=lzss-contextual-rans-best-length-probe-v1\n"
                  << "codec=lzss-contextual-rans-4m\n"
                  << "strategy=" << (use_probe ? "hash-chain-best-length-probe-exact"
                                               : "hash-chain-exact") << '\n'
                  << "instrumented_token_loop=0\n"
                  << "input_bytes=" << input.size() << '\n'
                  << "input_sha256=" << input_digest << '\n'
                  << "archive_bytes=" << public_size << '\n'
                  << "archive_sha256=" << expected_digest << '\n'
                  << "encoded_to_input_ratio=" << (input.empty() ? 0.0
                      : static_cast<double>(public_size) / input.size()) << '\n'
                  << "frame_size=" << encoder_config.frame_size << '\n'
                  << "window_size=" << encoder_config.window_size << '\n'
                  << "min_match_length=" << encoder_config.min_match_length << '\n'
                  << "max_match_length=" << encoder_config.max_match_length << '\n'
                  << "encoder_workspace_bytes=" << workspace_bytes << '\n'
                  << "decoder_workspace_bytes=" << decoder_workspace_bytes << '\n'
                  << "codec_peak_workspace_bytes="
                  << std::max(workspace_bytes, decoder_workspace_bytes) << '\n'
                  << "iterations=" << iterations << '\n';
        for (std::uint32_t index = 0; index < iterations; ++index) {
            if (!run_private(stream, limits, private_requirements, input, archive,
                             nullptr, nullptr, actual_size, token_count, elapsed,
                             use_probe) || !matches()) {
                std::cerr << "timed private archive byte mismatch\n";
                return 1;
            }
            std::chrono::nanoseconds decode_elapsed{};
            if (!run_public(decoder_config,
                            std::span<const std::byte>{archive}.first(actual_size),
                            decoded, decoded_size, decoder_requirements,
                            &decode_elapsed)
                || decoded_size != input.size() || decoded != input) {
                std::cerr << "timed public round trip failed\n";
                return 1;
            }
            const auto throughput = [&](const std::chrono::nanoseconds duration) {
                return duration.count() > 0
                    ? static_cast<double>(input.size()) / (1024.0 * 1024.0)
                        / std::chrono::duration<double>(duration).count()
                    : 0.0;
            };
            std::cout << "iteration=" << index + 1 << '\n'
                      << "encode_nanoseconds=" << elapsed.count() << '\n'
                      << "decode_nanoseconds=" << decode_elapsed.count() << '\n'
                      << "encode_mib_per_second=" << throughput(elapsed) << '\n'
                      << "decode_mib_per_second=" << throughput(decode_elapsed) << '\n';
        }
        return 0;
    }
    std::string actual_digest{};
    if (!digest_hex(std::span<const std::byte>{archive}.first(actual_size),
                    actual_digest)
        || actual_digest != expected_digest) {
        std::cerr << "untimed private archive mismatch\n";
        return 1;
    }

    if (mode == ReportMode::tokenize_breakdown) {
        std::cout << "report_schema=lzss-contextual-rans-tokenize-breakdown-v1\n"
                  << "instrumented_token_loop=1\n";
    }
    std::cout << "codec=lzss-contextual-rans-4m\n"
              << "input_bytes=" << input.size() << '\n'
              << "input_sha256=" << input_digest << '\n'
              << "archive_bytes=" << public_size << '\n'
              << "archive_sha256=" << expected_digest << '\n'
              << "frame_size=" << encoder_config.frame_size << '\n'
              << "frame_count=" << (input.empty() ? std::size_t{0}
                  : std::size_t{1} + (input.size() - 1)
                      / encoder_config.frame_size) << '\n'
              << "window_size=" << encoder_config.window_size << '\n'
              << "encoder_workspace_primary_bytes="
              << public_requirements.primary_bytes << '\n'
              << "encoder_workspace_secondary_bytes="
              << public_requirements.secondary_bytes << '\n'
              << "encoder_workspace_views_bytes="
              << public_requirements.views_bytes << '\n'
              << "encoder_workspace_views_alignment="
              << public_requirements.views_alignment << '\n'
              << "encoder_workspace_bytes=" << workspace_bytes << '\n'
              << "iterations=" << iterations << '\n';
    for (std::uint32_t index = 0; index < iterations; ++index) {
        LzssContextualRansEncodePhaseTiming timing{};
        LzssTypedTokenizeTiming tokenize_timing{};
        auto* const inner = mode == ReportMode::tokenize_breakdown
            ? &tokenize_timing : nullptr;
        if (!run_private(stream, limits, private_requirements, input, archive,
                         &timing, inner, actual_size, token_count, elapsed)
            || actual_size != public_size
            || !digest_hex(std::span<const std::byte>{archive}.first(actual_size),
                           actual_digest)
            || actual_digest != expected_digest) {
            std::cerr << "timed private archive mismatch\n";
            return 1;
        }
        LzssContextualRansEncodePhaseSummary summary{};
        if (!timing.summarize(elapsed, summary)) {
            std::cerr << "invalid phase partition\n";
            return 1;
        }
        LzssTypedTokenizeSummary breakdown{};
        if (inner != nullptr) {
            const auto outer_tokenize = summary.phase_nanoseconds[0];
            if (!std::in_range<std::chrono::nanoseconds::rep>(outer_tokenize)
                || !inner->summarize(
                    std::chrono::nanoseconds{
                        static_cast<std::chrono::nanoseconds::rep>(
                            outer_tokenize)},
                    token_count, input.size(), breakdown)) {
                std::cerr << "invalid token-production partition\n";
                return 1;
            }
        }
        std::cout << "iteration=" << index + 1 << '\n'
                  << "total_nanoseconds=" << summary.total_nanoseconds << '\n'
                  << "tokenize_nanoseconds=" << summary.phase_nanoseconds[0] << '\n'
                  << "first_plan_nanoseconds=" << summary.phase_nanoseconds[1] << '\n'
                  << "second_plan_nanoseconds=" << summary.phase_nanoseconds[2] << '\n'
                  << "reverse_write_nanoseconds=" << summary.phase_nanoseconds[3] << '\n'
                  << "frame_finish_nanoseconds=" << summary.phase_nanoseconds[4] << '\n'
                  << "other_nanoseconds=" << summary.other_nanoseconds << '\n';
        if (inner != nullptr) {
            std::cout << "token_count=" << token_count << '\n'
                      << "finder_initialize_nanoseconds="
                      << breakdown.phase_nanoseconds[0] << '\n'
                      << "finder_query_nanoseconds="
                      << breakdown.phase_nanoseconds[1] << '\n'
                      << "finder_advance_nanoseconds="
                      << breakdown.phase_nanoseconds[2] << '\n'
                      << "token_other_nanoseconds="
                      << breakdown.token_other_nanoseconds << '\n'
                      << "finder_query_count="
                      << breakdown.finder_query_count << '\n'
                      << "finder_advance_count="
                      << breakdown.finder_advance_count << '\n'
                      << "advanced_input_bytes="
                      << breakdown.advanced_input_bytes << '\n';
        }
    }
    return 0;
}

} // namespace

int main(const int argc, char* argv[]) {
    const bool breakdown = argc > 1
        && std::string_view{argv[1]} == "--tokenize-breakdown";
    const bool baseline = argc > 1
        && std::string_view{argv[1]} == "--best-length-probe-baseline";
    const bool candidate = argc > 1
        && std::string_view{argv[1]} == "--best-length-probe-candidate";
    const int path_index = breakdown || baseline || candidate ? 2 : 1;
    if (argc < path_index + 1 || argc > path_index + 2) {
        std::cerr << "usage: marc_lzss_contextual_rans_phase_benchmark "
                     "[--tokenize-breakdown|--best-length-probe-baseline|"
                     "--best-length-probe-candidate] <input> [iterations]\n";
        return 2;
    }
    std::uint32_t iterations{1};
    if (argc == path_index + 2
        && !parse_iterations(argv[path_index + 1], iterations)) {
        std::cerr << "invalid iteration count\n";
        return 2;
    }
    return run(argv[path_index], iterations,
               baseline ? ReportMode::probe_baseline
               : candidate ? ReportMode::probe_candidate
               : breakdown ? ReportMode::tokenize_breakdown
                         : ReportMode::phases);
}
