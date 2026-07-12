#include "entropy/dynamic_range_model.hpp"

namespace marc::entropy::internal {

std::uint16_t DynamicRangeModel::frequency(const std::uint8_t symbol) const
    noexcept {
    return frequencies_[symbol];
}

std::uint32_t DynamicRangeModel::cumulative(const std::uint8_t symbol) const
    noexcept {
    std::uint32_t result{};
    for (std::size_t index = 0; index < symbol; ++index) {
        result += frequencies_[index];
    }
    return result;
}

bool DynamicRangeModel::find_symbol(
    const std::uint32_t scaled,
    std::uint8_t& symbol,
    std::uint32_t& cumulative_out,
    std::uint16_t& frequency_out) const noexcept {
    if (scaled >= total_) return false;
    std::uint32_t cumulative_value{};
    for (std::size_t index = 0; index < frequencies_.size(); ++index) {
        const auto next = cumulative_value + frequencies_[index];
        if (scaled < next) {
            symbol = static_cast<std::uint8_t>(index);
            cumulative_out = cumulative_value;
            frequency_out = frequencies_[index];
            return true;
        }
        cumulative_value = next;
    }
    return false;
}

void DynamicRangeModel::update(const std::uint8_t symbol) noexcept {
    ++frequencies_[symbol];
    ++total_;
    if (total_ == dynamic_range_model_total_limit) rescale();
}

void DynamicRangeModel::rescale() noexcept {
    total_ = 0;
    for (auto& frequency_value : frequencies_) {
        frequency_value = static_cast<std::uint16_t>(
            (static_cast<std::uint32_t>(frequency_value) + 1U) / 2U);
        total_ += frequency_value;
    }
}

bool DynamicRangeModel::validate() const noexcept {
    if (total_ < symbol_count || total_ >= dynamic_range_model_total_limit) {
        return false;
    }
    std::uint32_t sum{};
    for (const auto frequency_value : frequencies_) {
        if (frequency_value == 0) return false;
        sum += frequency_value;
    }
    return sum == total_;
}

} // namespace marc::entropy::internal
