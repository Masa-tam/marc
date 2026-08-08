#include "dictionary/lzss_typed_reconstructor.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace {

using namespace marc::dictionary::internal;

[[nodiscard]] constexpr LzssTypedToken literal(const std::uint8_t value) {
    return {LzssTypedTokenKind::literal, value, 0, 0};
}

[[nodiscard]] constexpr LzssTypedToken match(
    const std::uint32_t distance, const std::uint32_t length) {
    return {LzssTypedTokenKind::match, 0, distance, length};
}

template<std::size_t Size>
void expect_text(const std::array<std::byte, Size>& output,
                 const std::string_view expected) {
    ASSERT_EQ(output.size(), expected.size());
    for (std::size_t index = 0; index < output.size(); ++index) {
        EXPECT_EQ(output[index], static_cast<std::byte>(expected[index]))
            << "index=" << index;
    }
}

} // namespace

TEST(LzssTypedReconstructor, ReconstructsEmptyAndLiteralFrames) {
    std::array<std::byte, 1> sentinel{std::byte{0xcc}};
    auto result = reconstruct_lzss_typed_frame(
        {}, {}, {0, 0, 0}, marc::core::DecoderLimits{}, sentinel);
    EXPECT_EQ(result.error, LzssTypedReconstructError::none);
    EXPECT_EQ(result.output_size, 0U);
    EXPECT_EQ(sentinel[0], std::byte{0xcc});

    constexpr std::array tokens{literal('A')};
    result = reconstruct_lzss_typed_frame(
        tokens, {}, {1, 1, 0}, marc::core::DecoderLimits{}, sentinel);
    EXPECT_EQ(result.error, LzssTypedReconstructError::none);
    EXPECT_EQ(result.output_size, 1U);
    EXPECT_EQ(sentinel[0], std::byte{'A'});
}

TEST(LzssTypedReconstructor, ReconstructsBytewiseOverlap) {
    constexpr std::array tokens{literal('A'), match(1, 5)};
    std::array<std::byte, 6> output{};
    const auto result = reconstruct_lzss_typed_frame(
        tokens, {}, {2, 6, 0}, marc::core::DecoderLimits{}, output);
    EXPECT_EQ(result.error, LzssTypedReconstructError::none);
    EXPECT_EQ(result.validation.token_count, 2U);
    expect_text(output, "AAAAAA");
}

TEST(LzssTypedReconstructor, ReconstructsDistanceThreeThenLiteral) {
    constexpr std::array tokens{
        literal('A'), literal('B'), literal('C'), match(3, 6), literal('X')};
    std::array<std::byte, 10> output{};
    const auto result = reconstruct_lzss_typed_frame(
        tokens, {}, {5, 10, 0}, marc::core::DecoderLimits{}, output);
    EXPECT_EQ(result.error, LzssTypedReconstructError::none);
    expect_text(output, "ABCABCABCX");
}

TEST(LzssTypedReconstructor, MalformedFrameLeavesOutputUnchanged) {
    constexpr std::array tokens{literal('A'), match(2, 5)};
    std::array<std::byte, 6> output{};
    output.fill(std::byte{0xcc});
    const auto result = reconstruct_lzss_typed_frame(
        tokens, {}, {2, 6, 0}, marc::core::DecoderLimits{}, output);
    EXPECT_EQ(result.error, LzssTypedReconstructError::invalid_token_frame);
    EXPECT_EQ(result.validation.error,
              LzssTypedFrameValidationError::token_error);
    EXPECT_EQ(result.validation.token_error,
              LzssTypedTokenError::invalid_distance);
    EXPECT_EQ(result.validation.token_index, 1U);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));
}

TEST(LzssTypedReconstructor, SmallOutputIsAtomicAndReportsRequiredSize) {
    constexpr std::array tokens{literal('A'), match(1, 5)};
    std::array<std::byte, 6> output{};
    output.fill(std::byte{0xcc});
    const auto result = reconstruct_lzss_typed_frame(
        tokens, {}, {2, 6, 0}, marc::core::DecoderLimits{},
        std::span<std::byte>{output}.first(5));
    EXPECT_EQ(result.error, LzssTypedReconstructError::output_too_small);
    EXPECT_EQ(result.output_size, 6U);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xcc};
    }));
}

TEST(LzssTypedReconstructor, DoesNotWritePastDeclaredRawExtent) {
    constexpr std::array tokens{literal('A')};
    std::array<std::byte, 3> output{};
    output.fill(std::byte{0xcc});
    const auto result = reconstruct_lzss_typed_frame(
        tokens, {}, {1, 1, 0}, marc::core::DecoderLimits{}, output);
    EXPECT_EQ(result.error, LzssTypedReconstructError::none);
    EXPECT_EQ(output[0], std::byte{'A'});
    EXPECT_EQ(output[1], std::byte{0xcc});
    EXPECT_EQ(output[2], std::byte{0xcc});
}

TEST(LzssTypedReconstructor, RejectsTokenOutputAliasingAtomically) {
    std::array tokens{literal('A'), match(1, 5)};
    const auto original = tokens;
    auto* bytes = reinterpret_cast<std::byte*>(tokens.data());
    const std::span<std::byte> aliased_output{bytes, 6};
    const auto result = reconstruct_lzss_typed_frame(
        tokens, {}, {2, 6, 0}, marc::core::DecoderLimits{}, aliased_output);
    EXPECT_EQ(result.error, LzssTypedReconstructError::overlapping_buffers);
    EXPECT_EQ(tokens[0].kind, original[0].kind);
    EXPECT_EQ(tokens[0].literal, original[0].literal);
    EXPECT_EQ(tokens[1].kind, original[1].kind);
    EXPECT_EQ(tokens[1].distance, original[1].distance);
    EXPECT_EQ(tokens[1].length, original[1].length);
}

TEST(LzssTypedReconstructor, PolicyFailureLeavesOutputUnchanged) {
    constexpr std::array tokens{literal('A')};
    auto limits = marc::core::DecoderLimits{};
    limits.max_lz_distance = 65535;
    std::array<std::byte, 1> output{std::byte{0xcc}};
    const auto result = reconstruct_lzss_typed_frame(
        tokens, {}, {1, 1, 0}, limits, output);
    EXPECT_EQ(result.error, LzssTypedReconstructError::invalid_token_frame);
    EXPECT_EQ(result.validation.error,
              LzssTypedFrameValidationError::limit_exceeded);
    EXPECT_EQ(output[0], std::byte{0xcc});
}
