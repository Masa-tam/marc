#include "entropy/lzss_short_match_range_decoder.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using marc::entropy::internal::ContextualDynamicRangeDecodeError;
using marc::entropy::internal::LzssShortMatchRangeDecoder;

constexpr std::array hand_payload{
    std::byte{0x00}, std::byte{0x30}, std::byte{0xbf}, std::byte{0xff},
    std::byte{0x9e}, std::byte{0x80}, std::byte{0x00}};

} // namespace

TEST(LzssShortMatchRangeDecoder, DecodesHandCheckedFiveEvents) {
    LzssShortMatchRangeDecoder decoder;
    const auto limits = marc::core::DecoderLimits{};
    const marc::entropy::internal::ContextualDynamicRangeDescriptor
        descriptor{5, 7, 32};
    ASSERT_EQ(decoder.begin(descriptor, hand_payload, limits).error,
              ContextualDynamicRangeDecodeError::none);
    constexpr std::array<std::uint16_t, 5> contexts{0, 3, 1, 21, 23};
    constexpr std::array<std::uint16_t, 5> alphabets{2, 256, 2, 9, 17};
    constexpr std::array<std::uint32_t, 5> expected{0, 97, 1, 0, 0};
    for (std::size_t index = 0; index < contexts.size(); ++index) {
        std::uint32_t value = UINT32_MAX;
        const auto result = decoder.decode_symbol(contexts[index],
                                                  alphabets[index], value);
        ASSERT_EQ(result.error, ContextualDynamicRangeDecodeError::none)
            << index;
        EXPECT_EQ(value, expected[index]);
    }
    const auto finished = decoder.finish(5, 5);
    EXPECT_EQ(finished.error, ContextualDynamicRangeDecodeError::none);
    EXPECT_EQ(finished.event_count, 5U);
    EXPECT_EQ(finished.decision_count, 5U);
    EXPECT_EQ(finished.payload_consumed, hand_payload.size());
}

TEST(LzssShortMatchRangeDecoder, RejectsWrongDescriptorAndInitialState) {
    LzssShortMatchRangeDecoder decoder;
    const auto limits = marc::core::DecoderLimits{};
    auto descriptor = marc::entropy::internal::ContextualDynamicRangeDescriptor{
        5, 7, 31};
    EXPECT_EQ(decoder.begin(descriptor, hand_payload, limits).error,
              ContextualDynamicRangeDecodeError::invalid_descriptor);
    descriptor.context_count = 32;
    auto bad = hand_payload;
    bad[0] = std::byte{1};
    EXPECT_EQ(decoder.begin(descriptor, bad, limits).error,
              ContextualDynamicRangeDecodeError::invalid_interval);
    EXPECT_EQ(decoder.begin(descriptor,
                            std::span<const std::byte>{hand_payload}.first(6),
                            limits).error,
              ContextualDynamicRangeDecodeError::payload_size_mismatch);
}

TEST(LzssShortMatchRangeDecoder, ChecksContextAlphabetAndExactCounts) {
    const auto limits = marc::core::DecoderLimits{};
    const marc::entropy::internal::ContextualDynamicRangeDescriptor
        descriptor{5, 7, 32};
    LzssShortMatchRangeDecoder decoder;
    ASSERT_EQ(decoder.begin(descriptor, hand_payload, limits).error,
              ContextualDynamicRangeDecodeError::none);
    std::uint32_t value = 777;
    EXPECT_EQ(decoder.decode_symbol(32, 2, value).error,
              ContextualDynamicRangeDecodeError::invalid_context);
    EXPECT_EQ(value, 777U);
    ASSERT_EQ(decoder.begin(descriptor, hand_payload, limits).error,
              ContextualDynamicRangeDecodeError::none);
    EXPECT_EQ(decoder.decode_symbol(21, 8, value).error,
              ContextualDynamicRangeDecodeError::invalid_alphabet);
    ASSERT_EQ(decoder.begin(descriptor, hand_payload, limits).error,
              ContextualDynamicRangeDecodeError::none);
    EXPECT_EQ(decoder.finish(5, 5).error,
              ContextualDynamicRangeDecodeError::count_mismatch);
}

TEST(LzssShortMatchRangeDecoder, AcceptsNewLastContextAndResetsModels) {
    constexpr std::array<std::byte, 5> zero_payload{};
    const auto limits = marc::core::DecoderLimits{};
    const marc::entropy::internal::ContextualDynamicRangeDescriptor
        descriptor{1, 5, 32};
    LzssShortMatchRangeDecoder decoder;
    for (int iteration = 0; iteration < 2; ++iteration) {
        ASSERT_EQ(decoder.begin(descriptor, zero_payload, limits).error,
                  ContextualDynamicRangeDecodeError::none);
        std::uint32_t value = UINT32_MAX;
        ASSERT_EQ(decoder.decode_symbol(31, 17, value).error,
                  ContextualDynamicRangeDecodeError::none);
        EXPECT_EQ(value, 0U);
        EXPECT_EQ(decoder.finish(1, 1).error,
                  ContextualDynamicRangeDecodeError::none);
    }
}
