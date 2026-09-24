#ifndef MARC_BENCHMARKS_LZSS_MODEL_COST_HPP
#define MARC_BENCHMARKS_LZSS_MODEL_COST_HPP

#include "context/lzss_field_context.hpp"
#include "context/lzss_short_match_context_layout.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <span>

namespace marc::benchmarks {

enum class CostLayout { published_64k, short_match_64k };

// Benchmark-only information quantities. No serialized representation uses
// these floating-point calculations. Categories: kind, literal, length, distance.
struct ModelCost {
    std::array<double, 4> adaptive_bits{};
    std::array<double, 4> empirical_bits{};
    std::array<std::uint64_t, 4> symbols{};
    std::array<std::uint64_t, 2> bypass_bits{};
    bool valid{};
};

[[nodiscard]] inline ModelCost measure_model_cost(
    const std::span<const context::internal::ModeledOperation> operations,
    const CostLayout layout,
    const std::uint32_t literal_increment = 1) noexcept {
    using namespace context::internal;
    if (operations.size() > 5 * 65536
        || (literal_increment != 1 && literal_increment != 2
            && literal_increment != 4 && literal_increment != 8)) return {};
    std::span<const std::uint16_t> alphabets;
    if (layout == CostLayout::published_64k) {
        alphabets = lzss_field_context_alphabets_v1;
    } else if (layout == CostLayout::short_match_64k) {
        alphabets = lzss_short_match_alphabets;
    } else {
        return {};
    }
    std::array<std::array<std::uint32_t, 256>, 32> frequencies{};
    std::array<std::array<std::uint32_t, 256>, 32> observed{};
    std::array<std::uint32_t, 32> totals{};
    std::array<std::uint32_t, 32> counts{};
    const auto category = [](const std::size_t id) -> std::size_t {
        return id < 3 ? 0 : id < 20 ? 1 : id < 23 ? 2 : 3;
    };
    for (std::size_t id = 0; id < alphabets.size(); ++id) {
        frequencies[id].fill(1);
        totals[id] = alphabets[id];
    }
    ModelCost result{};
    std::size_t preceding = alphabets.size();
    for (const auto& operation : operations) {
        if (operation.kind == ModeledOperationKind::symbol) {
            const auto id = operation.context_id;
            if (id >= alphabets.size()
                || operation.alphabet_size != alphabets[id]
                || operation.value >= alphabets[id]
                || operation.bit_count != 0) return {};
            const auto group = category(id);
            auto& frequency = frequencies[id][operation.value];
            result.adaptive_bits[group] += std::log2(
                static_cast<double>(totals[id]) / frequency);
            ++result.symbols[group];
            ++observed[id][operation.value];
            ++counts[id];
            const auto increment = group == 1 ? literal_increment : 1U;
            frequency += increment;
            totals[id] += increment;
            if (totals[id] >= 32768) {
                totals[id] = 0;
                for (std::size_t symbol = 0; symbol < alphabets[id]; ++symbol) {
                    auto& value = frequencies[id][symbol];
                    value = (value + 1) / 2;
                    totals[id] += value;
                }
            }
            preceding = id;
        } else if (operation.kind == ModeledOperationKind::bypass_bits) {
            if (preceding < 20 || preceding >= alphabets.size()
                || operation.context_id != 0 || operation.alphabet_size != 0
                || operation.bit_count == 0 || operation.bit_count > 16
                || operation.value >= (UINT32_C(1) << operation.bit_count)) {
                return {};
            }
            result.bypass_bits[preceding < 23 ? 0 : 1] += operation.bit_count;
            preceding = alphabets.size();
        } else {
            return {};
        }
    }
    for (std::size_t id = 0; id < alphabets.size(); ++id) {
        for (std::size_t symbol = 0; symbol < alphabets[id]; ++symbol) {
            const auto count = observed[id][symbol];
            if (count == 0) continue;
            result.empirical_bits[category(id)] += count * std::log2(
                static_cast<double>(counts[id]) / count);
        }
    }
    result.valid = true;
    return result;
}

} // namespace marc::benchmarks

#endif
