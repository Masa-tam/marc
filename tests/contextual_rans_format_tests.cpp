#include "context/lzss_field_context_format.hpp"
#include "entropy/contextual_rans_format.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <vector>

namespace {

using marc::entropy::internal::ContextualRansFormatError;
using marc::entropy::internal::ContextualRansDescriptor;
using marc::entropy::internal::contextual_rans_max_descriptor_size;
using marc::entropy::internal::contextual_rans_max_descriptor_size_v3;
using marc::entropy::internal::contextual_rans_descriptor_capacity;
using marc::context::internal::LzssFieldContextVariant;

[[nodiscard]] ContextualRansDescriptor literal_a_descriptor() {
    ContextualRansDescriptor descriptor{};
    descriptor.decision_count = 2;
    descriptor.payload_size = 8;
    descriptor.frequencies[0] = 4096;
    descriptor.frequencies[71] = 4096;
    return descriptor;
}

[[nodiscard]] bool descriptors_equal(
    const ContextualRansDescriptor& left,
    const ContextualRansDescriptor& right) {
    return left.decision_count == right.decision_count
        && left.payload_size == right.payload_size
        && left.table_log == right.table_log
        && left.flags == right.flags
        && left.context_count == right.context_count
        && left.frequency_entry_count == right.frequency_entry_count
        && left.frequencies == right.frequencies;
}

[[nodiscard]] std::vector<std::byte> serialize(
    const ContextualRansDescriptor& descriptor,
    const LzssFieldContextVariant variant =
        LzssFieldContextVariant::field_context_64k) {
    std::array<
        std::byte,
        marc::entropy::internal::contextual_rans_descriptor_capacity>
        storage{};
    std::size_t written{};
    EXPECT_EQ(
        marc::entropy::internal::serialize_contextual_rans_descriptor(
            descriptor, descriptor.decision_count, descriptor.payload_size,
            {}, storage, written, variant),
        ContextualRansFormatError::none);
    return {storage.begin(), storage.begin() + written};
}

[[nodiscard]] ContextualRansDescriptor context_twenty_descriptor(
    const std::span<const std::uint16_t> frequencies) {
    ContextualRansDescriptor descriptor{};
    descriptor.decision_count = 5;
    descriptor.payload_size = 8;
    const auto offset =
        marc::context::internal::lzss_field_context_offsets[20];
    std::copy(frequencies.begin(), frequencies.end(),
              descriptor.frequencies.begin() + offset);
    return descriptor;
}

TEST(ContextualRansFormat, SerializesAndParsesOneLiteralVector) {
    constexpr std::array expected{
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x08}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x0c}, std::byte{0x00}, std::byte{0x1f}, std::byte{0x00},
        std::byte{0xa6}, std::byte{0x11}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x09}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x10}, std::byte{0x01},
        std::byte{0x00}, std::byte{0x41}};
    const auto descriptor = literal_a_descriptor();
    const auto bytes = serialize(descriptor);
    EXPECT_TRUE(std::ranges::equal(bytes, expected));

    ContextualRansDescriptor parsed{};
    ASSERT_EQ(marc::entropy::internal::parse_contextual_rans_descriptor(
                  bytes, 2, 8, {}, parsed),
              ContextualRansFormatError::none);
    EXPECT_TRUE(descriptors_equal(parsed, descriptor));
}

TEST(ContextualRansFormat, SelectsSparseStrictlyAndDenseOnTie) {
    constexpr std::array<std::uint16_t, 4> four{
        1024, 1024, 1024, 1024};
    const auto sparse = serialize(context_twenty_descriptor(four));
    ASSERT_EQ(sparse.size(), 32U);
    EXPECT_EQ(sparse[20], std::byte{0x01});

    constexpr std::array<std::uint16_t, 5> five{
        819, 819, 819, 819, 820};
    const auto dense = serialize(context_twenty_descriptor(five));
    ASSERT_EQ(dense.size(), 35U);
    EXPECT_EQ(dense[20], std::byte{0x00});

    ContextualRansDescriptor parsed{};
    ASSERT_EQ(marc::entropy::internal::parse_contextual_rans_descriptor(
                  dense, 5, 8, {}, parsed),
              ContextualRansFormatError::none);
    EXPECT_TRUE(descriptors_equal(parsed, context_twenty_descriptor(five)));
}

