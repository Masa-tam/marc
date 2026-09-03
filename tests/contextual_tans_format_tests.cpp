#include "context/lzss_field_context_format.hpp"
#include "entropy/contextual_tans_format.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <vector>

namespace {

using marc::entropy::internal::ContextualTansDescriptor;
using marc::entropy::internal::ContextualTansFormatError;
using marc::entropy::internal::contextual_tans_decode_table_entries;
using marc::entropy::internal::contextual_tans_descriptor_capacity;
using marc::entropy::internal::contextual_tans_max_descriptor_size;
using marc::entropy::internal::contextual_tans_max_descriptor_size_v2;
using marc::entropy::internal::contextual_tans_max_descriptor_size_v3;
using marc::entropy::internal::contextual_tans_max_descriptor_size_v4;
using marc::entropy::internal::contextual_tans_max_descriptor_size_v5;
using marc::context::internal::LzssFieldContextVariant;

[[nodiscard]] ContextualTansDescriptor literal_a_descriptor() {
    ContextualTansDescriptor descriptor{};
    descriptor.decision_count = 2;
    descriptor.payload_size = 2;
    descriptor.frequencies[0] = 4096;
    descriptor.frequencies[71] = 4096;
    return descriptor;
}

[[nodiscard]] bool descriptors_equal(
    const ContextualTansDescriptor& left,
    const ContextualTansDescriptor& right) {
    return left.decision_count == right.decision_count
        && left.payload_size == right.payload_size
        && left.table_log == right.table_log
        && left.final_valid_bits == right.final_valid_bits
        && left.flags == right.flags
        && left.context_count == right.context_count
        && left.frequency_entry_count == right.frequency_entry_count
        && left.frequencies == right.frequencies;
}

[[nodiscard]] std::vector<std::byte> serialize(
    const ContextualTansDescriptor& descriptor,
    const LzssFieldContextVariant variant =
        LzssFieldContextVariant::field_context_64k) {
    std::array<std::byte, contextual_tans_descriptor_capacity> storage{};
    std::size_t written{};
    EXPECT_EQ(
        marc::entropy::internal::serialize_contextual_tans_descriptor(
            descriptor, descriptor.decision_count, descriptor.payload_size,
            {}, storage, written, variant),
        ContextualTansFormatError::none);
    return {storage.begin(), storage.begin() + written};
}

[[nodiscard]] ContextualTansDescriptor extended_distance_descriptor() {
    ContextualTansDescriptor descriptor{};
    descriptor.decision_count = 1;
    descriptor.payload_size = 2;
    descriptor.frequency_entry_count = static_cast<std::uint32_t>(
        marc::context::internal::lzss_field_context_frequency_entries_v2);
    const auto offset =
        marc::context::internal::lzss_field_context_offsets_v2[23];
    descriptor.frequencies[offset + 20] = 4096;
    return descriptor;
}

[[nodiscard]] ContextualTansDescriptor four_mib_distance_descriptor() {
    ContextualTansDescriptor descriptor{};
    descriptor.decision_count = 1;
    descriptor.payload_size = 2;
    descriptor.frequency_entry_count = static_cast<std::uint32_t>(
        marc::context::internal::lzss_field_context_frequency_entries_v3);
    const auto offset =
        marc::context::internal::lzss_field_context_offsets_v3[23];
    descriptor.frequencies[offset + 22] = 4096;
    return descriptor;
}

[[nodiscard]] ContextualTansDescriptor sixteen_mib_distance_descriptor() {
    ContextualTansDescriptor descriptor{};
    descriptor.decision_count = 1;
    descriptor.payload_size = 2;
    descriptor.frequency_entry_count = static_cast<std::uint32_t>(
        marc::context::internal::lzss_field_context_frequency_entries_v4);
    const auto offset =
        marc::context::internal::lzss_field_context_offsets_v4[23];
    descriptor.frequencies[offset + 24] = 4096;
    return descriptor;
}

[[nodiscard]] ContextualTansDescriptor context_twenty_descriptor(
    const std::span<const std::uint16_t> frequencies) {
    ContextualTansDescriptor descriptor{};
    descriptor.decision_count = 5;
    descriptor.payload_size = 2;
    const auto offset =
        marc::context::internal::lzss_field_context_offsets[20];
    std::copy(frequencies.begin(), frequencies.end(),
              descriptor.frequencies.begin() + offset);
    return descriptor;
}

TEST(ContextualTansFormat, SerializesAndParsesOneLiteralVector) {
    constexpr std::array expected{
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x0c}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x1f}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0xa6}, std::byte{0x11}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x09}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x10}, std::byte{0x01},
        std::byte{0x00}, std::byte{0x41}};
    const auto descriptor = literal_a_descriptor();
    const auto bytes = serialize(descriptor);
    EXPECT_TRUE(std::ranges::equal(bytes, expected));

    ContextualTansDescriptor parsed{};
    ASSERT_EQ(marc::entropy::internal::parse_contextual_tans_descriptor(
                  bytes, 2, 2, {}, parsed),
              ContextualTansFormatError::none);
    EXPECT_TRUE(descriptors_equal(parsed, descriptor));
}

