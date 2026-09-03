#include "entropy/contextual_blocked_huffman_format.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace {

using namespace marc::entropy::internal;
using marc::context::internal::LzssFieldContextVariant;

[[nodiscard]] ContextualBlockedHuffmanDescriptor one_literal_descriptor() {
    ContextualBlockedHuffmanDescriptor descriptor{};
    descriptor.decision_count = 2;
    descriptor.field_active_mask = 0x03;
    descriptor.field_models[0].active = true;
    descriptor.field_models[0].single_symbol = 0;
    descriptor.field_models[1].active = true;
    descriptor.field_models[1].single_symbol = 'A';
    return descriptor;
}

constexpr std::array<std::byte, 24> one_literal_bytes{
    std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x0F}, std::byte{0x03}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x41}, std::byte{0x00},
};

void make_complete_dense_model(
    ContextualBlockedHuffmanModel& model, const std::uint16_t alphabet) {
    model = {};
    model.active = true;
    model.single_symbol = contextual_blocked_huffman_no_single_symbol;
    const auto long_length = static_cast<std::uint8_t>(
        std::bit_width(static_cast<std::uint16_t>(alphabet - 1U)));
    const auto short_count = static_cast<std::uint16_t>(
        (UINT32_C(1) << long_length) - alphabet);
    for (std::uint16_t symbol = 0; symbol < alphabet; ++symbol) {
        model.lengths[symbol] = static_cast<std::uint8_t>(
            symbol < short_count ? long_length - 1U : long_length);
    }
}

[[nodiscard]] ContextualBlockedHuffmanDescriptor extended_single_descriptor() {
    ContextualBlockedHuffmanDescriptor descriptor{};
    descriptor.decision_count = 4;
    descriptor.field_active_mask = 0x0f;
    for (auto& model : descriptor.field_models) model.active = true;
    descriptor.field_models[0].single_symbol = 0;
    descriptor.field_models[1].single_symbol = 'A';
    descriptor.field_models[2].single_symbol = 0;
    descriptor.field_models[3].single_symbol = 20;
    return descriptor;
}

[[nodiscard]] ContextualBlockedHuffmanDescriptor four_mib_single_descriptor() {
    auto descriptor = extended_single_descriptor();
    descriptor.field_models[3].single_symbol = 22;
    return descriptor;
}

[[nodiscard]] ContextualBlockedHuffmanDescriptor
sixteen_mib_single_descriptor() {
    auto descriptor = extended_single_descriptor();
    descriptor.field_models[3].single_symbol = 24;
    return descriptor;
}

[[nodiscard]] ContextualBlockedHuffmanDescriptor extended_dense_descriptor() {
    ContextualBlockedHuffmanDescriptor descriptor{};
    descriptor.decision_count = 1;
    descriptor.field_active_mask = 0x0f;
    descriptor.override_mask = UINT32_C(0x7fffffff);
    constexpr std::array<std::uint16_t, 4> field_alphabets{2, 256, 8, 21};
    for (std::size_t field = 0; field < field_alphabets.size(); ++field) {
        make_complete_dense_model(
            descriptor.field_models[field], field_alphabets[field]);
    }
    for (std::size_t context_id = 0;
         context_id < descriptor.context_models.size(); ++context_id) {
        make_complete_dense_model(
            descriptor.context_models[context_id],
            marc::context::internal::
                lzss_field_context_alphabets_v2[context_id]);
    }
    return descriptor;
}

[[nodiscard]] ContextualBlockedHuffmanDescriptor four_mib_dense_descriptor() {
    ContextualBlockedHuffmanDescriptor descriptor{};
    descriptor.decision_count = 1;
    descriptor.field_active_mask = 0x0f;
    descriptor.override_mask = UINT32_C(0x7fffffff);
    constexpr std::array<std::uint16_t, 4> field_alphabets{2, 256, 8, 23};
    for (std::size_t field = 0; field < field_alphabets.size(); ++field) {
        make_complete_dense_model(
            descriptor.field_models[field], field_alphabets[field]);
    }
    for (std::size_t context_id = 0;
         context_id < descriptor.context_models.size(); ++context_id) {
        make_complete_dense_model(
            descriptor.context_models[context_id],
            marc::context::internal::
                lzss_field_context_alphabets_v3[context_id]);
    }
    return descriptor;
}

[[nodiscard]] ContextualBlockedHuffmanDescriptor sixteen_mib_dense_descriptor() {
    ContextualBlockedHuffmanDescriptor descriptor{};
    descriptor.decision_count = 1;
    descriptor.field_active_mask = 0x0f;
    descriptor.override_mask = UINT32_C(0x7fffffff);
    constexpr std::array<std::uint16_t, 4> field_alphabets{2, 256, 8, 25};
    for (std::size_t field = 0; field < field_alphabets.size(); ++field) {
        make_complete_dense_model(
            descriptor.field_models[field], field_alphabets[field]);
    }
    for (std::size_t context_id = 0;
         context_id < descriptor.context_models.size(); ++context_id) {
        make_complete_dense_model(
            descriptor.context_models[context_id],
            marc::context::internal::
                lzss_field_context_alphabets_v4[context_id]);
    }
    return descriptor;
}