TEST(ContextualRansFormat, EveryDenseModelReachesExactMaximum) {
    ContextualRansDescriptor descriptor{};
    descriptor.decision_count = 1;
    descriptor.payload_size = 8;
    for (std::size_t context = 0;
         context < marc::context::internal::lzss_field_context_count;
         ++context) {
        const auto begin =
            marc::context::internal::lzss_field_context_offsets[context];
        const auto alphabet =
            marc::context::internal::lzss_field_context_alphabets[context];
        for (std::uint16_t symbol = 0; symbol + 1 < alphabet; ++symbol) {
            descriptor.frequencies[begin + symbol] = 1;
        }
        descriptor.frequencies[begin + alphabet - 1] =
            static_cast<std::uint16_t>(4096 - (alphabet - 1));
    }

    const auto bytes = serialize(descriptor);
    ASSERT_EQ(bytes.size(), contextual_rans_max_descriptor_size);
    ContextualRansDescriptor parsed{};
    ASSERT_EQ(marc::entropy::internal::parse_contextual_rans_descriptor(
                  bytes, 1, 8, {}, parsed),
              ContextualRansFormatError::none);
    EXPECT_TRUE(descriptors_equal(parsed, descriptor));
}

TEST(ContextualRansFormat, ExtendedLayoutPreservesGrammarAndSelectsCount) {
    const auto frozen = serialize(literal_a_descriptor());
    auto descriptor = literal_a_descriptor();
    descriptor.frequency_entry_count =
        marc::context::internal::lzss_field_context_frequency_entries_v2;
    const auto extended = serialize(
        descriptor, LzssFieldContextVariant::field_context_1m);
    auto expected = frozen;
    expected[12] = std::byte{0xc6};
    EXPECT_EQ(extended, expected);

    ContextualRansDescriptor parsed{};
    ASSERT_EQ(marc::entropy::internal::parse_contextual_rans_descriptor(
                  extended, 2, 8, {}, parsed,
                  LzssFieldContextVariant::field_context_1m),
              ContextualRansFormatError::none);
    EXPECT_TRUE(descriptors_equal(parsed, descriptor));

    auto sentinel = literal_a_descriptor();
    sentinel.flags = 0xa5;
    const auto before = sentinel;
    EXPECT_EQ(marc::entropy::internal::parse_contextual_rans_descriptor(
                  extended, 2, 8, {}, sentinel),
              ContextualRansFormatError::invalid_frequency_entry_count);
    EXPECT_TRUE(descriptors_equal(sentinel, before));
    EXPECT_EQ(marc::entropy::internal::parse_contextual_rans_descriptor(
                  frozen, 2, 8, {}, sentinel,
                  LzssFieldContextVariant::field_context_1m),
              ContextualRansFormatError::invalid_frequency_entry_count);
    EXPECT_TRUE(descriptors_equal(sentinel, before));
}

TEST(ContextualRansFormat, ExtendedEveryDenseModelReachesExactMaximum) {
    ContextualRansDescriptor descriptor{};
    descriptor.decision_count = 1;
    descriptor.payload_size = 8;
    descriptor.frequency_entry_count =
        marc::context::internal::lzss_field_context_frequency_entries_v2;
    const auto selected = marc::context::internal::
        get_lzss_field_context_layout(
            LzssFieldContextVariant::field_context_1m);
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

    const auto bytes = serialize(
        descriptor, LzssFieldContextVariant::field_context_1m);
    ASSERT_EQ(bytes.size(),
              marc::entropy::internal::
                  contextual_rans_max_descriptor_size_v2);
    ContextualRansDescriptor parsed{};
    ASSERT_EQ(marc::entropy::internal::parse_contextual_rans_descriptor(
                  bytes, 1, 8, {}, parsed,
                  LzssFieldContextVariant::field_context_1m),
              ContextualRansFormatError::none);
    EXPECT_TRUE(descriptors_equal(parsed, descriptor));
}

