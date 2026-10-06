#include "context/lzss_position_distance_64m_mapper.hpp"
#include "core/checked_math.hpp"
#include "entropy/lzss_position_distance_64m_compact_range_encoder.hpp"
#include "entropy/lzss_position_distance_64m_range_encoder.hpp"
#include "entropy/lzss_position_distance_64m_token_range_encoder.hpp"
#include "lzss_position_distance_64m_token_range_vectors.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <gtest/gtest.h>
#include <iostream>
#include <limits>
#include <vector>
using namespace marc;
using namespace context::internal;
using namespace entropy::internal;
using Token = dictionary::internal::LzssTypedToken;
using Kind = dictionary::internal::LzssTypedTokenKind;
using Error = LzssPositionDistance64mTokenRangeError;
constexpr auto guard = std::byte{0xa5};
std::vector<std::byte> unhex(std::string_view s) {
  auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
  std::vector<std::byte> r;
  for (size_t i = 0; i < s.size(); i += 2)
    r.push_back(std::byte((digit(s[i]) << 4) | digit(s[i + 1])));
  return r;
}
std::vector<std::byte> canonical(std::span<const Token> tokens) {
  std::vector<std::byte> b;
  for (auto t : tokens) {
    b.push_back(std::byte{static_cast<unsigned char>(t.kind)});
    if (t.kind == Kind::literal)
      b.push_back(std::byte{t.literal});
    else
      for (auto v : {t.distance, t.length})
        for (unsigned i = 0; i < 4; ++i)
          b.push_back(std::byte{static_cast<unsigned char>(v >> (8 * i))});
  }
  return b;
}
struct Fixture {
  std::vector<Token> tokens;
  std::vector<std::byte> expected, out, scratch, compact;
  dictionary::internal::LzssParameters p{67108864, 3, 258, 0};
  core::DecoderLimits limits{};
  LzssFieldContextValidationContext c{};
  ContextualDynamicRangeDescriptor descriptor{123, 456, 7};
  explicit Fixture(const token_range_vectors::Vector &v) {
    limits.max_block_size = 67108864;
    limits.max_frame_size = 67108864;
    limits.max_lz_distance = 67108864;
    c = {v.t, v.e, v.d, v.f, 0};
    expected = unhex(v.payload);
    out.assign(expected.size() + 7, guard);
    scratch = out;
    if (v.tokens.empty()) {
      if (v.name == "literal_rescale")
        tokens.assign(70000, {Kind::literal, 65, 0, 0});
      else if (v.name == "far3" || v.name == "far258") {
        const auto length = v.name == "far3" ? 3u : 258u;
        const auto distance = 67108864u - length;
        tokens.push_back({Kind::literal, 65, 0, 0});
        auto remaining = distance - 1;
        while (remaining >= 258) {
          tokens.push_back({Kind::match, 0, 1, 258});
          remaining -= 258;
        }
        if (remaining >= 3)
          tokens.push_back({Kind::match, 0, 1, remaining});
        else
          while (remaining--)
            tokens.push_back({Kind::literal, 65, 0, 0});
        tokens.push_back({Kind::match, 0, distance, length});
      } else if (v.name == "match_rescale") {
        tokens.push_back({Kind::literal, 65, 0, 0});
        tokens.insert(tokens.end(), 70000, {Kind::match, 0, 1, 5});
      } else {
        tokens.assign(4096, {Kind::literal, 65, 0, 0});
        for (unsigned i = 0; i <= 12; ++i)
          tokens.push_back({Kind::match, 0, 1u << i, 5});
      }
    } else {
      auto b = unhex(v.tokens);
      for (size_t i = 0; i < b.size(); i += 10) {
        Token t{static_cast<Kind>(b[i]), std::to_integer<uint8_t>(b[i + 1]), 0,
                0};
        for (unsigned j = 0; j < 4; ++j) {
          t.distance |= std::to_integer<uint32_t>(b[i + 2 + j]) << (8 * j);
          t.length |= std::to_integer<uint32_t>(b[i + 6 + j]) << (8 * j);
        }
        tokens.push_back(t);
      }
    }
    compact = canonical(tokens);
  }
  auto query(size_t oc, size_t sc, size_t retained = 0) {
    return query_lzss_position_distance_64m_compact_range_encode(
        compact, p, c, limits, oc, sc, retained);
  }
  auto encode(size_t retained = 0) {
    return encode_lzss_position_distance_64m_compact_range(
        compact, p, c, limits, out, scratch, descriptor, retained);
  }
  void failure() {
    auto before = out;
    auto saved = descriptor;
    auto r = encode();
    EXPECT_NE(r.details.error, Error::none);
    EXPECT_EQ(r.bytes_committed, 0u);
    EXPECT_EQ(out, before);
    EXPECT_EQ(std::memcmp(&saved, &descriptor, sizeof(saved)), 0);
  }
};
const auto &named(std::string_view s) {
  return *std::find_if(token_range_vectors::vectors.begin(),
                       token_range_vectors::vectors.end(),
                       [&](auto &v) { return v.name == s; });
}
class CompactMathematicalVectors : public testing::TestWithParam<unsigned> {};
TEST_P(CompactMathematicalVectors, IndependentBytesTypedPathAndConsumer) {
  const auto &v = token_range_vectors::vectors[GetParam()];
  SCOPED_TRACE(v.name);
  Fixture f(v);
  std::vector<std::byte> typed(f.expected.size()), ts(typed.size());
  std::vector<Token> dt(v.t), ds(v.t);
  auto owner = [](const auto &b) { return b.capacity() * sizeof(b[0]); };
  const auto controls =
      sizeof(f) + sizeof(typed) + sizeof(ts) + sizeof(dt) + sizeof(ds) +
      4 * sizeof(ContextualDynamicRangeDescriptor) +
      4 * sizeof(LzssPositionDistance64mTokenRangePlan) +
      4 * sizeof(LzssPositionDistance64mTokenRangeResult) +
      sizeof(LzssContextualRangeDecodeResult) + 16 * sizeof(std::size_t);
  const auto working =
      std::max({lzss_position_distance_64m_compact_range_working_bytes(),
                lzss_position_distance_64m_token_range_working_bytes(),
                lzss_position_distance_64m_token_working_bytes()});
  const auto total = owner(f.tokens) + owner(f.compact) + owner(f.expected) +
                     owner(f.out) + owner(f.scratch) + owner(typed) +
                     owner(ts) + owner(dt) + owner(ds) + controls + working;
  ASSERT_LE(total, f.limits.max_internal_buffered_bytes);
  const auto retained =
      total - f.compact.size() - f.out.size() - f.scratch.size() -
      lzss_position_distance_64m_compact_range_working_bytes();
  auto q = f.query(f.out.size(), f.scratch.size(), retained);
  ASSERT_EQ(q.details.error, Error::none);
  EXPECT_EQ(q.aggregate_bytes, total);
  auto result = f.encode(retained);
  ASSERT_EQ(result.details.error, Error::none);
  ASSERT_EQ(result.bytes_committed, f.expected.size());
  EXPECT_TRUE(std::equal(f.expected.begin(), f.expected.end(), f.out.begin()));
  EXPECT_TRUE(std::ranges::all_of(std::span(f.out).subspan(f.expected.size()),
                                  [](auto b) { return b == guard; }));
  EXPECT_EQ(f.descriptor.decision_count, v.d);
  EXPECT_EQ(f.descriptor.context_count, 50);
  ContextualDynamicRangeDescriptor td{};
  auto old = encode_lzss_position_distance_64m_token_range(
      f.tokens, f.p, f.c, f.limits, typed, ts, td,
      total - f.tokens.size() * sizeof(Token) - typed.size() - ts.size() -
          lzss_position_distance_64m_token_range_working_bytes());
  ASSERT_EQ(old.details.error, Error::none);
  EXPECT_EQ(typed, f.expected);
  auto decoded = decode_lzss_position_distance_64m_tokens(
      f.descriptor, std::span(f.out).first(f.expected.size()), f.p, f.c,
      f.limits, dt, ds,
      total - f.expected.size() - (dt.size() + ds.size()) * sizeof(Token) -
          lzss_position_distance_64m_token_working_bytes());
  ASSERT_EQ(decoded.error, LzssContextualRangeDecodeError::none);
  for (std::size_t i = 0; i < dt.size(); ++i) {
    EXPECT_EQ(dt[i].kind, f.tokens[i].kind);
    EXPECT_EQ(dt[i].literal, f.tokens[i].literal);
    EXPECT_EQ(dt[i].distance, f.tokens[i].distance);
    EXPECT_EQ(dt[i].length, f.tokens[i].length);
  }
  EXPECT_EQ(f.encode(retained).details.error, Error::none);
}
INSTANTIATE_TEST_SUITE_P(Independent, CompactMathematicalVectors,
                         testing::Range(0u, 29u));
