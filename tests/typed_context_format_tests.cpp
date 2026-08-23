#include "frame/typed_context_format.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using namespace marc::frame::internal;

[[nodiscard]] std::array<std::byte, typed_context_stream_header_size>
stream_vector() {
    std::array<std::byte, typed_context_stream_header_size> bytes{};
    bytes[0] = std::byte{0x4d};
    bytes[1] = std::byte{0x41};
    bytes[2] = std::byte{0x52};
    bytes[3] = std::byte{0x43};
    bytes[4] = std::byte{0x02};
    bytes[8] = std::byte{0x40};
    bytes[10] = std::byte{0x01};
    bytes[12] = std::byte{0x02};
    bytes[14] = std::byte{0x02};
    bytes[16] = std::byte{0x03};
    bytes[18] = std::byte{0x02};
    bytes[20] = std::byte{0x40};
    bytes[28] = std::byte{0x10};
    bytes[32] = std::byte{0x10};
    bytes[40] = std::byte{0x01};
    bytes[48] = std::byte{0x10};
    bytes[66] = std::byte{0x01};
    bytes[68] = std::byte{0x05};
    bytes[72] = std::byte{0x02};
    bytes[73] = std::byte{0x01};
    bytes[81] = std::byte{0x80};
    bytes[84] = std::byte{0x1f};
    bytes[96] = std::byte{0x01};
    bytes[98] = std::byte{0x01};
    return bytes;
}

[[nodiscard]] std::array<std::byte, typed_context_stream_header_size>
extended_stream_vector() {
    auto bytes = stream_vector();
    bytes[14] = std::byte{0x03};
    bytes[20] = std::byte{0x00};
    bytes[22] = std::byte{0x10};
    bytes[66] = std::byte{0x10};
    bytes[98] = std::byte{0x02};
    return bytes;
}

[[nodiscard]] std::array<std::byte, 86> frame_vector() {
    std::array<std::byte, 86> bytes{};
    bytes[0] = std::byte{0x4d};
    bytes[1] = std::byte{0x52};
    bytes[2] = std::byte{0x46};
    bytes[3] = std::byte{0x32};
    bytes[4] = std::byte{0x40};
    bytes[16] = std::byte{0x01};
    bytes[20] = std::byte{0x01};
    bytes[24] = std::byte{0x02};
    bytes[28] = std::byte{0x02};
    bytes[32] = std::byte{0x06};
    bytes[36] = std::byte{0x10};
    bytes[64] = std::byte{0x02};
    bytes[68] = std::byte{0x06};
    bytes[72] = std::byte{0x1f};
    bytes[80] = std::byte{0x00};
    bytes[81] = std::byte{0x20};
    bytes[82] = std::byte{0x7f};
    bytes[83] = std::byte{0xff};
    bytes[84] = std::byte{0xbf};
    bytes[85] = std::byte{0x00};
    return bytes;
}

[[nodiscard]] TypedContextStreamHeader stream_config() {
    TypedContextStreamHeader stream{};
    stream.frame_size = 64;
    stream.original_size = 1;
    stream.range_model_total = typed_context_model_total;
    stream.context_count = typed_context_count;
    return stream;
}

[[nodiscard]] TypedContextStreamHeader extended_stream_config() {
    auto stream = stream_config();
    stream.frame_size = 1048576;
    stream.dictionary.window_size = 1048576;
    stream.dictionary_variant = 3;
    stream.context_variant = 2;
    return stream;
}

[[nodiscard]] TypedContextStreamHeader four_mib_stream_config() {
    auto stream = stream_config();
    stream.frame_size = 4194304;
    stream.dictionary.window_size = 4194304;
    stream.dictionary_variant = 4;
    stream.context_variant = 3;
    return stream;
}

[[nodiscard]] TypedContextFrameValidationContext frame_context(
    const TypedContextStreamHeader& stream,
    const marc::core::DecoderLimits& limits) {
    return {stream, limits, 0, 0};
}

} // namespace

