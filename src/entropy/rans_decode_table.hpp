#ifndef MARC_ENTROPY_RANS_DECODE_TABLE_HPP
#define MARC_ENTROPY_RANS_DECODE_TABLE_HPP

#include "entropy/rans_format.hpp"

#include <array>
#include <cstdint>

namespace marc::entropy::internal {

struct RansDecodeEntry {
    std::uint16_t cumulative{};
    std::uint16_t frequency{};
    std::uint8_t symbol{};
};

using RansDecodeTable =
    std::array<RansDecodeEntry, rans_total_frequency>;

enum class RansDecodeTableError : std::uint8_t {
    none,
    invalid_frequency_table,
    arithmetic_overflow,
};

[[nodiscard]] RansDecodeTableError build_rans_decode_table(
    const RansDescriptor& descriptor,
    RansDecodeTable& table) noexcept;

} // namespace marc::entropy::internal

#endif
