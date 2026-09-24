#ifndef MARC_BENCHMARKS_LZSS_SHORT_DISTANCE_POLICY_HPP
#define MARC_BENCHMARKS_LZSS_SHORT_DISTANCE_POLICY_HPP

#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include "dictionary/lzss_short_prefix_match_finder.hpp"

#include <array>

namespace marc::benchmarks {

struct ShortDistancePolicy {
    std::uint32_t length3_max_distance{};
    std::uint32_t length4_max_distance{};
};

inline constexpr std::array short_distance_policies{
    ShortDistancePolicy{0, 0}, ShortDistancePolicy{0, 65536},
    ShortDistancePolicy{65536, 65536}, ShortDistancePolicy{256, 4096},
    ShortDistancePolicy{1024, 16384}, ShortDistancePolicy{4096, 65536},
    ShortDistancePolicy{16384, 65536}};

struct ShortDistanceResult {
    std::size_t token_count{};
    bool valid{};
};

template <dictionary::internal::LzssMatchFinder Finder>
[[nodiscard]] ShortDistanceResult parse_short_distance_with_finder(
    const std::span<const std::byte> input,
    const ShortDistancePolicy policy,
    const std::span<dictionary::internal::LzssTypedToken> output,
    Finder& finder) noexcept {
    using namespace dictionary::internal;
    std::size_t count{};
    for (std::size_t position = 0; position < input.size();) {
        const auto match = finder.find_match(position);
        const bool accept = match.length >= 5
            || (match.length == 3 && match.distance <= policy.length3_max_distance)
            || (match.length == 4 && match.distance <= policy.length4_max_distance);
        const std::size_t advance = accept ? match.length : 1U;
        if (advance > input.size() - position || count >= output.size()) return {};
        output[count++] = accept
            ? LzssTypedToken{LzssTypedTokenKind::match, 0, match.distance, match.length}
            : LzssTypedToken{LzssTypedTokenKind::literal,
                            std::to_integer<std::uint8_t>(input[position]), 0, 0};
        finder.advance(position, position + advance);
        position += advance;
    }
    return {count, true};
}

// Benchmark-only, one-pass parser. The caller supplies worst-case token
// capacity (one token per raw byte) and keeps all regions disjoint.
[[nodiscard]] inline ShortDistanceResult tokenize_short_distance_policy(
    const std::span<const std::byte> input,
    const dictionary::internal::LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const ShortDistancePolicy policy,
    const std::span<dictionary::internal::LzssTypedToken> output,
    const std::span<std::byte> workspace,
    const bool indexed) noexcept {
    using namespace dictionary::internal;
    constexpr auto variant = LzssTypedTokenVariant::field_context_64k_short_length_escape;
    if (policy.length3_max_distance > 65536 || policy.length4_max_distance > 65536
        || validate_lzss_typed_parameters(parameters, limits, variant)
            != LzssTypedTokenError::none
        || input.size() > 65536 || input.size() > limits.max_frame_size
        || input.size() > limits.max_block_size
        || input.size() > limits.max_total_output_size
        || output.size() < input.size() || (!indexed && !workspace.empty())) return {};
    std::size_t output_bytes{};
    std::size_t needed_tokens{};
    if (!core::checked_multiply(output.size(), sizeof(LzssTypedToken), output_bytes)
        || !core::checked_multiply(input.size(), sizeof(LzssTypedToken), needed_tokens))
        return {};
    if (core::check_buffer_overlap(input.data(), input.size(),
            output.data(), output_bytes) != core::BufferOverlap::disjoint
        || core::check_buffer_overlap(input.data(), input.size(),
            workspace.data(), workspace.size()) != core::BufferOverlap::disjoint
        || core::check_buffer_overlap(output.data(), output_bytes,
            workspace.data(), workspace.size()) != core::BufferOverlap::disjoint)
        return {};
    std::size_t finder_bytes{};
    if (indexed) {
        const auto required = calculate_lzss_short_prefix_workspace(
            input.size(), parameters, limits, variant);
        if (required.error != LzssShortPrefixError::none) return {};
        finder_bytes = required.workspace_size;
    }
    std::size_t aggregate{};
    if (!core::checked_add(input.size(), needed_tokens, aggregate)
        || !core::checked_add(aggregate, finder_bytes, aggregate)
        || aggregate > limits.max_internal_buffered_bytes) return {};
    if (indexed) {
        LzssShortPrefixMatchFinder finder{};
        if (initialize_lzss_short_prefix_match_finder(
                input, parameters, limits, workspace, finder, variant)
            != LzssShortPrefixError::none) return {};
        return parse_short_distance_with_finder(input, policy, output, finder);
    }
    LzssExhaustiveMatchFinder finder{input, parameters};
    return parse_short_distance_with_finder(input, policy, output, finder);
}

} // namespace marc::benchmarks
#endif