TEST(TypedContextStreamFormat, ParsesSpecifiedOneLiteralHeader) {
    TypedContextStreamHeader parsed{};
    std::size_t consumed{};
    ASSERT_EQ(parse_typed_context_stream_header(
                  stream_vector(), marc::core::DecoderLimits{}, parsed,
                  consumed),
              TypedContextStreamHeaderError::none);
    EXPECT_EQ(consumed, typed_context_stream_header_size);
    EXPECT_EQ(parsed.frame_size, 64U);
    EXPECT_EQ(parsed.original_size, 1U);
    EXPECT_EQ(parsed.dictionary.window_size, 65536U);
    EXPECT_EQ(parsed.dictionary.min_match_length, 5U);
    EXPECT_EQ(parsed.dictionary.max_match_length, 258U);
    EXPECT_EQ(parsed.range_model_total, 32768U);
    EXPECT_EQ(parsed.context_count, 31U);
}

TEST(TypedContextStreamFormat, SerializesSpecifiedHeaderTransactionally) {
    const auto expected = stream_vector();
    const auto stream = stream_config();
    std::array<std::byte, typed_context_stream_header_size> output{};
    ASSERT_EQ(serialize_typed_context_stream_header(stream, {}, output),
              TypedContextStreamHeaderError::none);
    EXPECT_EQ(output, expected);

    output.fill(std::byte{0xCC});
    auto invalid = stream;
    invalid.context_count = 30;
    EXPECT_EQ(serialize_typed_context_stream_header(invalid, {}, output),
              TypedContextStreamHeaderError::invalid_entropy_parameters);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xCC};
    }));
}

TEST(TypedContextStreamFormat, ParsesAndSerializesExtendedWindowHeader) {
    const auto expected = extended_stream_vector();
    TypedContextStreamHeader parsed{};
    std::size_t consumed{};
    ASSERT_EQ(parse_typed_context_stream_header(
                  expected, marc::core::DecoderLimits{}, parsed, consumed),
              TypedContextStreamHeaderError::none);
    EXPECT_EQ(consumed, typed_context_stream_header_size);
    EXPECT_EQ(parsed.dictionary_variant, 3U);
    EXPECT_EQ(parsed.context_algorithm, 1U);
    EXPECT_EQ(parsed.context_variant, 2U);
    EXPECT_EQ(parsed.dictionary.window_size, 1048576U);

    std::array<std::byte, typed_context_stream_header_size> output{};
    ASSERT_EQ(serialize_typed_context_stream_header(
                  extended_stream_config(), marc::core::DecoderLimits{},
                  output),
              TypedContextStreamHeaderError::none);
    EXPECT_EQ(output, expected);
}

TEST(TypedContextStreamFormat, RejectsEveryTruncatedHeaderAtomically) {
    const auto bytes = stream_vector();
    for (std::size_t size = 0; size < bytes.size(); ++size) {
        TypedContextStreamHeader output{};
        output.original_size = 123;
        std::size_t consumed = 7;
        EXPECT_EQ(parse_typed_context_stream_header(
                      std::span<const std::byte>{bytes}.first(size),
                      marc::core::DecoderLimits{}, output, consumed),
                  TypedContextStreamHeaderError::truncated_header)
            << "size=" << size;
        EXPECT_EQ(output.original_size, 123U) << "size=" << size;
        EXPECT_EQ(consumed, 7U) << "size=" << size;
    }
}

TEST(TypedContextStreamFormat, RejectsUnknownIdentitiesAtomically) {
    struct Mutation {
        std::size_t offset;
        std::byte value;
        TypedContextStreamHeaderError error;
    };
    constexpr std::array mutations{
        Mutation{0, std::byte{0}, TypedContextStreamHeaderError::invalid_magic},
        Mutation{4, std::byte{1},
                 TypedContextStreamHeaderError::unsupported_version},
        Mutation{8, std::byte{63},
                 TypedContextStreamHeaderError::invalid_header_size},
        Mutation{10, std::byte{3},
                 TypedContextStreamHeaderError::unknown_flags},
        Mutation{12, std::byte{1},
                 TypedContextStreamHeaderError::unknown_dictionary_algorithm},
        Mutation{14, std::byte{1},
                 TypedContextStreamHeaderError::unsupported_dictionary_variant},
        Mutation{14, std::byte{5},
                 TypedContextStreamHeaderError::unsupported_dictionary_variant},
        Mutation{16, std::byte{4},
                 TypedContextStreamHeaderError::unknown_entropy_algorithm},
        Mutation{18, std::byte{1},
                 TypedContextStreamHeaderError::unsupported_entropy_variant},
        Mutation{96, std::byte{2},
                 TypedContextStreamHeaderError::unknown_context_model},
        Mutation{98, std::byte{2},
                 TypedContextStreamHeaderError::contradictory_parameters},
        Mutation{98, std::byte{4},
                 TypedContextStreamHeaderError::unsupported_context_variant},
    };
    for (const auto& mutation : mutations) {
        auto bytes = stream_vector();
        bytes[mutation.offset] = mutation.value;
        TypedContextStreamHeader output{};
        output.original_size = 123;
        std::size_t consumed = 7;
        EXPECT_EQ(parse_typed_context_stream_header(
                      bytes, marc::core::DecoderLimits{}, output, consumed),
                  mutation.error)
            << "offset=" << mutation.offset;
        EXPECT_EQ(output.original_size, 123U);
        EXPECT_EQ(consumed, 7U);
    }
}