TEST(ContextualTansFormat, SelectsSparseStrictlyAndDenseOnTie) {
    constexpr std::array<std::uint16_t, 4> four{
        1024, 1024, 1024, 1024};
    const auto sparse = serialize(context_twenty_descriptor(four));
    ASSERT_EQ(sparse.size(), 36U);
    EXPECT_EQ(sparse[24], std::byte{0x01});

    constexpr std::array<std::uint16_t, 5> five{
        819, 819, 819, 819, 820};
    const auto dense = serialize(context_twenty_descriptor(five));
    ASSERT_EQ(dense.size(), 39U);
    EXPECT_EQ(dense[24], std::byte{0x00});

    ContextualTansDescriptor parsed{};
    ASSERT_EQ(marc::entropy::internal::parse_contextual_tans_descriptor(
                  dense, 5, 2, {}, parsed),
              ContextualTansFormatError::none);
    EXPECT_TRUE(descriptors_equal(parsed, context_twenty_descriptor(five)));
}

TEST(ContextualTansFormat, EveryDenseModelReachesExactMaximum) {
    ContextualTansDescriptor descriptor{};
    descriptor.decision_count = 1;
    descriptor.payload_size = 2;
    for (std::size_t context_id = 0;
         context_id < marc::context::internal::lzss_field_context_count;
         ++context_id) {
        const auto begin =
            marc::context::internal::lzss_field_context_offsets[context_id];
        const auto alphabet =
            marc::context::internal::lzss_field_context_alphabets[context_id];
        for (std::uint16_t symbol = 0; symbol + 1 < alphabet; ++symbol) {
            descriptor.frequencies[begin + symbol] = 1;
        }
        descriptor.frequencies[begin + alphabet - 1] =
            static_cast<std::uint16_t>(4096 - (alphabet - 1));
    }

    const auto bytes = serialize(descriptor);
    ASSERT_EQ(bytes.size(), contextual_tans_max_descriptor_size);
    ContextualTansDescriptor parsed{};
    ASSERT_EQ(marc::entropy::internal::parse_contextual_tans_descriptor(
                  bytes, 1, 2, {}, parsed),
              ContextualTansFormatError::none);
    EXPECT_TRUE(descriptors_equal(parsed, descriptor));
}

TEST(ContextualTansFormat, SerializesAndParsesSelectedExtendedLayout) {
    const auto descriptor = extended_distance_descriptor();
    const auto bytes = serialize(
        descriptor, LzssFieldContextVariant::field_context_1m);
    ASSERT_EQ(bytes.size(), 27U);
    EXPECT_EQ(bytes[24], std::byte{0x01});
    EXPECT_EQ(bytes[25], std::byte{0x00});
    EXPECT_EQ(bytes[26], std::byte{0x14});

    ContextualTansDescriptor parsed{};
    ASSERT_EQ(marc::entropy::internal::parse_contextual_tans_descriptor(
                  bytes, 1, 2, {}, parsed,
                  LzssFieldContextVariant::field_context_1m),
              ContextualTansFormatError::none);
    EXPECT_TRUE(descriptors_equal(parsed, descriptor));
}

TEST(ContextualTansFormat, SelectedExtendedDenseModelsReachExactMaximum) {
    ContextualTansDescriptor descriptor{};
    descriptor.decision_count = 1;
    descriptor.payload_size = 2;
    descriptor.frequency_entry_count = static_cast<std::uint32_t>(
        marc::context::internal::lzss_field_context_frequency_entries_v2);
    for (std::size_t context_id = 0;
         context_id < marc::context::internal::lzss_field_context_count;
         ++context_id) {
        const auto begin =
            marc::context::internal::lzss_field_context_offsets_v2[context_id];
        const auto alphabet =
            marc::context::internal::lzss_field_context_alphabets_v2[context_id];
        for (std::uint16_t symbol = 0; symbol + 1 < alphabet; ++symbol) {
            descriptor.frequencies[begin + symbol] = 1;
        }
        descriptor.frequencies[begin + alphabet - 1] =
            static_cast<std::uint16_t>(4096 - (alphabet - 1));
    }

    const auto bytes = serialize(
        descriptor, LzssFieldContextVariant::field_context_1m);
    ASSERT_EQ(bytes.size(), contextual_tans_max_descriptor_size_v2);
    ContextualTansDescriptor parsed{};
    ASSERT_EQ(marc::entropy::internal::parse_contextual_tans_descriptor(
                  bytes, 1, 2, {}, parsed,
                  LzssFieldContextVariant::field_context_1m),
              ContextualTansFormatError::none);
    EXPECT_TRUE(descriptors_equal(parsed, descriptor));
}