TEST(CompactRange, EveryTruncationAndExtraSyntaxPreservesPreviousPayload) {
  Fixture f(named("short_matches"));
  ASSERT_EQ(f.encode().details.error, Error::none);
  const auto valid = f.compact;
  for (std::size_t n = 0; n < valid.size(); ++n) {
    f.compact.assign(valid.begin(), valid.begin() + n);
    f.failure();
  }
  f.compact = valid;
  f.compact.push_back(std::byte{255});
  f.failure();
  f.compact = valid;
  f.compact.push_back(std::byte{0});
  f.compact.push_back(std::byte{65});
  f.failure();
}
TEST(CompactRange, InvalidHistoryAndLengths) {
  for (unsigned change = 0; change < 6; ++change) {
    Fixture f(named("short_matches"));
    auto &t = f.tokens.back();
    switch (change) {
    case 0:
      t.distance = 0;
      break;
    case 1:
      t.distance = 67108865;
      break;
    case 2:
      t.distance = 1000;
      break;
    case 3:
      t.length = 2;
      break;
    case 4:
      t.length = 259;
      break;
    case 5:
      f.p.max_match_length = 257;
      break;
    }
    f.compact = canonical(f.tokens);
    f.failure();
  }
}
TEST(CompactRange, InvalidParametersAndCounts) {
  for (unsigned change = 0; change < 11; ++change) {
    Fixture f(named("short_matches"));
    switch (change) {
    case 0:
      f.p.window_size = 0;
      break;
    case 1:
      f.p.window_size = 67108865;
      break;
    case 2:
      f.p.min_match_length = 2;
      break;
    case 3:
      f.p.max_match_length = 259;
      break;
    case 4:
      f.p.flags = 1;
      break;
    case 5:
      ++f.c.declared_token_count;
      break;
    case 6:
      --f.c.declared_raw_size;
      break;
    case 7:
      ++f.c.declared_raw_size;
      break;
    case 8:
      ++f.c.declared_event_count;
      break;
    case 9:
      ++f.c.declared_decision_count;
      break;
    case 10:
      f.c.declared_raw_size = 0;
      break;
    }
    f.failure();
  }
}
TEST(CompactRange, ConfiguredLimitsAndOverflow) {
  for (unsigned change = 0; change < 8; ++change) {
    Fixture f(named("short_matches"));
    switch (change) {
    case 0:
      f.limits.max_frame_size = f.c.declared_raw_size - 1;
      break;
    case 1:
      f.limits.max_block_size = f.c.declared_raw_size - 1;
      break;
    case 2:
      f.limits.max_total_output_size = f.c.declared_raw_size - 1;
      break;
    case 3:
      f.limits.max_entropy_table_entries = 2631;
      break;
    case 4:
      f.limits.max_range_model_total = 32767;
      break;
    case 5:
      f.limits.max_compressed_payload_size = f.expected.size() - 1;
      break;
    case 6:
      f.c.output_already_committed = UINT64_MAX;
      break;
    case 7:
      f.limits.max_internal_buffered_bytes = 0;
      break;
    }
    f.failure();
  }
  Fixture f(named("literal_A"));
  EXPECT_EQ(f.query(SIZE_MAX, 0).details.error, Error::arithmetic_overflow);
  EXPECT_EQ(f.query(0, SIZE_MAX).details.error, Error::arithmetic_overflow);
  EXPECT_EQ(f.query(0, 0, SIZE_MAX).details.error, Error::arithmetic_overflow);
}
TEST(CompactRange, ExactCapacityAndRetainedLedger) {
  Fixture f(named("short_matches"));
  f.limits.max_block_size = 1024;
  auto q = f.query(f.out.size(), f.scratch.size(), 1234);
  ASSERT_EQ(q.details.error, Error::none);
  EXPECT_EQ(q.aggregate_bytes, f.compact.size() + f.out.size() +
                                   f.scratch.size() + q.working_state_bytes +
                                   1234);
  f.limits.max_internal_buffered_bytes = q.aggregate_bytes;
  EXPECT_EQ(f.encode(1234).details.error, Error::none);
  --f.limits.max_internal_buffered_bytes;
  auto before = f.out;
  auto saved = f.descriptor;
  EXPECT_EQ(f.encode(1234).details.error, Error::limit_exceeded);
  EXPECT_EQ(f.out, before);
  EXPECT_EQ(std::memcmp(&saved, &f.descriptor, sizeof(saved)), 0);
}
TEST(CompactRange, WholeCapacityAndMetadataAliases) {
  Fixture f(named("literal_A"));
  auto before = f.out;
  auto saved = f.descriptor;
  auto call = [&](std::span<std::byte> out, std::span<std::byte> scratch) {
    return encode_lzss_position_distance_64m_compact_range(
        f.compact, f.p, f.c, f.limits, out, scratch, f.descriptor);
  };
  EXPECT_EQ(call(f.out, std::span(f.out).last(1)).details.error,
            Error::overlapping_buffers);
  EXPECT_EQ(f.out, before);
  const auto original = f.compact;
  EXPECT_EQ(call(f.compact, f.scratch).details.error,
            Error::overlapping_buffers);
  EXPECT_EQ(f.compact, original);
  for (auto bytes : {std::as_writable_bytes(std::span(&f.p, 1)),
                     std::as_writable_bytes(std::span(&f.c, 1)),
                     std::as_writable_bytes(std::span(&f.limits, 1)),
                     std::as_writable_bytes(std::span(&f.descriptor, 1))}) {
    const auto old = std::vector<std::byte>(bytes.begin(), bytes.end());
    EXPECT_EQ(call(bytes, f.scratch).details.error, Error::overlapping_buffers);
    EXPECT_TRUE(std::equal(old.begin(), old.end(), bytes.begin()));
  }
  EXPECT_EQ(std::memcmp(&saved, &f.descriptor, sizeof(saved)), 0);
}
TEST(CompactRange, ShortPayloadBuffers) {
  for (bool scratch : {false, true}) {
    Fixture f(named("all_literals"));
    (scratch ? f.scratch : f.out).resize(f.expected.size() - 1);
    f.failure();
  }
}
TEST(CompactRange, GenericLengthFourAndResetDistanceBoundary) {
  Fixture f(named("literal_A"));
  f.tokens.push_back({Kind::match, 0, 1, 4});
  f.compact = canonical(f.tokens);
  f.p.max_match_length = 4;
  f.c = {2, 6, 6, 5, 0};
  f.out.assign(32, guard);
  f.scratch = f.out;
  EXPECT_EQ(f.encode().details.error, Error::none);
  f.p.max_match_length = 258;
  f.tokens.back() = {Kind::match, 0, 67108864, 3};
  f.compact = canonical(f.tokens);
  f.c = {2, 7, 32, 67108864, 0};
  f.failure();
}
