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
using marc::entropy::internal::contextual_tans_max_descriptor_size;

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
    const ContextualTansDescriptor& descriptor) {
    std::array<std::byte, contextual_tans_max_descriptor_size> storage{};
    std::size_t written{};
    EXPECT_EQ(
        marc::entropy::internal::serialize_contextual_tans_descriptor(
            descriptor, descriptor.decision_count, descriptor.payload_size,
            {}, storage, written),
        ContextualTansFormatError::none);
    return {storage.begin(), storage.begin() + written};
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
