#include "context/lzss_position_distance_32m_mapper.hpp"
#include "dictionary/lzss_position_distance_32m_reference.hpp"
#include "entropy/lzss_position_distance_32m_range_encoder.hpp"
#include "lzss_position_distance_32m_reference_oracle.hpp"
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
namespace {
using namespace marc;
using namespace dictionary::internal;
using E = LzssPositionDistance32mParseError;
constexpr LzssTypedToken guard{LzssTypedTokenKind::literal, 0xa5, 77, 88};
LzssParameters params() { return {33554432, 3, 258, 0}; }
core::DecoderLimits limits() {
  core::DecoderLimits l{};
  l.max_block_size = 33554432;
  l.max_frame_size = 33554432;
  l.max_lz_distance = 33554432;
  l.max_internal_buffered_bytes = 512u * 1024u * 1024u;
  return l;
}
std::vector<std::byte> bytes(std::string_view s) {
  std::vector<std::byte> out;
  for (unsigned char b : s)
    out.push_back(std::byte{b});
  return out;
}
std::vector<LzssTypedToken> checked(std::span<const std::byte> raw,
                                    LzssParameters p = params(),
                                    bool oracle = true) {
  auto l = limits();
  const auto q = query_lzss_position_distance_32m_reference(raw, p, l, 0, 0);
  EXPECT_EQ(q.error, E::output_too_small);
  if (q.error != E::output_too_small)
    return {};
  std::vector<LzssTypedToken> out(q.details.token_count + 3, guard),
      scratch(q.details.token_count + 5, guard);
  LzssPositionDistance32mParseMetadata meta{777, 888};
  auto r = tokenize_lzss_position_distance_32m_reference(raw, p, l, out,
                                                         scratch, meta);
  EXPECT_EQ(r.error, E::none);
  EXPECT_EQ(r.tokens_committed, q.details.token_count);
  EXPECT_EQ(meta.raw_size, raw.size());
  if (oracle) {
    auto expected = reference_oracle::parse(raw, p);
    EXPECT_EQ(expected.size(), r.tokens_committed);
    for (std::size_t i = 0; i < expected.size(); ++i)
      EXPECT_TRUE(reference_oracle::equal(out[i], expected[i]));
  }
  for (std::size_t i = r.tokens_committed; i < out.size(); ++i)
    EXPECT_TRUE(reference_oracle::equal(out[i], guard));
  for (std::size_t i = r.tokens_committed; i < scratch.size(); ++i)
    EXPECT_TRUE(reference_oracle::equal(scratch[i], guard));
  out.resize(r.tokens_committed);
  EXPECT_EQ(reference_oracle::reconstruct(out),
            std::vector<std::byte>(raw.begin(), raw.end()));
  return out;
}
void failure(std::span<const std::byte> raw, E expected,
             LzssParameters p = params(), core::DecoderLimits l = limits(),
             std::size_t oc = 512, std::size_t sc = 512,
             std::size_t retained = 0, std::uint64_t committed = 0) {
  std::vector<LzssTypedToken> out(oc, guard), scratch(sc, guard);
  LzssPositionDistance32mParseMetadata meta{777, 888};
  std::array<std::byte, sizeof(meta)> original{};
  std::memcpy(original.data(), &meta, sizeof(meta));
  auto r = tokenize_lzss_position_distance_32m_reference(
      raw, p, l, out, scratch, meta, retained, committed);
  EXPECT_EQ(r.error, expected);
  EXPECT_EQ(r.tokens_committed, 0);
  EXPECT_EQ(std::memcmp(original.data(), &meta, sizeof(meta)), 0);
  for (auto t : out)
    EXPECT_TRUE(reference_oracle::equal(t, guard));
}
void pipeline(std::span<const std::byte> raw) {
  auto tokens = checked(raw);
  auto p = params();
  auto l = limits();
  context::internal::LzssFieldContextValidationContext c{};
  c.declared_token_count = static_cast<std::uint32_t>(tokens.size());
  c.declared_raw_size = static_cast<std::uint32_t>(raw.size());
  for (auto t : tokens) {
    if (t.kind == LzssTypedTokenKind::literal) {
      c.declared_event_count += 2;
      c.declared_decision_count += 2;
    } else {
      unsigned lc = 0;
      while ((1u << (lc + 1)) <= t.length - 4)
        ++lc;
      unsigned dc = 0;
      while ((1u << (dc + 1)) <= t.distance)
        ++dc;
      c.declared_event_count += 3 + bool(lc) + bool(dc);
      c.declared_decision_count += 3 + lc + dc;
    }
  }
  std::vector<context::internal::ModeledOperation> ops(c.declared_event_count),
      op_scratch(ops.size());
  context::internal::LzssFieldContextResult meta{};
  auto m = context::internal::map_lzss_position_distance_32m_tokens(
      tokens, p, c, l, ops, op_scratch, meta);
  ASSERT_EQ(m.details.error, context::internal::LzssFieldContextError::none);
  std::vector<std::byte> out(2 * c.declared_decision_count + 5),
      scratch(out.size());
  entropy::internal::ContextualDynamicRangeDescriptor desc{};
  auto enc =
      entropy::internal::encode_lzss_position_distance_32m_range_operations(
          ops, l, out, scratch, desc);
  ASSERT_EQ(enc.details.error,
            entropy::internal::ContextualDynamicRangeEncodeError::none);
  std::vector<LzssTypedToken> decoded(tokens.size()), ts(tokens.size());
  auto dec = context::internal::decode_lzss_position_distance_32m_tokens(
      desc, std::span(out).first(enc.bytes_committed), p, c, l, decoded, ts);
  ASSERT_EQ(dec.error, context::internal::LzssContextualRangeDecodeError::none);
  for (std::size_t i = 0; i < tokens.size(); ++i)
    EXPECT_TRUE(reference_oracle::equal(tokens[i], decoded[i]));
  EXPECT_EQ(reference_oracle::reconstruct(decoded),
            std::vector<std::byte>(raw.begin(), raw.end()));
}
TEST(PositionDistance32mReference, EmptyRejected) {
  failure({}, E::invalid_parameters);
}
TEST(PositionDistance32mReference, EverySingleByte) {
  for (unsigned b = 0; b < 256; ++b) {
    std::array raw{std::byte{static_cast<unsigned char>(b)}};
    auto t = checked(raw);
    ASSERT_EQ(t.size(), 1);
    EXPECT_EQ(t[0].literal, b);
  }
}
TEST(PositionDistance32mReference, HandOverlap) {
  auto t = checked(bytes("aaaaaaaaaa"));
  ASSERT_EQ(t.size(), 2);
  EXPECT_EQ(t[1].distance, 1);
  EXPECT_EQ(t[1].length, 9);
}
TEST(PositionDistance32mReference, ShortMatchesStayLiterals) {
  for (unsigned n = 2; n <= 5; ++n) {
    auto t = checked(std::vector<std::byte>(n, std::byte{65}));
    EXPECT_EQ(t.size(), n);
  }
  auto t = checked(std::vector<std::byte>(6, std::byte{65}));
  EXPECT_EQ(t.size(), 2);
}
TEST(PositionDistance32mReference, NearestTie) {
  auto t = checked(bytes("abcde0abcde1abcde2"));
  ASSERT_EQ(t.size(), 10);
  EXPECT_EQ(t[8].kind, LzssTypedTokenKind::match);
  EXPECT_EQ(t[8].distance, 6);
  EXPECT_EQ(t[8].length, 5);
}
TEST(PositionDistance32mReference, LongerWinsOverNearer) {
  checked(bytes("abcdefghXabcdeYabcdefghZ"));
}
TEST(PositionDistance32mReference, WindowBoundary) {
  auto raw = bytes("abcde012345abcde");
  for (unsigned w : {9, 10, 11, 12}) {
    auto p = params();
    p.window_size = w;
    auto t = checked(raw, p);
    EXPECT_EQ(t.back().kind, w >= 11 ? LzssTypedTokenKind::match
                                     : LzssTypedTokenKind::literal);
  }
}
TEST(PositionDistance32mReference, MatchLengthBoundaries) {
  for (unsigned n : {257, 258, 259, 260, 516, 517})
    checked(std::vector<std::byte>(n, std::byte{65}));
  for (unsigned max : {3, 4, 5, 17, 257, 258}) {
    auto p = params();
    p.max_match_length = max;
    checked(std::vector<std::byte>(520, std::byte{65}), p);
  }
}
TEST(PositionDistance32mReference, ExhaustiveSmallBinary) {
  for (unsigned n = 1; n <= 10; ++n)
    for (unsigned mask = 0; mask < (1u << n); ++mask) {
      std::vector<std::byte> raw(n);
      for (unsigned i = 0; i < n; ++i)
        raw[i] = std::byte{static_cast<unsigned char>((mask >> i) & 1)};
      for (unsigned w : {1, 3, 33554432}) {
        auto p = params();
        p.window_size = w;
        checked(raw, p);
      }
    }
}
TEST(PositionDistance32mReference, DeterministicRandomAndPatterns) {
  unsigned state = 1426;
  for (unsigned run = 0; run < 100; ++run) {
    std::vector<std::byte> raw(1 + run * 3);
    for (auto &b : raw) {
      state = state * 1664525u + 1013904223u;
      b = std::byte{
          static_cast<unsigned char>((state >> 24) % (run % 2 ? 4 : 256))};
    }
    checked(raw);
  }
}
TEST(PositionDistance32mReference, RangeTokenRawDifferential) {
  for (auto s : {"a", "aaaaaaaaaaaaaaaa",
                 "abcdef012345abcdef012345abcdef012345", "abcde0abcde1abcde2"})
    pipeline(bytes(s));
  pipeline(std::vector<std::byte>(520, std::byte{65}));
}
TEST(PositionDistance32mReference, MaximumFrameRepetitive) {
  std::vector<std::byte> raw(33554432, std::byte{65});
  auto p = params();
  auto l = limits();
  constexpr std::size_t count = 130057;
  std::vector<LzssTypedToken> out(count), scratch(count);
  LzssPositionDistance32mParseMetadata meta{};
  auto r = tokenize_lzss_position_distance_32m_reference(raw, p, l, out,
                                                         scratch, meta);
  ASSERT_EQ(r.error, E::none);
  ASSERT_EQ(r.tokens_committed, count);
  EXPECT_EQ(out.back().length, 241);
  EXPECT_EQ(meta.raw_size, 33554432);
  EXPECT_EQ(reference_oracle::reconstruct(out), raw);
}
TEST(PositionDistance32mReference, ReachableFarFive) {
  std::vector<std::byte> raw(33554432, std::byte{});
  const auto phrase = bytes("abcde");
  std::copy(phrase.begin(), phrase.end(), raw.begin());
  std::copy(phrase.begin(), phrase.end(), raw.end() - 5);
  const auto tokens = checked(raw, params(), false);
  ASSERT_FALSE(tokens.empty());
  EXPECT_EQ(tokens.back().kind, LzssTypedTokenKind::match);
  EXPECT_EQ(tokens.back().distance, 33554427);
  EXPECT_EQ(tokens.back().length, 5);
}
TEST(PositionDistance32mReference, ReachableFar258) {
  std::vector<std::byte> raw(33554432, std::byte{});
  for (std::size_t i = 0; i < 258; ++i)
    raw[i] = raw[raw.size() - 258 + i] =
        std::byte{static_cast<unsigned char>(1 + i % 255)};
  const auto tokens = checked(raw, params(), false);
  ASSERT_FALSE(tokens.empty());
  EXPECT_EQ(tokens.back().kind, LzssTypedTokenKind::match);
  EXPECT_EQ(tokens.back().distance, 33554174);
  EXPECT_EQ(tokens.back().length, 258);
}
TEST(PositionDistance32mReference, Parameters) {
  auto raw = bytes("a");
  for (unsigned which = 0; which < 5; ++which) {
    auto p = params();
    switch (which) {
    case 0:
      p.flags = 1;
      break;
    case 1:
      p.window_size = 0;
      break;
    case 2:
      p.window_size = 33554433;
      break;
    case 3:
      p.min_match_length = 5;
      break;
    case 4:
      p.max_match_length = 259;
      break;
    }
    failure(raw, E::invalid_parameters, p);
  }
}
TEST(PositionDistance32mReference, BothCapacitiesAndCountOnly) {
  auto raw = bytes("aaaaaaaaaa");
  for (unsigned n = 0; n < 2; ++n) {
    failure(raw, E::output_too_small, params(), limits(), n, 2);
    failure(raw, E::output_too_small, params(), limits(), 2, n);
  }
  auto q =
      query_lzss_position_distance_32m_reference(raw, params(), limits(), 0, 0);
  EXPECT_EQ(q.error, E::output_too_small);
  EXPECT_EQ(q.details.token_count, 2);
  EXPECT_EQ(q.details.raw_size, 10);
}
TEST(PositionDistance32mReference, ExactLedger) {
  auto raw = bytes("a");
  auto p = params();
  auto l = limits();
  l.max_block_size = 1;
  auto q = query_lzss_position_distance_32m_reference(raw, p, l, 7, 9, 123);
  ASSERT_EQ(q.error, E::none);
  RecordProperty("reference_working_bytes",
                 static_cast<int>(q.working_state_bytes));
  EXPECT_EQ(q.aggregate_bytes, raw.size() + 16 * sizeof(LzssTypedToken) +
                                   q.working_state_bytes + 123);
  l.max_internal_buffered_bytes = q.aggregate_bytes;
  EXPECT_EQ(
      query_lzss_position_distance_32m_reference(raw, p, l, 7, 9, 123).error,
      E::none);
  --l.max_internal_buffered_bytes;
  failure(raw, E::limit_exceeded, p, l, 7, 9, 123);
}
TEST(PositionDistance32mReference, Overflow) {
  auto raw = bytes("a");
  auto p = params();
  auto l = limits();
  auto max = std::numeric_limits<std::size_t>::max();
  EXPECT_EQ(query_lzss_position_distance_32m_reference(raw, p, l, max, 1).error,
            E::arithmetic_overflow);
  EXPECT_EQ(query_lzss_position_distance_32m_reference(raw, p, l, 1, max).error,
            E::arithmetic_overflow);
  failure(raw, E::arithmetic_overflow, p, l, 1, 1, max);
  failure(raw, E::arithmetic_overflow, p, l, 1, 1, 0,
          std::numeric_limits<std::uint64_t>::max());
}
TEST(PositionDistance32mReference, Policies) {
  auto raw = bytes("abcdef");
  for (unsigned which = 0; which < 4; ++which) {
    auto l = limits();
    switch (which) {
    case 0:
      l.max_block_size = 5;
      break;
    case 1:
      l.max_frame_size = 5;
      l.max_block_size = 5;
      break;
    case 2:
      l.max_total_output_size = 5;
      break;
    case 3:
      l.max_lz_distance = 33554431;
      break;
    }
    failure(raw, E::limit_exceeded, params(), l);
  }
  failure(raw, E::limit_exceeded, params(), limits(), 6, 6, 0,
          limits().max_total_output_size - 5);
  std::vector<std::byte> oversized(33554433);
  failure(oversized, E::invalid_parameters);
}
TEST(PositionDistance32mReference, FullTailOverlap) {
  auto raw = bytes("a");
  auto p = params();
  auto l = limits();
  std::array<LzssTypedToken, 16> storage;
  storage.fill(guard);
  LzssPositionDistance32mParseMetadata meta{777, 888};
  auto r = tokenize_lzss_position_distance_32m_reference(
      raw, p, l, std::span(storage).first(10), std::span(storage).subspan(9),
      meta);
  EXPECT_EQ(r.error, E::overlapping_buffers);
  EXPECT_EQ(r.tokens_committed, 0);
  for (auto t : storage)
    EXPECT_TRUE(reference_oracle::equal(t, guard));
  EXPECT_EQ(meta.token_count, 777);
}
TEST(PositionDistance32mReference, RawAndMetadataAlias) {
  auto p = params();
  auto l = limits();
  std::array<LzssTypedToken, 2> out, scratch;
  out.fill(guard);
  scratch.fill(guard);
  LzssPositionDistance32mParseMetadata meta{777, 888};
  std::array<std::byte, sizeof(meta)> original{};
  std::memcpy(original.data(), &meta, sizeof(meta));
  auto raw = std::as_bytes(std::span(&meta, 1));
  auto r = tokenize_lzss_position_distance_32m_reference(raw, p, l, out,
                                                         scratch, meta);
  EXPECT_EQ(r.error, E::overlapping_buffers);
  EXPECT_EQ(std::memcmp(original.data(), &meta, sizeof(meta)), 0);
  for (auto t : out)
    EXPECT_TRUE(reference_oracle::equal(t, guard));
}
TEST(PositionDistance32mReference, OutputAndInputAlias) {
  auto p = params();
  auto l = limits();
  std::array<LzssTypedToken, 2> out, scratch;
  out.fill(guard);
  scratch.fill(guard);
  LzssPositionDistance32mParseMetadata meta{777, 888};
  auto raw = std::as_bytes(std::span(out));
  auto r = tokenize_lzss_position_distance_32m_reference(raw, p, l, out,
                                                         scratch, meta);
  EXPECT_EQ(r.error, E::overlapping_buffers);
  for (auto t : out)
    EXPECT_TRUE(reference_oracle::equal(t, guard));
}
} // namespace
