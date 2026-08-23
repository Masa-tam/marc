#include "frame/lzss_typed_context_frame_decoder.hpp"

#include "entropy/contextual_dynamic_range_encoder.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using namespace marc::frame::internal;
using marc::dictionary::internal::LzssTypedToken;
using marc::dictionary::internal::LzssTypedTokenKind;

[[nodiscard]] constexpr std::array<std::byte, 86> frame_vector() {
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

[[nodiscard]] constexpr TypedContextStreamHeader stream_config() {
    TypedContextStreamHeader stream{};
    stream.frame_size = 64;
    stream.original_size = 1;
    stream.range_model_total = typed_context_model_total;
    stream.context_count = typed_context_count;
    return stream;
}

[[nodiscard]] constexpr TypedContextStreamHeader extended_stream_config() {
    auto stream = stream_config();
    stream.frame_size = 1048576;
    stream.dictionary.window_size = 1048576;
    stream.dictionary_variant = 3;
    stream.context_variant = 2;
    return stream;
}

[[nodiscard]] constexpr TypedContextStreamHeader four_mib_stream_config() {
    auto stream = stream_config();
    stream.frame_size = 4194304;
    stream.dictionary.window_size = 4194304;
    stream.dictionary_variant = 4;
    stream.context_variant = 3;
    return stream;
}

[[nodiscard]] constexpr TypedContextStreamHeader sixteen_mib_stream_config() {
    auto stream = stream_config();
    stream.frame_size = 16777216;
    stream.dictionary.window_size = 16777216;
    stream.dictionary_variant = 5;
    stream.context_variant = 4;
    return stream;
}

[[nodiscard]] marc::core::DecoderLimits sixteen_mib_limits() {
    auto limits = marc::core::DecoderLimits{};
    limits.max_frame_size = 16777216;
    limits.max_block_size = 16777216;
    limits.max_lz_distance = 16777216;
    limits.max_entropy_table_entries = 4582;
    return limits;
}

} // namespace

TEST(LzssTypedContextFrameDecoder, DecodesSpecifiedFrameAtomically) {
    constexpr auto frame = frame_vector();
    constexpr auto stream = stream_config();
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 2> tokens{};
    tokens[1].literal = 0xCC;
    std::array raw{std::byte{0xCC}, std::byte{0xCC}};

    const auto result = decode_lzss_typed_context_frame(
        frame, {stream, limits, 0, 0}, tokens, raw);
    ASSERT_EQ(result.error, LzssTypedContextFrameDecodeError::none);
    EXPECT_EQ(result.serialized_consumed, frame.size());
    EXPECT_EQ(result.required_token_count, 1U);
    EXPECT_EQ(result.required_raw_size, 1U);
    EXPECT_EQ(result.token_decode.token_count, 1U);
    EXPECT_EQ(result.reconstruction.output_size, 1U);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::literal);
    EXPECT_EQ(tokens[0].literal, 'A');
    EXPECT_EQ(tokens[1].literal, 0xCC);
    EXPECT_EQ(raw[0], std::byte{'A'});
    EXPECT_EQ(raw[1], std::byte{0xCC});
}

TEST(LzssTypedContextFrameDecoder, DecodesExtendedWindowLiteralFrame) {
    constexpr auto frame = frame_vector();
    constexpr auto stream = extended_stream_config();
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};

    const auto result = decode_lzss_typed_context_frame(
        frame, {stream, limits, 0, 0}, tokens, raw);
    ASSERT_EQ(result.error, LzssTypedContextFrameDecodeError::none);
    EXPECT_EQ(result.serialized_consumed, frame.size());
    EXPECT_EQ(result.token_decode.token_count, 1U);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::literal);
    EXPECT_EQ(tokens[0].literal, 'A');
    EXPECT_EQ(raw[0], std::byte{'A'});
}

TEST(LzssTypedContextFrameDecoder, DecodesFourMiBWindowLiteralFrame) {
    constexpr auto frame = frame_vector();
    auto stream = four_mib_stream_config();
    stream.frame_size = 1;
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};

    const auto result = decode_lzss_typed_context_frame(
        frame, {stream, limits, 0, 0}, tokens, raw);
    ASSERT_EQ(result.error, LzssTypedContextFrameDecodeError::none);
    EXPECT_EQ(result.serialized_consumed, frame.size());
    EXPECT_EQ(result.token_decode.token_count, 1U);
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::literal);
    EXPECT_EQ(tokens[0].literal, 'A');
    EXPECT_EQ(raw[0], std::byte{'A'});
}

