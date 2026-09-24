#include "context/lzss_field_context_format.hpp"
#include "context/lzss_short_match_context_layout.hpp"
#include "dictionary/lzss_typed_token.hpp"
#include "frame/lzss_short_match_preflight.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

namespace {

using namespace marc::frame::internal;

[[nodiscard]] TypedContextStreamHeader short_stream() {
    TypedContextStreamHeader stream{};
    stream.frame_size = 4;
    stream.original_size = 4;
    stream.dictionary.min_match_length = 3;
    stream.range_model_total = typed_context_model_total;
    stream.context_count =
        marc::context::internal::lzss_short_match_context_count;
    stream.dictionary_variant = 7;
    stream.context_variant = 6;
    return stream;
}

[[nodiscard]] constexpr TypedContextFrameHeader short_frame() {
    return {0, 0, 4, 2, 5, 5, 7, 16, 0, 0};
}

[[nodiscard]] constexpr TypedContextRangeDescriptor short_descriptor() {
    return {5, 7, 32};
}

} // namespace

TEST(LzssShortMatchContextLayout, AddsOnlyTheNinthDistanceContext) {
    using namespace marc::context::internal;
    EXPECT_EQ(lzss_field_context_count, 31U);
    EXPECT_EQ(lzss_field_context_frequency_entries_v1, 4518U);
    EXPECT_EQ(lzss_short_match_context_count, 32U);
    EXPECT_EQ(lzss_short_match_frequency_entries, 4538U);
    EXPECT_EQ(lzss_short_match_offsets[20], 4358U);
    EXPECT_EQ(lzss_short_match_offsets[23], 4385U);
    EXPECT_EQ(lzss_short_match_offsets[32], 4538U);
    for (std::size_t index = 0; index < 3; ++index) {
        EXPECT_EQ(lzss_short_match_alphabets[index], 2U);
    }
    for (std::size_t index = 3; index < 20; ++index) {
        EXPECT_EQ(lzss_short_match_alphabets[index], 256U);
    }
    for (std::size_t index = 20; index < 23; ++index) {
        EXPECT_EQ(lzss_short_match_alphabets[index], 9U);
    }
    for (std::size_t index = 23; index < 32; ++index) {
        EXPECT_EQ(lzss_short_match_alphabets[index], 17U);
    }
}

TEST(LzssShortMatchPreflight, ValidatesOnlyTheReservedStreamSemantics) {
    auto stream = short_stream();
    const auto limits = marc::core::DecoderLimits{};
    EXPECT_EQ(validate_lzss_short_match_stream_semantics(stream, limits),
              LzssShortMatchPreflightError::none);

    stream.context_count = 31;
    EXPECT_EQ(validate_lzss_short_match_stream_semantics(stream, limits),
              LzssShortMatchPreflightError::invalid_stream);
    stream = short_stream();
    stream.dictionary_variant = 6;
    EXPECT_EQ(validate_lzss_short_match_stream_semantics(stream, limits),
              LzssShortMatchPreflightError::invalid_stream);
    stream = short_stream();
    stream.context_variant = 5;
    EXPECT_EQ(validate_lzss_short_match_stream_semantics(stream, limits),
              LzssShortMatchPreflightError::invalid_stream);
    stream = short_stream();
    stream.frame_size = 65537;
    EXPECT_EQ(validate_lzss_short_match_stream_semantics(stream, limits),
              LzssShortMatchPreflightError::invalid_stream);
    stream = short_stream();
    stream.dictionary.min_match_length = 5;
    EXPECT_EQ(validate_lzss_short_match_stream_semantics(stream, limits),
              LzssShortMatchPreflightError::invalid_stream);

    auto strict = limits;
    strict.max_entropy_table_entries = 4537;
    EXPECT_EQ(validate_lzss_short_match_stream_semantics(short_stream(),
                                                          strict),
              LzssShortMatchPreflightError::limit_exceeded);
}

TEST(LzssShortMatchPreflight, ComputesCompleteHandVectorRequirements) {
    const auto stream = short_stream();
    const auto limits = marc::core::DecoderLimits{};
    const TypedContextFrameValidationContext context{stream, limits, 0, 0};
    LzssShortMatchFrameRequirements requirements{};
    ASSERT_EQ(preflight_lzss_short_match_frame_semantics(
                  short_frame(), short_descriptor(), context, requirements),
              LzssShortMatchPreflightError::none);
    EXPECT_EQ(requirements.serialized_frame_bytes, 87U);
    EXPECT_EQ(requirements.token_count, 2U);
    EXPECT_EQ(requirements.raw_frame_bytes, 4U);
    constexpr auto model_bytes =
        marc::context::internal::lzss_short_match_frequency_entries
            * sizeof(std::uint16_t)
        + marc::context::internal::lzss_short_match_context_count
            * sizeof(std::uint32_t);
    EXPECT_EQ(requirements.aggregate_working_bytes,
              87U + 2 * sizeof(marc::dictionary::internal::LzssTypedToken)
                  + 4U + model_bytes);
}