TEST(TypedContextStreamFormat, KeepsReservedSixteenMibPairUnadmitted) {
    auto stream = four_mib_stream_config();
    stream.frame_size = 16777216;
    stream.dictionary.window_size = 16777216;
    stream.dictionary_variant = 5;
    stream.context_variant = 4;
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = stream.frame_size;
    limits.max_entropy_table_entries = 4582;

    EXPECT_EQ(validate_typed_context_stream_header(stream, limits),
              TypedContextStreamHeaderError::unsupported_dictionary_variant);
    std::array<std::byte, typed_context_stream_header_size> output{};
    output.fill(std::byte{0xa5});
    EXPECT_EQ(serialize_typed_context_stream_header(stream, limits, output),
              TypedContextStreamHeaderError::unsupported_dictionary_variant);
    EXPECT_TRUE(std::ranges::all_of(output, [](const std::byte value) {
        return value == std::byte{0xa5};
    }));
}

TEST(TypedContextStreamFormat, RejectsCrossedKnownVariantPairsAtomically) {
    for (const auto& bytes : {[] {
             auto value = stream_vector();
             value[14] = std::byte{3};
             return value;
         }(), [] {
             auto value = extended_stream_vector();
             value[14] = std::byte{2};
             return value;
         }()}) {
        TypedContextStreamHeader output{};
        output.original_size = 123;
        std::size_t consumed = 7;
        EXPECT_EQ(parse_typed_context_stream_header(
                      bytes, marc::core::DecoderLimits{}, output, consumed),
                  TypedContextStreamHeaderError::contradictory_parameters);
        EXPECT_EQ(output.original_size, 123U);
        EXPECT_EQ(consumed, 7U);
    }
}

TEST(TypedContextStreamFormat, SelectsVariantSpecificTableLimit) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_entropy_table_entries = 4518;
    EXPECT_EQ(validate_typed_context_stream_header(stream_config(), limits),
              TypedContextStreamHeaderError::none);
    EXPECT_EQ(validate_typed_context_stream_header(
                  extended_stream_config(), limits),
              TypedContextStreamHeaderError::limit_exceeded);

    limits.max_entropy_table_entries = 4550;
    EXPECT_EQ(validate_typed_context_stream_header(
                  extended_stream_config(), limits),
              TypedContextStreamHeaderError::none);

    EXPECT_EQ(validate_typed_context_stream_header(
                  four_mib_stream_config(), limits),
              TypedContextStreamHeaderError::limit_exceeded);
    limits.max_entropy_table_entries = 4566;
    EXPECT_EQ(validate_typed_context_stream_header(
                  four_mib_stream_config(), limits),
              TypedContextStreamHeaderError::none);
}

TEST(TypedContextStreamFormat, RejectsReservedAndParameterViolations) {
    for (const std::size_t offset : {std::size_t{63}, std::size_t{95},
                                     std::size_t{111}}) {
        auto bytes = stream_vector();
        bytes[offset] = std::byte{1};
        TypedContextStreamHeader output{};
        std::size_t consumed{};
        EXPECT_EQ(parse_typed_context_stream_header(
                      bytes, marc::core::DecoderLimits{}, output, consumed),
                  TypedContextStreamHeaderError::nonzero_reserved)
            << "offset=" << offset;
    }

    auto bytes = stream_vector();
    bytes[68] = std::byte{4};
    TypedContextStreamHeader output{};
    std::size_t consumed{};
    EXPECT_EQ(parse_typed_context_stream_header(
                  bytes, marc::core::DecoderLimits{}, output, consumed),
              TypedContextStreamHeaderError::invalid_dictionary_parameters);

    bytes = stream_vector();
    bytes[84] = std::byte{30};
    EXPECT_EQ(parse_typed_context_stream_header(
                  bytes, marc::core::DecoderLimits{}, output, consumed),
              TypedContextStreamHeaderError::invalid_entropy_parameters);
}