TEST(LzssTypedContextFrameDecoder,
     FourMiBIdentityRejectsCrossedAndShortWorkspaceAtomically) {
    constexpr auto frame = frame_vector();
    auto stream = four_mib_stream_config();
    stream.frame_size = 1;
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 1> tokens{
        LzssTypedToken{LzssTypedTokenKind::match, 0xCC, 0xCCCCCCCCU,
                       0xCCCCCCCCU}};
    std::array raw{std::byte{0xCC}};

    auto result = decode_lzss_typed_context_frame(
        frame, {stream, limits, 0, 0},
        std::span<LzssTypedToken>{tokens}.first(0), raw);
    EXPECT_EQ(result.error,
              LzssTypedContextFrameDecodeError::token_output_too_small);
    EXPECT_EQ(raw[0], std::byte{0xCC});

    const auto before = tokens[0];
    result = decode_lzss_typed_context_frame(
        frame, {stream, limits, 0, 0}, tokens,
        std::span<std::byte>{raw}.first(0));
    EXPECT_EQ(result.error,
              LzssTypedContextFrameDecodeError::raw_output_too_small);
    EXPECT_EQ(tokens[0].kind, before.kind);
    EXPECT_EQ(tokens[0].literal, before.literal);
    EXPECT_EQ(tokens[0].distance, before.distance);
    EXPECT_EQ(tokens[0].length, before.length);

    auto crossed = stream;
    crossed.context_variant = 2;
    result = decode_lzss_typed_context_frame(
        frame, {crossed, limits, 0, 0}, tokens, raw);
    EXPECT_EQ(result.error,
              LzssTypedContextFrameDecodeError::preflight_error);
    EXPECT_EQ(tokens[0].kind, before.kind);
    EXPECT_EQ(tokens[0].literal, before.literal);
    EXPECT_EQ(tokens[0].distance, before.distance);
    EXPECT_EQ(tokens[0].length, before.length);
    EXPECT_EQ(raw[0], std::byte{0xCC});
}

