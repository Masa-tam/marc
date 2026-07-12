#include "entropy/rans_normalizer.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::entropy::internal {
namespace {

[[nodiscard]] std::int64_t error_for(
    const std::uint32_t count, const std::uint16_t frequency,
    const std::uint32_t size) noexcept {
    return static_cast<std::int64_t>(count) * rans_total_frequency
        - static_cast<std::int64_t>(frequency) * size;
}

} // namespace

RansNormalizationError normalize_rans_frequencies(
    const std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    std::array<std::uint16_t, 256>& frequencies) noexcept {
    if (input.empty()) return RansNormalizationError::empty_input;
    if (input.size() > rans_max_block_size
        || input.size() > std::numeric_limits<std::uint32_t>::max()) {
        return RansNormalizationError::block_too_large;
    }
    if (input.size() > limits.max_block_size
        || rans_total_frequency > limits.max_entropy_table_entries
        || rans_descriptor_size > limits.max_internal_buffered_bytes) {
        return RansNormalizationError::limit_exceeded;
    }
    std::array<std::uint32_t, 256> counts{};
    for (const auto byte : input) {
        ++counts[std::to_integer<std::uint8_t>(byte)];
    }
    const auto size = static_cast<std::uint32_t>(input.size());
    std::array<std::uint16_t, 256> normalized{};
    std::uint32_t sum{};
    for (std::size_t symbol = 0; symbol < counts.size(); ++symbol) {
        if (counts[symbol] == 0) continue;
        const auto scaled = static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(counts[symbol])
             * rans_total_frequency) / size);
        normalized[symbol] = static_cast<std::uint16_t>(
            scaled == 0 ? 1 : scaled);
        sum += normalized[symbol];
    }
    while (sum < rans_total_frequency) {
        std::size_t selected{};
        std::int64_t best = std::numeric_limits<std::int64_t>::min();
        bool found{};
        for (std::size_t symbol = 0; symbol < counts.size(); ++symbol) {
            if (counts[symbol] == 0) continue;
            const auto error = error_for(
                counts[symbol], normalized[symbol], size);
            if (!found || error > best) {
                selected = symbol;
                best = error;
                found = true;
            }
        }
        if (!found || normalized[selected] == UINT16_MAX) {
            return RansNormalizationError::internal_error;
        }
        ++normalized[selected];
        ++sum;
    }
    while (sum > rans_total_frequency) {
        std::size_t selected{};
        std::int64_t best = std::numeric_limits<std::int64_t>::max();
        bool found{};
        for (std::size_t symbol = 0; symbol < counts.size(); ++symbol) {
            if (normalized[symbol] <= 1) continue;
            const auto error = error_for(
                counts[symbol], normalized[symbol], size);
            if (!found || error < best
                || (error == best && symbol > selected)) {
                selected = symbol;
                best = error;
                found = true;
            }
        }
        if (!found) return RansNormalizationError::internal_error;
        --normalized[selected];
        --sum;
    }
    frequencies = normalized;
    return RansNormalizationError::none;
}

} // namespace marc::entropy::internal