TEST(ContextualTansFormat, SerializesAndParsesFourMibLayout) {
    const auto descriptor = four_mib_distance_descriptor();
    const auto bytes = serialize(
        descriptor, LzssFieldContextVariant::field_context_4m);
    ASSERT_EQ(bytes.size(), 27U);
    EXPECT_EQ(bytes[16], std::byte{0xd6});
    EXPECT_EQ(bytes[17], std::byte{0x11});
    EXPECT_EQ(bytes[24], std::byte{0x01});
    EXPECT_EQ(bytes[25], std::byte{0x00});
    EXPECT_EQ(bytes[26], std::byte{0x16});

    ContextualTansDescriptor parsed{};
    ASSERT_EQ(marc::entropy::internal::parse_contextual_tans_descriptor(
                  bytes, 1, 2, {}, parsed,
                  LzssFieldContextVariant::field_context_4m),
              ContextualTansFormatError::none);
    EXPECT_TRUE(descriptors_equal(parsed, descriptor));
}

TEST(ContextualTansFormat, FourMibDenseModelsReachExactMaximum) {
    ContextualTansDescriptor descriptor{};
    descriptor.decision_count = 1;
    descriptor.payload_size = 2;
    descriptor.frequency_entry_count = static_cast<std::uint32_t>(
        marc::context::internal::lzss_field_context_frequency_entries_v3);
    for (std::size_t context_id = 0;
         context_id < marc::context::internal::lzss_field_context_count;
         ++context_id) {
        const auto begin =
            marc::context::internal::lzss_field_context_offsets_v3[context_id];
        const auto alphabet =
            marc::context::internal::lzss_field_context_alphabets_v3[context_id];
        for (std::uint16_t symbol = 0; symbol + 1 < alphabet; ++symbol) {
            descriptor.frequencies[begin + symbol] = 1;
        }
        descriptor.frequencies[begin + alphabet - 1] =
            static_cast<std::uint16_t>(4096 - (alphabet - 1));
    }

    const auto bytes = serialize(
        descriptor, LzssFieldContextVariant::field_context_4m);
    ASSERT_EQ(bytes.size(), contextual_tans_max_descriptor_size_v3);
    ContextualTansDescriptor parsed{};
    ASSERT_EQ(marc::entropy::internal::parse_contextual_tans_descriptor(
                  bytes, 1, 2, {}, parsed,
                  LzssFieldContextVariant::field_context_4m),
              ContextualTansFormatError::none);
    EXPECT_TRUE(descriptors_equal(parsed, descriptor));
    EXPECT_LT(contextual_tans_max_descriptor_size_v3,
              contextual_tans_descriptor_capacity);
}

TEST(ContextualTansFormat, SixteenMiBLayoutSelectsCountAndCanonicalGrammar) {
    const auto frozen = serialize(literal_a_descriptor());
    auto descriptor = literal_a_descriptor();
    descriptor.frequency_entry_count =
        marc::context::internal::lzss_field_context_frequency_entries_v4;
    constexpr auto variant = LzssFieldContextVariant::field_context_16m;
    const auto selected = serialize(descriptor, variant);
    auto expected = frozen;
    expected[16] = std::byte{0xe6};
    EXPECT_EQ(selected, expected);

    ContextualTansDescriptor parsed{};
    ASSERT_EQ(marc::entropy::internal::parse_contextual_tans_descriptor(
                  selected, 2, 2, {}, parsed, variant),
              ContextualTansFormatError::none);
    EXPECT_TRUE(descriptors_equal(parsed, descriptor));

    auto sentinel = literal_a_descriptor();
    sentinel.flags = 0xa5;
    const auto before = sentinel;
    EXPECT_EQ(marc::entropy::internal::parse_contextual_tans_descriptor(
                  selected, 2, 2, {}, sentinel,
                  LzssFieldContextVariant::field_context_4m),
              ContextualTansFormatError::invalid_frequency_entry_count);
    EXPECT_TRUE(descriptors_equal(sentinel, before));
}

