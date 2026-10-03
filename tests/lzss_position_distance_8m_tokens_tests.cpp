#include "context/lzss_position_distance_8m_tokens.hpp"
#include "lzss_position_distance_8m_token_vectors.hpp"
#include <gtest/gtest.h>
#include <limits>
#include <vector>
namespace {
using namespace marc;
using namespace context::internal;
using namespace dictionary::internal;
using Error = LzssContextualRangeDecodeError;
using TokenError = LzssTypedTokenError;
core::DecoderLimits limits() {
  auto l = core::DecoderLimits{};
  l.max_frame_size = 8388608;
  l.max_block_size = 8388608;
  l.max_lz_distance = 8388608;
  return l;
}
LzssParameters parameters() { return {8388608, 3, 258, 0}; }
LzssTypedToken literal(std::uint8_t v) {
  return {LzssTypedTokenKind::literal, v, 0, 0};
}
LzssTypedToken match(std::uint32_t d, std::uint32_t n) {
  return {LzssTypedTokenKind::match, 0, d, n};
}
const auto sentinel = match(12345, 54321);
void equal(const LzssTypedToken &a, const LzssTypedToken &b) {
  EXPECT_EQ(a.kind, b.kind);
  EXPECT_EQ(a.literal, b.literal);
  EXPECT_EQ(a.distance, b.distance);
  EXPECT_EQ(a.length, b.length);
}
void unchanged(const std::vector<LzssTypedToken> &a) {
  for (const auto &t : a)
    equal(t, sentinel);
}
struct Fixture {
  std::span<const std::byte> payload;
  entropy::internal::ContextualDynamicRangeDescriptor descriptor;
  LzssFieldContextValidationContext context;
};
template <std::size_t N>
Fixture fixture(const std::array<std::uint8_t, N> &b, std::uint32_t t,
                std::uint32_t e, std::uint32_t d, std::uint32_t f) {
  return {std::as_bytes(std::span(b)),
          {d, static_cast<std::uint32_t>(N), 47},
          {t, e, d, f, 0}};
}
#define FIX(name)                                                              \
  fixture(token_vectors::name, token_vectors::name##_tokens,                   \
          token_vectors::name##_events, token_vectors::name##_decisions,       \
          token_vectors::name##_raw)
auto decode(const Fixture &f, std::span<LzssTypedToken> out,
            std::span<LzssTypedToken> scratch,
            const LzssParameters &p = parameters(),
            const core::DecoderLimits &l = limits(), std::size_t retained = 0) {
  return decode_lzss_position_distance_8m_tokens(
      f.descriptor, f.payload, p, f.context, l, out, scratch, retained);
}
auto query(const Fixture &f, std::size_t out, std::size_t scratch,
           const core::DecoderLimits &l = limits(), std::size_t retained = 0) {
  return query_lzss_position_distance_8m_tokens(f.descriptor, f.payload,
                                                parameters(), f.context, l, out,
                                                scratch, retained);
}
std::vector<LzssTypedToken> success(const Fixture &f) {
  std::vector<LzssTypedToken> out(f.context.declared_token_count + 2, sentinel),
      scratch(f.context.declared_token_count + 3, sentinel);
  const auto r = decode(f, out, scratch);
  EXPECT_EQ(r.error, Error::none);
  EXPECT_EQ(r.raw_size, f.context.declared_raw_size);
  EXPECT_EQ(r.token_count, f.context.declared_token_count);
  equal(out[out.size() - 1], sentinel);
  equal(out[out.size() - 2], sentinel);
  equal(scratch.back(), sentinel);
  out.resize(f.context.declared_token_count);
  return out;
}
void failure(const Fixture &f, Error expected,
             const LzssParameters &p = parameters()) {
  std::vector<LzssTypedToken> out(f.context.declared_token_count + 1, sentinel),
      scratch(out.size(), sentinel);
  const auto r = decode(f, out, scratch, p);
  EXPECT_EQ(r.error, expected);
  unchanged(out);
}
TEST(PositionDistance8mTokens, LiteralAndTailGuards) {
  const auto f = FIX(literal);
  auto out = success(f);
  equal(out[0], literal(65));
  const auto q = query(f, 3, 4);
  EXPECT_EQ(q.error, Error::none);
  EXPECT_EQ(q.aggregate_bytes, f.payload.size() + 7 * sizeof(LzssTypedToken) +
                                   q.working_state_bytes);
  RecordProperty("working_state_bytes",
                 static_cast<int>(q.working_state_bytes));
}
TEST(PositionDistance8mTokens, ShortOverlapAndIndependentRawOracle) {
  auto out = success(FIX(short_overlap));
  std::vector<LzssTypedToken> expected = {literal(65), match(1, 3), match(1, 4),
                                          literal(66), match(2, 258)};
  ASSERT_EQ(out.size(), expected.size());
  std::vector<std::uint8_t> raw;
  for (std::size_t i = 0; i < out.size(); ++i) {
    equal(out[i], expected[i]);
    auto t = out[i];
    if (t.kind == LzssTypedTokenKind::literal)
      raw.push_back(t.literal);
    else
      for (std::uint32_t n = 0; n < t.length; ++n)
        raw.push_back(raw[raw.size() - t.distance]);
  }
  ASSERT_EQ(raw.size(), 267);
  for (std::size_t i = 0; i < raw.size(); ++i)
    EXPECT_EQ(raw[i], i < 8 ? 65 : i % 2 ? 65 : 66);
}
TEST(PositionDistance8mTokens, EveryLength) {
  auto out = success(FIX(all_lengths));
  ASSERT_EQ(out.size(), 257);
  equal(out[0], literal(65));
  for (std::size_t i = 1; i < out.size(); ++i)
    equal(out[i], match(1, static_cast<std::uint32_t>(i + 2)));
}
TEST(PositionDistance8mTokens, UpperHalfDistanceWithRealFrameHistory) {
  auto out = success(FIX(upper_half));
  ASSERT_GT(out.size(), 2);
  equal(out.front(), literal(65));
  equal(out.back(), match(4194305, 3));
  std::uint64_t position{};
  for (std::size_t i = 0; i + 1 < out.size(); ++i)
    position += i ? out[i].length : 1;
  EXPECT_EQ(position, 4194305);
}
TEST(PositionDistance8mTokens, ExactEightMiBOutputBoundary) {
  auto out = success(FIX(maximum_history));
  ASSERT_EQ(out.size(), 32515);
  equal(out.front(), literal(65));
  equal(out.back(), match(1, 253));
  std::uint64_t raw = 1;
  for (std::size_t i = 1; i < out.size(); ++i)
    raw += out[i].length;
  EXPECT_EQ(raw, 8388608);
}
TEST(PositionDistance8mTokens, ParametersAndCallerDistanceMatchLimits) {
  for (int i = 0; i < 6; ++i) {
    auto p = parameters();
    switch (i) {
    case 0:
      p.window_size = 0;
      break;
    case 1:
      p.window_size = 8388609;
      break;
    case 2:
      p.min_match_length = 2;
      break;
    case 3:
      p.max_match_length = 2;
      break;
    case 4:
      p.max_match_length = 259;
      break;
    case 5:
      p.flags = 1;
      break;
    }
    EXPECT_EQ(validate_lzss_position_distance_8m_parameters(p, limits()),
              TokenError::invalid_parameters);
    failure(FIX(literal), Error::invalid_parameters, p);
  }
  auto l = limits();
  --l.max_lz_distance;
  EXPECT_EQ(validate_lzss_position_distance_8m_parameters(parameters(), l),
            TokenError::limit_exceeded);
  l = limits();
  l.max_lz_match_length = 257;
  EXPECT_EQ(validate_lzss_position_distance_8m_parameters(parameters(), l),
            TokenError::limit_exceeded);
}
TEST(PositionDistance8mTokens, ValidatorKindsUnusedFields) {
  auto p = parameters();
  for (auto t : {LzssTypedToken{static_cast<LzssTypedTokenKind>(2), 0, 0, 0},
                 LzssTypedToken{LzssTypedTokenKind::literal, 65, 1, 0},
                 LzssTypedToken{LzssTypedTokenKind::match, 65, 1, 3}}) {
    auto r = validate_lzss_position_distance_8m_token(t, p, {1, 4}, limits());
    EXPECT_NE(r.error, TokenError::none);
    EXPECT_EQ(r.next_raw_size, 0);
  }
}
TEST(PositionDistance8mTokens, ValidatorHistoryWindowAndRemainingOutput) {
  auto p = parameters();
  for (auto d : {0u, 2u, 8388609u}) {
    auto r = validate_lzss_position_distance_8m_token(match(d, 3), p, {1, 4},
                                                      limits());
    EXPECT_EQ(r.error, TokenError::invalid_distance);
    EXPECT_EQ(r.next_raw_size, 0);
  }
  for (auto n : {2u, 259u})
    EXPECT_EQ(validate_lzss_position_distance_8m_token(match(1, n), p, {1, 300},
                                                       limits())
                  .error,
              TokenError::invalid_length);
  EXPECT_EQ(validate_lzss_position_distance_8m_token(match(1, 258), p, {1, 258},
                                                     limits())
                .error,
            TokenError::output_size_mismatch);
  EXPECT_EQ(
      validate_lzss_position_distance_8m_token(match(1, 3), p, {1, 4}, limits())
          .next_raw_size,
      4);
  EXPECT_EQ(validate_lzss_position_distance_8m_token(
                literal(65), p, {8388608, 8388608}, limits())
                .error,
            TokenError::output_size_mismatch);
  EXPECT_EQ(validate_lzss_position_distance_8m_token(literal(65), p,
                                                     {0, 8388609}, limits())
                .error,
            TokenError::limit_exceeded);
}
TEST(PositionDistance8mTokens, EndpointCannotFitValidFrameHistory) {
  for (auto d : {8388605u, 8388606u, 8388607u, 8388608u}) {
    const auto r = validate_lzss_position_distance_8m_token(
        match(d, 3), parameters(), {8388605, 8388608}, limits());
    if (d == 8388605)
      EXPECT_EQ(r.next_raw_size, 8388608);
    else {
      EXPECT_EQ(r.error, TokenError::invalid_distance);
      EXPECT_EQ(r.next_raw_size, 0);
    }
  }
}
TEST(PositionDistance8mTokens, InvalidBeforeHistoryAndWindow) {
  failure(FIX(before_history), Error::invalid_token);
  failure(FIX(history_overrun), Error::invalid_token);
  auto p = parameters();
  p.window_size = 1;
  failure(FIX(window_overrun), Error::invalid_token, p);
  failure(FIX(endpoint), Error::invalid_token);
}
TEST(PositionDistance8mTokens, RawOverflowAndFinalMismatch) {
  auto f = FIX(raw_overrun);
  f.context.declared_raw_size = 3;
  failure(f, Error::invalid_token);
  f = FIX(declared_long);
  f.context.declared_raw_size = 5;
  failure(f, Error::raw_size_mismatch);
}
TEST(PositionDistance8mTokens, InvalidLengthCanonicalGrammar) {
  failure(FIX(invalid_length), Error::entropy_error);
}
TEST(PositionDistance8mTokens, CanonicalFailureLeavesOutputAndScratchPrefix) {
  auto f = FIX(short_overlap);
  std::vector<std::byte> p(f.payload.begin(), f.payload.end());
  p.back() ^= std::byte{1};
  f.payload = p;
  std::vector<LzssTypedToken> out(5, sentinel), scratch(5, sentinel);
  EXPECT_EQ(decode(f, out, scratch).error, Error::entropy_error);
  unchanged(out);
  equal(scratch[0], literal(65));
}
TEST(PositionDistance8mTokens, EveryTruncationPreservesOutput) {
  auto f = FIX(short_overlap);
  for (std::size_t n = 0; n < f.payload.size(); ++n) {
    auto short_f = f;
    short_f.payload = f.payload.first(n);
    short_f.descriptor.payload_size = static_cast<std::uint32_t>(n);
    std::vector<LzssTypedToken> out(5, sentinel), scratch(5, sentinel);
    EXPECT_NE(decode(short_f, out, scratch).error, Error::none);
    unchanged(out);
  }
}
TEST(PositionDistance8mTokens, CountsAndDescriptorsRejected) {
  auto f = FIX(short_overlap);
  for (int i = 0; i < 10; ++i) {
    auto bad = f;
    switch (i) {
    case 0:
      bad.context.declared_token_count = 0;
      break;
    case 1:
      bad.context.declared_token_count = 268;
      break;
    case 2:
      bad.context.declared_event_count = 9;
      break;
    case 3:
      bad.context.declared_event_count = 26;
      break;
    case 4:
      bad.context.declared_decision_count =
          bad.context.declared_event_count - 1;
      break;
    case 5:
      bad.context.declared_decision_count = 166;
      bad.descriptor.decision_count = 166;
      break;
    case 6:
      bad.descriptor.context_count = 46;
      break;
    case 7:
      ++bad.descriptor.decision_count;
      break;
    case 8:
      ++bad.descriptor.payload_size;
      break;
    case 9:
      bad.context.declared_raw_size = 8388609;
      break;
    }
    failure(bad, Error::invalid_counts);
  }
}
TEST(PositionDistance8mTokens, UnexpectedExtraTokenCanonicalFinish) {
  auto f = FIX(extra_token);
  f.context.declared_token_count = 1;
  f.context.declared_raw_size = 2;
  failure(f, Error::entropy_error);
}
TEST(PositionDistance8mTokens, ExactFullCapacityBudgetAndOneByteUnder) {
  auto f = FIX(literal);
  auto q = query(f, 3, 4, limits(), 123);
  ASSERT_EQ(q.error, Error::none);
  auto l = limits();
  l.max_block_size = 1;
  l.max_internal_buffered_bytes = q.aggregate_bytes;
  EXPECT_EQ(query(f, 3, 4, l, 123).error, Error::none);
  std::vector<LzssTypedToken> out(3, sentinel), scratch(4, sentinel);
  EXPECT_EQ(decode(f, out, scratch, parameters(), l, 123).error, Error::none);
  --l.max_internal_buffered_bytes;
  out.assign(3, sentinel);
  EXPECT_EQ(decode(f, out, scratch, parameters(), l, 123).error,
            Error::limit_exceeded);
  unchanged(out);
  EXPECT_EQ(query(f, 3, 5, l, 123).error, Error::limit_exceeded);
}
TEST(PositionDistance8mTokens, CapacityAndCheckedArithmetic) {
  auto f = FIX(literal);
  std::vector<LzssTypedToken> out(1, sentinel), scratch(1, sentinel);
  EXPECT_EQ(decode(f, std::span<LzssTypedToken>{}, scratch).error,
            Error::output_too_small);
  EXPECT_EQ(decode(f, out, std::span<LzssTypedToken>{}).error,
            Error::output_too_small);
  unchanged(out);
  const auto max = std::numeric_limits<std::size_t>::max();
  EXPECT_EQ(query(f, max, 1).error, Error::arithmetic_overflow);
  EXPECT_EQ(query(f, 1, 1, limits(), max).error, Error::arithmetic_overflow);
  auto overflow = f;
  overflow.context.output_already_committed = UINT64_MAX;
  EXPECT_EQ(query(overflow, 1, 1).error, Error::arithmetic_overflow);
}
TEST(PositionDistance8mTokens, TotalFrameBlockPayloadModelAndExpansionLimits) {
  auto f = FIX(short_overlap);
  for (int i = 0; i < 7; ++i) {
    auto l = limits();
    switch (i) {
    case 0:
      l.max_total_output_size = 266;
      l.max_frame_size = 266;
      break;
    case 1:
      l.max_frame_size = 266;
      break;
    case 2:
      l.max_block_size = 266;
      break;
    case 3:
      l.max_compressed_payload_size = f.payload.size() - 1;
      break;
    case 4:
      l.max_entropy_table_entries = 2598;
      break;
    case 5:
      l.max_range_model_total = 32767;
      break;
    case 6:
      l.max_expansion_ratio = 1;
      l.expansion_slack = 0;
      break;
    }
    std::vector<LzssTypedToken> out(5, sentinel), scratch(5, sentinel);
    EXPECT_EQ(decode(f, out, scratch, parameters(), l).error,
              Error::limit_exceeded);
    unchanged(out);
  }
}
TEST(PositionDistance8mTokens, BothBuffersAndUnusedTailsCannotOverlap) {
  auto f = FIX(literal);
  std::vector<LzssTypedToken> storage(5, sentinel);
  EXPECT_EQ(
      decode(f, std::span(storage).first(3), std::span(storage).subspan(2))
          .error,
      Error::overlapping_buffers);
  unchanged(storage);
  EXPECT_EQ(
      decode(f, std::span(storage).first(1), std::span(storage).subspan(1, 1))
          .error,
      Error::none);
}
TEST(PositionDistance8mTokens, PayloadOverlapRejectedBeforeWrite) {
  auto f = FIX(literal);
  std::vector<LzssTypedToken> out(3, sentinel), scratch(3, sentinel);
  auto p = std::as_bytes(std::span(out));
  f.payload = p.first(6);
  EXPECT_EQ(decode(f, out, scratch).error, Error::overlapping_buffers);
  unchanged(out);
  unchanged(scratch);
  f.payload = std::as_bytes(std::span(scratch)).first(6);
  EXPECT_EQ(decode(f, out, scratch).error, Error::overlapping_buffers);
  unchanged(out);
  unchanged(scratch);
}
TEST(PositionDistance8mTokens, FullTailInputOverlapIsRejected) {
  auto f = FIX(literal);
  std::vector<LzssTypedToken> out(3, sentinel), scratch(3, sentinel);
  f.payload = std::as_bytes(std::span(out).subspan(2)).first(6);
  EXPECT_EQ(decode(f, out, scratch).error, Error::overlapping_buffers);
  unchanged(out);
  unchanged(scratch);
}
} // namespace