TEST(ContextualRansFormat, FourMiBLayoutSelectsCountAndCanonicalGrammar) {
    const auto frozen = serialize(literal_a_descriptor());
    auto descriptor = literal_a_descriptor();
    descriptor.frequency_entry_count =
        marc::context::internal::lzss_field_context_frequency_entries_v3;
    constexpr auto variant = LzssFieldContextVariant::field_context_4m;
    const auto selected = serialize(descriptor, variant);
    auto expected = frozen;
    expected[12] = std::byte{0xd6};
    EXPECT_EQ(selected, expected);

    ContextualRansDescriptor parsed{};
    ASSERT_EQ(marc::entropy::internal::parse_contextual_rans_descriptor(
                  selected, 2, 8, {}, parsed, variant),
              ContextualRansFormatError::none);
    EXPECT_TRUE(descriptors_equal(parsed, descriptor));

    auto sentinel = literal_a_descriptor();
    sentinel.flags = 0xa5;
    const auto before = sentinel;
    EXPECT_EQ(marc::entropy::internal::parse_contextual_rans_descriptor(
                  selected, 2, 8, {}, sentinel,
                  LzssFieldContextVariant::field_context_1m),
              ContextualRansFormatError::invalid_frequency_entry_count);
    EXPECT_TRUE(descriptors_equal(sentinel, before));
}

TEST(ContextualRansFormat, FourMiBEveryDenseModelReachesExactMaximum) {
    ContextualRansDescriptor descriptor{};
    descriptor.decision_count = 1;
    descriptor.payload_size = 8;
    descriptor.frequency_entry_count =
        marc::context::internal::lzss_field_context_frequency_entries_v3;
    constexpr auto variant = LzssFieldContextVariant::field_context_4m;
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
    ASSERT_EQ(bytes.size(), contextual_rans_max_descriptor_size_v3);
    EXPECT_EQ(contextual_rans_descriptor_capacity, bytes.size());
    ContextualRansDescriptor parsed{};
    ASSERT_EQ(marc::entropy::internal::parse_contextual_rans_descriptor(
                  bytes, 1, 8, {}, parsed, variant),
              ContextualRansFormatError::none);
    EXPECT_TRUE(descriptors_equal(parsed, descriptor));
}

TEST(ContextualRansFormat, ExtendedPrefixesAndUnusedFrozenTailAreAtomic) {
    auto descriptor = literal_a_descriptor();
    descriptor.frequency_entry_count =
        marc::context::internal::lzss_field_context_frequency_entries_v2;
    const auto extended = serialize(
        descriptor, LzssFieldContextVariant::field_context_1m);
    std::vector<std::byte> short_output(
        extended.size() - 1, std::byte{0xa5});
    std::size_t written = 0xa5a5;
    EXPECT_EQ(marc::entropy::internal::serialize_contextual_rans_descriptor(
                  descriptor, 2, 8, {}, short_output, written,
                  LzssFieldContextVariant::field_context_1m),
              ContextualRansFormatError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        short_output,
        [](const auto value) { return value == std::byte{0xa5}; }));
    EXPECT_EQ(written, 0xa5a5U);

    for (std::size_t extent = 0; extent < extended.size(); ++extent) {
        auto sentinel = literal_a_descriptor();
        sentinel.flags = 0xa5;
        const auto before = sentinel;
        EXPECT_NE(marc::entropy::internal::parse_contextual_rans_descriptor(
                      std::span<const std::byte>{extended}.first(extent),
                      2, 8, {}, sentinel,
                      LzssFieldContextVariant::field_context_1m),
                  ContextualRansFormatError::none)
            << extent;
        EXPECT_TRUE(descriptors_equal(sentinel, before)) << extent;
    }
    auto trailing = extended;
    trailing.push_back(std::byte{});
    ContextualRansDescriptor sentinel{};
    sentinel.flags = 0xa5;
    const auto before = sentinel;
    EXPECT_EQ(marc::entropy::internal::parse_contextual_rans_descriptor(
                  trailing, 2, 8, {}, sentinel,
                  LzssFieldContextVariant::field_context_1m),
              ContextualRansFormatError::trailing_data);
    EXPECT_TRUE(descriptors_equal(sentinel, before));

    auto frozen = literal_a_descriptor();
    frozen.frequencies[
        marc::context::internal::lzss_field_context_frequency_entries_v1] = 1;
    std::array<std::byte, marc::entropy::internal::
        contextual_rans_descriptor_capacity> output{};
    std::ranges::fill(output, std::byte{0xa5});
    written = 0xa5a5;
    EXPECT_EQ(marc::entropy::internal::serialize_contextual_rans_descriptor(
                  frozen, 2, 8, {}, output, written),
              ContextualRansFormatError::invalid_frequency_table);
    EXPECT_TRUE(std::ranges::all_of(
        output, [](const auto value) { return value == std::byte{0xa5}; }));
    EXPECT_EQ(written, 0xa5a5U);
}