TEST(ContextualTansFormat, SixteenMiBEveryDenseModelReachesExactMaximum) {
    ContextualTansDescriptor descriptor{};
    descriptor.decision_count = 1;
    descriptor.payload_size = 2;
    descriptor.frequency_entry_count =
        marc::context::internal::lzss_field_context_frequency_entries_v4;
    constexpr auto variant = LzssFieldContextVariant::field_context_16m;
    const auto selected = marc::context::internal::
        get_lzss_field_context_layout(variant);
    ASSERT_EQ(selected.error,
              marc::context::internal::LzssFieldContextLayoutError::none);
    for (std::size_t context = 0;
         context < marc::context::internal::lzss_field_context_count;
         ++context) {
        const auto begin = (*selected.layout.offsets)[context];
        const auto alphabet = (*selected.layout.alphabets)[context];
        for (std::uint16_t symbol = 0; symbol + 1 < alphabet; ++symbol) {
            descriptor.frequencies[begin + symbol] = 1;
        }
        descriptor.frequencies[begin + alphabet - 1] =
            static_cast<std::uint16_t>(4096 - (alphabet - 1));
    }

    const auto bytes = serialize(descriptor, variant);
    ASSERT_EQ(bytes.size(), contextual_tans_max_descriptor_size_v4);
    EXPECT_LT(bytes.size(), contextual_tans_descriptor_capacity);
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 1;
    limits.max_internal_buffered_bytes = bytes.size() + 2;
    ContextualTansDescriptor parsed{};
    ASSERT_EQ(marc::entropy::internal::parse_contextual_tans_descriptor(
                  bytes, 1, 2, limits, parsed, variant),
              ContextualTansFormatError::none);
    EXPECT_TRUE(descriptors_equal(parsed, descriptor));

    limits.max_internal_buffered_bytes = bytes.size() + 1;
    auto sentinel = literal_a_descriptor();
    sentinel.flags = 0xa5;
    const auto before = sentinel;
    EXPECT_EQ(marc::entropy::internal::parse_contextual_tans_descriptor(
                  bytes, 1, 2, limits, sentinel, variant),
              ContextualTansFormatError::limit_exceeded);
    EXPECT_TRUE(descriptors_equal(sentinel, before));
}

TEST(ContextualTansFormat, SixtyFourMiBLayoutSelectsCountAndCanonicalGrammar) {
    const auto frozen = serialize(literal_a_descriptor());
    auto descriptor = literal_a_descriptor();
    descriptor.frequency_entry_count =
        marc::context::internal::lzss_field_context_frequency_entries_v5;
    constexpr auto variant = LzssFieldContextVariant::field_context_64m;
    const auto selected = serialize(descriptor, variant);
    auto expected = frozen;
    expected[16] = std::byte{0xf6};
    EXPECT_EQ(selected, expected);

    ContextualTansDescriptor parsed{};
    ASSERT_EQ(marc::entropy::internal::parse_contextual_tans_descriptor(
                  selected, 2, 2, {}, parsed, variant),
              ContextualTansFormatError::none);
    EXPECT_TRUE(descriptors_equal(parsed, descriptor));

    auto sentinel = literal_a_descriptor();
    sentinel.flags = 0xa5;
    const auto before = sentinel;
    EXPECT_EQ(marc::entropy::internal::parse_contextual_tans_descriptor(
                  selected, 2, 2, {}, sentinel,
                  LzssFieldContextVariant::field_context_16m),
              ContextualTansFormatError::invalid_frequency_entry_count);
    EXPECT_TRUE(descriptors_equal(sentinel, before));
}

TEST(ContextualTansFormat, SixtyFourMiBEveryDenseModelReachesExactMaximum) {
    ContextualTansDescriptor descriptor{};
    descriptor.decision_count = 1;
    descriptor.payload_size = 2;
    descriptor.frequency_entry_count =
        marc::context::internal::lzss_field_context_frequency_entries_v5;
    constexpr auto variant = LzssFieldContextVariant::field_context_64m;
    const auto selected = marc::context::internal::
        get_lzss_field_context_layout(variant);
    ASSERT_EQ(selected.error,
              marc::context::internal::LzssFieldContextLayoutError::none);
    for (std::size_t context = 0;
         context < marc::context::internal::lzss_field_context_count;
         ++context) {
        const auto begin = (*selected.layout.offsets)[context];
        const auto alphabet = (*selected.layout.alphabets)[context];
        for (std::uint16_t symbol = 0; symbol + 1 < alphabet; ++symbol) {
            descriptor.frequencies[begin + symbol] = 1;
        }
        descriptor.frequencies[begin + alphabet - 1] =
            static_cast<std::uint16_t>(4096 - (alphabet - 1));
    }

    const auto bytes = serialize(descriptor, variant);
    ASSERT_EQ(bytes.size(), contextual_tans_max_descriptor_size_v5);
    EXPECT_EQ(contextual_tans_descriptor_capacity, bytes.size());
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = 1;
    limits.max_internal_buffered_bytes = bytes.size() + 2;
    ContextualTansDescriptor parsed{};
    ASSERT_EQ(marc::entropy::internal::parse_contextual_tans_descriptor(
                  bytes, 1, 2, limits, parsed, variant),
              ContextualTansFormatError::none);
    EXPECT_TRUE(descriptors_equal(parsed, descriptor));

    limits.max_internal_buffered_bytes = bytes.size() + 1;
    auto sentinel = literal_a_descriptor();
    sentinel.flags = 0xa5;
    const auto before = sentinel;
    EXPECT_EQ(marc::entropy::internal::parse_contextual_tans_descriptor(
                  bytes, 1, 2, limits, sentinel, variant),
              ContextualTansFormatError::limit_exceeded);
    EXPECT_TRUE(descriptors_equal(sentinel, before));
}