TEST(LzssTypedContextFrameDecoder,
     FourMiBIdentityDecodesFirstNewDistanceExactly) {
    constexpr std::uint32_t distance = 1048577;
    std::vector<LzssTypedToken> source_tokens;
    source_tokens.reserve(4067);
    source_tokens.push_back({LzssTypedTokenKind::literal, 'A', 0, 0});
    for (std::size_t index = 0; index < 4064; ++index) {
        source_tokens.push_back({LzssTypedTokenKind::match, 0, 1, 258});
    }
    source_tokens.push_back({LzssTypedTokenKind::match, 0, 1, 64});
    source_tokens.push_back(
        {LzssTypedTokenKind::match, 0, distance, 258});
    constexpr std::uint32_t raw_size = distance + 258;

    auto stream = four_mib_stream_config();
    stream.frame_size = raw_size;
    stream.original_size = raw_size;
    auto limits = marc::core::DecoderLimits{};
    limits.max_block_size = limits.max_frame_size;
    const marc::dictionary::internal::LzssTypedFrameValidationContext
        token_context{static_cast<std::uint32_t>(source_tokens.size()),
                      raw_size, 0};
    const auto operation_plan =
        marc::context::internal::plan_lzss_field_context_operations(
            source_tokens, stream.dictionary, token_context, limits,
            marc::context::internal::LzssFieldContextVariant::
                field_context_4m);
    ASSERT_EQ(operation_plan.error,
              marc::context::internal::LzssFieldContextError::none);
    std::vector<marc::context::internal::ModeledOperation> operations(
        operation_plan.operation_count);
    const auto modeled =
        marc::context::internal::model_lzss_field_context_tokens(
            source_tokens, stream.dictionary, token_context, limits,
            operations,
            marc::context::internal::LzssFieldContextVariant::
                field_context_4m);
    ASSERT_EQ(modeled.error,
              marc::context::internal::LzssFieldContextError::none);

    TypedContextRangeDescriptor descriptor{};
    const auto entropy_plan = marc::entropy::internal::
        plan_contextual_dynamic_range_operations(
            operations, limits, descriptor,
            marc::context::internal::LzssFieldContextVariant::
                field_context_4m);
    ASSERT_EQ(entropy_plan.error,
              marc::entropy::internal::
                  ContextualDynamicRangeEncodeError::none);
    std::vector<std::byte> payload(entropy_plan.payload_size);
    ASSERT_EQ(marc::entropy::internal::
                  encode_contextual_dynamic_range_operations(
                      operations, limits, payload, descriptor,
                      marc::context::internal::LzssFieldContextVariant::
                          field_context_4m).error,
              marc::entropy::internal::
                  ContextualDynamicRangeEncodeError::none);

    const TypedContextFrameHeader header{
        0,
        0,
        raw_size,
        static_cast<std::uint32_t>(source_tokens.size()),
        static_cast<std::uint32_t>(operations.size()),
        modeled.decision_count,
        static_cast<std::uint32_t>(payload.size()),
        typed_context_range_descriptor_size,
        0,
        0};
    std::vector<std::byte> frame(
        typed_context_frame_header_size
        + typed_context_range_descriptor_size + payload.size());
    ASSERT_EQ(serialize_typed_context_frame_header(
                  header, {stream, limits, 0, 0},
                  std::span<std::byte, typed_context_frame_header_size>{
                      frame.data(), typed_context_frame_header_size}),
              TypedContextFrameHeaderError::none);
    ASSERT_EQ(serialize_typed_context_range_descriptor(
                  descriptor, header, limits,
                  std::span<std::byte,
                            typed_context_range_descriptor_size>{
                      frame.data() + typed_context_frame_header_size,
                      typed_context_range_descriptor_size}),
              TypedContextRangeDescriptorError::none);
    std::ranges::copy(
        payload,
        frame.begin() + typed_context_frame_header_size
            + typed_context_range_descriptor_size);

    std::vector<LzssTypedToken> decoded_tokens(source_tokens.size());
    std::vector<std::byte> raw(raw_size, std::byte{0xCC});
    const auto decoded = decode_lzss_typed_context_frame(
        frame, {stream, limits, 0, 0}, decoded_tokens, raw);
    ASSERT_EQ(decoded.error, LzssTypedContextFrameDecodeError::none);
    EXPECT_EQ(decoded.serialized_consumed, frame.size());
    EXPECT_EQ(decoded_tokens.back().distance, distance);
    EXPECT_EQ(decoded_tokens.back().length, 258U);
    EXPECT_TRUE(std::ranges::all_of(raw, [](const std::byte value) {
        return value == std::byte{'A'};
    }));
}

TEST(LzssTypedContextFrameDecoder,
     SixteenMiBIdentityChecksWorkspaceAndCrossingBeforeWrites) {
    constexpr auto frame = frame_vector();
    auto stream = sixteen_mib_stream_config();
    stream.frame_size = 1;
    const auto limits = sixteen_mib_limits();
    std::array<LzssTypedToken, 1> tokens{
        LzssTypedToken{LzssTypedTokenKind::match, 0xCC, 0xCCCCCCCCU,
                       0xCCCCCCCCU}};
    const auto before = tokens[0];
    std::array raw{std::byte{0xCC}};

    auto result = decode_lzss_typed_context_frame(
        frame, {stream, limits, 0, 0}, tokens, raw);
    ASSERT_EQ(result.error, LzssTypedContextFrameDecodeError::none);
    EXPECT_EQ(result.serialized_consumed, frame.size());
    EXPECT_EQ(tokens[0].kind, LzssTypedTokenKind::literal);
    EXPECT_EQ(tokens[0].literal, 'A');
    EXPECT_EQ(raw[0], std::byte{'A'});

    tokens[0] = before;
    raw[0] = std::byte{0xCC};
    result = decode_lzss_typed_context_frame(
        frame, {stream, limits, 0, 0},
        std::span<LzssTypedToken>{tokens}.first(0), raw);
    EXPECT_EQ(result.error,
              LzssTypedContextFrameDecodeError::token_output_too_small);
    EXPECT_EQ(tokens[0].kind, before.kind);
    EXPECT_EQ(raw[0], std::byte{0xCC});

    result = decode_lzss_typed_context_frame(
        frame, {stream, limits, 0, 0}, tokens,
        std::span<std::byte>{raw}.first(0));
    EXPECT_EQ(result.error,
              LzssTypedContextFrameDecodeError::raw_output_too_small);
    EXPECT_EQ(tokens[0].kind, before.kind);
    EXPECT_EQ(tokens[0].literal, before.literal);
    EXPECT_EQ(tokens[0].distance, before.distance);
    EXPECT_EQ(tokens[0].length, before.length);
    EXPECT_EQ(raw[0], std::byte{0xCC});

    auto crossed = stream;
    crossed.context_variant = 3;
    result = decode_lzss_typed_context_frame(
        frame, {crossed, limits, 0, 0}, tokens, raw);
    EXPECT_EQ(result.error,
              LzssTypedContextFrameDecodeError::preflight_error);
    EXPECT_EQ(tokens[0].kind, before.kind);
    EXPECT_EQ(raw[0], std::byte{0xCC});
}