TEST(TypedContextStreamFormat, EnforcesLocalLimitsBeforePublication) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_range_model_total = 32767;
    TypedContextStreamHeader output{};
    output.original_size = 123;
    std::size_t consumed = 7;
    EXPECT_EQ(parse_typed_context_stream_header(
                  stream_vector(), limits, output, consumed),
              TypedContextStreamHeaderError::limit_exceeded);
    EXPECT_EQ(output.original_size, 123U);
    EXPECT_EQ(consumed, 7U);

    limits = marc::core::DecoderLimits{};
    limits.max_lz_distance = 65535;
    EXPECT_EQ(parse_typed_context_stream_header(
                  stream_vector(), limits, output, consumed),
              TypedContextStreamHeaderError::limit_exceeded);
    EXPECT_EQ(output.original_size, 123U);
    EXPECT_EQ(consumed, 7U);
}

TEST(TypedContextFrameFormat, PreflightsSpecifiedOneLiteralFrame) {
    const auto stream = stream_config();
    const auto limits = marc::core::DecoderLimits{};
    TypedContextFrameLayout layout{};
    const auto result = preflight_typed_context_frame(
        frame_vector(), frame_context(stream, limits), layout);
    ASSERT_EQ(result.error, TypedContextFramePreflightError::none);
    EXPECT_EQ(layout.serialized_size, 86U);
    EXPECT_EQ(layout.header.uncompressed_size, 1U);
    EXPECT_EQ(layout.header.token_count, 1U);
    EXPECT_EQ(layout.header.event_count, 2U);
    EXPECT_EQ(layout.header.decision_count, 2U);
    EXPECT_EQ(layout.header.payload_size, 6U);
    EXPECT_EQ(layout.descriptor.context_count, 31U);
}

TEST(TypedContextFrameFormat, SelectsVariantSpecificDecisionCeiling) {
    auto old_stream = stream_config();
    old_stream.frame_size = 5;
    old_stream.original_size = 5;
    auto extended_stream = old_stream;
    extended_stream.dictionary.window_size = 1048576;
    extended_stream.dictionary_variant = 3;
    extended_stream.context_variant = 2;
    const auto limits = marc::core::DecoderLimits{};
    const TypedContextFrameHeader header{
        0, 0, 5, 1, 5, 27, 5, 16, 0, 0};
    EXPECT_EQ(validate_typed_context_frame_header(
                  header, frame_context(old_stream, limits)),
              TypedContextFrameHeaderError::contradictory_counts);
    EXPECT_EQ(validate_typed_context_frame_header(
                  header, frame_context(extended_stream, limits)),
              TypedContextFrameHeaderError::none);

    auto four_mib_stream = four_mib_stream_config();
    four_mib_stream.frame_size = 5;
    four_mib_stream.original_size = 5;
    const TypedContextFrameHeader four_mib_header{
        0, 0, 5, 1, 5, 32, 5, 16, 0, 0};
    EXPECT_EQ(validate_typed_context_frame_header(
                  four_mib_header,
                  frame_context(four_mib_stream, limits)),
              TypedContextFrameHeaderError::none);
    EXPECT_EQ(validate_typed_context_frame_header(
                  four_mib_header,
                  frame_context(extended_stream, limits)),
              TypedContextFrameHeaderError::contradictory_counts);
}