[[nodiscard]] ContextualBlockedHuffmanDescriptor
sixty_four_mib_single_descriptor() {
    auto descriptor = extended_single_descriptor();
    descriptor.field_models[3].single_symbol = 26;
    return descriptor;
}

[[nodiscard]] ContextualBlockedHuffmanDescriptor sixty_four_mib_dense_descriptor() {
    ContextualBlockedHuffmanDescriptor descriptor{};
    descriptor.decision_count = 1;
    descriptor.field_active_mask = 0x0f;
    descriptor.override_mask = UINT32_C(0x7fffffff);
    constexpr std::array<std::uint16_t, 4> field_alphabets{2, 256, 8, 27};
    for (std::size_t field = 0; field < field_alphabets.size(); ++field) {
        make_complete_dense_model(
            descriptor.field_models[field], field_alphabets[field]);
    }
    for (std::size_t context_id = 0;
         context_id < descriptor.context_models.size(); ++context_id) {
        make_complete_dense_model(
            descriptor.context_models[context_id],
            marc::context::internal::
                lzss_field_context_alphabets_v5[context_id]);
    }
    return descriptor;
}

} // namespace

TEST(ContextualBlockedHuffmanFormat, SerializesDocumentedOneLiteralVector) {
    const auto descriptor = one_literal_descriptor();
    std::array<std::byte, 25> output{};
    output.back() = std::byte{0xA5};
    std::size_t written{};
    ASSERT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  descriptor, 2, 0, marc::core::DecoderLimits{}, output,
                  written),
              ContextualBlockedHuffmanFormatError::none);
    EXPECT_EQ(written, one_literal_bytes.size());
    EXPECT_TRUE(std::equal(one_literal_bytes.begin(), one_literal_bytes.end(),
                           output.begin()));
    EXPECT_EQ(output.back(), std::byte{0xA5});

    ContextualBlockedHuffmanDescriptor parsed{};
    ASSERT_EQ(parse_contextual_blocked_huffman_descriptor(
                  one_literal_bytes, 2, 0, marc::core::DecoderLimits{},
                  parsed),
              ContextualBlockedHuffmanFormatError::none);
    EXPECT_EQ(parsed.field_active_mask, 0x03);
    EXPECT_EQ(parsed.field_models[0].single_symbol, 0U);
    EXPECT_EQ(parsed.field_models[1].single_symbol,
              static_cast<std::uint16_t>('A'));
}

TEST(ContextualBlockedHuffmanFormat, UsesCanonicalDenseAndSparseRecords) {
    auto dense = one_literal_descriptor();
    dense.decision_count = 3;
    dense.payload_size = 1;
    dense.final_valid_bits = 3;
    dense.field_models[0].single_symbol =
        contextual_blocked_huffman_no_single_symbol;
    dense.field_models[0].lengths[0] = 1;
    dense.field_models[0].lengths[1] = 1;
    std::array<std::byte, contextual_blocked_huffman_max_descriptor_size>
        output{};
    std::size_t written{};
    ASSERT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  dense, 3, 1, marc::core::DecoderLimits{}, output, written),
              ContextualBlockedHuffmanFormatError::none);
    ASSERT_EQ(written, 25U);
    EXPECT_EQ(output[16], std::byte{0x02});
    EXPECT_EQ(output[17], std::byte{0x01});
    EXPECT_EQ(output[20], std::byte{0x11});

    auto sparse = one_literal_descriptor();
    sparse.decision_count = 3;
    sparse.payload_size = 1;
    sparse.final_valid_bits = 2;
    sparse.field_models[1].single_symbol =
        contextual_blocked_huffman_no_single_symbol;
    sparse.field_models[1].lengths['A'] = 1;
    sparse.field_models[1].lengths['B'] = 1;
    ASSERT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  sparse, 3, 1, marc::core::DecoderLimits{}, output, written),
              ContextualBlockedHuffmanFormatError::none);
    ASSERT_EQ(written, 28U);
    EXPECT_EQ(output[20], std::byte{0x01});
    EXPECT_EQ(output[21], std::byte{0x01});
    EXPECT_EQ(output[24], std::byte{'A'});
    EXPECT_EQ(output[25], std::byte{0x01});
    EXPECT_EQ(output[26], std::byte{'B'});
    EXPECT_EQ(output[27], std::byte{0x01});
}

