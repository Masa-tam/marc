#include "entropy/rans_decode_table.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {

RansDecodeTableError build_rans_decode_table(
    const RansDescriptor& descriptor,
    RansDecodeTable& table) noexcept {
    RansDecodeTable built{};
    std::uint32_t cumulative{};
    for (std::size_t symbol = 0; symbol < descriptor.frequencies.size();
         ++symbol) {
        const auto frequency = descriptor.frequencies[symbol];
        if (frequency == 0) continue;
        if (cumulative > rans_total_frequency
            || frequency > rans_total_frequency - cumulative) {
            return RansDecodeTableError::invalid_frequency_table;
        }
        const RansDecodeEntry entry{
            static_cast<std::uint16_t>(cumulative), frequency,
            static_cast<std::uint8_t>(symbol)};
        for (std::uint32_t slot = cumulative;
             slot < cumulative + frequency; ++slot) {
            built[slot] = entry;
        }
        cumulative += frequency;
    }
    if (cumulative != rans_total_frequency) {
        return RansDecodeTableError::invalid_frequency_table;
    }
    table = built;
    return RansDecodeTableError::none;
}

} // namespace marc::entropy::internal