TEST(TypedContextFrameFormat, SerializesHeaderAndDescriptorTransactionally) {
    const auto stream = stream_config();
    const auto limits = marc::core::DecoderLimits{};
    const TypedContextFrameHeader header{
        0, 0, 1, 1, 2, 2, 6, 16, 0, 0};
    const TypedContextRangeDescriptor descriptor{2, 6, 31};
    std::array<std::byte, typed_context_frame_header_size> header_output{};
    std::array<std::byte, typed_context_range_descriptor_size>
        descriptor_output{};
    ASSERT_EQ(serialize_typed_context_frame_header(
                  header, frame_context(stream, limits), header_output),
              TypedContextFrameHeaderError::none);
    ASSERT_EQ(serialize_typed_context_range_descriptor(
                  descriptor, header, limits, descriptor_output),
              TypedContextRangeDescriptorError::none);
    const auto expected = frame_vector();
    EXPECT_TRUE(std::ranges::equal(
        header_output,
        std::span<const std::byte>{expected}.first(header_output.size())));
    EXPECT_TRUE(std::ranges::equal(
        descriptor_output,
        std::span<const std::byte>{expected}.subspan(
            typed_context_frame_header_size, descriptor_output.size())));

    header_output.fill(std::byte{0xCC});
    auto invalid_header = header;
    invalid_header.token_count = 0;
    EXPECT_EQ(serialize_typed_context_frame_header(
                  invalid_header, frame_context(stream, limits),
                  header_output),
              TypedContextFrameHeaderError::contradictory_counts);
    EXPECT_TRUE(std::ranges::all_of(
        header_output, [](const std::byte value) {
            return value == std::byte{0xCC};
        }));

    descriptor_output.fill(std::byte{0xCC});
    auto invalid_descriptor = descriptor;
    invalid_descriptor.decision_count = 3;
    EXPECT_EQ(serialize_typed_context_range_descriptor(
                  invalid_descriptor, header, limits, descriptor_output),
              TypedContextRangeDescriptorError::contradictory_counts);
    EXPECT_TRUE(std::ranges::all_of(
        descriptor_output, [](const std::byte value) {
            return value == std::byte{0xCC};
        }));
}

TEST(TypedContextFrameFormat, AcceptsTrailingNextFrameBytesByExtent) {
    const auto canonical = frame_vector();
    std::vector<std::byte> bytes(canonical.begin(), canonical.end());
    bytes.push_back(std::byte{0xa5});
    const auto stream = stream_config();
    const auto limits = marc::core::DecoderLimits{};
    TypedContextFrameLayout layout{};
    EXPECT_EQ(preflight_typed_context_frame(
                  bytes, frame_context(stream, limits), layout).error,
              TypedContextFramePreflightError::none);
    EXPECT_EQ(layout.serialized_size, canonical.size());
}

TEST(TypedContextFrameFormat, RejectsEveryTruncationWithoutPublishingLayout) {
    const auto bytes = frame_vector();
    const auto stream = stream_config();
    const auto limits = marc::core::DecoderLimits{};
    for (std::size_t size = 0; size < bytes.size(); ++size) {
        TypedContextFrameLayout layout{};
        layout.serialized_size = 123;
        const auto result = preflight_typed_context_frame(
            std::span<const std::byte>{bytes}.first(size),
            frame_context(stream, limits), layout);
        if (size < typed_context_frame_header_size) {
            EXPECT_EQ(result.error,
                      TypedContextFramePreflightError::header_error)
                << "size=" << size;
            EXPECT_EQ(result.header_error,
                      TypedContextFrameHeaderError::truncated_header);
        } else {
            EXPECT_EQ(result.error,
                      TypedContextFramePreflightError::truncated_frame)
                << "size=" << size;
        }
        EXPECT_EQ(layout.serialized_size, 123U) << "size=" << size;
    }
}