TEST(ContextualBlockedHuffmanFormat, RejectsNoncanonicalAndInvalidModels) {
    auto noncanonical = one_literal_bytes;
    std::array<std::byte, 152> dense_literal{};
    std::copy(noncanonical.begin(), noncanonical.begin() + 20,
              dense_literal.begin());
    dense_literal[20] = std::byte{0x02};
    dense_literal[21] = std::byte{0x01};
    dense_literal[22] = dense_literal[23] = std::byte{0};
    dense_literal[24 + ('A' / 2)] = std::byte{0x10};
    dense_literal[24 + ('B' / 2)] |= std::byte{0x01};
    ContextualBlockedHuffmanDescriptor parsed{};
    EXPECT_EQ(parse_contextual_blocked_huffman_descriptor(
                  dense_literal, 2, 0, marc::core::DecoderLimits{}, parsed),
              ContextualBlockedHuffmanFormatError::
                  noncanonical_representation);

    auto oversubscribed = one_literal_descriptor();
    oversubscribed.field_models[1].single_symbol =
        contextual_blocked_huffman_no_single_symbol;
    oversubscribed.field_models[1].lengths['A'] = 1;
    oversubscribed.field_models[1].lengths['B'] = 1;
    oversubscribed.field_models[1].lengths['C'] = 1;
    std::size_t size{};
    EXPECT_EQ(validate_contextual_blocked_huffman_descriptor(
                  oversubscribed, 2, 0, marc::core::DecoderLimits{}, size),
              ContextualBlockedHuffmanFormatError::invalid_huffman_table);
}

TEST(ContextualBlockedHuffmanFormat, RejectsMasksSizesAndLimitsAtomically) {
    auto bytes = one_literal_bytes;
    ContextualBlockedHuffmanDescriptor sentinel{};
    sentinel.decision_count = 0xCCCCCCCCU;
    EXPECT_EQ(parse_contextual_blocked_huffman_descriptor(
                  std::span<const std::byte>{bytes}.first(23), 2, 0,
                  marc::core::DecoderLimits{}, sentinel),
              ContextualBlockedHuffmanFormatError::invalid_descriptor_size);
    EXPECT_EQ(sentinel.decision_count, 0xCCCCCCCCU);

    bytes[14] = std::byte{0x07};
    EXPECT_EQ(parse_contextual_blocked_huffman_descriptor(
                  bytes, 2, 0, marc::core::DecoderLimits{}, sentinel),
              ContextualBlockedHuffmanFormatError::invalid_field_mask);
    bytes = one_literal_bytes;
    bytes[11] = std::byte{0x80};
    EXPECT_EQ(parse_contextual_blocked_huffman_descriptor(
                  bytes, 2, 0, marc::core::DecoderLimits{}, sentinel),
              ContextualBlockedHuffmanFormatError::invalid_override_mask);

    auto limits = marc::core::DecoderLimits{};
    limits.max_huffman_code_length = 14;
    EXPECT_EQ(parse_contextual_blocked_huffman_descriptor(
                  one_literal_bytes, 2, 0, limits, sentinel),
              ContextualBlockedHuffmanFormatError::limit_exceeded);

    std::array<std::byte, 23> short_output{};
    std::size_t written = 0xCCCCU;
    EXPECT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  one_literal_descriptor(), 2, 0,
                  marc::core::DecoderLimits{}, short_output, written),
              ContextualBlockedHuffmanFormatError::output_too_small);
    EXPECT_EQ(written, 0xCCCCU);
}

TEST(ContextualBlockedHuffmanFormat, SerializesSelectedExtendedDistance) {
    const auto descriptor = extended_single_descriptor();
    std::array<std::byte, 32> output{};
    std::size_t written{};
    ASSERT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  descriptor, 4, 0, {}, output, written,
                  LzssFieldContextVariant::field_context_1m),
              ContextualBlockedHuffmanFormatError::none);
    ASSERT_EQ(written, output.size());
    EXPECT_EQ(output[28], std::byte{0x00});
    EXPECT_EQ(output[30], std::byte{0x14});

    ContextualBlockedHuffmanDescriptor parsed{};
    ASSERT_EQ(parse_contextual_blocked_huffman_descriptor(
                  output, 4, 0, {}, parsed,
                  LzssFieldContextVariant::field_context_1m),
              ContextualBlockedHuffmanFormatError::none);
    EXPECT_EQ(parsed.field_models[3].single_symbol, 20U);

    std::size_t size = 0xa5a5;
    EXPECT_EQ(validate_contextual_blocked_huffman_descriptor(
                  descriptor, 4, 0, {}, size,
                  LzssFieldContextVariant::field_context_64k),
              ContextualBlockedHuffmanFormatError::invalid_model_symbol);
    EXPECT_EQ(size, 0xa5a5U);
    EXPECT_EQ(validate_contextual_blocked_huffman_descriptor(
                  descriptor, 4, 0, {}, size,
                  static_cast<LzssFieldContextVariant>(0xfe)),
              ContextualBlockedHuffmanFormatError::
                  unsupported_context_variant);
    EXPECT_EQ(size, 0xa5a5U);
}

