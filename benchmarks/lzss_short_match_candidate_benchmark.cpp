#include "context/lzss_field_context.hpp"
#include "dictionary/lzss_hash_chain_match_finder.hpp"
#include "dictionary/lzss_short_prefix_match_finder.hpp"
#include "dictionary/lzss_typed_encoder.hpp"
#include "entropy/contextual_dynamic_range_encoder.hpp"
#include "frame/lzss_short_match_candidate_selector.hpp"
#include "frame/lzss_short_match_frame_decoder.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
using marc::context::internal::ModeledOperation;
using marc::dictionary::internal::LzssParameters;
using marc::dictionary::internal::LzssTypedToken;

[[nodiscard]] bool parse_positive(const char* const text,
                                  const std::size_t upper,
                                  std::size_t& value) noexcept {
    const std::string_view input{text};
    const auto parsed = std::from_chars(input.data(),
                                        input.data() + input.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == input.data() + input.size()
        && value > 0 && value <= upper;
}

[[nodiscard]] bool baseline_frame_size_from_tokens(
    const std::size_t raw_size,
    const std::uint64_t committed,
    const marc::core::DecoderLimits& limits,
    const std::span<const LzssTypedToken> tokens,
    const std::span<ModeledOperation> operations,
    std::size_t& serialized_size) noexcept {
    const LzssParameters parameters{65536, 5, 258, 0};
    const marc::dictionary::internal::LzssTypedFrameValidationContext context{
        static_cast<std::uint32_t>(tokens.size()),
        static_cast<std::uint32_t>(raw_size), committed};
    const auto modeled = marc::context::internal::
        model_lzss_field_context_tokens(
            tokens, parameters, context, limits, operations);
    if (modeled.error != marc::context::internal::LzssFieldContextError::none)
        return false;
    marc::entropy::internal::ContextualDynamicRangeDescriptor descriptor{};
    const auto planned = marc::entropy::internal::
        plan_contextual_dynamic_range_operations(
            operations.first(modeled.operation_count), limits, descriptor);
    if (planned.error != marc::entropy::internal::
            ContextualDynamicRangeEncodeError::none
        || planned.decision_count != modeled.decision_count) return false;
    serialized_size = marc::frame::internal::typed_context_frame_header_size
        + marc::frame::internal::typed_context_range_descriptor_size
        + planned.payload_size;
    return true;
}

[[nodiscard]] bool baseline_frame_size(
    const std::span<const std::byte> raw,
    const std::uint64_t committed,
    const marc::core::DecoderLimits& limits,
    const std::span<LzssTypedToken> tokens,
    const std::span<ModeledOperation> operations,
    const std::span<std::byte> finder_workspace,
    std::size_t& token_count,
    std::size_t& serialized_size) noexcept {
    const LzssParameters parameters{65536, 5, 258, 0};
    const auto parsed = marc::dictionary::internal::
        encode_lzss_typed_tokens_hash_chain_single_pass(
            raw, parameters, limits, tokens, finder_workspace);
    if (parsed.error != marc::dictionary::internal::
            LzssTypedEncodeError::none) return false;
    token_count = parsed.token_count;
    return baseline_frame_size_from_tokens(
        raw.size(), committed, limits, tokens.first(token_count),
        operations, serialized_size);
}

[[nodiscard]] bool same_token(const LzssTypedToken& first,
                              const LzssTypedToken& second) noexcept {
    return first.kind == second.kind && first.literal == second.literal
        && first.distance == second.distance && first.length == second.length;
}

} // namespace

