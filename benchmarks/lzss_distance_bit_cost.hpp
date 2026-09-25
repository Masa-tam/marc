#ifndef MARC_BENCHMARKS_LZSS_DISTANCE_BIT_COST_HPP
#define MARC_BENCHMARKS_LZSS_DISTANCE_BIT_COST_HPP

#include "context/lzss_short_match_operations.hpp"
#include "context/lzss_short_match_context_layout.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <span>

namespace marc::benchmarks {

using BinaryCounts = std::array<std::uint32_t, 2>;
struct DistanceBitCost {
    std::uint64_t uniform_bits{};
    std::array<double, 2> adaptive_bits{}, empirical_bits{};
    std::array<BinaryCounts, 16> position_counts{};
    std::array<BinaryCounts, 17 * 16> class_position_counts{};
    bool valid{};
};

// Diagnostic for context-7 operations, not a token-grammar validator. Callers
// supply already validated frames. No coding path uses these floating scores.
[[nodiscard]] inline DistanceBitCost measure_distance_bit_cost(
    const std::span<const context::internal::ModeledOperation> operations) noexcept {
    using namespace context::internal;
    if (operations.size() > 5 * 65536) return {};
    std::array<BinaryCounts, 16> positions{};
    std::array<BinaryCounts, 17 * 16> classes{};
    for (auto& pair : positions) pair = {1, 1};
    for (auto& pair : classes) pair = {1, 1};
    DistanceBitCost result{};
    std::uint32_t pending_width{}, pending_class{};
    bool distance{};
    const auto observe = [](BinaryCounts& frequencies, BinaryCounts& counts,
                            const unsigned bit, double& cost) {
        const auto total = frequencies[0] + frequencies[1];
        cost += std::log2(static_cast<double>(total) / frequencies[bit]);
        ++frequencies[bit];
        ++counts[bit];
        if (total + 1 >= 32768)
            for (auto& frequency : frequencies) frequency = (frequency + 1) / 2;
    };
    for (const auto& operation : operations) {
        if (operation.kind == ModeledOperationKind::symbol) {
            if (pending_width != 0 || operation.context_id >= lzss_short_match_alphabets.size()
                || operation.alphabet_size != lzss_short_match_alphabets[operation.context_id]
                || operation.value >= operation.alphabet_size || operation.bit_count != 0)
                return {};
            distance = operation.context_id >= 23;
            pending_class = operation.value;
            if (distance) pending_width = operation.value;
            else if (operation.context_id >= 20)
                pending_width = operation.value == 8 ? 1 : operation.value;
        } else if (operation.kind == ModeledOperationKind::bypass_bits) {
            if (pending_width == 0 || operation.bit_count != pending_width
                || operation.context_id != 0 || operation.alphabet_size != 0
                || operation.value >= (UINT32_C(1) << pending_width)
                || (distance && pending_class == 16 && operation.value != 0)
                || (!distance && pending_class == 7 && operation.value == 127)) return {};
            if (distance) {
                for (unsigned position = 0; position < pending_width; ++position) {
                    const auto bit = (operation.value >> position) & 1U;
                    const auto index = pending_class * 16 + position;
                    observe(positions[position], result.position_counts[position], bit,
                            result.adaptive_bits[0]);
                    observe(classes[index], result.class_position_counts[index], bit,
                            result.adaptive_bits[1]);
                    ++result.uniform_bits;
                }
            }
            pending_width = 0;
        } else return {};
    }
    if (pending_width != 0) return {};
    const auto empirical = [](const auto& bank) {
        double cost{};
        for (const auto& pair : bank) {
            const auto total = pair[0] + pair[1];
            for (const auto count : pair)
                if (count != 0) cost += count * std::log2(static_cast<double>(total) / count);
        }
        return cost;
    };
    result.empirical_bits = {empirical(result.position_counts), empirical(result.class_position_counts)};
    result.valid = true;
    return result;
}

} // namespace marc::benchmarks
#endif
