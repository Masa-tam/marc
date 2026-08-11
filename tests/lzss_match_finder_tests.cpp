#include "dictionary/lzss_match_finder.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <string_view>
#include <vector>

namespace {
using namespace marc::dictionary::internal;

[[nodiscard]] std::vector<std::byte> bytes(const std::string_view text) {
    std::vector<std::byte> result;
    result.reserve(text.size());
    for (const char value : text) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

[[nodiscard]] LzssMatch find_at(
    const std::string_view text, const std::size_t position,
    const LzssParameters& parameters = {}) {
    const auto input = bytes(text);
    LzssExhaustiveMatchFinder finder{input, parameters};
    finder.advance(0, position);
    return finder.find_match(position);
}

TEST(LzssExhaustiveMatchFinder, ReturnsNoMatchAtEmptyAndExactEnd) {
    const auto empty = bytes("");
    const LzssExhaustiveMatchFinder empty_finder{empty, {}};
    EXPECT_EQ(empty_finder.find_match(0), LzssMatch{});

    const auto one = bytes("A");
    const LzssExhaustiveMatchFinder one_finder{one, {}};
    EXPECT_EQ(one_finder.find_match(0), LzssMatch{});
    EXPECT_EQ(one_finder.find_match(one.size()), LzssMatch{});
}

TEST(LzssExhaustiveMatchFinder, FindsOverlapAndHonorsMaximumLength) {
    EXPECT_EQ(find_at("ABABABAB", 2), (LzssMatch{2, 6}));

    LzssParameters parameters{};
    parameters.max_match_length = 5;
    EXPECT_EQ(find_at("ABABABAB", 2, parameters), (LzssMatch{2, 5}));
}

TEST(LzssExhaustiveMatchFinder, UsesNearestDistanceForEqualLength) {
    EXPECT_EQ(find_at("ABCDE1ABCDE2ABCDE3", 12),
              (LzssMatch{6, 5}));
}

TEST(LzssExhaustiveMatchFinder, EnforcesWindowBoundary) {
    LzssParameters parameters{};
    parameters.window_size = 4;
    EXPECT_EQ(find_at("XABCDEABCDE", 6, parameters), LzssMatch{});

    parameters.window_size = 5;
    EXPECT_EQ(find_at("XABCDEABCDE", 6, parameters),
              (LzssMatch{5, 5}));
}

TEST(LzssExhaustiveMatchFinder, RejectsMatchesBelowConfiguredMinimum) {
    EXPECT_EQ(find_at("ABCDXABCDY", 5), LzssMatch{});
}

TEST(LzssExhaustiveMatchFinder, AdvanceDoesNotChangeReferenceResult) {
    const auto input = bytes("ABCDE1ABCDE2ABCDE3");
    LzssExhaustiveMatchFinder finder{input, {}};
    const auto before = finder.find_match(12);
    finder.advance(0, 12);
    EXPECT_EQ(finder.find_match(12), before);
}

} // namespace
