#include "dictionary/lzss_short_prefix_match_finder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using namespace marc::dictionary::internal;
constexpr LzssParameters parameters{65536, 3, 258, 0};

void compare_every_position(const std::span<const std::byte> input,
                            const LzssParameters& configuration) {
    const marc::core::DecoderLimits limits{};
    const auto requirements = calculate_lzss_short_prefix_workspace(
        input.size(), configuration, limits);
    ASSERT_EQ(requirements.error, LzssShortPrefixError::none);
    std::vector<std::uint32_t> storage(
        (requirements.workspace_size + sizeof(std::uint32_t) - 1)
            / sizeof(std::uint32_t));
    auto workspace = std::span<std::byte>{
        reinterpret_cast<std::byte*>(storage.data()),
        storage.size() * sizeof(std::uint32_t)};
    LzssShortPrefixMatchFinder indexed{};
    ASSERT_EQ(initialize_lzss_short_prefix_match_finder(
                  input, configuration, limits, workspace, indexed),
              LzssShortPrefixError::none);
    LzssExhaustiveMatchFinder exhaustive{input, configuration};
    for (std::size_t position = 0; position < input.size(); ++position) {
        EXPECT_EQ(indexed.find_match(position), exhaustive.find_match(position))
            << "position " << position;
        indexed.advance(position, position + 1);
    }
}

} // namespace

TEST(LzssShortPrefixMatchFinder, MatchesExhaustiveOnHandInputs) {
    constexpr std::array<std::byte, 0> empty{};
    constexpr std::array one{std::byte{0x61}};
    constexpr std::array two{std::byte{0x61}, std::byte{0x61}};
    constexpr std::array run{
        std::byte{0x61}, std::byte{0x61}, std::byte{0x61},
        std::byte{0x61}, std::byte{0x61}, std::byte{0x61}};
    constexpr std::array ties{
        std::byte{0x61}, std::byte{0x62}, std::byte{0x63}, std::byte{0x58},
        std::byte{0x61}, std::byte{0x62}, std::byte{0x63}, std::byte{0x59},
        std::byte{0x61}, std::byte{0x62}, std::byte{0x63}};
    // Distinct three-byte prefixes 03 00 00 and 60 C5 00 collide in the
    // current bucket table. The nearer collision must not hide the older hit.
    constexpr std::array collision{
        std::byte{0x03}, std::byte{0x00}, std::byte{0x00}, std::byte{0xff},
        std::byte{0x60}, std::byte{0xc5}, std::byte{0x00}, std::byte{0xfe},
        std::byte{0x03}, std::byte{0x00}, std::byte{0x00}};
    compare_every_position(empty, parameters);
    compare_every_position(one, parameters);
    compare_every_position(two, parameters);
    compare_every_position(run, parameters);
    compare_every_position(ties, parameters);
    compare_every_position(collision, parameters);
    auto small_window = parameters;
    small_window.window_size = 4;
    compare_every_position(ties, small_window);
}

TEST(LzssShortPrefixMatchFinder, MatchesExhaustiveOnDeterministicBinaryData) {
    std::vector<std::byte> input(513);
    std::uint32_t state = UINT32_C(0x6d617263);
    for (auto& value : input) {
        state ^= state << 13U;
        state ^= state >> 17U;
        state ^= state << 5U;
        value = std::byte{static_cast<std::uint8_t>(state & 0x0fU)};
    }
    compare_every_position(input, parameters);
    auto small_window = parameters;
    small_window.window_size = 37;
    compare_every_position(input, small_window);
}

TEST(LzssShortPrefixMatchFinder, RequiresBoundedDisjointWorkspace) {
    const std::array input{
        std::byte{0x61}, std::byte{0x62}, std::byte{0x63},
        std::byte{0x61}, std::byte{0x62}, std::byte{0x63}};
    const marc::core::DecoderLimits limits{};
    const auto requirements = calculate_lzss_short_prefix_workspace(
        input.size(), parameters, limits);
    ASSERT_EQ(requirements.error, LzssShortPrefixError::none);
    std::vector<std::uint32_t> storage(
        requirements.workspace_size / sizeof(std::uint32_t));
    const auto workspace = std::span<std::byte>{
        reinterpret_cast<std::byte*>(storage.data()),
        storage.size() * sizeof(std::uint32_t)};
    LzssShortPrefixMatchFinder finder{};
    EXPECT_EQ(initialize_lzss_short_prefix_match_finder(
                  input, parameters, limits,
                  workspace.first(workspace.size() - 1), finder),
              LzssShortPrefixError::workspace_too_small);
    auto storage_alias = std::vector<std::uint32_t>(
        requirements.workspace_size / sizeof(std::uint32_t));
    const auto aliased_input = std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(storage_alias.data()), input.size()};
    const auto aliased_workspace = std::span<std::byte>{
        reinterpret_cast<std::byte*>(storage_alias.data()),
        storage_alias.size() * sizeof(std::uint32_t)};
    EXPECT_EQ(initialize_lzss_short_prefix_match_finder(
                  aliased_input, parameters, limits,
                  aliased_workspace, finder),
              LzssShortPrefixError::overlapping_buffers);
    auto tiny_limits = limits;
    tiny_limits.max_block_size = input.size();
    tiny_limits.max_internal_buffered_bytes = 1000;
    EXPECT_EQ(calculate_lzss_short_prefix_workspace(
                  input.size(), parameters, tiny_limits).error,
              LzssShortPrefixError::workspace_limit_exceeded);
}
