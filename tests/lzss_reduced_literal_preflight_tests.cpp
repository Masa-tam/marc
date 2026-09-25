#include "frame/lzss_reduced_literal_preflight.hpp"
#include "frame/lzss_short_length_escape_preflight.hpp"
#include "entropy/lzss_reduced_literal_range_state.hpp"
#include "dictionary/lzss_typed_token.hpp"

#include <gtest/gtest.h>

namespace {
using namespace marc::frame::internal;
using Error = LzssShortMatchPreflightError;
TypedContextStreamHeader stream_header() {
    TypedContextStreamHeader s{};
    s.frame_size = 7;
    s.original_size = 7;
    s.dictionary = {65536, 3, 258, 0};
    s.range_model_total = typed_context_model_total;
    s.context_count = 24;
    s.dictionary_variant = 8;
    s.context_variant = 8;
    return s;
}
constexpr TypedContextFrameHeader frame_header() { return {0,0,7,5,12,12,8,16,0,0}; }
constexpr TypedContextRangeDescriptor descriptor() { return {12,8,24}; }

TEST(LzssReducedLiteralPreflight, ExactMemoryThresholdAndUnchangedFailureOutput) {
    const auto s = stream_header();
    auto limits = marc::core::DecoderLimits{};
    LzssShortMatchFrameRequirements r{};
    ASSERT_EQ(preflight_lzss_reduced_literal_frame_semantics(frame_header(), descriptor(), {s,limits}, r), Error::none);
    const auto expected = 88 + 5 * sizeof(marc::dictionary::internal::LzssTypedToken)
        + 7 + sizeof(marc::entropy::internal::LzssReducedLiteralRangeState);
    EXPECT_EQ(r.serialized_frame_bytes, 88);
    EXPECT_EQ(r.token_count, 5);
    EXPECT_EQ(r.raw_frame_bytes, 7);
    EXPECT_EQ(r.aggregate_working_bytes, expected);
    limits.max_block_size = 7;
    limits.max_internal_buffered_bytes = expected;
    ASSERT_EQ(marc::core::validate_limits(limits), marc::core::LimitError::none);
    EXPECT_EQ(preflight_lzss_reduced_literal_frame_semantics(frame_header(), descriptor(), {s,limits}, r), Error::none);
    --limits.max_internal_buffered_bytes;
    ASSERT_EQ(marc::core::validate_limits(limits), marc::core::LimitError::none);
    r = {11,22,33,44};
    EXPECT_EQ(preflight_lzss_reduced_literal_frame_semantics(frame_header(), descriptor(), {s,limits}, r), Error::limit_exceeded);
    EXPECT_EQ(r.serialized_frame_bytes, 11);
    EXPECT_EQ(r.token_count, 22);
    EXPECT_EQ(r.raw_frame_bytes, 33);
    EXPECT_EQ(r.aggregate_working_bytes, 44);
}

TEST(LzssReducedLiteralPreflight, RejectsOldPairsAndDescriptorCounts) {
    auto s = stream_header();
    const auto limits = marc::core::DecoderLimits{};
    EXPECT_EQ(validate_lzss_reduced_literal_stream_semantics(s, limits), Error::none);
    EXPECT_EQ(validate_lzss_short_length_escape_stream_semantics(s, limits), Error::invalid_stream);
    s.context_variant = 7; s.context_count = 32;
    EXPECT_EQ(validate_lzss_reduced_literal_stream_semantics(s, limits), Error::invalid_stream);
    EXPECT_EQ(validate_lzss_short_length_escape_stream_semantics(s, limits), Error::none);
    s = stream_header();
    LzssShortMatchFrameRequirements r{};
    auto d = descriptor(); d.context_count = 32;
    EXPECT_EQ(preflight_lzss_reduced_literal_frame_semantics(frame_header(), d, {s,limits}, r), Error::invalid_descriptor);
    s.context_count = 32;
    EXPECT_EQ(validate_lzss_reduced_literal_stream_semantics(s, limits), Error::invalid_stream);
    s = stream_header(); s.dictionary_variant = 7;
    EXPECT_EQ(validate_lzss_reduced_literal_stream_semantics(s, limits), Error::invalid_stream);
}

TEST(LzssReducedLiteralPreflight, EnforcesTablePayloadAndOutputLimits) {
    const auto s = stream_header();
    auto limits = marc::core::DecoderLimits{};
    limits.max_entropy_table_entries = 2490;
    EXPECT_EQ(validate_lzss_reduced_literal_stream_semantics(s, limits), Error::none);
    --limits.max_entropy_table_entries;
    EXPECT_EQ(validate_lzss_reduced_literal_stream_semantics(s, limits), Error::limit_exceeded);
    limits = {}; limits.max_total_output_size = 6;
    limits.max_frame_size = 6;
    ASSERT_EQ(marc::core::validate_limits(limits), marc::core::LimitError::none);
    EXPECT_EQ(validate_lzss_reduced_literal_stream_semantics(s, limits), Error::limit_exceeded);
    limits = {}; limits.max_frame_size = 6;
    EXPECT_EQ(validate_lzss_reduced_literal_stream_semantics(s, limits), Error::limit_exceeded);
    limits = {}; limits.max_compressed_payload_size = 7;
    LzssShortMatchFrameRequirements r{};
    EXPECT_EQ(preflight_lzss_reduced_literal_frame_semantics(frame_header(), descriptor(), {s,limits}, r), Error::limit_exceeded);
}

TEST(LzssReducedLiteralPreflight, RejectsContradictionsAndAllowsOnlyFinalShortFrame) {
    auto s = stream_header();
    const auto limits = marc::core::DecoderLimits{};
    LzssShortMatchFrameRequirements r{};
    auto f = frame_header(); f.event_count = 9;
    EXPECT_EQ(preflight_lzss_reduced_literal_frame_semantics(f, descriptor(), {s,limits}, r), Error::contradictory_counts);
    f = frame_header(); f.payload_size = UINT32_MAX;
    EXPECT_EQ(preflight_lzss_reduced_literal_frame_semantics(f, descriptor(), {s,limits}, r), Error::contradictory_counts);
    f = frame_header(); f.flags = 1;
    EXPECT_EQ(preflight_lzss_reduced_literal_frame_semantics(f, descriptor(), {s,limits}, r), Error::unsupported_feature);
    f = frame_header(); f.sequence = 1;
    EXPECT_EQ(preflight_lzss_reduced_literal_frame_semantics(f, descriptor(), {s,limits}, r), Error::unexpected_sequence);
    s.frame_size = 65536;
    EXPECT_EQ(preflight_lzss_reduced_literal_frame_semantics(frame_header(), descriptor(), {s,limits}, r), Error::none);
    s.original_size = 65536;
    EXPECT_EQ(preflight_lzss_reduced_literal_frame_semantics(frame_header(), descriptor(), {s,limits}, r), Error::unexpected_frame_size);
    EXPECT_EQ(preflight_lzss_reduced_literal_frame_semantics(frame_header(), descriptor(), {s,limits,0,65529}, r), Error::none);
}
} // namespace