TEST(ContextualTansFormat, RejectsCrossedAndUnknownSelectedLayouts) {
    std::size_t size = 0xa5a5;
    const auto frozen = literal_a_descriptor();
    const auto extended = extended_distance_descriptor();
    EXPECT_EQ(marc::entropy::internal::validate_contextual_tans_descriptor(
                  frozen, 2, 2, {}, size,
                  LzssFieldContextVariant::field_context_1m),
              ContextualTansFormatError::invalid_frequency_entry_count);
    EXPECT_EQ(size, 0xa5a5U);
    const auto four_mib = four_mib_distance_descriptor();
    const auto sixteen_mib = sixteen_mib_distance_descriptor();
    EXPECT_EQ(marc::entropy::internal::validate_contextual_tans_descriptor(
                  four_mib, 1, 2, {}, size,
                  LzssFieldContextVariant::field_context_1m),
              ContextualTansFormatError::invalid_frequency_entry_count);
    EXPECT_EQ(size, 0xa5a5U);
    EXPECT_EQ(marc::entropy::internal::validate_contextual_tans_descriptor(
                  sixteen_mib, 1, 2, {}, size,
                  LzssFieldContextVariant::field_context_4m),
              ContextualTansFormatError::invalid_frequency_entry_count);
    EXPECT_EQ(size, 0xa5a5U);
    EXPECT_EQ(marc::entropy::internal::validate_contextual_tans_descriptor(
                  four_mib, 1, 2, {}, size,
                  LzssFieldContextVariant::field_context_16m),
              ContextualTansFormatError::invalid_frequency_entry_count);
    EXPECT_EQ(size, 0xa5a5U);
    EXPECT_EQ(marc::entropy::internal::validate_contextual_tans_descriptor(
                  extended, 1, 2, {}, size,
                  LzssFieldContextVariant::field_context_4m),
              ContextualTansFormatError::invalid_frequency_entry_count);
    EXPECT_EQ(size, 0xa5a5U);
    EXPECT_EQ(marc::entropy::internal::validate_contextual_tans_descriptor(
                  extended, 1, 2, {}, size,
                  LzssFieldContextVariant::field_context_64k),
              ContextualTansFormatError::invalid_frequency_entry_count);
    EXPECT_EQ(size, 0xa5a5U);
    EXPECT_EQ(marc::entropy::internal::validate_contextual_tans_descriptor(
                  frozen, 2, 2, {}, size,
                  static_cast<LzssFieldContextVariant>(0xff)),
              ContextualTansFormatError::unsupported_context_variant);
    EXPECT_EQ(size, 0xa5a5U);
    auto nonzero_tail = frozen;
    nonzero_tail.frequencies[
        marc::context::internal::lzss_field_context_frequency_entries_v1] = 1;
    EXPECT_EQ(marc::entropy::internal::validate_contextual_tans_descriptor(
                  nonzero_tail, 2, 2, {}, size),
              ContextualTansFormatError::invalid_frequency_table);
    EXPECT_EQ(size, 0xa5a5U);
}

