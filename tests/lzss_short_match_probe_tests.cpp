#include "lzss_short_match_probe.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {
using marc::benchmark::internal::ShortMatchPrefixFlags;
using marc::benchmark::internal::ShortMatchPrefixIndex;

std::vector<ShortMatchPrefixFlags> exhaustive_flags(
    const std::span<const std::byte> input) {
    std::vector<ShortMatchPrefixFlags> expected(input.size());
    for (std::size_t position = 0; position < input.size(); ++position) {
        for (std::size_t previous = 0; previous < position; ++previous) {
            if (position + 3 <= input.size()
                && input[previous] == input[position]
                && input[previous + 1] == input[position + 1]
                && input[previous + 2] == input[position + 2]) {
                expected[position].has_three = true;
                const auto distance = static_cast<std::uint32_t>(
                    position - previous);
                if (expected[position].nearest_three == 0
                    || distance < expected[position].nearest_three) {
                    expected[position].nearest_three = distance;
                }
                if (position + 4 <= input.size()
                    && input[previous + 3] == input[position + 3]) {
                    expected[position].has_four = true;
                    if (expected[position].nearest_four == 0
                        || distance < expected[position].nearest_four) {
                        expected[position].nearest_four = distance;
                    }
                }
            }
        }
    }
    return expected;
}

std::vector<std::byte> bytes(const std::initializer_list<unsigned> values) {
    std::vector<std::byte> output;
    for (const auto value : values) {
        output.push_back(static_cast<std::byte>(value));
    }
    return output;
}

TEST(LzssShortMatchPrefixIndex, MatchesExhaustiveSmallInputs) {
    ShortMatchPrefixIndex index{};
    ASSERT_TRUE(index.ready());
    std::uint32_t random_state{0x21544903U};
    for (std::size_t size = 0; size <= 128; ++size) {
        for (unsigned alphabet = 1; alphabet <= 8; ++alphabet) {
            std::vector<std::byte> input(size);
            for (auto& value : input) {
                random_state ^= random_state << 13U;
                random_state ^= random_state >> 17U;
                random_state ^= random_state << 5U;
                value = static_cast<std::byte>(random_state % alphabet);
            }
            std::vector<ShortMatchPrefixFlags> actual(size);
            ASSERT_TRUE(index.analyze(input, actual));
            const auto expected = exhaustive_flags(input);
            for (std::size_t position = 0; position < size; ++position) {
                EXPECT_EQ(actual[position].has_three,
                          expected[position].has_three)
                    << "size=" << size << " alphabet=" << alphabet
                    << " position=" << position;
                EXPECT_EQ(actual[position].has_four,
                          expected[position].has_four)
                    << "size=" << size << " alphabet=" << alphabet
                    << " position=" << position;
                EXPECT_EQ(actual[position].nearest_three,
                          expected[position].nearest_three);
                EXPECT_EQ(actual[position].nearest_four,
                          expected[position].nearest_four);
            }
        }
    }
}

TEST(LzssShortMatchPrefixIndex, ResetsBetweenFrames) {
    ShortMatchPrefixIndex index{};
    ASSERT_TRUE(index.ready());
    const auto first = bytes({1, 2, 3, 4, 1, 2, 3, 4});
    const auto second = bytes({1, 2, 3, 4});
    std::vector<ShortMatchPrefixFlags> first_flags(first.size());
    std::vector<ShortMatchPrefixFlags> second_flags(second.size());
    ASSERT_TRUE(index.analyze(first, first_flags));
    EXPECT_TRUE(first_flags[4].has_three);
    EXPECT_TRUE(first_flags[4].has_four);
    ASSERT_TRUE(index.analyze(second, second_flags));
    for (const auto flags : second_flags) {
        EXPECT_FALSE(flags.has_three);
        EXPECT_FALSE(flags.has_four);
    }
}

TEST(LzssShortMatchPrefixIndex, RejectsInvalidExtentWithoutWriting) {
    ShortMatchPrefixIndex index{};
    ASSERT_TRUE(index.ready());
    const auto input = bytes({1, 2, 3, 1, 2, 3});
    std::vector<ShortMatchPrefixFlags> too_short(input.size() - 1);
    EXPECT_FALSE(index.analyze(input, too_short));

    std::vector<std::byte> overlong_input(ShortMatchPrefixIndex::frame_limit
                                          + 1);
    std::vector<ShortMatchPrefixFlags> overlong_output(
        overlong_input.size(), {true, true});
    EXPECT_FALSE(index.analyze(overlong_input, overlong_output));
    EXPECT_TRUE(overlong_output.front().has_three);
    EXPECT_TRUE(overlong_output.back().has_four);
}
} // namespace
