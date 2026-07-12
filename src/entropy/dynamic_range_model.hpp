#ifndef MARC_ENTROPY_DYNAMIC_RANGE_MODEL_HPP
#define MARC_ENTROPY_DYNAMIC_RANGE_MODEL_HPP

#include "entropy/dynamic_range_format.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {

class DynamicRangeModel {
public:
    static constexpr std::size_t symbol_count = 256;

    [[nodiscard]] std::uint32_t total() const noexcept { return total_; }
    [[nodiscard]] std::uint16_t frequency(std::uint8_t symbol) const noexcept;
    [[nodiscard]] std::uint32_t cumulative(std::uint8_t symbol) const noexcept;
    [[nodiscard]] bool find_symbol(std::uint32_t scaled,
                                   std::uint8_t& symbol,
                                   std::uint32_t& cumulative,
                                   std::uint16_t& frequency) const noexcept;
    void update(std::uint8_t symbol) noexcept;
    [[nodiscard]] bool validate() const noexcept;

private:
    void rescale() noexcept;

    std::array<std::uint16_t, symbol_count> frequencies_ = [] {
        std::array<std::uint16_t, symbol_count> values{};
        values.fill(1);
        return values;
    }();
    std::uint32_t total_{symbol_count};
};

} // namespace marc::entropy::internal

#endif