TEST(TypedContextFrameFormat, RejectsHeaderAndCountViolations) {
    const auto stream = stream_config();
    const auto limits = marc::core::DecoderLimits{};
    struct Mutation {
        std::size_t offset;
        std::byte value;
        TypedContextFrameHeaderError error;
    };
    constexpr std::array mutations{
        Mutation{0, std::byte{0}, TypedContextFrameHeaderError::invalid_magic},
        Mutation{4, std::byte{63},
                 TypedContextFrameHeaderError::invalid_header_size},
        Mutation{6, std::byte{1}, TypedContextFrameHeaderError::unknown_flags},
        Mutation{8, std::byte{1},
                 TypedContextFrameHeaderError::unexpected_sequence},
        Mutation{16, std::byte{2},
                 TypedContextFrameHeaderError::unexpected_frame_size},
        Mutation{20, std::byte{0},
                 TypedContextFrameHeaderError::contradictory_counts},
        Mutation{24, std::byte{1},
                 TypedContextFrameHeaderError::contradictory_counts},
        Mutation{28, std::byte{1},
                 TypedContextFrameHeaderError::contradictory_counts},
        Mutation{36, std::byte{15},
                 TypedContextFrameHeaderError::contradictory_counts},
        Mutation{40, std::byte{1},
                 TypedContextFrameHeaderError::unsupported_feature},
        Mutation{63, std::byte{1},
                 TypedContextFrameHeaderError::nonzero_reserved},
    };
    for (const auto& mutation : mutations) {
        auto bytes = frame_vector();
        bytes[mutation.offset] = mutation.value;
        TypedContextFrameLayout layout{};
        const auto result = preflight_typed_context_frame(
            bytes, frame_context(stream, limits), layout);
        EXPECT_EQ(result.error, TypedContextFramePreflightError::header_error)
            << "offset=" << mutation.offset;
        EXPECT_EQ(result.header_error, mutation.error)
            << "offset=" << mutation.offset;
    }
}

TEST(TypedContextFrameFormat, RejectsDescriptorMismatchAndReservedBytes) {
    const auto stream = stream_config();
    const auto limits = marc::core::DecoderLimits{};
    struct Mutation {
        std::size_t offset;
        std::byte value;
        TypedContextRangeDescriptorError error;
    };
    constexpr std::array mutations{
        Mutation{64, std::byte{3},
                 TypedContextRangeDescriptorError::contradictory_counts},
        Mutation{68, std::byte{5},
                 TypedContextRangeDescriptorError::contradictory_counts},
        Mutation{72, std::byte{30},
                 TypedContextRangeDescriptorError::contradictory_counts},
        Mutation{74, std::byte{1},
                 TypedContextRangeDescriptorError::unknown_flags},
        Mutation{79, std::byte{1},
                 TypedContextRangeDescriptorError::nonzero_reserved},
    };
    for (const auto& mutation : mutations) {
        auto bytes = frame_vector();
        bytes[mutation.offset] = mutation.value;
        TypedContextFrameLayout layout{};
        const auto result = preflight_typed_context_frame(
            bytes, frame_context(stream, limits), layout);
        EXPECT_EQ(result.error,
                  TypedContextFramePreflightError::descriptor_error)
            << "offset=" << mutation.offset;
        EXPECT_EQ(result.descriptor_error, mutation.error)
            << "offset=" << mutation.offset;
    }
}

TEST(TypedContextFrameFormat, DescriptorEnforcesFixedTableWorkspace) {
    TypedContextFrameHeader frame{};
    frame.decision_count = 2;
    frame.payload_size = 6;
    const TypedContextRangeDescriptor descriptor{2, 6, 31};
    auto limits = marc::core::DecoderLimits{};
    limits.max_entropy_table_entries = typed_context_table_entries - 1;
    EXPECT_EQ(validate_typed_context_range_descriptor(
                  descriptor, frame, limits),
              TypedContextRangeDescriptorError::limit_exceeded);
}

TEST(TypedContextFrameFormat, RejectsAvailableDescriptorBeforeMissingPayload) {
    auto bytes = frame_vector();
    bytes[64] = std::byte{3};
    const auto stream = stream_config();
    const auto limits = marc::core::DecoderLimits{};
    TypedContextFrameLayout layout{};
    const auto result = preflight_typed_context_frame(
        std::span<const std::byte>{bytes}.first(80),
        frame_context(stream, limits), layout);
    EXPECT_EQ(result.error,
              TypedContextFramePreflightError::descriptor_error);
    EXPECT_EQ(result.descriptor_error,
              TypedContextRangeDescriptorError::contradictory_counts);
}

TEST(TypedContextFrameFormat, RejectsPayloadBeyondLocalPolicy) {
    auto limits = marc::core::DecoderLimits{};
    limits.max_compressed_payload_size = 5;
    const auto stream = stream_config();
    TypedContextFrameLayout layout{};
    const auto result = preflight_typed_context_frame(
        frame_vector(), frame_context(stream, limits), layout);
    EXPECT_EQ(result.error, TypedContextFramePreflightError::header_error);
    EXPECT_EQ(result.header_error, TypedContextFrameHeaderError::limit_exceeded);
}
