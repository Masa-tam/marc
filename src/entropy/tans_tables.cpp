#include "entropy/tans_tables.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {
namespace {

[[nodiscard]] std::uint8_t floor_log2(std::uint32_t value) noexcept {
    std::uint8_t result{};
    while (value > 1) {
        value >>= 1;
        ++result;
    }
    return result;
}

} // namespace

TansTableError build_tans_tables(
    const TansDescriptor& descriptor, TansTables& tables) noexcept {
    if (descriptor.table_log != tans_table_log || descriptor.flags != 0)
        return TansTableError::invalid_descriptor;
    std::uint32_t sum{};
    for (const auto frequency : descriptor.frequencies) sum += frequency;
    if (sum != tans_table_size)
        return TansTableError::invalid_frequency_table;

    std::array<std::uint8_t, tans_table_size> spread{};
    std::array<bool, tans_table_size> written{};
    std::uint32_t position{};
    for (std::size_t symbol = 0; symbol < descriptor.frequencies.size();
         ++symbol) {
        for (std::uint32_t occurrence = 0;
             occurrence < descriptor.frequencies[symbol]; ++occurrence) {
            if (written[position]) return TansTableError::invalid_spread;
            written[position] = true;
            spread[position] = static_cast<std::uint8_t>(symbol);
            position = (position + tans_spread_step) & (tans_table_size - 1);
        }
    }
    if (position != 0) return TansTableError::invalid_spread;
    for (const bool slot : written)
        if (!slot) return TansTableError::invalid_spread;

    TansTables built{};
    std::uint32_t cumulative{};
    for (std::size_t symbol = 0; symbol < descriptor.frequencies.size();
         ++symbol) {
        built.symbol_offsets[symbol] = static_cast<std::uint16_t>(cumulative);
        cumulative += descriptor.frequencies[symbol];
    }
    std::array<std::uint16_t, 256> occurrences{};
    for (std::uint32_t slot = 0; slot < tans_table_size; ++slot) {
        const auto symbol = spread[slot];
        const auto frequency = descriptor.frequencies[symbol];
        const std::uint32_t q = frequency + occurrences[symbol]++;
        const auto bit_count = static_cast<std::uint8_t>(
            tans_table_log - floor_log2(q));
        const std::uint32_t state_base = q << bit_count;
        const std::uint32_t state_last =
            state_base + (UINT32_C(1) << bit_count) - 1;
        if (state_base < tans_table_size
            || state_last >= 2 * tans_table_size)
            return TansTableError::invalid_transition;
        built.decode[slot] = {
            static_cast<std::uint16_t>(state_base), symbol, bit_count};
        const auto index = static_cast<std::uint32_t>(
            built.symbol_offsets[symbol]) + q - frequency;
        if (index >= built.encode_states.size())
            return TansTableError::invalid_transition;
        built.encode_states[index] = static_cast<std::uint16_t>(
            tans_table_size + slot);
    }
    for (std::size_t symbol = 0; symbol < descriptor.frequencies.size();
         ++symbol) {
        if (occurrences[symbol] != descriptor.frequencies[symbol])
            return TansTableError::invalid_transition;
    }
    tables = built;
    return TansTableError::none;
}

} // namespace marc::entropy::internal