TEST(LzssTypedContextFrameDecoder,
     SixteenMiBIdentityDecodesFirstNewDistanceExactly) {
    constexpr std::uint32_t distance = 4194305;
    constexpr std::size_t full_matches = 16256;
    constexpr std::uint32_t tail_length = 256;
    constexpr std::uint32_t final_match_length = 258;
    constexpr std::uint32_t raw_size = distance + final_match_length;

    std::vector<LzssTypedToken> source_tokens;
    source_tokens.reserve(full_matches + 3);
    source_tokens.push_back({LzssTypedTokenKind::literal, 'A', 0, 0});
    for (std::size_t index = 0; index < full_matches; ++index) {
        source_tokens.push_back({LzssTypedTokenKind::match, 0, 1, 258});
    }
    source_tokens.push_back(
        {LzssTypedTokenKind::match, 0, 1, tail_length});
    source_tokens.push_back(
        {LzssTypedTokenKind::match, 0, distance, final_match_length});

    auto stream = sixteen_mib_stream_config();
    stream.frame_size = raw_size;
    stream.original_size = raw_size;
    auto limits = sixteen_mib_limits();
    const marc::dictionary::internal::LzssTypedFrameValidationContext
        token_context{static_cast<std::uint32_t>(source_tokens.size()),
                      raw_size, 0};
    const auto operation_plan =
        marc::context::internal::plan_lzss_field_context_operations(
            source_tokens, stream.dictionary, token_context, limits,
            marc::context::internal::LzssFieldContextVariant::
                field_context_16m);
    ASSERT_EQ(operation_plan.error,
              marc::context::internal::LzssFieldContextError::none);
    std::vector<marc::context::internal::ModeledOperation> operations(
        operation_plan.operation_count);
    const auto modeled =
        marc::context::internal::model_lzss_field_context_tokens(
            source_tokens, stream.dictionary, token_context, limits,
            operations,
            marc::context::internal::LzssFieldContextVariant::
                field_context_16m);
    ASSERT_EQ(modeled.error,
              marc::context::internal::LzssFieldContextError::none);

    TypedContextRangeDescriptor descriptor{};
    const auto entropy_plan = marc::entropy::internal::
        plan_contextual_dynamic_range_operations(
            operations, limits, descriptor,
            marc::context::internal::LzssFieldContextVariant::
                field_context_16m);
    ASSERT_EQ(entropy_plan.error,
              marc::entropy::internal::
                  ContextualDynamicRangeEncodeError::none);
    std::vector<std::byte> payload(entropy_plan.payload_size);
    ASSERT_EQ(marc::entropy::internal::
                  encode_contextual_dynamic_range_operations(
                      operations, limits, payload, descriptor,
                      marc::context::internal::LzssFieldContextVariant::
                          field_context_16m).error,
              marc::entropy::internal::
                  ContextualDynamicRangeEncodeError::none);

    const TypedContextFrameHeader header{
        0,
        0,
        raw_size,
        static_cast<std::uint32_t>(source_tokens.size()),
        static_cast<std::uint32_t>(operations.size()),
        modeled.decision_count,
        static_cast<std::uint32_t>(payload.size()),
        typed_context_range_descriptor_size,
        0,
        0};
    std::vector<std::byte> frame(
        typed_context_frame_header_size
        + typed_context_range_descriptor_size + payload.size());
    ASSERT_EQ(serialize_typed_context_frame_header(
                  header, {stream, limits, 0, 0},
                  std::span<std::byte, typed_context_frame_header_size>{
                      frame.data(), typed_context_frame_header_size}),
              TypedContextFrameHeaderError::none);
    ASSERT_EQ(serialize_typed_context_range_descriptor(
                  descriptor, header, limits,
                  std::span<std::byte,
                            typed_context_range_descriptor_size>{
                      frame.data() + typed_context_frame_header_size,
                      typed_context_range_descriptor_size}),
              TypedContextRangeDescriptorError::none);
    std::ranges::copy(
        payload,
        frame.begin() + typed_context_frame_header_size
            + typed_context_range_descriptor_size);

    std::vector<LzssTypedToken> decoded_tokens(source_tokens.size());
    std::vector<std::byte> raw(raw_size, std::byte{0xCC});
    const auto decoded = decode_lzss_typed_context_frame(
        frame, {stream, limits, 0, 0}, decoded_tokens, raw);
    ASSERT_EQ(decoded.error, LzssTypedContextFrameDecodeError::none);
    EXPECT_EQ(decoded.serialized_consumed, frame.size());
    EXPECT_EQ(decoded_tokens.back().distance, distance);
    EXPECT_EQ(decoded_tokens.back().length, final_match_length);
    EXPECT_TRUE(std::ranges::all_of(raw, [](const std::byte value) {
        return value == std::byte{'A'};
    }));
}

