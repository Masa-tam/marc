#ifndef MARC_ENTROPY_TANS_TABLES_HPP
#define MARC_ENTROPY_TANS_TABLES_HPP

#include "entropy/tans_format.hpp"

#include <array>
#include <cstdint>

namespace marc::entropy::internal {

struct TansDecodeEntry {
    std::uint16_t state_base{};
    std::uint8_t symbol{};
    std::uint8_t bit_count{};

    bool operator==(const TansDecodeEntry&) const = default;
};

struct TansTables {
    std::array<TansDecodeEntry, tans_table_size> decode{};
    std::array<std::uint16_t, tans_table_size> encode_states{};
    std::array<std::uint16_t, 256> symbol_offsets{};
};

enum class TansTableError : std::uint8_t {
    none,
    invalid_descriptor,
    invalid_frequency_table,
    invalid_spread,
    invalid_transition,
};

[[nodiscard]] TansTableError build_tans_tables(
    const TansDescriptor& descriptor, TansTables& tables) noexcept;

} // namespace marc::entropy::internal

#endif