TEST(ContextualBlockedHuffmanFormat, SelectedDenseModelsReachExactMaximum) {
    const auto descriptor = extended_dense_descriptor();
    std::array<std::byte, contextual_blocked_huffman_descriptor_capacity>
        output{};
    std::size_t written{};
    ASSERT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  descriptor, 1, 0, {}, output, written,
                  LzssFieldContextVariant::field_context_1m),
              ContextualBlockedHuffmanFormatError::none);
    ASSERT_EQ(written, contextual_blocked_huffman_max_descriptor_size_v2);

    ContextualBlockedHuffmanDescriptor parsed{};
    ASSERT_EQ(parse_contextual_blocked_huffman_descriptor(
                  std::span<const std::byte>{output}.first(written), 1, 0, {},
                  parsed,
                  LzssFieldContextVariant::field_context_1m),
              ContextualBlockedHuffmanFormatError::none);
    EXPECT_EQ(parsed.override_mask, UINT32_C(0x7fffffff));

    EXPECT_EQ(parse_contextual_blocked_huffman_descriptor(
                  output, 1, 0, {}, parsed,
                  LzssFieldContextVariant::field_context_64k),
              ContextualBlockedHuffmanFormatError::invalid_descriptor_size);
    for (std::size_t size = 2562; size <= output.size(); ++size) {
        EXPECT_EQ(parse_contextual_blocked_huffman_descriptor(
                      std::span<const std::byte>{output}.first(size), 1, 0,
                      {}, parsed,
                      LzssFieldContextVariant::field_context_64k),
                  ContextualBlockedHuffmanFormatError::
                      invalid_descriptor_size)
            << size;
    }

    auto bad_padding = output;
    bad_padding[175] |= std::byte{0xf0};
    EXPECT_EQ(parse_contextual_blocked_huffman_descriptor(
                  std::span<const std::byte>{bad_padding}.first(written), 1, 0,
                  {}, parsed,
                  LzssFieldContextVariant::field_context_1m),
              ContextualBlockedHuffmanFormatError::
                  noncanonical_representation);
}

TEST(ContextualBlockedHuffmanFormat, SelectedLayoutFailuresAreAtomic) {
    const auto descriptor = extended_single_descriptor();
    std::array<std::byte, 32> valid{};
    std::size_t valid_size{};
    ASSERT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  descriptor, 4, 0, {}, valid, valid_size,
                  LzssFieldContextVariant::field_context_1m),
              ContextualBlockedHuffmanFormatError::none);
    ASSERT_EQ(valid_size, valid.size());

    for (std::size_t extent = 0; extent < valid.size(); ++extent) {
        ContextualBlockedHuffmanDescriptor sentinel{};
        sentinel.decision_count = 0xccccccccU;
        EXPECT_NE(parse_contextual_blocked_huffman_descriptor(
                      std::span<const std::byte>{valid}.first(extent), 4, 0,
                      {}, sentinel,
                      LzssFieldContextVariant::field_context_1m),
                  ContextualBlockedHuffmanFormatError::none)
            << extent;
        EXPECT_EQ(sentinel.decision_count, 0xccccccccU) << extent;
    }
    std::array<std::byte, 33> trailing{};
    std::ranges::copy(valid, trailing.begin());
    ContextualBlockedHuffmanDescriptor sentinel{};
    sentinel.decision_count = 0xccccccccU;
    EXPECT_EQ(parse_contextual_blocked_huffman_descriptor(
                  trailing, 4, 0, {}, sentinel,
                  LzssFieldContextVariant::field_context_1m),
              ContextualBlockedHuffmanFormatError::trailing_data);
    EXPECT_EQ(sentinel.decision_count, 0xccccccccU);
    EXPECT_EQ(parse_contextual_blocked_huffman_descriptor(
                  valid, 4, 0, {}, sentinel,
                  LzssFieldContextVariant::field_context_64k),
              ContextualBlockedHuffmanFormatError::invalid_model_symbol);
    EXPECT_EQ(sentinel.decision_count, 0xccccccccU);

    std::array<std::byte, 31> output{};
    std::ranges::fill(output, std::byte{0xa5});
    std::size_t written = 0xa5a5;
    EXPECT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  descriptor, 4, 0, {}, output, written,
                  LzssFieldContextVariant::field_context_1m),
              ContextualBlockedHuffmanFormatError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        output, [](const auto value) { return value == std::byte{0xa5}; }));
    EXPECT_EQ(written, 0xa5a5U);

    std::size_t size = 0xa5a5;
    EXPECT_EQ(validate_contextual_blocked_huffman_descriptor(
                  descriptor, 4, 0, {}, size,
                  static_cast<LzssFieldContextVariant>(0xff)),
              ContextualBlockedHuffmanFormatError::
                  unsupported_context_variant);
    EXPECT_EQ(size, 0xa5a5U);
}