TEST(ContextualTansFormat, FourMibLayoutFailuresAreAtomic) {
    const auto descriptor = four_mib_distance_descriptor();
    const auto valid = serialize(
        descriptor, LzssFieldContextVariant::field_context_4m);
    for (std::size_t extent = 0; extent < valid.size(); ++extent) {
        auto sentinel = literal_a_descriptor();
        sentinel.flags = 0xa5;
        const auto before = sentinel;
        EXPECT_NE(marc::entropy::internal::parse_contextual_tans_descriptor(
                      std::span<const std::byte>{valid}.first(extent), 1, 2,
                      {}, sentinel,
                      LzssFieldContextVariant::field_context_4m),
                  ContextualTansFormatError::none)
            << extent;
        EXPECT_TRUE(descriptors_equal(sentinel, before)) << extent;
    }

    auto trailing = valid;
    trailing.push_back(std::byte{});
    auto sentinel = literal_a_descriptor();
    sentinel.flags = 0xa5;
    const auto before = sentinel;
    EXPECT_EQ(marc::entropy::internal::parse_contextual_tans_descriptor(
                  trailing, 1, 2, {}, sentinel,
                  LzssFieldContextVariant::field_context_4m),
              ContextualTansFormatError::trailing_data);
    EXPECT_TRUE(descriptors_equal(sentinel, before));

    auto malformed = valid;
    malformed[24] = std::byte{0x02};
    EXPECT_EQ(marc::entropy::internal::parse_contextual_tans_descriptor(
                  malformed, 1, 2, {}, sentinel,
                  LzssFieldContextVariant::field_context_4m),
              ContextualTansFormatError::invalid_mode);
    EXPECT_TRUE(descriptors_equal(sentinel, before));

    std::array<std::byte, 26> output{};
    std::ranges::fill(output, std::byte{0xa5});
    std::size_t written = 0xa5a5;
    EXPECT_EQ(marc::entropy::internal::serialize_contextual_tans_descriptor(
                  descriptor, 1, 2, {}, output, written,
                  LzssFieldContextVariant::field_context_4m),
              ContextualTansFormatError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        output, [](const auto value) { return value == std::byte{0xa5}; }));
    EXPECT_EQ(written, 0xa5a5U);
}

TEST(ContextualTansFormat, ExtendedLayoutFailuresAreAtomic) {
    const auto descriptor = extended_distance_descriptor();
    const auto valid = serialize(
        descriptor, LzssFieldContextVariant::field_context_1m);
    for (std::size_t extent = 0; extent < valid.size(); ++extent) {
        auto sentinel = literal_a_descriptor();
        sentinel.flags = 0xa5;
        const auto before = sentinel;
        EXPECT_NE(marc::entropy::internal::parse_contextual_tans_descriptor(
                      std::span<const std::byte>{valid}.first(extent), 1, 2,
                      {}, sentinel,
                      LzssFieldContextVariant::field_context_1m),
                  ContextualTansFormatError::none)
            << extent;
        EXPECT_TRUE(descriptors_equal(sentinel, before)) << extent;
    }

    auto trailing = valid;
    trailing.push_back(std::byte{});
    auto sentinel = literal_a_descriptor();
    sentinel.flags = 0xa5;
    const auto before = sentinel;
    EXPECT_EQ(marc::entropy::internal::parse_contextual_tans_descriptor(
                  trailing, 1, 2, {}, sentinel,
                  LzssFieldContextVariant::field_context_1m),
              ContextualTansFormatError::trailing_data);
    EXPECT_TRUE(descriptors_equal(sentinel, before));

    auto malformed = valid;
    malformed[23] |= std::byte{0x80};
    EXPECT_EQ(marc::entropy::internal::parse_contextual_tans_descriptor(
                  malformed, 1, 2, {}, sentinel,
                  LzssFieldContextVariant::field_context_1m),
              ContextualTansFormatError::invalid_active_context_mask);
    EXPECT_TRUE(descriptors_equal(sentinel, before));
    malformed = valid;
    malformed[24] = std::byte{0x02};
    EXPECT_EQ(marc::entropy::internal::parse_contextual_tans_descriptor(
                  malformed, 1, 2, {}, sentinel,
                  LzssFieldContextVariant::field_context_1m),
              ContextualTansFormatError::invalid_mode);
    EXPECT_TRUE(descriptors_equal(sentinel, before));

    std::array<std::byte, 26> output{};
    std::ranges::fill(output, std::byte{0xa5});
    std::size_t written = 0xa5a5;
    EXPECT_EQ(marc::entropy::internal::serialize_contextual_tans_descriptor(
                  descriptor, 1, 2, {}, output, written,
                  LzssFieldContextVariant::field_context_1m),
              ContextualTansFormatError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        output, [](const auto value) { return value == std::byte{0xa5}; }));
    EXPECT_EQ(written, 0xa5a5U);
}

TEST(ContextualTansFormat, RejectsEveryStrictPrefixAndTrailingData) {
    const auto valid = serialize(literal_a_descriptor());
    for (std::size_t extent = 0; extent < valid.size(); ++extent) {
        auto sentinel = literal_a_descriptor();
        sentinel.flags = 0xa5;
        const auto before = sentinel;
        EXPECT_NE(
            marc::entropy::internal::parse_contextual_tans_descriptor(
                std::span<const std::byte>{valid}.first(extent), 2, 2, {},
                sentinel),
            ContextualTansFormatError::none) << extent;
        EXPECT_TRUE(descriptors_equal(sentinel, before)) << extent;
    }

    auto trailing = valid;
    trailing.push_back(std::byte{});
    auto sentinel = literal_a_descriptor();
    sentinel.flags = 0xa5;
    const auto before = sentinel;
    EXPECT_EQ(marc::entropy::internal::parse_contextual_tans_descriptor(
                  trailing, 2, 2, {}, sentinel),
              ContextualTansFormatError::trailing_data);
    EXPECT_TRUE(descriptors_equal(sentinel, before));
}

