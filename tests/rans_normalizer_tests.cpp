#include "entropy/rans_normalizer.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

using marc::entropy::internal::RansNormalizationError;

[[nodiscard]] std::array<std::uint16_t, 256> normalize(
    const std::vector<std::byte>& input) {
    std::array<std::uint16_t, 256> result{};
    EXPECT_EQ(marc::entropy::internal::normalize_rans_frequencies(
                  input, {}, result),
              RansNormalizationError::none);
    return result;
}

TEST(RansNormalizer, ProducesHandModels) {
    auto frequencies = normalize({std::byte{0x41}});
    EXPECT_EQ(frequencies[0x41], 4096U);

    frequencies = normalize({std::byte{0x41}, std::byte{0x42}});
    EXPECT_EQ(frequencies[0x41], 2048U);
    EXPECT_EQ(frequencies[0x42], 2048U);

    frequencies = normalize(
        {std::byte{0x41}, std::byte{0x42}, std::byte{0x41}});
    EXPECT_EQ(frequencies[0x41], 2731U);
    EXPECT_EQ(frequencies[0x42], 1365U);
}

TEST(RansNormalizer, BreaksEqualAdjustmentTowardLowerSymbol) {
    const auto frequencies = normalize(
        {std::byte{2}, std::byte{1}, std::byte{0}});
    EXPECT_EQ(frequencies[0], 1366U);
    EXPECT_EQ(frequencies[1], 1365U);
    EXPECT_EQ(frequencies[2], 1365U);
}

TEST(RansNormalizer, PreservesAllPresentSymbolsAndExactSum) {
    std::vector<std::byte> input(4096, std::byte{0});
    for (std::size_t symbol = 1; symbol < 256; ++symbol) {
        input[symbol] = static_cast<std::byte>(symbol);
    }
    const auto frequencies = normalize(input);
    std::uint32_t sum{};
    for (std::size_t symbol = 0; symbol < 256; ++symbol) {
        EXPECT_GT(frequencies[symbol], 0U);
        sum += frequencies[symbol];
    }
    EXPECT_EQ(sum, 4096U);
}

TEST(RansNormalizer, RejectsEmptyAndLocalLimitsWithoutPublishing) {
    std::array<std::uint16_t, 256> output{};
    output.fill(7);
    EXPECT_EQ(marc::entropy::internal::normalize_rans_frequencies(
                  {}, {}, output),
              RansNormalizationError::empty_input);
    EXPECT_EQ(output[0], 7U);

    marc::core::DecoderLimits limits{};
    limits.max_entropy_table_entries = 4095;
    constexpr std::array input{std::byte{0x41}};
    EXPECT_EQ(marc::entropy::internal::normalize_rans_frequencies(
                  input, limits, output),
              RansNormalizationError::limit_exceeded);
    EXPECT_EQ(output[0], 7U);
}

} // namespace