TEST(ContextualBlockedHuffmanFormat,
     SerializesFourMiBDistanceWithoutWideningOlderLayouts) {
    const auto descriptor = four_mib_single_descriptor();
    std::array<std::byte, 32> output{};
    std::size_t written{};
    ASSERT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  descriptor, 4, 0, {}, output, written,
                  LzssFieldContextVariant::field_context_4m),
              ContextualBlockedHuffmanFormatError::none);
    ASSERT_EQ(written, output.size());
    EXPECT_EQ(output[28], std::byte{0x00});
    EXPECT_EQ(output[30], std::byte{0x16});

    ContextualBlockedHuffmanDescriptor parsed{};
    ASSERT_EQ(parse_contextual_blocked_huffman_descriptor(
                  output, 4, 0, {}, parsed,
                  LzssFieldContextVariant::field_context_4m),
              ContextualBlockedHuffmanFormatError::none);
    EXPECT_EQ(parsed.field_models[3].single_symbol, 22U);

    std::size_t size = 0xa5a5;
    EXPECT_EQ(validate_contextual_blocked_huffman_descriptor(
                  descriptor, 4, 0, {}, size,
                  LzssFieldContextVariant::field_context_1m),
              ContextualBlockedHuffmanFormatError::invalid_model_symbol);
    EXPECT_EQ(size, 0xa5a5U);
}

TEST(ContextualBlockedHuffmanFormat,
     FourMiBDenseModelsReachExactMaximumAndRejectBadPadding) {
    const auto descriptor = four_mib_dense_descriptor();
    std::array<std::byte, contextual_blocked_huffman_descriptor_capacity>
        output{};
    std::size_t written{};
    ASSERT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  descriptor, 1, 0, {}, output, written,
                  LzssFieldContextVariant::field_context_4m),
              ContextualBlockedHuffmanFormatError::none);
    ASSERT_EQ(written, contextual_blocked_huffman_max_descriptor_size_v3);

    ContextualBlockedHuffmanDescriptor parsed{};
    ASSERT_EQ(parse_contextual_blocked_huffman_descriptor(
                  std::span{output}.first(written), 1, 0, {}, parsed,
                  LzssFieldContextVariant::field_context_4m),
              ContextualBlockedHuffmanFormatError::none);
    EXPECT_EQ(parsed.override_mask, UINT32_C(0x7fffffff));
    EXPECT_NE(parsed.field_models[3].lengths[22], 0U);

    EXPECT_EQ(parse_contextual_blocked_huffman_descriptor(
                  std::span{output}.first(written), 1, 0, {}, parsed,
                  LzssFieldContextVariant::field_context_1m),
              ContextualBlockedHuffmanFormatError::invalid_descriptor_size);

    auto bad_padding = output;
    bad_padding[176] |= std::byte{0xf0};
    ContextualBlockedHuffmanDescriptor sentinel{};
    sentinel.decision_count = 0xccccccccU;
    EXPECT_EQ(parse_contextual_blocked_huffman_descriptor(
                  std::span{bad_padding}.first(written), 1, 0, {}, sentinel,
                  LzssFieldContextVariant::field_context_4m),
              ContextualBlockedHuffmanFormatError::
                  noncanonical_representation);
    EXPECT_EQ(sentinel.decision_count, 0xccccccccU);

    std::array<std::byte,
               contextual_blocked_huffman_max_descriptor_size_v3 - 1>
        short_output{};
    std::ranges::fill(short_output, std::byte{0xa5});
    written = 0xa5a5;
    EXPECT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  descriptor, 1, 0, {}, short_output, written,
                  LzssFieldContextVariant::field_context_4m),
              ContextualBlockedHuffmanFormatError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        short_output,
        [](const auto value) { return value == std::byte{0xa5}; }));
    EXPECT_EQ(written, 0xa5a5U);
}

TEST(ContextualBlockedHuffmanFormat, FourMiBLayoutFailuresAreAtomic) {
    const auto descriptor = four_mib_single_descriptor();
    std::array<std::byte, 32> valid{};
    std::size_t valid_size{};
    ASSERT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  descriptor, 4, 0, {}, valid, valid_size,
                  LzssFieldContextVariant::field_context_4m),
              ContextualBlockedHuffmanFormatError::none);
    ASSERT_EQ(valid_size, valid.size());

    for (std::size_t extent = 0; extent < valid.size(); ++extent) {
        ContextualBlockedHuffmanDescriptor sentinel{};
        sentinel.decision_count = 0xccccccccU;
        EXPECT_NE(parse_contextual_blocked_huffman_descriptor(
                      std::span<const std::byte>{valid}.first(extent), 4, 0,
                      {}, sentinel,
                      LzssFieldContextVariant::field_context_4m),
                  ContextualBlockedHuffmanFormatError::none)
            << extent;
        EXPECT_EQ(sentinel.decision_count, 0xccccccccU) << extent;
    }

    std::array<std::byte, 33> trailing{};
    std::ranges::copy(valid, trailing.begin());
    ContextualBlockedHuffmanDescriptor sentinel{};
    sentinel.decision_count = 0xccccccccU;
    EXPECT_EQ(parse_contextual_blocked_huffman_descriptor(
                  trailing, 4, 0, {}, sentinel,
                  LzssFieldContextVariant::field_context_4m),
              ContextualBlockedHuffmanFormatError::trailing_data);
    EXPECT_EQ(sentinel.decision_count, 0xccccccccU);
}

