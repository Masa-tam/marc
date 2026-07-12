#include "entropy/tans_decoder.hpp"
#include "entropy/tans_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <vector>

namespace {

using marc::entropy::internal::TansDecodeError;
using marc::entropy::internal::TansDescriptor;

[[nodiscard]] TansDescriptor descriptor_for(
    const std::span<const std::byte> input,
    std::vector<std::byte>& payload) {
    TansDescriptor descriptor{};
    const auto plan = marc::entropy::internal::plan_tans_block(
        input, {}, descriptor);
    EXPECT_EQ(plan.error, marc::entropy::internal::TansEncodeError::none);
    payload.resize(plan.payload_size);
    EXPECT_EQ(marc::entropy::internal::encode_tans_block(
                  input, {}, payload, descriptor).error,
              marc::entropy::internal::TansEncodeError::none);
    return descriptor;
}

TEST(TansDecoder, DecodesHandVectors) {
    const std::array inputs{
        std::vector<std::byte>{std::byte{0x41}},
        std::vector<std::byte>{std::byte{0x41}, std::byte{0x41}},
        std::vector<std::byte>{std::byte{0x41}, std::byte{0x42}},
        std::vector<std::byte>{std::byte{0x41}, std::byte{0x42},
                               std::byte{0x41}}};
    for (const auto& input : inputs) {
        std::vector<std::byte> payload;
        const auto descriptor = descriptor_for(input, payload);
        ASSERT_EQ(marc::entropy::internal::validate_tans_block(
                      descriptor, payload, {}).error,
                  TansDecodeError::none);
        std::vector<std::byte> output(input.size());
        const auto decoded = marc::entropy::internal::decode_tans_block(
            descriptor, payload, {}, output);
        EXPECT_EQ(decoded.error, TansDecodeError::none);
        EXPECT_EQ(output, input);
    }
}

TEST(TansDecoder, RoundTripsAllByteValuesAndLongInput) {
    std::vector<std::byte> input(8193);
    for (std::size_t i = 0; i < input.size(); ++i)
        input[i] = static_cast<std::byte>((i * 53U + i / 7U) & 0xffU);
    std::vector<std::byte> payload;
    const auto descriptor = descriptor_for(input, payload);
    std::vector<std::byte> output(input.size());
    const auto decoded = marc::entropy::internal::decode_tans_block(
        descriptor, payload, {}, output);
    EXPECT_EQ(decoded.error, TansDecodeError::none);
    EXPECT_EQ(output, input);
}

TEST(TansDecoder, RejectsStateAndTerminalMismatchAtomically) {
    constexpr std::array input{
        std::byte{0x41}, std::byte{0x42}, std::byte{0x41}};
    std::vector<std::byte> payload;
    const auto descriptor = descriptor_for(input, payload);
    std::array<std::byte, input.size()> output{};
    output.fill(std::byte{0x5a});

    auto malformed = payload;
    malformed[0] = std::byte{0x00};
    malformed[1] = std::byte{0x10};
    auto result = marc::entropy::internal::decode_tans_block(
        descriptor, malformed, {}, output);
    EXPECT_EQ(result.error, TansDecodeError::invalid_state);
    EXPECT_TRUE(std::ranges::all_of(
        output, [](const std::byte b) { return b == std::byte{0x5a}; }));

    malformed = payload;
    malformed[0] ^= std::byte{0x01};
    result = marc::entropy::internal::decode_tans_block(
        descriptor, malformed, {}, output);
    EXPECT_NE(result.error, TansDecodeError::none);
    EXPECT_TRUE(std::ranges::all_of(
        output, [](const std::byte b) { return b == std::byte{0x5a}; }));
}

TEST(TansDecoder, RejectsBitExtentAndPaddingAtomically) {
    constexpr std::array input{std::byte{0x41}, std::byte{0x42}};
    std::vector<std::byte> payload;
    auto descriptor = descriptor_for(input, payload);
    std::array<std::byte, input.size()> output{};
    output.fill(std::byte{0x5a});

    auto altered = descriptor;
    altered.final_valid_bits = 1;
    auto result = marc::entropy::internal::decode_tans_block(
        altered, payload, {}, output);
    EXPECT_EQ(result.error, TansDecodeError::truncated_bits);

    altered.final_valid_bits = 3;
    result = marc::entropy::internal::decode_tans_block(
        altered, payload, {}, output);
    EXPECT_EQ(result.error, TansDecodeError::trailing_bits);

    altered = descriptor;
    auto padded = payload;
    padded.back() |= std::byte{0x80};
    result = marc::entropy::internal::decode_tans_block(
        altered, padded, {}, output);
    EXPECT_EQ(result.error, TansDecodeError::nonzero_padding);
    EXPECT_TRUE(std::ranges::all_of(
        output, [](const std::byte b) { return b == std::byte{0x5a}; }));
}

TEST(TansDecoder, ChecksPayloadOutputAndPolicyBeforeWriting) {
    constexpr std::array input{std::byte{0x41}, std::byte{0x42}};
    std::vector<std::byte> payload;
    const auto descriptor = descriptor_for(input, payload);
    std::array<std::byte, 1> short_output{std::byte{0x5a}};
    EXPECT_EQ(marc::entropy::internal::decode_tans_block(
                  descriptor, payload, {}, short_output).error,
              TansDecodeError::output_too_small);
    EXPECT_EQ(short_output[0], std::byte{0x5a});

    EXPECT_EQ(marc::entropy::internal::validate_tans_block(
                  descriptor,
                  std::span<const std::byte>{payload}.first(payload.size() - 1),
                  {}).error,
              TansDecodeError::payload_size_mismatch);
    marc::core::DecoderLimits limits{};
    limits.max_entropy_table_entries = 4095;
    EXPECT_EQ(marc::entropy::internal::validate_tans_block(
                  descriptor, payload, limits).error,
              TansDecodeError::invalid_descriptor);
}

} // namespace