TEST(ContextualTansFormat, RejectsMasksModesAndNoncanonicalRecords) {
    const auto valid = serialize(literal_a_descriptor());
    auto expect_error = [&](std::vector<std::byte> bytes,
                            const ContextualTansFormatError expected) {
        auto sentinel = literal_a_descriptor();
        sentinel.flags = 0xa5;
        const auto before = sentinel;
        EXPECT_EQ(marc::entropy::internal::parse_contextual_tans_descriptor(
                      bytes, 2, 2, {}, sentinel),
                  expected);
        EXPECT_TRUE(descriptors_equal(sentinel, before));
    };

    auto malformed = valid;
    malformed[23] |= std::byte{0x80};
    expect_error(malformed,
                 ContextualTansFormatError::invalid_active_context_mask);
    malformed = valid;
    std::fill(malformed.begin() + 20, malformed.begin() + 24, std::byte{});
    expect_error(malformed,
                 ContextualTansFormatError::invalid_active_context_mask);
    malformed = valid;
    malformed[20] = std::byte{0x08};
    expect_error(malformed,
                 ContextualTansFormatError::invalid_frequency_table);
    malformed = valid;
    malformed[20] = std::byte{0x01};
    expect_error(malformed, ContextualTansFormatError::trailing_data);
    malformed = valid;
    malformed[24] = std::byte{0x02};
    expect_error(malformed, ContextualTansFormatError::invalid_mode);
    malformed = valid;
    malformed[25] = std::byte{0x01};
    malformed[26] = std::byte{0x10};
    expect_error(malformed,
                 ContextualTansFormatError::invalid_frequency_table);
    malformed = valid;
    malformed[24] = std::byte{0x01};
    malformed[25] = std::byte{0x00};
    malformed[26] = std::byte{0x00};
    expect_error(malformed,
                 ContextualTansFormatError::noncanonical_representation);
}

TEST(ContextualTansFormat, RejectsMalformedSparseModels) {
    constexpr std::array<std::uint16_t, 2> two{2048, 2048};
    const auto valid = serialize(context_twenty_descriptor(two));
    ASSERT_EQ(valid.size(), 30U);
    ASSERT_EQ(valid[24], std::byte{0x01});

    auto expect_invalid = [&](std::vector<std::byte> bytes) {
        auto sentinel = literal_a_descriptor();
        const auto before = sentinel;
        EXPECT_EQ(marc::entropy::internal::parse_contextual_tans_descriptor(
                      bytes, 5, 2, {}, sentinel),
                  ContextualTansFormatError::invalid_frequency_table);
        EXPECT_TRUE(descriptors_equal(sentinel, before));
    };
    auto malformed = valid;
    malformed[29] = malformed[26];
    expect_invalid(malformed);
    malformed = valid;
    malformed[26] = std::byte{0x01};
    malformed[29] = std::byte{0x00};
    expect_invalid(malformed);
    malformed = valid;
    malformed[27] = std::byte{};
    malformed[28] = std::byte{};
    expect_invalid(malformed);
    malformed = valid;
    malformed[27] = std::byte{0x00};
    malformed[28] = std::byte{0x10};
    expect_invalid(malformed);
}