TEST(ContextualRansFormat, RejectsUnsupportedSelectedLayoutAtomically) {
    const auto invalid = static_cast<LzssFieldContextVariant>(99);
    auto descriptor = literal_a_descriptor();
    std::size_t size = 0xa5a5;
    EXPECT_EQ(marc::entropy::internal::validate_contextual_rans_descriptor(
                  descriptor, 2, 8, {}, size, invalid),
              ContextualRansFormatError::unsupported_context_variant);
    EXPECT_EQ(size, 0xa5a5U);
}

TEST(ContextualRansFormat, RejectsEveryStrictPrefixAndTrailingData) {
    const auto valid = serialize(literal_a_descriptor());
    for (std::size_t extent = 0; extent < valid.size(); ++extent) {
        auto sentinel = literal_a_descriptor();
        sentinel.flags = 0xa5;
        const auto before = sentinel;
        EXPECT_NE(
            marc::entropy::internal::parse_contextual_rans_descriptor(
                std::span<const std::byte>{valid}.first(extent), 2, 8, {},
                sentinel),
                  ContextualRansFormatError::none)
            << extent;
        EXPECT_TRUE(descriptors_equal(sentinel, before)) << extent;
    }

    auto trailing = valid;
    trailing.push_back(std::byte{});
    auto sentinel = literal_a_descriptor();
    sentinel.flags = 0xa5;
    const auto before = sentinel;
    EXPECT_EQ(marc::entropy::internal::parse_contextual_rans_descriptor(
                  trailing, 2, 8, {}, sentinel),
              ContextualRansFormatError::trailing_data);
    EXPECT_TRUE(descriptors_equal(sentinel, before));
}

TEST(ContextualRansFormat, RejectsMasksModesAndNoncanonicalRecords) {
    const auto valid = serialize(literal_a_descriptor());
    auto expect_error = [&](std::vector<std::byte> bytes,
                            const ContextualRansFormatError expected) {
        auto sentinel = literal_a_descriptor();
        sentinel.flags = 0xa5;
        const auto before = sentinel;
        EXPECT_EQ(
            marc::entropy::internal::parse_contextual_rans_descriptor(
                bytes, 2, 8, {}, sentinel),
                  expected);
        EXPECT_TRUE(descriptors_equal(sentinel, before));
    };

    auto malformed = valid;
    malformed[19] |= std::byte{0x80};
    expect_error(malformed,
                 ContextualRansFormatError::invalid_active_context_mask);
    malformed = valid;
    std::fill(malformed.begin() + 16, malformed.begin() + 20, std::byte{});
    expect_error(malformed,
                 ContextualRansFormatError::invalid_active_context_mask);
    malformed = valid;
    malformed[16] = std::byte{0x08};
    expect_error(malformed,
                 ContextualRansFormatError::invalid_frequency_table);
    malformed = valid;
    malformed[16] = std::byte{0x01};
    expect_error(malformed, ContextualRansFormatError::trailing_data);
    malformed = valid;
    malformed[20] = std::byte{0x02};
    expect_error(malformed, ContextualRansFormatError::invalid_mode);
    malformed = valid;
    malformed[21] = std::byte{0x01};
    malformed[22] = std::byte{0x10};
    expect_error(malformed,
                 ContextualRansFormatError::invalid_frequency_table);
    malformed = valid;
    malformed[20] = std::byte{0x01};
    malformed[21] = std::byte{0x00};
    malformed[22] = std::byte{0x00};
    expect_error(
        malformed,
        ContextualRansFormatError::noncanonical_representation);
}

