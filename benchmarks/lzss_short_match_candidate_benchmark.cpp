#include "context/lzss_field_context.hpp"
#include "context/lzss_short_length_escape_operations.hpp"
#include "lzss_model_cost.hpp"
#include "lzss_short_distance_policy.hpp"
#include "lzss_short_distance_selector.hpp"
#include "dictionary/lzss_hash_chain_match_finder.hpp"
#include "dictionary/lzss_short_prefix_match_finder.hpp"
#include "dictionary/lzss_typed_encoder.hpp"
#include "entropy/contextual_dynamic_range_encoder.hpp"
#include "frame/lzss_short_match_candidate_selector.hpp"
#include "frame/lzss_short_match_frame_decoder.hpp"
#include "frame/lzss_short_length_escape_candidate_selector.hpp"
#include "frame/lzss_short_length_escape_frame_decoder.hpp"
#include "frame/lzss_short_length_escape_frame_encoder.hpp"
#include "frame/lzss_reduced_literal_frame_encoder.hpp"
#include "frame/lzss_reduced_literal_frame_decoder.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
constexpr std::array<unsigned, 6> distance_subset_masks{9, 17, 24, 25, 29, 127};
using marc::context::internal::ModeledOperation;
using marc::context::internal::ModeledOperationKind;
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

struct MatchOperationSummary {
    std::uint64_t length_symbols{};
    std::uint64_t length_bypass_bits{};
    std::uint64_t distance_symbols{};
    std::uint64_t distance_bypass_bits{};
};

[[nodiscard]] bool accumulate_cost(
    marc::benchmarks::ModelCost& total,
    const marc::benchmarks::ModelCost& frame) noexcept {
    if (!frame.valid) return false;
    for (std::size_t index = 0; index < 4; ++index) {
        total.adaptive_bits[index] += frame.adaptive_bits[index];
        total.empirical_bits[index] += frame.empirical_bits[index];
        total.symbols[index] += frame.symbols[index];
    }
    for (std::size_t index = 0; index < 2; ++index)
        total.bypass_bits[index] += frame.bypass_bits[index];
    return true;
}

void print_cost(const std::string_view prefix,
                const marc::benchmarks::ModelCost& cost) {
    constexpr std::array names{"kind", "literal", "length", "distance"};
    std::cout << std::fixed << std::setprecision(6);
    for (std::size_t index = 0; index < names.size(); ++index) {
        std::cout << prefix << '_' << names[index] << "_adaptive_bits="
                  << cost.adaptive_bits[index] << '\n'
                  << prefix << '_' << names[index] << "_empirical_bits="
                  << cost.empirical_bits[index] << '\n'
                  << prefix << '_' << names[index] << "_symbols="
                  << cost.symbols[index] << '\n';
    }
    std::cout << prefix << "_length_bypass_bits=" << cost.bypass_bits[0]
              << '\n' << prefix << "_distance_bypass_bits="
              << cost.bypass_bits[1] << '\n';
}