TEST(LzssShortMatchPreflight, RejectsCountsAndDescriptorBeforePublication) {
    const auto stream = short_stream();
    const auto limits = marc::core::DecoderLimits{};
    const TypedContextFrameValidationContext context{stream, limits, 0, 0};
    auto frame = short_frame();
    auto descriptor = short_descriptor();
    LzssShortMatchFrameRequirements requirements{777, 777, 777, 777};

    frame.event_count = 9;
    EXPECT_EQ(preflight_lzss_short_match_frame_semantics(
                  frame, descriptor, context, requirements),
              LzssShortMatchPreflightError::contradictory_counts);
    frame = short_frame();
    frame.decision_count = 37;
    EXPECT_EQ(preflight_lzss_short_match_frame_semantics(
                  frame, descriptor, context, requirements),
              LzssShortMatchPreflightError::contradictory_counts);
    frame = short_frame();
    frame.payload_size = 16;
    EXPECT_EQ(preflight_lzss_short_match_frame_semantics(
                  frame, descriptor, context, requirements),
              LzssShortMatchPreflightError::contradictory_counts);
    frame = short_frame();
    descriptor.context_count = 31;
    EXPECT_EQ(preflight_lzss_short_match_frame_semantics(
                  frame, descriptor, context, requirements),
              LzssShortMatchPreflightError::invalid_descriptor);
    EXPECT_EQ(requirements.serialized_frame_bytes, 777U);
    EXPECT_EQ(requirements.token_count, 777U);
    EXPECT_EQ(requirements.raw_frame_bytes, 777U);
    EXPECT_EQ(requirements.aggregate_working_bytes, 777U);
}

TEST(LzssShortMatchPreflight, RejectsSequenceSizeAndWorkspaceLimits) {
    const auto stream = short_stream();
    auto limits = marc::core::DecoderLimits{};
    LzssShortMatchFrameRequirements requirements{777, 777, 777, 777};
    auto frame = short_frame();
    frame.sequence = 1;
    EXPECT_EQ(preflight_lzss_short_match_frame_semantics(
                  frame, short_descriptor(), {stream, limits, 0, 0},
                  requirements),
              LzssShortMatchPreflightError::unexpected_sequence);
    frame = short_frame();
    frame.uncompressed_size = 3;
    EXPECT_EQ(preflight_lzss_short_match_frame_semantics(
                  frame, short_descriptor(), {stream, limits, 0, 0},
                  requirements),
              LzssShortMatchPreflightError::unexpected_frame_size);

    limits.max_block_size = 4;
    constexpr auto model_bytes =
        marc::context::internal::lzss_short_match_frequency_entries
            * sizeof(std::uint16_t)
        + marc::context::internal::lzss_short_match_context_count
            * sizeof(std::uint32_t);
    limits.max_internal_buffered_bytes =
        87 + 2 * sizeof(marc::dictionary::internal::LzssTypedToken)
        + 4 + model_bytes - 1;
    EXPECT_EQ(preflight_lzss_short_match_frame_semantics(
                  short_frame(), short_descriptor(), {stream, limits, 0, 0},
                  requirements),
              LzssShortMatchPreflightError::limit_exceeded);
    EXPECT_EQ(requirements.aggregate_working_bytes, 777U);
}

TEST(LzssShortMatchPreflight, EnforcesMaximumFramePayloadCeiling) {
    auto stream = short_stream();
    stream.frame_size = 65536;
    stream.original_size = 65536;
    constexpr std::uint32_t raw = 65536;
    constexpr std::uint32_t decisions = 9 * raw;
    constexpr std::uint32_t maximum_payload = 18 * raw + 5;
    TypedContextFrameHeader frame{
        0, 0, raw, 21846, 43692, decisions, maximum_payload, 16, 0, 0};
    TypedContextRangeDescriptor descriptor{
        decisions, maximum_payload, 32};
    const auto limits = marc::core::DecoderLimits{};
    LzssShortMatchFrameRequirements requirements{};
    ASSERT_EQ(preflight_lzss_short_match_frame_semantics(
                  frame, descriptor, {stream, limits, 0, 0}, requirements),
              LzssShortMatchPreflightError::none);
    EXPECT_EQ(requirements.serialized_frame_bytes, 18U * raw + 85U);

    ++frame.payload_size;
    ++descriptor.payload_size;
    EXPECT_EQ(preflight_lzss_short_match_frame_semantics(
                  frame, descriptor, {stream, limits, 0, 0}, requirements),
              LzssShortMatchPreflightError::contradictory_counts);
}
