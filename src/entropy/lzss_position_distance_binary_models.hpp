#ifndef MARC_ENTROPY_LZSS_POSITION_DISTANCE_BINARY_MODELS_HPP
#define MARC_ENTROPY_LZSS_POSITION_DISTANCE_BINARY_MODELS_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {

struct DistanceBinaryInterval {
    std::uint32_t cumulative{};
    std::uint16_t frequency{};
    std::uint32_t total{};
};

// Bounded private primitive, not a token/operation grammar validator.
// Ask for an interval before coding; update only after coding succeeds.
class LzssPositionDistanceBinaryModels {
public:
    LzssPositionDistanceBinaryModels() noexcept { reset(); }

    void reset() noexcept {
        for (auto& pair : frequencies_) pair = {1, 1};
    }

    [[nodiscard]] bool interval(const std::size_t position,
                                const std::uint32_t bit,
                                DistanceBinaryInterval& result) const noexcept {
        if (position >= frequencies_.size() || bit > 1) return false;
        const auto& pair = frequencies_[position];
        result = {bit == 0 ? 0U : static_cast<std::uint32_t>(pair[0]),
                  pair[bit], static_cast<std::uint32_t>(pair[0]) + pair[1]};
        return true;
    }

    [[nodiscard]] bool update(const std::size_t position,
                              const std::uint32_t bit) noexcept {
        if (position >= frequencies_.size() || bit > 1) return false;
        auto& pair = frequencies_[position];
        ++pair[bit];
        if (static_cast<std::uint32_t>(pair[0]) + pair[1] == 32768) {
            for (auto& frequency : pair)
                frequency = static_cast<std::uint16_t>((frequency + 1U) / 2U);
        }
        return true;
    }

private:
    std::array<std::array<std::uint16_t, 2>, 16> frequencies_{};
};

} // namespace marc::entropy::internal
#endif