[[nodiscard]] bool summarize_match_operations(
    const std::span<const ModeledOperation> operations,
    MatchOperationSummary& summary) noexcept {
    std::uint16_t preceding_context{};
    for (const auto& operation : operations) {
        if (operation.kind == ModeledOperationKind::symbol) {
            preceding_context = operation.context_id;
            if (preceding_context >= 20 && preceding_context < 23) {
                ++summary.length_symbols;
            } else if (preceding_context >= 23 && preceding_context < 32) {
                ++summary.distance_symbols;
            } else if (preceding_context >= 32) {
                return false;
            }
        } else if (operation.kind == ModeledOperationKind::bypass_bits) {
            if (preceding_context >= 20 && preceding_context < 23) {
                summary.length_bypass_bits += operation.bit_count;
            } else if (preceding_context >= 23 && preceding_context < 32) {
                summary.distance_bypass_bits += operation.bit_count;
            } else {
                return false;
            }
        } else {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool baseline_frame_size_from_tokens(
    const std::size_t raw_size,
    const std::uint64_t committed,
    const marc::core::DecoderLimits& limits,
    const std::span<const LzssTypedToken> tokens,
    const std::span<ModeledOperation> operations,
    std::size_t& operation_count,
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
    operation_count = modeled.operation_count;
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
    std::size_t& operation_count,
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
        operations, operation_count, serialized_size);
}

[[nodiscard]] bool same_token(const LzssTypedToken& first,
                              const LzssTypedToken& second) noexcept {
    return first.kind == second.kind && first.literal == second.literal
        && first.distance == second.distance && first.length == second.length;
}

} // namespace

int main(const int argc, const char* const argv[]) {
    if (argc < 4 || argc > 6) {
        std::cerr << "usage: marc_lzss_short_match_candidate_benchmark "
                     "<input> <max-frames:1..1024> <frame-bytes:1..65536> "
                     "[indexed|reference] [distance-policies]\n";
        return 2;
    }
    const std::string_view search = argc >= 5 ? argv[4] : "indexed";
    const bool distance_policies = argc == 6;
    if (distance_policies && std::string_view{argv[5]} != "distance-policies") {
        std::cerr << "invalid experiment mode\n";
        return 2;
    }
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
    std::vector<std::byte> distance_winner(distance_policies ? serialized.size() : 0);
    std::vector<LzssTypedToken> tokens(frame_bytes);
    std::vector<LzssTypedToken> baseline_tokens(frame_bytes);
    std::vector<LzssTypedToken> decoded_tokens(frame_bytes);
    std::vector<ModeledOperation> operations(5 * frame_bytes);
    const marc::frame::internal::TypedContextStreamHeader stream{
        static_cast<std::uint32_t>(frame_bytes), sample_bytes,
        {65536, 3, 258, 0},
        marc::frame::internal::typed_context_model_total,
        32, 7, 1, 6};
    const marc::frame::internal::TypedContextStreamHeader escape_stream{
        static_cast<std::uint32_t>(frame_bytes), sample_bytes,
        {65536, 3, 258, 0},
        marc::frame::internal::typed_context_model_total,
        32, 8, 1, 7};

    std::uint64_t baseline_archive_bytes =
        marc::frame::internal::typed_context_stream_header_size;
    std::uint64_t candidate_archive_bytes = baseline_archive_bytes;
    std::uint64_t escape_archive_bytes = baseline_archive_bytes;
    std::array<std::uint64_t, marc::benchmarks::short_distance_policies.size()>
        distance_policy_archive_bytes{};
    distance_policy_archive_bytes.fill(baseline_archive_bytes);
    std::array<std::uint64_t, distance_subset_masks.size()> distance_subset_bytes{};
    distance_subset_bytes.fill(baseline_archive_bytes);
    std::uint64_t distance_policy_selected_archive_bytes = baseline_archive_bytes;
    std::uint64_t baseline_escape_oracle_archive_bytes =
        baseline_archive_bytes;
    std::uint64_t three_way_oracle_archive_bytes = baseline_archive_bytes;
    std::uint64_t exact_baseline_archive_bytes = baseline_archive_bytes;
    std::uint64_t exact_baseline_equal_token_frames{};
    std::array<std::uint64_t, 3> threshold_archive_bytes{
        baseline_archive_bytes, baseline_archive_bytes,
        baseline_archive_bytes};
    std::array<std::uint64_t, 3> escape_threshold_archive_bytes{
        baseline_archive_bytes, baseline_archive_bytes,
        baseline_archive_bytes};
    std::array<std::uint64_t, 3> selected_counts{};
    std::array<std::uint64_t, 3> escape_selected_counts{};
    std::uint64_t selected_better_frames{};
    std::uint64_t selected_equal_frames{};
    std::uint64_t selected_worse_frames{};
    std::uint64_t selected_saved_bytes{};
    std::uint64_t selected_extra_bytes{};
    std::uint64_t escape_better_frames{};
    std::uint64_t escape_equal_frames{};
    std::uint64_t escape_worse_frames{};
    std::uint64_t escape_saved_bytes{};
    std::uint64_t escape_extra_bytes{};
    MatchOperationSummary baseline_operations{};
    MatchOperationSummary reserved_five_operations{};
    marc::benchmarks::ModelCost baseline_cost{};
    marc::benchmarks::ModelCost escape_cost{};
    marc::benchmarks::ModelCost retained_cost{};
    constexpr std::array<std::uint32_t, 4> literal_increments{1, 2, 4, 8};
    std::array<double, 4> literal_increment_bits{};
    using marc::benchmarks::LiteralPartition;
    constexpr std::array literal_partitions{LiteralPartition::shared, LiteralPartition::high0,
        LiteralPartition::high1, LiteralPartition::high2, LiteralPartition::high3, LiteralPartition::high4};
    constexpr std::array literal_partition_names{"shared", "high0", "high1", "high2", "high3", "high4"};
    std::array<double, 6> literal_partition_bits{}, literal_partition_empirical_bits{};
    double baseline_plan_seconds{};
    double candidate_encode_seconds{};
    double candidate_decode_seconds{};
    double escape_encode_seconds{};
    double escape_decode_seconds{};
    double distance_selector_encode_seconds{}, distance_selector_decode_seconds{};
    std::size_t distance_selector_buffer_bytes{};
    std::size_t distance_selector_required_bytes{};
    std::uint64_t distance_selector_archive_bytes = baseline_archive_bytes;
    std::uint64_t reduced_literal_archive_bytes = baseline_archive_bytes;
    std::uint64_t reduced_literal_verified_frames{}, reduced_literal_saved_bytes{},
        reduced_literal_extra_bytes{};
    double reduced_literal_encode_seconds{}, reduced_literal_decode_seconds{};
    std::array<double, 7> distance_encode_seconds{};
    std::array<double, 7> distance_decode_seconds{};
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
        std::size_t baseline_operation_count{};
        const auto baseline_start = Clock::now();
        if (!baseline_frame_size(frame, committed, limits, baseline_tokens,
                                 operations, finder_workspace,
                                 baseline_token_count,
                                 baseline_operation_count, baseline_size)) {
            std::cerr << "baseline frame failed\n";
            return 2;
        }
        baseline_plan_seconds += std::chrono::duration<double>(
            Clock::now() - baseline_start).count();
        if (!summarize_match_operations(
                std::span<const ModeledOperation>{operations}.first(
                    baseline_operation_count),
                baseline_operations)) {
            std::cerr << "baseline operation profile failed\n";
            return 2;
        }
        if (!accumulate_cost(baseline_cost, marc::benchmarks::measure_model_cost(
                std::span<const ModeledOperation>{operations}.first(
                    baseline_operation_count),
                marc::benchmarks::CostLayout::published_64k))) {
            std::cerr << "baseline cost profile failed\n";
            return 2;
        }

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
        const auto escape_encode_start = Clock::now();
        const auto escape = search == "indexed"
            ? marc::frame::internal::
                encode_lzss_short_length_escape_candidate_frame_indexed(
                    escape_stream, limits, frames, committed, frame, tokens,
                    operations, candidate_workspace, serialized)
            : marc::frame::internal::
                encode_lzss_short_length_escape_candidate_frame(
                    escape_stream, limits, frames, committed, frame, tokens,
                    operations, serialized);
        escape_encode_seconds += std::chrono::duration<double>(
            Clock::now() - escape_encode_start).count();
        if (escape.error != marc::frame::internal::
                LzssShortMatchSelectionError::none
            || escape.selected_minimum_length < 3
            || escape.selected_minimum_length > 5
            || escape.selected_frame_size != *std::min_element(
                   escape.candidate_frame_sizes.begin(),
                   escape.candidate_frame_sizes.end())) {
            std::cerr << "escape frame failed at sequence " << frames
                      << " error " << static_cast<unsigned>(escape.error)
                      << '\n';
            return 2;
        }
        const marc::frame::internal::TypedContextFrameValidationContext
            escape_context{escape_stream, limits, frames, committed};
        const auto escape_decode_start = Clock::now();
        const auto escape_reconstructed = marc::frame::internal::
            decode_lzss_short_length_escape_frame(
                std::span<const std::byte>{serialized}.first(
                    escape.selected_frame_size),
                escape_context, decoded_tokens,
                std::span<std::byte>{decoded}.first(count));
        escape_decode_seconds += std::chrono::duration<double>(
            Clock::now() - escape_decode_start).count();
        if (escape_reconstructed.error != marc::frame::internal::
                LzssShortMatchFrameDecodeError::none
            || escape_reconstructed.serialized_consumed
                != escape.selected_frame_size
            || !std::equal(frame.begin(), frame.end(), decoded.begin())) {
            std::cerr << "escape decode mismatch at sequence " << frames
                      << '\n';
            return 2;
        }
        const marc::dictionary::internal::LzssTypedFrameValidationContext
            escape_token_context{
                static_cast<std::uint32_t>(escape_reconstructed.required_token_count),
                static_cast<std::uint32_t>(count), committed};
        const auto escape_modeled = marc::context::internal::
            model_lzss_short_length_escape_tokens(
                std::span<const LzssTypedToken>{decoded_tokens}.first(
                    escape_reconstructed.required_token_count),
                candidate_parameters, escape_token_context, limits, operations);
        if (escape_modeled.error != marc::context::internal::LzssFieldContextError::none
            || !accumulate_cost(escape_cost, marc::benchmarks::measure_model_cost(
                std::span<const ModeledOperation>{operations}.first(
                    escape_modeled.operation_count),
                marc::benchmarks::CostLayout::short_match_64k))) {
            std::cerr << "escape cost profile failed\n";
            return 2;
        }
        if (distance_policies) {
            auto best_size = escape.selected_frame_size;
            std::array<std::size_t, marc::benchmarks::short_distance_policies.size()> sizes{};
            for (std::size_t i = 0; i < marc::benchmarks::short_distance_policies.size(); ++i) {
                const auto policy_start = Clock::now();
                const auto parsed = marc::benchmarks::tokenize_short_distance_policy(
                    frame, candidate_parameters, limits,
                    marc::benchmarks::short_distance_policies[i], tokens,
                    search == "indexed" ? candidate_workspace : std::span<std::byte>{},
                    search == "indexed");
                if (!parsed.valid) {
                    std::cerr << "distance policy parse failed\n";
                    return 2;
                }
                const auto encoded = marc::frame::internal::
                    encode_lzss_short_length_escape_frame(
                        escape_stream, limits, frames, committed,
                        std::span<const LzssTypedToken>{tokens}.first(parsed.token_count),
                        operations, serialized);
                if (encoded.error != marc::frame::internal::LzssShortMatchFrameEncodeError::none) {
                    std::cerr << "distance policy encode failed\n";
                    return 2;
                }
                distance_encode_seconds[i] += std::chrono::duration<double>(
                    Clock::now() - policy_start).count();
                const auto policy_decode_start = Clock::now();
                const auto verified = marc::frame::internal::decode_lzss_short_length_escape_frame(
                    std::span<const std::byte>{serialized}.first(encoded.serialized_size),
                    escape_context, decoded_tokens, std::span<std::byte>{decoded}.first(count));
                distance_decode_seconds[i] += std::chrono::duration<double>(
                    Clock::now() - policy_decode_start).count();
                if (verified.error != marc::frame::internal::LzssShortMatchFrameDecodeError::none
                    || verified.serialized_consumed != encoded.serialized_size
                    || !std::equal(frame.begin(), frame.end(), decoded.begin())) {
                    std::cerr << "distance policy decode mismatch\n";
                    return 2;
                }
                distance_policy_archive_bytes[i] += encoded.serialized_size;
                sizes[i] = encoded.serialized_size;
                best_size = std::min(best_size, encoded.serialized_size);
            }
            for (std::size_t subset = 0; subset < distance_subset_masks.size(); ++subset) {
                auto minimum = std::numeric_limits<std::size_t>::max();
                for (std::size_t i = 0; i < sizes.size(); ++i) {
                    if ((distance_subset_masks[subset] & (1U << i)) != 0)
                        minimum = std::min(minimum, sizes[i]);
                }
                distance_subset_bytes[subset] += minimum;
            }
            distance_policy_selected_archive_bytes += best_size;
            const auto selection_start = Clock::now();
            const auto selection = marc::benchmarks::select_short_distance_frame(
                escape_stream, limits, frames, committed, frame, tokens, operations,
                search == "indexed" ? candidate_workspace : std::span<std::byte>{},
                serialized, distance_winner, search == "indexed");
            distance_selector_encode_seconds += std::chrono::duration<double>(
                Clock::now() - selection_start).count();
            if (!selection.valid || selection.sizes != std::array{sizes[0], sizes[3], sizes[4]}
                || selection.serialized_size != std::min({sizes[0], sizes[3], sizes[4]})) {
                std::cerr << "distance selector mismatch\n";
                return 2;
            }
            distance_selector_buffer_bytes = std::max(distance_selector_buffer_bytes,
                                                       selection.supplied_buffer_bytes);
            distance_selector_required_bytes = std::max(distance_selector_required_bytes,
                                                         selection.required_buffered_bytes);
            distance_selector_archive_bytes += selection.serialized_size;
            const auto selected_decode_start = Clock::now();
            const auto verified = marc::frame::internal::decode_lzss_short_length_escape_frame(
                std::span<const std::byte>{distance_winner}.first(selection.serialized_size),
                escape_context, decoded_tokens, std::span<std::byte>{decoded}.first(count));
            distance_selector_decode_seconds += std::chrono::duration<double>(
                Clock::now() - selected_decode_start).count();
            if (verified.error != marc::frame::internal::LzssShortMatchFrameDecodeError::none
                || verified.serialized_consumed != selection.serialized_size
                || !std::equal(frame.begin(), frame.end(), decoded.begin())) {
                std::cerr << "distance selector decode mismatch\n";
                return 2;
            }
            // Profile the decoded winner, not scratch tokens left by policy 4.
            const marc::dictionary::internal::LzssTypedFrameValidationContext selected_context{
                static_cast<std::uint32_t>(verified.required_token_count),
                static_cast<std::uint32_t>(count), committed};
            const auto selected_modeled = marc::context::internal::
                model_lzss_short_length_escape_tokens(
                    std::span<const LzssTypedToken>{decoded_tokens}.first(
                        verified.required_token_count),
                    candidate_parameters, selected_context, limits, operations);
            if (selected_modeled.error != marc::context::internal::LzssFieldContextError::none
                || !accumulate_cost(retained_cost, marc::benchmarks::measure_model_cost(
                    std::span<const ModeledOperation>{operations}.first(selected_modeled.operation_count),
                    marc::benchmarks::CostLayout::short_match_64k))) {
                std::cerr << "retained cost profile failed\n";
                return 2;
            }
            for (std::size_t i = 0; i < literal_increments.size(); ++i) {
                const auto cost = marc::benchmarks::measure_model_cost(
                    std::span<const ModeledOperation>{operations}.first(selected_modeled.operation_count),
                    marc::benchmarks::CostLayout::short_match_64k, literal_increments[i]);
                if (!cost.valid) return 2;
                literal_increment_bits[i] += cost.adaptive_bits[1];
            }
            for (std::size_t i = 0; i < literal_partitions.size(); ++i) {
                const auto cost = marc::benchmarks::measure_model_cost(
                    std::span<const ModeledOperation>{operations}.first(selected_modeled.operation_count),
                    marc::benchmarks::CostLayout::short_match_64k, 1, literal_partitions[i]);
                if (!cost.valid) return 2;
                literal_partition_bits[i] += cost.adaptive_bits[1];
                literal_partition_empirical_bits[i] += cost.empirical_bits[1];
            }
            // Freeze the old model's selected token sequence. Do not reselect
            // policies under the new model or parse the dictionary again.
            auto reduced_stream = escape_stream;
            reduced_stream.context_count = 24;
            reduced_stream.context_variant = 8;
            const auto retained_tokens = std::span<const LzssTypedToken>{decoded_tokens}
                .first(verified.required_token_count);
            const auto reduced_start = Clock::now();
            const auto reduced = marc::frame::internal::encode_lzss_reduced_literal_frame(
                reduced_stream, limits, frames, committed, retained_tokens,
                operations, serialized);
            reduced_literal_encode_seconds += std::chrono::duration<double>(
                Clock::now() - reduced_start).count();
            if (reduced.error != marc::frame::internal::LzssShortMatchFrameEncodeError::none) {
                std::cerr << "reduced literal encode failed\n";
                return 2;
            }
            const auto reduced_decode_start = Clock::now();
            const marc::frame::internal::TypedContextFrameValidationContext reduced_context{
                reduced_stream, limits, frames, committed};
            const auto restored = marc::frame::internal::decode_lzss_reduced_literal_frame(
                std::span<const std::byte>{serialized}.first(reduced.serialized_size),
                reduced_context, tokens, std::span<std::byte>{decoded}.first(count));
            reduced_literal_decode_seconds += std::chrono::duration<double>(
                Clock::now() - reduced_decode_start).count();
            if (restored.error != marc::frame::internal::LzssShortMatchFrameDecodeError::none
                || restored.serialized_consumed != reduced.serialized_size
                || restored.required_token_count != retained_tokens.size()
                || !std::equal(retained_tokens.begin(), retained_tokens.end(), tokens.begin(), same_token)
                || !std::equal(frame.begin(), frame.end(), decoded.begin())) {
                std::cerr << "reduced literal fixed-token round trip mismatch\n";
                return 2;
            }
            ++reduced_literal_verified_frames;
            reduced_literal_archive_bytes += reduced.serialized_size;
            if (reduced.serialized_size < selection.serialized_size)
                reduced_literal_saved_bytes += selection.serialized_size - reduced.serialized_size;
            else
                reduced_literal_extra_bytes += reduced.serialized_size - selection.serialized_size;
        }
        const auto exact_tokens = marc::dictionary::internal::
            tokenize_lzss_short_match_candidate_indexed(
                frame, candidate_parameters, limits, 5, tokens,
                candidate_workspace);
        std::size_t exact_baseline_size{};
        std::size_t exact_baseline_operation_count{};
        if (exact_tokens.error != marc::dictionary::internal::
                LzssShortMatchCandidateError::none
            || !baseline_frame_size_from_tokens(
                count, committed, limits,
                std::span<const LzssTypedToken>{tokens}.first(
                    exact_tokens.token_count), operations,
                exact_baseline_operation_count, exact_baseline_size)) {
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
        const marc::dictionary::internal::LzssTypedFrameValidationContext
            token_context{static_cast<std::uint32_t>(exact_tokens.token_count),
                          static_cast<std::uint32_t>(count), committed};
        const auto reserved_modeled = marc::context::internal::
            model_lzss_short_match_tokens(
                std::span<const LzssTypedToken>{tokens}.first(
                    exact_tokens.token_count),
                candidate_parameters, token_context, limits, operations);
        if (reserved_modeled.error != marc::context::internal::
                LzssFieldContextError::none
            || !summarize_match_operations(
                std::span<const ModeledOperation>{operations}.first(
                    reserved_modeled.operation_count),
                reserved_five_operations)) {
            std::cerr << "reserved operation profile failed at sequence "
                      << frames << '\n';
            return 2;
        }
        baseline_archive_bytes += baseline_size;
        candidate_archive_bytes += candidate.selected_frame_size;
        escape_archive_bytes += escape.selected_frame_size;
        baseline_escape_oracle_archive_bytes +=
            std::min(baseline_size, escape.selected_frame_size);
        three_way_oracle_archive_bytes += std::min(
            {baseline_size, candidate.selected_frame_size,
             escape.selected_frame_size});
        exact_baseline_archive_bytes += exact_baseline_size;
        for (std::size_t index = 0; index < threshold_archive_bytes.size();
             ++index) {
            threshold_archive_bytes[index] +=
                candidate.candidate_frame_sizes[index];
            escape_threshold_archive_bytes[index] +=
                escape.candidate_frame_sizes[index];
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
        if (escape.selected_frame_size < baseline_size) {
            ++escape_better_frames;
            escape_saved_bytes += baseline_size - escape.selected_frame_size;
        } else if (escape.selected_frame_size > baseline_size) {
            ++escape_worse_frames;
            escape_extra_bytes += escape.selected_frame_size - baseline_size;
        } else {
            ++escape_equal_frames;
        }
        ++selected_counts[candidate.selected_minimum_length - 3];
        ++escape_selected_counts[escape.selected_minimum_length - 3];
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
              << "escape_archive_bytes=" << escape_archive_bytes << '\n'
              << "baseline_escape_oracle_archive_bytes="
              << baseline_escape_oracle_archive_bytes << '\n'
              << "three_way_oracle_archive_bytes="
              << three_way_oracle_archive_bytes << '\n'
              << "threshold_3_archive_bytes=" << threshold_archive_bytes[0]
              << '\n'
              << "threshold_4_archive_bytes=" << threshold_archive_bytes[1]
              << '\n'
              << "threshold_5_archive_bytes=" << threshold_archive_bytes[2]
              << '\n'
              << "escape_threshold_3_archive_bytes="
              << escape_threshold_archive_bytes[0] << '\n'
              << "escape_threshold_4_archive_bytes="
              << escape_threshold_archive_bytes[1] << '\n'
              << "escape_threshold_5_archive_bytes="
              << escape_threshold_archive_bytes[2] << '\n'
              << "selected_better_frames=" << selected_better_frames << '\n'
              << "selected_equal_frames=" << selected_equal_frames << '\n'
              << "selected_worse_frames=" << selected_worse_frames << '\n'
              << "selected_saved_bytes=" << selected_saved_bytes << '\n'
              << "selected_extra_bytes=" << selected_extra_bytes << '\n'
              << "escape_better_frames=" << escape_better_frames << '\n'
              << "escape_equal_frames=" << escape_equal_frames << '\n'
              << "escape_worse_frames=" << escape_worse_frames << '\n'
              << "escape_saved_bytes=" << escape_saved_bytes << '\n'
              << "escape_extra_bytes=" << escape_extra_bytes << '\n'
              << "baseline_length_symbols="
              << baseline_operations.length_symbols << '\n'
              << "reserved_5_length_symbols="
              << reserved_five_operations.length_symbols << '\n'
              << "baseline_length_bypass_bits="
              << baseline_operations.length_bypass_bits << '\n'
              << "reserved_5_length_bypass_bits="
              << reserved_five_operations.length_bypass_bits << '\n'
              << "baseline_distance_symbols="
              << baseline_operations.distance_symbols << '\n'
              << "reserved_5_distance_symbols="
              << reserved_five_operations.distance_symbols << '\n'
              << "baseline_distance_bypass_bits="
              << baseline_operations.distance_bypass_bits << '\n'
              << "reserved_5_distance_bypass_bits="
              << reserved_five_operations.distance_bypass_bits << '\n'
              << "selected_3=" << selected_counts[0] << '\n'
              << "selected_4=" << selected_counts[1] << '\n'
              << "selected_5=" << selected_counts[2] << '\n'
              << "escape_selected_3=" << escape_selected_counts[0] << '\n'
              << "escape_selected_4=" << escape_selected_counts[1] << '\n'
              << "escape_selected_5=" << escape_selected_counts[2] << '\n'
              << "baseline_plan_seconds=" << baseline_plan_seconds << '\n'
              << "candidate_encode_seconds=" << candidate_encode_seconds << '\n'
              << "candidate_decode_seconds=" << candidate_decode_seconds
              << '\n'
              << "escape_encode_seconds=" << escape_encode_seconds << '\n'
              << "escape_decode_seconds=" << escape_decode_seconds
              << '\n';
    print_cost("baseline_cost", baseline_cost);
    print_cost("escape_cost", escape_cost);
    if (distance_policies) {
        print_cost("retained_cost", retained_cost);
        std::cout << "reduced_literal_archive_bytes=" << reduced_literal_archive_bytes << '\n'
                  << "reduced_literal_verified_frames=" << reduced_literal_verified_frames << '\n'
                  << "reduced_literal_saved_bytes=" << reduced_literal_saved_bytes << '\n'
                  << "reduced_literal_extra_bytes=" << reduced_literal_extra_bytes << '\n'
                  << "reduced_literal_encode_seconds=" << reduced_literal_encode_seconds << '\n'
                  << "reduced_literal_decode_seconds=" << reduced_literal_decode_seconds << '\n';
        for (std::size_t i = 0; i < literal_partitions.size(); ++i)
            std::cout << "literal_partition_" << literal_partition_names[i]
                      << "_adaptive_bits=" << literal_partition_bits[i] << '\n'
                      << "literal_partition_" << literal_partition_names[i]
                      << "_empirical_bits=" << literal_partition_empirical_bits[i] << '\n';
        for (std::size_t i = 0; i < literal_increments.size(); ++i)
            std::cout << "literal_increment_" << literal_increments[i]
                      << "_adaptive_bits=" << literal_increment_bits[i] << '\n';
        for (std::size_t i = 0; i < distance_policy_archive_bytes.size(); ++i) {
            const auto policy = marc::benchmarks::short_distance_policies[i];
            std::cout << "distance_policy_" << i << "_length3_cap="
                      << policy.length3_max_distance << '\n'
                      << "distance_policy_" << i << "_length4_cap="
                      << policy.length4_max_distance << '\n'
                      << "distance_policy_" << i << "_archive_bytes="
                      << distance_policy_archive_bytes[i] << '\n'
                      << "distance_policy_" << i << "_encode_seconds="
                      << distance_encode_seconds[i] << '\n'
                      << "distance_policy_" << i << "_decode_seconds="
                      << distance_decode_seconds[i] << '\n';
        }
        std::cout << "distance_policy_selected_archive_bytes="
                  << distance_policy_selected_archive_bytes << '\n';
        std::cout << "distance_selector_archive_bytes=" << distance_selector_archive_bytes << '\n'
                  << "distance_selector_encode_seconds=" << distance_selector_encode_seconds << '\n'
                  << "distance_selector_decode_seconds=" << distance_selector_decode_seconds << '\n'
                  << "distance_selector_supplied_buffer_bytes=" << distance_selector_buffer_bytes << '\n'
                  << "distance_selector_required_buffered_bytes=" << distance_selector_required_bytes << '\n';
        for (std::size_t subset = 0; subset < distance_subset_masks.size(); ++subset) {
            double encode_sum{};
            for (std::size_t i = 0; i < distance_encode_seconds.size(); ++i) {
                if ((distance_subset_masks[subset] & (1U << i)) != 0)
                    encode_sum += distance_encode_seconds[i];
            }
            std::cout << "distance_subset_" << subset << "_mask="
                      << distance_subset_masks[subset] << '\n'
                      << "distance_subset_" << subset << "_archive_bytes="
                      << distance_subset_bytes[subset] << '\n'
                      << "distance_subset_" << subset << "_encode_seconds_sum="
                      << encode_sum << '\n';
        }
    }
    return 0;
}
