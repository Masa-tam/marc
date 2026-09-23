#include "lzss_short_match_probe.hpp"

#include "context/lzss_field_context.hpp"
#include "dictionary/lzss_hash_chain_match_finder.hpp"
#include "dictionary/lzss_typed_encoder.hpp"
#include "entropy/contextual_dynamic_range_encoder.hpp"
#include "frame/typed_context_format.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <span>

namespace {
using marc::benchmark::internal::ShortMatchPrefixFlags;
using marc::benchmark::internal::ShortMatchPrefixIndex;
using namespace marc::dictionary::internal;
using marc::context::internal::ModeledOperation;
using marc::context::internal::ModeledOperationKind;

[[nodiscard]] bool add(std::uint64_t& total,
                       const std::uint64_t increment = 1) noexcept {
    if (increment > std::numeric_limits<std::uint64_t>::max() - total) {
        return false;
    }
    total += increment;
    return true;
}

[[nodiscard]] std::size_t distance_bucket(
    const std::uint32_t distance) noexcept {
    if (distance <= 16) return 0;
    if (distance <= 256) return 1;
    if (distance <= 4096) return 2;
    return 3;
}

[[nodiscard]] std::size_t length_bucket(
    const std::uint32_t length) noexcept {
    if (length == 5) return 0;
    if (length <= 7) return 1;
    if (length <= 15) return 2;
    if (length <= 31) return 3;
    if (length <= 63) return 4;
    if (length <= 127) return 5;
    return 6;
}

struct Summary {
    std::uint64_t input_bytes{};
    std::uint64_t frame_count{};
    std::uint64_t all_three{};
    std::uint64_t all_four{};
    std::uint64_t visited_three{};
    std::uint64_t visited_four{};
    std::uint64_t literal_three_only{};
    std::uint64_t literal_four{};
    std::uint64_t literal_count{};
    std::uint64_t match_count{};
    std::uint64_t matched_bytes{};
    std::uint64_t token_kind_symbols{};
    std::uint64_t literal_symbols{};
    std::uint64_t length_symbols{};
    std::uint64_t distance_symbols{};
    std::uint64_t length_bypass_operations{};
    std::uint64_t distance_bypass_operations{};
    std::uint64_t length_bypass_bits{};
    std::uint64_t distance_bypass_bits{};
    std::uint64_t modeled_operation_count{};
    std::uint64_t modeled_decision_count{};
    std::uint64_t range_payload_bytes{};
    std::uint64_t predicted_archive_bytes{};
    std::array<std::uint64_t, 4> three_distance{};
    std::array<std::uint64_t, 4> four_distance{};
    std::array<std::uint64_t, 7> match_lengths{};
};

[[nodiscard]] bool process_frame(
    const std::span<const std::byte> frame,
    ShortMatchPrefixIndex& prefix_index,
    const std::span<ShortMatchPrefixFlags> flags,
    const std::span<std::byte> workspace,
    const std::span<LzssTypedToken> token_storage,
    const std::span<ModeledOperation> operation_storage,
    Summary& total) noexcept {
    if (!prefix_index.analyze(frame, flags)
        || !add(total.input_bytes, frame.size())
        || !add(total.frame_count)) {
        return false;
    }
    for (const auto& value : flags.first(frame.size())) {
        if (value.has_three && !add(total.all_three)) return false;
        if (value.has_four && !add(total.all_four)) return false;
    }

    const LzssParameters parameters{};
    const marc::core::DecoderLimits limits{};
    const auto token_result = encode_lzss_typed_tokens_hash_chain_single_pass(
        frame, parameters, limits, token_storage, workspace);
    if (token_result.error != LzssTypedEncodeError::none) {
        return false;
    }
    const auto tokens = token_storage.first(token_result.token_count);
    std::size_t position{};
    for (const auto& token : tokens) {
        if (position >= frame.size()) return false;
        const auto prefix = flags[position];
        if (prefix.has_three && !add(total.visited_three)) return false;
        if (prefix.has_four && !add(total.visited_four)) return false;
        const bool use_match = token.kind == LzssTypedTokenKind::match;
        const auto advance = use_match
            ? static_cast<std::size_t>(token.length) : 1U;
        if (advance > frame.size() - position) return false;
        if (use_match) {
            if (!add(total.match_count)
                || !add(total.matched_bytes, token.length)
                || !add(total.match_lengths[length_bucket(token.length)])) {
                return false;
            }
        } else {
            if (!add(total.literal_count)) return false;
            if (prefix.has_four) {
                if (!add(total.literal_four)
                    || !add(total.four_distance[
                        distance_bucket(prefix.nearest_four)])) {
                    return false;
                }
            } else if (prefix.has_three) {
                if (!add(total.literal_three_only)
                    || !add(total.three_distance[
                        distance_bucket(prefix.nearest_three)])) {
                    return false;
                }
            }
        }
        position += advance;
    }
    if (position != frame.size()) return false;

    const LzssTypedFrameValidationContext token_context{
        static_cast<std::uint32_t>(tokens.size()),
        static_cast<std::uint32_t>(frame.size()),
        total.input_bytes - frame.size()};
    const auto modeled = marc::context::internal::
        model_lzss_field_context_tokens(
            tokens, parameters, token_context, limits, operation_storage);
    if (modeled.error != marc::context::internal::LzssFieldContextError::none)
        return false;
    const auto operations = operation_storage.first(modeled.operation_count);
    std::uint16_t previous_context{};
    for (const auto& operation : operations) {
        if (operation.kind == ModeledOperationKind::symbol) {
            previous_context = operation.context_id;
            auto* category = operation.context_id <= 2
                ? &total.token_kind_symbols
                : operation.context_id <= 19 ? &total.literal_symbols
                : operation.context_id <= 22 ? &total.length_symbols
                : operation.context_id <= 30 ? &total.distance_symbols
                : nullptr;
            if (category == nullptr || !add(*category)) return false;
        } else if (operation.kind == ModeledOperationKind::bypass_bits) {
            if (previous_context >= 20 && previous_context <= 22) {
                if (!add(total.length_bypass_operations)
                    || !add(total.length_bypass_bits,
                            operation.bit_count)) return false;
            } else if (previous_context >= 23 && previous_context <= 30) {
                if (!add(total.distance_bypass_operations)
                    || !add(total.distance_bypass_bits,
                            operation.bit_count)) return false;
            } else {
                return false;
            }
        } else {
            return false;
        }
    }
    marc::entropy::internal::ContextualDynamicRangeDescriptor descriptor{};
    const auto ranged = marc::entropy::internal::
        plan_contextual_dynamic_range_operations(
            operations, limits, descriptor);
    if (ranged.error
            != marc::entropy::internal::ContextualDynamicRangeEncodeError::none
        || ranged.decision_count != modeled.decision_count
        || !add(total.modeled_operation_count, modeled.operation_count)
        || !add(total.modeled_decision_count, modeled.decision_count)
        || !add(total.range_payload_bytes, ranged.payload_size)) {
        return false;
    }
    return true;
}

void report(const Summary& value) {
    std::cout << "mode=lzss-short-match-diagnostic\n"
              << "window_bytes=65536\n"
              << "frame_bytes=65536\n"
              << "minimum_match_length=5\n"
              << "maximum_match_length=258\n"
              << "input_bytes=" << value.input_bytes << '\n'
              << "frame_count=" << value.frame_count << '\n'
              << "all_prefix3=" << value.all_three << '\n'
              << "all_prefix4=" << value.all_four << '\n'
              << "visited_prefix3=" << value.visited_three << '\n'
              << "visited_prefix4=" << value.visited_four << '\n'
              << "literal_prefix3_only=" << value.literal_three_only << '\n'
              << "literal_prefix4=" << value.literal_four << '\n'
              << "baseline_literal_count=" << value.literal_count << '\n'
              << "baseline_match_count=" << value.match_count << '\n'
              << "baseline_matched_bytes=" << value.matched_bytes << '\n'
              << "token_kind_symbols=" << value.token_kind_symbols << '\n'
              << "literal_symbols=" << value.literal_symbols << '\n'
              << "length_symbols=" << value.length_symbols << '\n'
              << "distance_symbols=" << value.distance_symbols << '\n'
              << "length_bypass_operations="
              << value.length_bypass_operations << '\n'
              << "distance_bypass_operations="
              << value.distance_bypass_operations << '\n'
              << "length_bypass_bits=" << value.length_bypass_bits << '\n'
              << "distance_bypass_bits=" << value.distance_bypass_bits << '\n'
              << "modeled_operation_count="
              << value.modeled_operation_count << '\n'
              << "modeled_decision_count="
              << value.modeled_decision_count << '\n'
              << "range_payload_bytes=" << value.range_payload_bytes << '\n'
              << "predicted_archive_bytes="
              << value.predicted_archive_bytes << '\n';
    for (std::size_t index = 0; index < value.three_distance.size(); ++index) {
        std::cout << "literal_prefix3_distance_bucket_" << index << '='
                  << value.three_distance[index] << '\n'
                  << "literal_prefix4_distance_bucket_" << index << '='
                  << value.four_distance[index] << '\n';
    }
    for (std::size_t index = 0; index < value.match_lengths.size(); ++index) {
        std::cout << "baseline_match_length_bucket_" << index << '='
                  << value.match_lengths[index] << '\n';
    }
}

} // namespace