TEST(ContextualBlockedHuffmanFormat,
     SerializesSixteenMiBDistanceWithoutChangingOlderBytes) {
    const auto descriptor = sixteen_mib_single_descriptor();
    std::array<std::byte, 32> output{};
    std::size_t written{};
    ASSERT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  descriptor, 4, 0, {}, output, written,
                  LzssFieldContextVariant::field_context_16m),
              ContextualBlockedHuffmanFormatError::none);
    ASSERT_EQ(written, output.size());
    EXPECT_EQ(output[28], std::byte{0x00});
    EXPECT_EQ(output[30], std::byte{0x18});

    ContextualBlockedHuffmanDescriptor parsed{};
    ASSERT_EQ(parse_contextual_blocked_huffman_descriptor(
                  output, 4, 0, {}, parsed,
                  LzssFieldContextVariant::field_context_16m),
              ContextualBlockedHuffmanFormatError::none);
    EXPECT_EQ(parsed.field_models[3].single_symbol, 24U);

    std::size_t size = 0xa5a5;
    EXPECT_EQ(validate_contextual_blocked_huffman_descriptor(
                  descriptor, 4, 0, {}, size,
                  LzssFieldContextVariant::field_context_4m),
              ContextualBlockedHuffmanFormatError::invalid_model_symbol);
    EXPECT_EQ(size, 0xa5a5U);

    const auto older_descriptor = four_mib_single_descriptor();
    std::array<std::byte, 32> older_output{};
    std::array<std::byte, 32> widened_output{};
    std::size_t older_written{};
    std::size_t widened_written{};
    ASSERT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  older_descriptor, 4, 0, {}, older_output, older_written,
                  LzssFieldContextVariant::field_context_4m),
              ContextualBlockedHuffmanFormatError::none);
    ASSERT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  older_descriptor, 4, 0, {}, widened_output, widened_written,
                  LzssFieldContextVariant::field_context_16m),
              ContextualBlockedHuffmanFormatError::none);
    EXPECT_EQ(widened_written, older_written);
    EXPECT_EQ(widened_output, older_output);
}

TEST(ContextualBlockedHuffmanFormat,
     SixteenMiBDenseModelsReachExactMaximumAndRejectBadPadding) {
    const auto descriptor = sixteen_mib_dense_descriptor();
    std::array<std::byte, contextual_blocked_huffman_max_descriptor_size_v4>
        output{};
    std::size_t written{};
    ASSERT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  descriptor, 1, 0, {}, output, written,
                  LzssFieldContextVariant::field_context_16m),
              ContextualBlockedHuffmanFormatError::none);
    ASSERT_EQ(written, contextual_blocked_huffman_max_descriptor_size_v4);

    ContextualBlockedHuffmanDescriptor parsed{};
    ASSERT_EQ(parse_contextual_blocked_huffman_descriptor(
                  output, 1, 0, {}, parsed,
                  LzssFieldContextVariant::field_context_16m),
              ContextualBlockedHuffmanFormatError::none);
    EXPECT_EQ(parsed.override_mask, UINT32_C(0x7fffffff));
    EXPECT_NE(parsed.field_models[3].lengths[24], 0U);

    EXPECT_EQ(parse_contextual_blocked_huffman_descriptor(
                  output, 1, 0, {}, parsed,
                  LzssFieldContextVariant::field_context_4m),
              ContextualBlockedHuffmanFormatError::invalid_descriptor_size);

    auto bad_padding = output;
    bad_padding[177] |= std::byte{0xf0};
    ContextualBlockedHuffmanDescriptor sentinel{};
    sentinel.decision_count = 0xccccccccU;
    EXPECT_EQ(parse_contextual_blocked_huffman_descriptor(
                  bad_padding, 1, 0, {}, sentinel,
                  LzssFieldContextVariant::field_context_16m),
              ContextualBlockedHuffmanFormatError::
                  noncanonical_representation);
    EXPECT_EQ(sentinel.decision_count, 0xccccccccU);

    std::array<std::byte,
               contextual_blocked_huffman_max_descriptor_size_v4 - 1>
        short_output{};
    std::ranges::fill(short_output, std::byte{0xa5});
    written = 0xa5a5;
    EXPECT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  descriptor, 1, 0, {}, short_output, written,
                  LzssFieldContextVariant::field_context_16m),
              ContextualBlockedHuffmanFormatError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        short_output,
        [](const auto value) { return value == std::byte{0xa5}; }));
    EXPECT_EQ(written, 0xa5a5U);
}