int main(const int argc, const char* const argv[]) {
    if (argc != 4 && argc != 5) {
        std::cerr << "usage: marc_lzss_short_match_candidate_benchmark "
                     "<input> <max-frames:1..1024> <frame-bytes:1..65536> "
                     "[indexed|reference]\n";
        return 2;
    }
    const std::string_view search = argc == 5 ? argv[4] : "indexed";
    if (search != "indexed" && search != "reference") {
        std::cerr << "invalid search mode\n";
        return 2;
    }
    std::size_t maximum_frames{};
    std::size_t frame_bytes{};
    if (!parse_positive(argv[2], 1024, maximum_frames)
        || !parse_positive(argv[3], 65536, frame_bytes)) {
        std::cerr << "invalid bounded sample parameters\n";
        return 2;
    }
    std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
    if (!input) {
        std::cerr << "cannot open input\n";
        return 2;
    }
    const auto end = input.tellg();
    if (end < 0) {
        std::cerr << "cannot determine input size\n";
        return 2;
    }
    const auto input_bytes = static_cast<std::uint64_t>(end);
    const auto sample_bytes = std::min<std::uint64_t>(
        input_bytes, maximum_frames * frame_bytes);
    input.seekg(0, std::ios::beg);
    if (!input) return 2;

    const marc::core::DecoderLimits limits{};
    const LzssParameters baseline_parameters{65536, 5, 258, 0};
    const auto required = marc::dictionary::internal::
        calculate_lzss_hash_chain_workspace(
            frame_bytes, baseline_parameters, limits);
    const LzssParameters candidate_parameters{65536, 3, 258, 0};
    const auto candidate_required = marc::dictionary::internal::
        calculate_lzss_short_prefix_workspace(
            frame_bytes, candidate_parameters, limits);
    if (required.error != marc::dictionary::internal::LzssHashChainError::none)
        return 2;
    if (candidate_required.error
        != marc::dictionary::internal::LzssShortPrefixError::none) return 2;
    const auto workspace_bytes = std::max(
        required.workspace_size, candidate_required.workspace_size);
    std::vector<std::max_align_t> finder_storage(
        (workspace_bytes + sizeof(std::max_align_t) - 1)
            / sizeof(std::max_align_t));
    const auto finder_workspace = std::span<std::byte>{
        reinterpret_cast<std::byte*>(finder_storage.data()),
        finder_storage.size() * sizeof(std::max_align_t)};
    const auto candidate_workspace = finder_workspace.first(
        candidate_required.workspace_size);
    std::vector<std::byte> raw(frame_bytes);
    std::vector<std::byte> decoded(frame_bytes);
    std::vector<std::byte> serialized(18 * frame_bytes + 85);
    std::vector<LzssTypedToken> tokens(frame_bytes);
    std::vector<LzssTypedToken> baseline_tokens(frame_bytes);
    std::vector<LzssTypedToken> decoded_tokens(frame_bytes);
    std::vector<ModeledOperation> operations(5 * frame_bytes);
    const marc::frame::internal::TypedContextStreamHeader stream{
        static_cast<std::uint32_t>(frame_bytes), sample_bytes,
        {65536, 3, 258, 0},
        marc::frame::internal::typed_context_model_total,
        32, 7, 1, 6};

    std::uint64_t baseline_archive_bytes =
        marc::frame::internal::typed_context_stream_header_size;
    std::uint64_t candidate_archive_bytes = baseline_archive_bytes;
    std::uint64_t exact_baseline_archive_bytes = baseline_archive_bytes;
    std::uint64_t exact_baseline_equal_token_frames{};
    std::array<std::uint64_t, 3> threshold_archive_bytes{
        baseline_archive_bytes, baseline_archive_bytes,
        baseline_archive_bytes};
    std::array<std::uint64_t, 3> selected_counts{};
    std::uint64_t selected_better_frames{};
    std::uint64_t selected_equal_frames{};
    std::uint64_t selected_worse_frames{};
    std::uint64_t selected_saved_bytes{};
    std::uint64_t selected_extra_bytes{};
    double baseline_plan_seconds{};
    double candidate_encode_seconds{};
    double candidate_decode_seconds{};
    std::uint64_t committed{};
    std::size_t frames{};
    while (committed < sample_bytes) {
        const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(
            frame_bytes, sample_bytes - committed));
        input.read(reinterpret_cast<char*>(raw.data()),
                   static_cast<std::streamsize>(count));
        if (input.gcount() != static_cast<std::streamsize>(count)) {
            std::cerr << "sample read failed\n";
            return 2;
        }
        const auto frame = std::span<const std::byte>{raw}.first(count);
        std::size_t baseline_size{};
        std::size_t baseline_token_count{};
        const auto baseline_start = Clock::now();
        if (!baseline_frame_size(frame, committed, limits, baseline_tokens,
                                 operations, finder_workspace,
                                 baseline_token_count, baseline_size)) {
            std::cerr << "baseline frame failed\n";
            return 2;
        }
        baseline_plan_seconds += std::chrono::duration<double>(
            Clock::now() - baseline_start).count();

        const auto encode_start = Clock::now();
        const auto candidate = search == "indexed"
            ? marc::frame::internal::
                encode_lzss_short_match_candidate_frame_indexed(
                    stream, limits, frames, committed, frame, tokens,
                    operations, candidate_workspace, serialized)
            : marc::frame::internal::
                encode_lzss_short_match_candidate_frame(
                    stream, limits, frames, committed, frame, tokens,
                    operations, serialized);
        candidate_encode_seconds += std::chrono::duration<double>(
            Clock::now() - encode_start).count();
        if (candidate.error != marc::frame::internal::
                LzssShortMatchSelectionError::none
            || candidate.selected_minimum_length < 3
            || candidate.selected_minimum_length > 5
            || candidate.selected_frame_size != *std::min_element(
                   candidate.candidate_frame_sizes.begin(),
                   candidate.candidate_frame_sizes.end())) {
            std::cerr << "candidate frame failed at sequence " << frames
                      << " error " << static_cast<unsigned>(candidate.error)
                      << '\n';
            return 2;
        }
        const marc::frame::internal::TypedContextFrameValidationContext
            context{stream, limits, frames, committed};
        const auto decode_start = Clock::now();
        const auto reconstructed = marc::frame::internal::
            decode_lzss_short_match_frame(
                std::span<const std::byte>{serialized}.first(
                    candidate.selected_frame_size),
                context, decoded_tokens,
                std::span<std::byte>{decoded}.first(count));
        candidate_decode_seconds += std::chrono::duration<double>(
            Clock::now() - decode_start).count();
        if (reconstructed.error != marc::frame::internal::
                LzssShortMatchFrameDecodeError::none
            || reconstructed.serialized_consumed
                != candidate.selected_frame_size
            || !std::equal(frame.begin(), frame.end(), decoded.begin())) {
            std::cerr << "candidate decode mismatch at sequence " << frames
                      << '\n';
            return 2;
        }
        const auto exact_tokens = marc::dictionary::internal::
            tokenize_lzss_short_match_candidate_indexed(
                frame, candidate_parameters, limits, 5, tokens,
                candidate_workspace);
        std::size_t exact_baseline_size{};
        if (exact_tokens.error != marc::dictionary::internal::
                LzssShortMatchCandidateError::none
            || !baseline_frame_size_from_tokens(
                count, committed, limits,
                std::span<const LzssTypedToken>{tokens}.first(
                    exact_tokens.token_count), operations,
                exact_baseline_size)) {
            std::cerr << "exact baseline frame failed at sequence "
                      << frames << '\n';
            return 2;
        }
        if (exact_tokens.token_count == baseline_token_count
            && std::equal(
                tokens.begin(), tokens.begin() + exact_tokens.token_count,
                baseline_tokens.begin(), same_token)) {
            ++exact_baseline_equal_token_frames;
        }
        baseline_archive_bytes += baseline_size;
        candidate_archive_bytes += candidate.selected_frame_size;
        exact_baseline_archive_bytes += exact_baseline_size;
        for (std::size_t index = 0; index < threshold_archive_bytes.size();
             ++index) {
            threshold_archive_bytes[index] +=
                candidate.candidate_frame_sizes[index];
        }
        if (candidate.selected_frame_size < baseline_size) {
            ++selected_better_frames;
            selected_saved_bytes += baseline_size - candidate.selected_frame_size;
        } else if (candidate.selected_frame_size > baseline_size) {
            ++selected_worse_frames;
            selected_extra_bytes += candidate.selected_frame_size - baseline_size;
        } else {
            ++selected_equal_frames;
        }
        ++selected_counts[candidate.selected_minimum_length - 3];
        committed += count;
        ++frames;
    }
    std::cout << "mode=lzss-short-match-candidate-sample\n"
              << "candidate_search=" << search << '\n'
              << "sample_bytes=" << sample_bytes << '\n'
              << "frame_bytes=" << frame_bytes << '\n'
              << "frame_count=" << frames << '\n'
              << "baseline_archive_bytes=" << baseline_archive_bytes << '\n'
              << "exact_baseline_archive_bytes="
              << exact_baseline_archive_bytes << '\n'
              << "exact_baseline_equal_token_frames="
              << exact_baseline_equal_token_frames << '\n'
              << "candidate_archive_bytes=" << candidate_archive_bytes << '\n'
              << "threshold_3_archive_bytes=" << threshold_archive_bytes[0]
              << '\n'
              << "threshold_4_archive_bytes=" << threshold_archive_bytes[1]
              << '\n'
              << "threshold_5_archive_bytes=" << threshold_archive_bytes[2]
              << '\n'
              << "selected_better_frames=" << selected_better_frames << '\n'
              << "selected_equal_frames=" << selected_equal_frames << '\n'
              << "selected_worse_frames=" << selected_worse_frames << '\n'
              << "selected_saved_bytes=" << selected_saved_bytes << '\n'
              << "selected_extra_bytes=" << selected_extra_bytes << '\n'
              << "selected_3=" << selected_counts[0] << '\n'
              << "selected_4=" << selected_counts[1] << '\n'
              << "selected_5=" << selected_counts[2] << '\n'
              << "baseline_plan_seconds=" << baseline_plan_seconds << '\n'
              << "candidate_encode_seconds=" << candidate_encode_seconds << '\n'
              << "candidate_decode_seconds=" << candidate_decode_seconds
              << '\n';
    return 0;
}