TEST(LzssTypedContextFrameDecoder, ConsumesOnlyPreflightedFrameExtent) {
    constexpr auto canonical = frame_vector();
    std::vector<std::byte> frame(canonical.begin(), canonical.end());
    frame.push_back(std::byte{0xA5});
    constexpr auto stream = stream_config();
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 1> tokens{};
    std::array<std::byte, 1> raw{};

    const auto result = decode_lzss_typed_context_frame(
        frame, {stream, limits, 0, 0}, tokens, raw);
    ASSERT_EQ(result.error, LzssTypedContextFrameDecodeError::none);
    EXPECT_EQ(result.serialized_consumed, canonical.size());
    EXPECT_EQ(raw[0], std::byte{'A'});
}

TEST(LzssTypedContextFrameDecoder, PreflightFailurePreservesAllWorkspace) {
    constexpr auto frame = frame_vector();
    constexpr auto stream = stream_config();
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 1> tokens{
        LzssTypedToken{LzssTypedTokenKind::match, 0xCC, 0xCCCCCCCCU,
                       0xCCCCCCCCU}};
    const auto before = tokens;
    std::array raw{std::byte{0xCC}};

    const auto result = decode_lzss_typed_context_frame(
        std::span<const std::byte>{frame}.first(frame.size() - 1),
        {stream, limits, 0, 0}, tokens, raw);
    EXPECT_EQ(result.error,
              LzssTypedContextFrameDecodeError::preflight_error);
    EXPECT_EQ(result.preflight.error,
              TypedContextFramePreflightError::truncated_frame);
    EXPECT_EQ(result.required_token_count, 0U);
    EXPECT_EQ(result.required_raw_size, 0U);
    EXPECT_EQ(tokens[0].kind, before[0].kind);
    EXPECT_EQ(tokens[0].literal, before[0].literal);
    EXPECT_EQ(tokens[0].distance, before[0].distance);
    EXPECT_EQ(tokens[0].length, before[0].length);
    EXPECT_EQ(raw[0], std::byte{0xCC});
}

TEST(LzssTypedContextFrameDecoder, EntropyFailurePreservesAllWorkspace) {
    auto frame = frame_vector();
    frame[80] = std::byte{1};
    constexpr auto stream = stream_config();
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 1> tokens{
        LzssTypedToken{LzssTypedTokenKind::match, 0xCC, 0xCCCCCCCCU,
                       0xCCCCCCCCU}};
    const auto before = tokens;
    std::array raw{std::byte{0xCC}};

    const auto result = decode_lzss_typed_context_frame(
        frame, {stream, limits, 0, 0}, tokens, raw);
    EXPECT_EQ(result.error,
              LzssTypedContextFrameDecodeError::token_decode_error);
    EXPECT_EQ(result.token_decode.error,
              marc::context::internal::LzssContextualRangeDecodeError::
                  entropy_error);
    EXPECT_EQ(result.serialized_consumed, 0U);
    EXPECT_EQ(tokens[0].kind, before[0].kind);
    EXPECT_EQ(tokens[0].literal, before[0].literal);
    EXPECT_EQ(tokens[0].distance, before[0].distance);
    EXPECT_EQ(tokens[0].length, before[0].length);
    EXPECT_EQ(raw[0], std::byte{0xCC});
}

