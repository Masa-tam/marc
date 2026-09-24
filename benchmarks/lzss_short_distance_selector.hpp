#ifndef MARC_BENCHMARKS_LZSS_SHORT_DISTANCE_SELECTOR_HPP
#define MARC_BENCHMARKS_LZSS_SHORT_DISTANCE_SELECTOR_HPP

#include "lzss_short_distance_policy.hpp"
#include "frame/lzss_short_length_escape_frame_encoder.hpp"
#include "entropy/lzss_short_match_range_decoder.hpp"

#include <algorithm>
#include <limits>

namespace marc::benchmarks {

struct ShortDistanceSelection {
    std::array<std::size_t, 3> sizes{};
    std::size_t selected_policy{};
    std::size_t serialized_size{};
    std::size_t supplied_buffer_bytes{};
    std::size_t required_buffered_bytes{};
    bool valid{};
};

// Private benchmark selector. Scratch regions are unspecified on failure;
// output is consumable only when valid is true. All spans must be disjoint.
// No allocation, and no second parse/encode of the selected candidate.
[[nodiscard]] inline ShortDistanceSelection select_short_distance_frame(
    const frame::internal::TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence, const std::uint64_t committed,
    const std::span<const std::byte> raw,
    const std::span<dictionary::internal::LzssTypedToken> tokens,
    const std::span<context::internal::ModeledOperation> operations,
    const std::span<std::byte> finder,
    const std::span<std::byte> scratch,
    const std::span<std::byte> output,
    const bool indexed) noexcept {
    struct Region { const void* data; std::size_t size; };
    std::size_t token_bytes{}, operation_bytes{};
    if (!core::checked_multiply(tokens.size(), sizeof(tokens[0]), token_bytes)
        || !core::checked_multiply(operations.size(), sizeof(operations[0]), operation_bytes))
        return {};
    const std::array regions{
        Region{raw.data(), raw.size()}, Region{tokens.data(), token_bytes},
        Region{operations.data(), operation_bytes}, Region{finder.data(), finder.size()},
        Region{scratch.data(), scratch.size()}, Region{output.data(), output.size()}};
    std::size_t total{};
    for (std::size_t i = 0; i < regions.size(); ++i) {
        if (!core::checked_add(total, regions[i].size, total)) return {};
        for (std::size_t j = 0; j < i; ++j) {
            if (core::check_buffer_overlap(regions[i].data, regions[i].size,
                    regions[j].data, regions[j].size) != core::BufferOverlap::disjoint)
                return {};
        }
    }
    std::size_t required{};
    // Match the private frame contract's fixed model charge; this is not
    // a measurement of process RSS or every transient scalar on the stack.
    if (!core::checked_add(total,
            sizeof(entropy::internal::LzssShortMatchRangeDecoder), required)
        || required > limits.max_internal_buffered_bytes) return {};
    constexpr std::array<std::size_t, 3> policies{0, 3, 4};
    ShortDistanceSelection result{};
    result.supplied_buffer_bytes = total;
    result.required_buffered_bytes = required;
    auto best = std::numeric_limits<std::size_t>::max();
    for (std::size_t i = 0; i < policies.size(); ++i) {
        const auto parsed = tokenize_short_distance_policy(
            raw, stream.dictionary, limits, short_distance_policies[policies[i]],
            tokens, finder, indexed);
        if (!parsed.valid) return {};
        const auto encoded = frame::internal::encode_lzss_short_length_escape_frame(
            stream, limits, sequence, committed, tokens.first(parsed.token_count),
            operations, scratch);
        if (encoded.error != frame::internal::LzssShortMatchFrameEncodeError::none)
            return {};
        result.sizes[i] = encoded.serialized_size;
        if (encoded.serialized_size < best) {
            // Output must hold each provisional winner, not only the final one.
            if (output.size() < encoded.serialized_size) return {};
            std::copy_n(scratch.begin(), encoded.serialized_size, output.begin());
            best = encoded.serialized_size;
            result.selected_policy = policies[i];
        }
    }
    result.serialized_size = best;
    result.valid = true;
    return result;
}

} // namespace marc::benchmarks
#endif