TEST(ContextualRansFormat, RejectsMalformedSparseModels) {
    constexpr std::array<std::uint16_t, 2> two{2048, 2048};
    const auto descriptor = context_twenty_descriptor(two);
    const auto valid = serialize(descriptor);
    ASSERT_EQ(valid.size(), 26U);
    ASSERT_EQ(valid[20], std::byte{0x01});

    auto expect_invalid = [&](std::vector<std::byte> bytes) {
        ContextualRansDescriptor sentinel = literal_a_descriptor();
        const auto before = sentinel;
        EXPECT_EQ(
            marc::entropy::internal::parse_contextual_rans_descriptor(
                bytes, 5, 8, {}, sentinel),
                  ContextualRansFormatError::invalid_frequency_table);
        EXPECT_TRUE(descriptors_equal(sentinel, before));
    };
    auto malformed = valid;
    malformed[25] = malformed[22];
    expect_invalid(malformed);
    malformed = valid;
    malformed[22] = std::byte{0x01};
    malformed[25] = std::byte{0x00};
    expect_invalid(malformed);
    malformed = valid;
    malformed[23] = std::byte{};
    malformed[24] = std::byte{};
    expect_invalid(malformed);
    malformed = valid;
    malformed[23] = std::byte{0x00};
    malformed[24] = std::byte{0x10};
    expect_invalid(malformed);
}

TEST(ContextualRansFormat, EnforcesFieldsLimitsAndAtomicOutput) {
    auto descriptor = literal_a_descriptor();
    std::size_t size = 0xa5a5;
    descriptor.decision_count = 0;
    EXPECT_EQ(
        marc::entropy::internal::validate_contextual_rans_descriptor(
            descriptor, 0, 8, {}, size),
              ContextualRansFormatError::invalid_decision_count);
    EXPECT_EQ(size, 0xa5a5U);

    descriptor = literal_a_descriptor();
    marc::core::DecoderLimits limits{};
    limits.max_block_size = 1;
    EXPECT_EQ(
        marc::entropy::internal::validate_contextual_rans_descriptor(
            descriptor, 2, 8, limits, size),
              ContextualRansFormatError::limit_exceeded);
    limits = {};
    limits.max_compressed_payload_size = 7;
    EXPECT_EQ(
        marc::entropy::internal::validate_contextual_rans_descriptor(
            descriptor, 2, 8, limits, size),
              ContextualRansFormatError::limit_exceeded);
    limits = {};
    limits.max_entropy_table_entries =
        marc::entropy::internal::contextual_rans_decode_table_entries - 1;
    EXPECT_EQ(
        marc::entropy::internal::validate_contextual_rans_descriptor(
            descriptor, 2, 8, limits, size),
              ContextualRansFormatError::limit_exceeded);
    limits = {};
    limits.max_internal_buffered_bytes = 33;
    EXPECT_EQ(
        marc::entropy::internal::validate_contextual_rans_descriptor(
            descriptor, 2, 8, limits, size),
              ContextualRansFormatError::limit_exceeded);

    std::array<std::byte, 25> output{};
    std::ranges::fill(output, std::byte{0xa5});
    std::size_t written = 0xa5a5;
    EXPECT_EQ(
        marc::entropy::internal::serialize_contextual_rans_descriptor(
            descriptor, 2, 8, {}, output, written),
        ContextualRansFormatError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        output, [](const auto value) { return value == std::byte{0xa5}; }));
    EXPECT_EQ(written, 0xa5a5U);

    descriptor.flags = 1;
    std::array<std::byte, contextual_rans_max_descriptor_size> large{};
    std::ranges::fill(large, std::byte{0xa5});
    EXPECT_EQ(
        marc::entropy::internal::serialize_contextual_rans_descriptor(
            descriptor, 2, 8, {}, large, written),
        ContextualRansFormatError::unknown_flags);
    EXPECT_TRUE(std::ranges::all_of(
        large, [](const auto value) { return value == std::byte{0xa5}; }));
    EXPECT_EQ(written, 0xa5a5U);
}

} // namespace