int main(const int argc, const char* const argv[]) {
    if (argc != 2) {
        std::cerr << "usage: marc_lzss_short_match_diagnostic <input>\n";
        return 2;
    }
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) {
        std::cerr << "cannot open input\n";
        return 2;
    }
    ShortMatchPrefixIndex prefix_index{};
    auto flags = std::unique_ptr<ShortMatchPrefixFlags[]>(
        new (std::nothrow)
            ShortMatchPrefixFlags[ShortMatchPrefixIndex::frame_limit]{});
    auto tokens = std::unique_ptr<LzssTypedToken[]>(
        new (std::nothrow)
            LzssTypedToken[ShortMatchPrefixIndex::frame_limit]{});
    auto operations = std::unique_ptr<ModeledOperation[]>(
        new (std::nothrow)
            ModeledOperation[5 * ShortMatchPrefixIndex::frame_limit]{});
    if (!prefix_index.ready() || flags == nullptr || tokens == nullptr
        || operations == nullptr) {
        std::cerr << "diagnostic allocation failed\n";
        return 2;
    }
    const LzssParameters parameters{};
    const marc::core::DecoderLimits limits{};
    const auto needed = calculate_lzss_hash_chain_workspace(
        ShortMatchPrefixIndex::frame_limit, parameters, limits);
    if (needed.error != LzssHashChainError::none
        || needed.workspace_size >
            std::numeric_limits<std::size_t>::max()
                - (sizeof(std::max_align_t) - 1)) {
        std::cerr << "invalid diagnostic workspace\n";
        return 2;
    }
    const auto words = (needed.workspace_size + sizeof(std::max_align_t) - 1)
        / sizeof(std::max_align_t);
    auto storage = std::unique_ptr<std::max_align_t[]>(
        new (std::nothrow) std::max_align_t[words]{});
    if (storage == nullptr) {
        std::cerr << "workspace allocation failed\n";
        return 2;
    }
    const auto workspace = std::span<std::byte>(
        reinterpret_cast<std::byte*>(storage.get()),
        words * sizeof(std::max_align_t));

    Summary summary{};
    std::array<std::byte, ShortMatchPrefixIndex::frame_limit> frame{};
    for (;;) {
        input.read(reinterpret_cast<char*>(frame.data()),
                   static_cast<std::streamsize>(frame.size()));
        const auto count = input.gcount();
        if (count == 0) {
            if (!input.eof()) {
                std::cerr << "input read failed\n";
                return 2;
            }
            break;
        }
        if (count < 0 || (!input && !input.eof())
            || !process_frame(
                std::span(frame.data(), static_cast<std::size_t>(count)),
                prefix_index,
                std::span(flags.get(), static_cast<std::size_t>(count)),
                workspace,
                std::span(tokens.get(),
                          ShortMatchPrefixIndex::frame_limit),
                std::span(operations.get(),
                          5 * ShortMatchPrefixIndex::frame_limit),
                summary)) {
            std::cerr << "diagnostic frame failed\n";
            return 2;
        }
        if (input.eof()) break;
    }
    const auto frame_overhead =
        marc::frame::internal::typed_context_frame_header_size
        + marc::frame::internal::typed_context_range_descriptor_size;
    if (summary.frame_count
            > (std::numeric_limits<std::uint64_t>::max()
               - marc::frame::internal::typed_context_stream_header_size)
                / frame_overhead) {
        std::cerr << "archive size overflow\n";
        return 2;
    }
    summary.predicted_archive_bytes =
        marc::frame::internal::typed_context_stream_header_size
        + summary.frame_count * frame_overhead;
    if (!add(summary.predicted_archive_bytes, summary.range_payload_bytes)) {
        std::cerr << "archive size overflow\n";
        return 2;
    }
    std::uint64_t symbol_count{};
    std::uint64_t operation_count{};
    std::uint64_t decision_count{};
    if (!add(symbol_count, summary.token_kind_symbols)
        || !add(symbol_count, summary.literal_symbols)
        || !add(symbol_count, summary.length_symbols)
        || !add(symbol_count, summary.distance_symbols)) {
        std::cerr << "symbol count overflow\n";
        return 2;
    }
    operation_count = decision_count = symbol_count;
    if (!add(operation_count, summary.length_bypass_operations)
        || !add(operation_count, summary.distance_bypass_operations)
        || !add(decision_count, summary.length_bypass_bits)
        || !add(decision_count, summary.distance_bypass_bits)
        || operation_count != summary.modeled_operation_count
        || decision_count != summary.modeled_decision_count) {
        std::cerr << "modeled decision mismatch\n";
        return 2;
    }
    report(summary);
    return 0;
}