TEST(ContextualBlockedHuffmanFormat, SixteenMiBLayoutFailuresAreAtomic) {
    const auto descriptor = sixteen_mib_single_descriptor();
    std::array<std::byte, 32> valid{};
    std::size_t valid_size{};
    ASSERT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  descriptor, 4, 0, {}, valid, valid_size,
                  LzssFieldContextVariant::field_context_16m),
              ContextualBlockedHuffmanFormatError::none);
    ASSERT_EQ(valid_size, valid.size());

    for (std::size_t extent = 0; extent < valid.size(); ++extent) {
        ContextualBlockedHuffmanDescriptor sentinel{};
        sentinel.decision_count = 0xccccccccU;
        EXPECT_NE(parse_contextual_blocked_huffman_descriptor(
                      std::span<const std::byte>{valid}.first(extent), 4, 0,
                      {}, sentinel,
                      LzssFieldContextVariant::field_context_16m),
                  ContextualBlockedHuffmanFormatError::none)
            << extent;
        EXPECT_EQ(sentinel.decision_count, 0xccccccccU) << extent;
    }

    std::array<std::byte, 33> trailing{};
    std::ranges::copy(valid, trailing.begin());
    ContextualBlockedHuffmanDescriptor sentinel{};
    sentinel.decision_count = 0xccccccccU;
    EXPECT_EQ(parse_contextual_blocked_huffman_descriptor(
                  trailing, 4, 0, {}, sentinel,
                  LzssFieldContextVariant::field_context_16m),
              ContextualBlockedHuffmanFormatError::trailing_data);
    EXPECT_EQ(sentinel.decision_count, 0xccccccccU);
}

TEST(ContextualBlockedHuffmanFormat,
     SerializesSixtyFourMiBDistanceWithoutChangingOlderBytes) {
    const auto descriptor = sixty_four_mib_single_descriptor();
    std::array<std::byte, 32> output{};
    std::size_t written{};
    ASSERT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  descriptor, 4, 0, {}, output, written,
                  LzssFieldContextVariant::field_context_64m),
              ContextualBlockedHuffmanFormatError::none);
    ASSERT_EQ(written, output.size());
    EXPECT_EQ(output[28], std::byte{0x00});
    EXPECT_EQ(output[30], std::byte{0x1a});

    ContextualBlockedHuffmanDescriptor parsed{};
    ASSERT_EQ(parse_contextual_blocked_huffman_descriptor(
                  output, 4, 0, {}, parsed,
                  LzssFieldContextVariant::field_context_64m),
              ContextualBlockedHuffmanFormatError::none);
    EXPECT_EQ(parsed.field_models[3].single_symbol, 26U);

    std::size_t size = 0xa5a5;
    EXPECT_EQ(validate_contextual_blocked_huffman_descriptor(
                  descriptor, 4, 0, {}, size,
                  LzssFieldContextVariant::field_context_16m),
              ContextualBlockedHuffmanFormatError::invalid_model_symbol);
    EXPECT_EQ(size, 0xa5a5U);

    const auto older_descriptor = sixteen_mib_single_descriptor();
    std::array<std::byte, 32> older_output{};
    std::array<std::byte, 32> widened_output{};
    std::size_t older_written{};
    std::size_t widened_written{};
    ASSERT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  older_descriptor, 4, 0, {}, older_output, older_written,
                  LzssFieldContextVariant::field_context_16m),
              ContextualBlockedHuffmanFormatError::none);
    ASSERT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  older_descriptor, 4, 0, {}, widened_output, widened_written,
                  LzssFieldContextVariant::field_context_64m),
              ContextualBlockedHuffmanFormatError::none);
    EXPECT_EQ(widened_written, older_written);
    EXPECT_EQ(widened_output, older_output);
}

TEST(ContextualBlockedHuffmanFormat,
     SixtyFourMiBDenseModelsReachExactMaximumAndRejectBadPadding) {
    const auto descriptor = sixty_four_mib_dense_descriptor();
    std::array<std::byte, contextual_blocked_huffman_descriptor_capacity>
        output{};
    std::size_t written{};
    ASSERT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  descriptor, 1, 0, {}, output, written,
                  LzssFieldContextVariant::field_context_64m),
              ContextualBlockedHuffmanFormatError::none);
    ASSERT_EQ(written, contextual_blocked_huffman_max_descriptor_size_v5);

    ContextualBlockedHuffmanDescriptor parsed{};
    ASSERT_EQ(parse_contextual_blocked_huffman_descriptor(
                  output, 1, 0, {}, parsed,
                  LzssFieldContextVariant::field_context_64m),
              ContextualBlockedHuffmanFormatError::none);
    EXPECT_EQ(parsed.override_mask, UINT32_C(0x7fffffff));
    EXPECT_NE(parsed.field_models[3].lengths[26], 0U);

    EXPECT_EQ(parse_contextual_blocked_huffman_descriptor(
                  output, 1, 0, {}, parsed,
                  LzssFieldContextVariant::field_context_16m),
              ContextualBlockedHuffmanFormatError::invalid_descriptor_size);

    auto bad_padding = output;
    bad_padding[178] |= std::byte{0xf0};
    ContextualBlockedHuffmanDescriptor sentinel{};
    sentinel.decision_count = 0xccccccccU;
    EXPECT_EQ(parse_contextual_blocked_huffman_descriptor(
                  bad_padding, 1, 0, {}, sentinel,
                  LzssFieldContextVariant::field_context_64m),
              ContextualBlockedHuffmanFormatError::
                  noncanonical_representation);
    EXPECT_EQ(sentinel.decision_count, 0xccccccccU);

    std::array<std::byte,
               contextual_blocked_huffman_max_descriptor_size_v5 - 1>
        short_output{};
    std::ranges::fill(short_output, std::byte{0xa5});
    written = 0xa5a5;
    EXPECT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  descriptor, 1, 0, {}, short_output, written,
                  LzssFieldContextVariant::field_context_64m),
              ContextualBlockedHuffmanFormatError::output_too_small);
    EXPECT_TRUE(std::ranges::all_of(
        short_output,
        [](const auto value) { return value == std::byte{0xa5}; }));
    EXPECT_EQ(written, 0xa5a5U);
}