TEST(ContextualTansFormat, EnforcesFieldsLimitsAndAtomicOutput) {
    auto descriptor = literal_a_descriptor();
    std::size_t size = 0xa5a5;
    descriptor.decision_count = 0;
    EXPECT_EQ(
        marc::entropy::internal::validate_contextual_tans_descriptor(
            descriptor, 0, 2, {}, size),
        ContextualTansFormatError::invalid_decision_count);
    EXPECT_EQ(size, 0xa5a5U);

    descriptor = literal_a_descriptor();
    descriptor.payload_size = 1;
    EXPECT_EQ(
        marc::entropy::internal::validate_contextual_tans_descriptor(
            descriptor, 2, 1, {}, size),
        ContextualTansFormatError::invalid_payload_size);
    descriptor.payload_size = 6;
    descriptor.final_valid_bits = 1;
    EXPECT_EQ(
        marc::entropy::internal::validate_contextual_tans_descriptor(
            descriptor, 2, 6, {}, size),
        ContextualTansFormatError::invalid_payload_size);
    descriptor = literal_a_descriptor();
    descriptor.final_valid_bits = 1;
    EXPECT_EQ(
        marc::entropy::internal::validate_contextual_tans_descriptor(
            descriptor, 2, 2, {}, size),
        ContextualTansFormatError::invalid_valid_bits);
    descriptor.payload_size = 3;
    descriptor.final_valid_bits = 0;
    EXPECT_EQ(
        marc::entropy::internal::validate_contextual_tans_descriptor(
            descriptor, 2, 3, {}, size),
        ContextualTansFormatError::invalid_valid_bits);
    descriptor.final_valid_bits = 9;
    EXPECT_EQ(
        marc::entropy::internal::validate_contextual_tans_descriptor(
            descriptor, 2, 3, {}, size),
        ContextualTansFormatError::invalid_valid_bits);
    descriptor = literal_a_descriptor();
    descriptor.table_log = 11;
    EXPECT_EQ(
        marc::entropy::internal::validate_contextual_tans_descriptor(
            descriptor, 2, 2, {}, size),
        ContextualTansFormatError::invalid_table_log);
    descriptor = literal_a_descriptor();
    descriptor.context_count = 30;
    EXPECT_EQ(
        marc::entropy::internal::validate_contextual_tans_descriptor(
            descriptor, 2, 2, {}, size),
        ContextualTansFormatError::invalid_context_count);
    descriptor = literal_a_descriptor();
    --descriptor.frequency_entry_count;
    EXPECT_EQ(
        marc::entropy::internal::validate_contextual_tans_descriptor(
            descriptor, 2, 2, {}, size),
        ContextualTansFormatError::invalid_frequency_entry_count);
    descriptor = literal_a_descriptor();
    EXPECT_EQ(
        marc::entropy::internal::validate_contextual_tans_descriptor(
            descriptor, 3, 2, {}, size),
        ContextualTansFormatError::contradictory_size);
    descriptor.frequencies[0] = 4095;
    EXPECT_EQ(
        marc::entropy::internal::validate_contextual_tans_descriptor(
            descriptor, 2, 2, {}, size),
        ContextualTansFormatError::invalid_frequency_table);

    descriptor = literal_a_descriptor();
    marc::core::DecoderLimits limits{};
    limits.max_block_size = 1;
    EXPECT_EQ(
        marc::entropy::internal::validate_contextual_tans_descriptor(
            descriptor, 2, 2, limits, size),
        ContextualTansFormatError::limit_exceeded);
    limits = {};
    limits.max_compressed_payload_size = 1;
    EXPECT_EQ(
        marc::entropy::internal::validate_contextual_tans_descriptor(
            descriptor, 2, 2, limits, size),
        ContextualTansFormatError::limit_exceeded);
    limits = {};
    limits.max_entropy_table_entries =
        contextual_tans_decode_table_entries - 1;
    EXPECT_EQ(
        marc::entropy::internal::validate_contextual_tans_descriptor(
            descriptor, 2, 2, limits, size),
        ContextualTansFormatError::limit_exceeded);
    limits = {};
    limits.max_internal_buffered_bytes = 31;
    EXPECT_EQ(
        marc::entropy::internal::validate_contextual_tans_descriptor(
            descriptor, 2, 2, limits, size),
        ContextualTansFormatError::limit_exceeded);

    const auto valid = serialize(descriptor);
    auto reserved = valid;
    reserved[14] = std::byte{0x01};
    ContextualTansDescriptor sentinel = literal_a_descriptor();
    const auto before = sentinel;
    EXPECT_EQ(marc::entropy::internal::parse_contextual_tans_descriptor(
                  reserved, 2, 2, {}, sentinel),
              ContextualTansFormatError::nonzero_reserved);
    EXPECT_TRUE(descriptors_equal(sentinel, before));

    std::array<std::byte, 29> output{};
    std::ranges::fill(output, std::byte{0xa5});
    std::size_t written = 0xa5a5;
    EXPECT_EQ(
        marc::entropy::internal::serialize_contextual_tans_descriptor(
            descriptor, 2, 2, {}, output, written),
        ContextualTansFormatError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        output, [](const auto value) { return value == std::byte{0xa5}; }));
    EXPECT_EQ(written, 0xa5a5U);

    descriptor.flags = 1;
    std::array<std::byte, contextual_tans_max_descriptor_size> large{};
    std::ranges::fill(large, std::byte{0xa5});
    EXPECT_EQ(
        marc::entropy::internal::serialize_contextual_tans_descriptor(
            descriptor, 2, 2, {}, large, written),
        ContextualTansFormatError::unknown_flags);
    EXPECT_TRUE(std::ranges::all_of(
        large, [](const auto value) { return value == std::byte{0xa5}; }));
    EXPECT_EQ(written, 0xa5a5U);
}

} // namespace
