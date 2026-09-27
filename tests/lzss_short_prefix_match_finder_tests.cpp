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
        EXPECT_EQ(indexed.find_match_reference(position), exhaustive.find_match(position))
            << "reference position " << position;
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
    // A near short hit establishes best.length before the middle colliding
    // prefix is visited; the older real prefix still supplies the longer hit.
    constexpr std::array guarded_collision{
        std::byte{3},std::byte{0},std::byte{0},std::byte{'a'},std::byte{'b'},std::byte{'c'},std::byte{0xfe},
        std::byte{0x60},std::byte{0xc5},std::byte{0},std::byte{'a'},std::byte{'q'},std::byte{0xfd},
        std::byte{3},std::byte{0},std::byte{0},std::byte{'a'},std::byte{'x'},std::byte{0xfc},
        std::byte{3},std::byte{0},std::byte{0},std::byte{'a'},std::byte{'b'},std::byte{'c'},std::byte{0xff}};
    compare_every_position(guarded_collision,parameters);
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

TEST(LzssShortPrefixMatchFinder, ProbeOrderAgreesAcrossGreedyBoundariesAndVariants) {
    std::vector<std::uint32_t> storage(65536+65536);
    const auto workspace=std::as_writable_bytes(std::span{storage});
    for(const auto size:{0U,1U,2U,3U,4U,17U,18U,19U,257U,258U,259U,65535U,65536U}) {
        SCOPED_TRACE(size);
        for(unsigned pattern=0;pattern<3;++pattern) {
            std::vector<std::byte> input(size);
            std::uint32_t random=0x6d617263;
            for(std::size_t i=0;i<input.size();++i) {
                random^=random<<13U; random^=random>>17U; random^=random<<5U;
                input[i]=std::byte(static_cast<unsigned char>(pattern==0?0:pattern==1?i%7:random));
            }
            for(const auto variant:{LzssTypedTokenVariant::field_context_64k_short_match,
                                    LzssTypedTokenVariant::field_context_64k_short_length_escape})
            for(const auto maximum:{3U,4U,258U}) for(const auto window:{37U,65536U})
            for(const auto eligibility:{3U,4U,5U}) {
                const LzssParameters config{window,3,maximum,0};
                LzssShortPrefixMatchFinder finder;
                ASSERT_EQ(initialize_lzss_short_prefix_match_finder(input,config,{},workspace,finder,variant),
                          LzssShortPrefixError::none);
                std::size_t position=0;
                while(position<input.size()) {
                    const auto match=finder.find_match(position);
                    ASSERT_EQ(match,finder.find_match_reference(position)) << position;
                    const auto next=position+(match.length>=eligibility?match.length:1);
                    ASSERT_LE(next,input.size());
                    finder.advance(position,next); position=next;
                }
                EXPECT_EQ(finder.find_match(position),LzssMatch{});
                EXPECT_EQ(finder.find_match_reference(position),LzssMatch{});
                finder.advance(position,position+1); // invalid advance poisons queries equally
                EXPECT_EQ(finder.find_match(0),LzssMatch{});
                EXPECT_EQ(finder.find_match_reference(0),LzssMatch{});
            }
        }
    }
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

TEST(LzssShortPrefixMatchFinder, CompactLayoutPreservesEveryRequiredSizeAndLimit) {
    marc::core::DecoderLimits limits{};
    limits.max_block_size=65536;
    for(std::size_t n=0;n<=65536;++n) {
        const auto required=calculate_lzss_short_prefix_workspace(n,parameters,limits);
        ASSERT_EQ(required.error,LzssShortPrefixError::none);
        ASSERT_EQ(required.workspace_size,n<3?0:4*(65536+n));
        ASSERT_EQ(required.workspace_alignment,alignof(std::uint32_t));
        if(n<3) continue;
        auto exact=limits;exact.max_internal_buffered_bytes=n+required.workspace_size;
        ASSERT_EQ(calculate_lzss_short_prefix_workspace(n,parameters,exact).error,LzssShortPrefixError::none);
        --exact.max_internal_buffered_bytes;
        ASSERT_EQ(calculate_lzss_short_prefix_workspace(n,parameters,exact).error,LzssShortPrefixError::workspace_limit_exceeded);
    }
}

TEST(LzssShortPrefixMatchFinder, CompactTailSentinelAndFailedReinitialization) {
    std::vector<std::byte> input(65536,std::byte{0});
    // Force queries and insertions at the highest legal compact positions.
    input[65531]=std::byte{1};
    std::vector<std::uint32_t> storage(131072);
    auto workspace=std::as_writable_bytes(std::span{storage});
    LzssShortPrefixMatchFinder finder;
    ASSERT_EQ(initialize_lzss_short_prefix_match_finder(input,parameters,{},workspace,finder),LzssShortPrefixError::none);
    finder.advance(0,65532);
    LzssExhaustiveMatchFinder exhaustive{input,parameters};
    for(std::size_t p=65532;p<65536;++p) {
        ASSERT_EQ(finder.find_match(p),exhaustive.find_match(p));
        ASSERT_EQ(finder.find_match_reference(p),exhaustive.find_match(p));
        finder.advance(p,p+1);
    }
    ASSERT_EQ(initialize_lzss_short_prefix_match_finder(input,parameters,{},workspace,finder),LzssShortPrefixError::none);
    finder.advance(0,4);
    const auto expected=finder.find_match(4);
    const std::vector<std::byte> saved(workspace.begin(),workspace.end());
    EXPECT_EQ(initialize_lzss_short_prefix_match_finder(input,parameters,{},workspace.first(workspace.size()-1),finder),LzssShortPrefixError::workspace_too_small);
    EXPECT_EQ(finder.find_match(4),expected);
    EXPECT_TRUE(std::equal(saved.begin(),saved.end(),workspace.begin()));
    const auto small_input=std::span<const std::byte>{input}.first(7);
    EXPECT_EQ(initialize_lzss_short_prefix_match_finder(small_input,parameters,{},workspace.subspan(1),finder),LzssShortPrefixError::misaligned_workspace);
    EXPECT_EQ(finder.find_match(4),expected);
    EXPECT_TRUE(std::equal(saved.begin(),saved.end(),workspace.begin()));
    const auto alias=std::span<const std::byte>{workspace}.first(7);
    EXPECT_EQ(initialize_lzss_short_prefix_match_finder(alias,parameters,{},workspace,finder),LzssShortPrefixError::overlapping_buffers);
    EXPECT_EQ(finder.find_match(4),expected);
    EXPECT_TRUE(std::equal(saved.begin(),saved.end(),workspace.begin()));
}

TEST(LzssShortPrefixMatchFinder, FourByteCollisionsAndShortFallback) {
    // The same colliding numeric keys as the three-byte test, extended by 00.
    const std::array input{
        std::byte{3},std::byte{0},std::byte{0},std::byte{0},std::byte{'a'},std::byte{0xff},
        std::byte{0x60},std::byte{0xc5},std::byte{0},std::byte{0},std::byte{'b'},std::byte{0xfe},
        std::byte{3},std::byte{0},std::byte{0},std::byte{1},std::byte{0xfd},
        std::byte{3},std::byte{0},std::byte{0},std::byte{0},std::byte{'a'}};
    compare_every_position(input,parameters);
    auto four=parameters;four.max_match_length=4;
    compare_every_position(input,four);
}