TEST(LzssTypedContextFrameDecoder, CapacityFailuresPrecedeTokenWrites) {
    constexpr auto frame = frame_vector();
    constexpr auto stream = stream_config();
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 1> tokens{
        LzssTypedToken{LzssTypedTokenKind::match, 0xCC, 0xCCCCCCCCU,
                       0xCCCCCCCCU}};
    const auto before = tokens;
    std::array raw{std::byte{0xCC}};

    auto result = decode_lzss_typed_context_frame(
        frame, {stream, limits, 0, 0},
        std::span<LzssTypedToken>{tokens}.first(0), raw);
    EXPECT_EQ(result.error,
              LzssTypedContextFrameDecodeError::token_output_too_small);
    EXPECT_EQ(raw[0], std::byte{0xCC});

    result = decode_lzss_typed_context_frame(
        frame, {stream, limits, 0, 0}, tokens,
        std::span<std::byte>{raw}.first(0));
    EXPECT_EQ(result.error,
              LzssTypedContextFrameDecodeError::raw_output_too_small);
    EXPECT_EQ(tokens[0].kind, before[0].kind);
    EXPECT_EQ(tokens[0].literal, before[0].literal);
    EXPECT_EQ(tokens[0].distance, before[0].distance);
    EXPECT_EQ(tokens[0].length, before[0].length);
    EXPECT_EQ(raw[0], std::byte{0xCC});
}

TEST(LzssTypedContextFrameDecoder, RejectsOverlappingWorkspacesBeforeWriting) {
    constexpr auto frame = frame_vector();
    constexpr auto stream = stream_config();
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 2> storage{};
    auto bytes = std::as_writable_bytes(std::span{storage});
    std::ranges::fill(bytes, std::byte{0xCC});
    const auto before = bytes[0];

    const auto result = decode_lzss_typed_context_frame(
        frame, {stream, limits, 0, 0},
        std::span<LzssTypedToken>{storage}.first(1), bytes.first(1));
    EXPECT_EQ(result.error,
              LzssTypedContextFrameDecodeError::overlapping_workspaces);
    EXPECT_EQ(bytes[0], before);
}

TEST(LzssTypedContextFrameDecoder, RejectsSerializedTokenAliasingBeforeWriting) {
    constexpr auto canonical = frame_vector();
    constexpr auto stream = stream_config();
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 9> storage{};
    auto bytes = std::as_writable_bytes(std::span{storage});
    std::ranges::copy(canonical, bytes.begin());
    const auto before = std::array{
        bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]};
    std::array raw{std::byte{0xCC}};

    const auto result = decode_lzss_typed_context_frame(
        std::span<const std::byte>{bytes}.first(canonical.size()),
        {stream, limits, 0, 0},
        std::span<LzssTypedToken>{storage}.first(1), raw);
    EXPECT_EQ(result.error,
              LzssTypedContextFrameDecodeError::overlapping_workspaces);
    EXPECT_TRUE(std::ranges::equal(
        before, std::span<const std::byte>{bytes}.first(before.size())));
    EXPECT_EQ(raw[0], std::byte{0xCC});
}

TEST(LzssTypedContextFrameDecoder, RejectsSerializedRawAliasingBeforeWriting) {
    auto frame = frame_vector();
    constexpr auto stream = stream_config();
    const auto limits = marc::core::DecoderLimits{};
    std::array<LzssTypedToken, 1> tokens{
        LzssTypedToken{LzssTypedTokenKind::match, 0xCC, 0xCCCCCCCCU,
                       0xCCCCCCCCU}};
    const auto before_token = tokens[0];
    const auto before_byte = frame[0];

    const auto result = decode_lzss_typed_context_frame(
        std::span<const std::byte>{frame}, {stream, limits, 0, 0}, tokens,
        std::span<std::byte>{frame}.first(1));
    EXPECT_EQ(result.error,
              LzssTypedContextFrameDecodeError::overlapping_workspaces);
    EXPECT_EQ(frame[0], before_byte);
    EXPECT_EQ(tokens[0].kind, before_token.kind);
    EXPECT_EQ(tokens[0].literal, before_token.literal);
    EXPECT_EQ(tokens[0].distance, before_token.distance);
    EXPECT_EQ(tokens[0].length, before_token.length);
}