TEST(ContextualBlockedHuffmanFormat, SixtyFourMiBLayoutFailuresAreAtomic) {
    const auto descriptor = sixty_four_mib_single_descriptor();
    std::array<std::byte, 32> valid{};
    std::size_t valid_size{};
    ASSERT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  descriptor, 4, 0, {}, valid, valid_size,
                  LzssFieldContextVariant::field_context_64m),
              ContextualBlockedHuffmanFormatError::none);
    ASSERT_EQ(valid_size, valid.size());

    for (std::size_t extent = 0; extent < valid.size(); ++extent) {
        ContextualBlockedHuffmanDescriptor sentinel{};
        sentinel.decision_count = 0xccccccccU;
        EXPECT_NE(parse_contextual_blocked_huffman_descriptor(
                      std::span<const std::byte>{valid}.first(extent), 4, 0,
                      {}, sentinel,
                      LzssFieldContextVariant::field_context_64m),
                  ContextualBlockedHuffmanFormatError::none)
            << extent;
        EXPECT_EQ(sentinel.decision_count, 0xccccccccU) << extent;
    }

    std::array<std::byte, 33> trailing{};
    std::ranges::copy(valid, trailing.begin());
    ContextualBlockedHuffmanDescriptor sentinel{};
    sentinel.decision_count = 0xccccccccU;
    EXPECT_EQ(parse_contextual_blocked_huffman_descriptor(
                  trailing, 4, 0, {}, sentinel,
                  LzssFieldContextVariant::field_context_64m),
              ContextualBlockedHuffmanFormatError::trailing_data);
    EXPECT_EQ(sentinel.decision_count, 0xccccccccU);
}

TEST(ContextualBlockedHuffmanFormat, SixtyFourMiBExactLimitsAreAtomic) {
    const auto descriptor = sixty_four_mib_dense_descriptor();
    marc::core::DecoderLimits limits{};
    limits.max_block_size = 1;
    limits.max_internal_buffered_bytes = 2606;
    limits.max_entropy_table_entries = 17885;
    std::size_t size{};
    ASSERT_EQ(validate_contextual_blocked_huffman_descriptor(
                  descriptor, 1, 0, limits, size,
                  LzssFieldContextVariant::field_context_64m),
              ContextualBlockedHuffmanFormatError::none);
    EXPECT_EQ(size, 2606U);
    --limits.max_internal_buffered_bytes;
    size = 0xa5a5;
    EXPECT_EQ(validate_contextual_blocked_huffman_descriptor(
                  descriptor, 1, 0, limits, size,
                  LzssFieldContextVariant::field_context_64m),
              ContextualBlockedHuffmanFormatError::limit_exceeded);
    EXPECT_EQ(size, 0xa5a5U);
    ++limits.max_internal_buffered_bytes;
    --limits.max_entropy_table_entries;
    EXPECT_EQ(validate_contextual_blocked_huffman_descriptor(
                  descriptor, 1, 0, limits, size,
                  LzssFieldContextVariant::field_context_64m),
              ContextualBlockedHuffmanFormatError::limit_exceeded);
    EXPECT_EQ(size, 0xa5a5U);

    auto invalid = sixty_four_mib_single_descriptor();
    invalid.field_models[3].single_symbol = 27;
    std::array<std::byte, 32> output{};
    std::ranges::fill(output, std::byte{0xa5});
    EXPECT_EQ(serialize_contextual_blocked_huffman_descriptor(
                  invalid, 4, 0, {}, output, size,
                  LzssFieldContextVariant::field_context_64m),
              ContextualBlockedHuffmanFormatError::invalid_model_symbol);
    EXPECT_EQ(size, 0xa5a5U);
    EXPECT_TRUE(std::ranges::all_of(
        output, [](auto byte) { return byte == std::byte{0xa5}; }));
}
