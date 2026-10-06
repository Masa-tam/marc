#include "context/lzss_position_distance_64m_mapper.hpp"
#include "dictionary/lzss_position_distance_64m_indexed.hpp"
#include "entropy/lzss_position_distance_64m_range_encoder.hpp"
#include "lzss_position_distance_64m_mapper_oracle.hpp"
#include "lzss_position_distance_64m_reference_oracle.hpp"
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
namespace {
using namespace marc;
using namespace dictionary::internal;
using E = LzssPositionDistance64mParseError;
constexpr LzssTypedToken guard{LzssTypedTokenKind::literal, 0xa5, 77, 88};
constexpr auto nil = std::numeric_limits<std::uint32_t>::max();
LzssParameters params() { return {67108864, 3, 258, 0}; }
core::DecoderLimits limits() {
  core::DecoderLimits l{};
  l.max_block_size = 67108864;
  l.max_frame_size = 67108864;
  l.max_lz_distance = 67108864;
  l.max_internal_buffered_bytes = 512u * 1024u * 1024u;
  return l;
}
std::vector<std::byte> bytes(std::string_view s) {
  std::vector<std::byte> out;
  for (unsigned char b : s)
    out.push_back(std::byte{b});
  return out;
}
std::uint32_t key(std::span<const std::byte> raw, std::size_t pos) {
  return (std::to_integer<unsigned>(raw[pos]) << 16) |
         (std::to_integer<unsigned>(raw[pos + 1]) << 8) |
         std::to_integer<unsigned>(raw[pos + 2]);
}
void index_invariants(std::span<const std::byte> raw,
                      std::span<const std::uint32_t> ws) {
  std::vector<unsigned> seen(raw.size());
  for (unsigned b = 0; b < 1048576; ++b) {
    auto pos = ws[b];
    std::size_t previous = raw.size();
    while (pos != nil) {
      ASSERT_LT(pos, previous);
      ASSERT_LT(pos + 2, raw.size());
      EXPECT_EQ((key(raw, pos) * UINT32_C(0x9e3779b1)) >> 12, b);
      ++seen[pos];
      previous = pos;
      pos = ws[1048576 + pos];
    }
  }
  for (std::size_t i = 0; i < raw.size(); ++i) {
    EXPECT_EQ(seen[i], raw.size() - i >= 3 ? 1u : 0u);
    if (raw.size() - i < 3)
      EXPECT_EQ(ws[1048576 + i], nil);
  }
}
std::vector<LzssTypedToken> checked(std::span<const std::byte> raw,
                                    LzssParameters p = params(),
                                    bool invariant = false) {
  auto l = limits();
  std::vector<LzssTypedToken> out(raw.size() + 3, guard),
      scratch(raw.size() + 5, guard), reference(raw.size()), rs(raw.size());
  std::vector<std::uint32_t> ws(1048576 + raw.size() + 7, 123);
  LzssPositionDistance64mParseMetadata meta{}, rm{};
  auto r = tokenize_lzss_position_distance_64m_indexed(raw, p, l, out, scratch,
                                                       ws, meta);
  EXPECT_EQ(r.error, E::none);
  if (r.error != E::none)
    return {};
  auto rr = tokenize_lzss_position_distance_64m_reference(raw, p, l, reference,
                                                          rs, rm);
  EXPECT_EQ(rr.error, E::none);
  EXPECT_EQ(r.tokens_committed, rr.tokens_committed);
  EXPECT_EQ(meta.raw_size, raw.size());
  auto oracle = reference_oracle::parse(raw, p);
  EXPECT_EQ(oracle.size(), r.tokens_committed);
  for (std::size_t i = 0; i < r.tokens_committed; ++i) {
    EXPECT_TRUE(reference_oracle::equal(out[i], reference[i]));
    EXPECT_TRUE(reference_oracle::equal(out[i], oracle[i]));
    EXPECT_TRUE(reference_oracle::equal(out[i], scratch[i]));
  }
  for (std::size_t i = r.tokens_committed; i < out.size(); ++i)
    EXPECT_TRUE(reference_oracle::equal(out[i], guard));
  for (std::size_t i = r.tokens_committed; i < scratch.size(); ++i)
    EXPECT_TRUE(reference_oracle::equal(scratch[i], guard));
  for (std::size_t i = 1048576 + raw.size(); i < ws.size(); ++i)
    EXPECT_EQ(ws[i], 123);
  if (invariant)
    index_invariants(raw, ws);
  out.resize(r.tokens_committed);
  EXPECT_EQ(reference_oracle::reconstruct(out),
            std::vector<std::byte>(raw.begin(), raw.end()));
  return out;
}
void failed(std::span<const std::byte> raw, E e, LzssParameters p = params(),
            core::DecoderLimits l = limits(), std::size_t oc = 512,
            std::size_t sc = 512, std::size_t wc = 1048576 + 512,
            std::size_t retained = 0, std::uint64_t committed = 0) {
  std::vector<LzssTypedToken> out(oc, guard), scratch(sc, guard);
  std::vector<std::uint32_t> ws(wc, 123);
  LzssPositionDistance64mParseMetadata meta{777, 888};
  std::array<std::byte, sizeof(meta)> original{};
  std::memcpy(original.data(), &meta, sizeof(meta));
  auto r = tokenize_lzss_position_distance_64m_indexed(
      raw, p, l, out, scratch, ws, meta, retained, committed);
  EXPECT_EQ(r.error, e);
  EXPECT_EQ(r.tokens_committed, 0);
  EXPECT_EQ(std::memcmp(&meta, original.data(), sizeof(meta)), 0);
  for (auto t : out)
    EXPECT_TRUE(reference_oracle::equal(t, guard));
}
TEST(PositionDistance64mIndexed, EveryOneByteAndShortTail) {
  for (unsigned b = 0; b < 256; ++b) {
    std::array raw{std::byte{static_cast<unsigned char>(b)}};
    checked(raw);
  }
  for (unsigned n = 2; n <= 6; ++n)
    checked(std::vector<std::byte>(n, std::byte{65}), params(), true);
}
TEST(PositionDistance64mIndexed, GreedyTieLongerAndOverlap) {
  for (auto s : {"aaaaaaaaaa", "abcde0abcde1abcde2", "abcdefghXabcdeYabcdefghZ",
                 "abcdef012345abcdef012345abcdef012345"})
    checked(bytes(s), params(), true);
}
TEST(PositionDistance64mIndexed, EveryConsumedPositionInserted) {
  checked(bytes("aaaaaaaaabaaaaaaaabaaaaaaaaab"), params(), true);
}
TEST(PositionDistance64mIndexed, CollisionKeysVerified) {
  std::vector<std::uint32_t> first(1048576, nil);
  unsigned a = 0, b = 0;
  for (unsigned code = 0; code <= 1048576; ++code) {
    auto bucket = (code * UINT32_C(0x9e3779b1)) >> 12;
    if (first[bucket] != nil) {
      a = first[bucket];
      b = code;
      break;
    }
    first[bucket] = code;
  }
  ASSERT_NE(a, b);
  std::vector<std::byte> raw;
  for (auto code : {a, b, a, b}) {
    raw.push_back(std::byte{static_cast<unsigned char>(code >> 16)});
    raw.push_back(std::byte{static_cast<unsigned char>(code >> 8)});
    raw.push_back(std::byte{static_cast<unsigned char>(code)});
    raw.push_back(std::byte{static_cast<unsigned char>(code % 251)});
    raw.push_back(std::byte{0xff});
  }
  checked(raw, params(), true);
}
TEST(PositionDistance64mIndexed, WindowExpiry) {
  auto raw = bytes("abcde012345abcde012345abcde");
  for (unsigned w : {1, 3, 9, 10, 11, 12, 67108864}) {
    auto p = params();
    p.window_size = w;
    checked(raw, p, true);
  }
}
TEST(PositionDistance64mIndexed, MaximumLengths) {
  for (unsigned max : {3, 4, 5, 17, 257, 258}) {
    auto p = params();
    p.max_match_length = max;
    checked(std::vector<std::byte>(520, std::byte{65}), p, true);
  }
  for (unsigned n : {257, 258, 259, 260, 516, 517})
    checked(std::vector<std::byte>(n, std::byte{65}));
}
TEST(PositionDistance64mIndexed, ExhaustiveBinary) {
  for (unsigned n = 1; n <= 8; ++n)
    for (unsigned mask = 0; mask < (1u << n); ++mask) {
      std::vector<std::byte> raw(n);
      for (unsigned i = 0; i < n; ++i)
        raw[i] = std::byte{static_cast<unsigned char>((mask >> i) & 1)};
      for (unsigned w : {1, 3, 67108864}) {
        auto p = params();
        p.window_size = w;
        checked(raw, p);
      }
    }
}
TEST(PositionDistance64mIndexed, SeededRandomPatterns) {
  unsigned state = 1427;
  for (unsigned run = 0; run < 100; ++run) {
    std::vector<std::byte> raw(1 + run * 3);
    for (auto &b : raw) {
      state = state * 1664525u + 1013904223u;
      b = std::byte{
          static_cast<unsigned char>((state >> 24) % (run % 2 ? 4 : 256))};
    }
    checked(raw, params(), true);
  }
}
TEST(PositionDistance64mIndexed, WorkspaceReset) {
  auto p = params();
  auto l = limits();
  std::vector<std::uint32_t> ws(1048576 + 256, 123);
  std::vector<LzssTypedToken> out(256), scratch(256);
  LzssPositionDistance64mParseMetadata meta{};
  for (auto s : {"aaaaaaaaaaaaaaaa", "abcdef012345abcdef012345", "a",
                 "aaaaaaaaaaaaaaaa"}) {
    auto raw = bytes(s);
    auto r = tokenize_lzss_position_distance_64m_indexed(raw, p, l, out,
                                                         scratch, ws, meta);
    ASSERT_EQ(r.error, E::none);
    auto expected = reference_oracle::parse(raw, p);
    ASSERT_EQ(r.tokens_committed, expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i)
      EXPECT_TRUE(reference_oracle::equal(out[i], expected[i]));
    index_invariants(raw, ws);
  }
}
TEST(PositionDistance64mIndexed, MaximumRepetitiveFrame) {
  std::vector<std::byte> raw(67108864, std::byte{65});
  auto p = params();
  auto l = limits();
  std::vector<std::uint32_t> ws(1048576 + raw.size());
  std::vector<LzssTypedToken> out(260113), scratch(260113), ref(260113),
      rs(260113);
  LzssPositionDistance64mParseMetadata meta{}, rm{};
  const auto plan = query_lzss_position_distance_64m_indexed(
      raw, p, l, out.size(), scratch.size(), ws);
  ASSERT_EQ(plan.error, E::none);
  EXPECT_EQ(plan.aggregate_bytes,
            raw.size() +
                (out.size() + scratch.size()) * sizeof(LzssTypedToken) +
                ws.size() * sizeof(std::uint32_t) + plan.working_state_bytes);
  RecordProperty("maximum_indexed_aggregate_bytes",
                 std::to_string(plan.aggregate_bytes));
  l.max_internal_buffered_bytes = plan.aggregate_bytes;
  auto r = tokenize_lzss_position_distance_64m_indexed(raw, p, l, out, scratch,
                                                       ws, meta);
  auto rr =
      tokenize_lzss_position_distance_64m_reference(raw, p, l, ref, rs, rm);
  ASSERT_EQ(r.error, E::none);
  ASSERT_EQ(rr.error, E::none);
  ASSERT_EQ(r.tokens_committed, 260113);
  for (std::size_t i = 0; i < out.size(); ++i)
    EXPECT_TRUE(reference_oracle::equal(out[i], ref[i]));
  EXPECT_EQ(reference_oracle::reconstruct(out), raw);
  const auto previous = out;
  const auto previous_meta = meta;
  --l.max_internal_buffered_bytes;
  r = tokenize_lzss_position_distance_64m_indexed(raw, p, l, out, scratch, ws,
                                                  meta);
  EXPECT_EQ(r.error, E::limit_exceeded);
  EXPECT_EQ(r.tokens_committed, 0);
  EXPECT_EQ(meta.token_count, previous_meta.token_count);
  EXPECT_EQ(meta.raw_size, previous_meta.raw_size);
  for (std::size_t i = 0; i < out.size(); ++i)
    EXPECT_TRUE(reference_oracle::equal(out[i], previous[i]));
}
TEST(PositionDistance64mIndexed, RangeTokenRawDifferential) {
  auto raw = bytes("abcdef012345abcdef012345abcdef012345");
  auto tokens = checked(raw);
  auto p = params();
  auto l = limits();
  auto c = mapper_oracle::counts(tokens);
  std::vector<context::internal::ModeledOperation> ops(c.declared_event_count),
      os(ops.size());
  context::internal::LzssFieldContextResult meta{};
  auto m = context::internal::map_lzss_position_distance_64m_tokens(
      tokens, p, c, l, ops, os, meta);
  ASSERT_EQ(m.details.error, context::internal::LzssFieldContextError::none);
  std::array<std::byte, 1024> payload{}, ps{};
  entropy::internal::ContextualDynamicRangeDescriptor desc{};
  auto enc =
      entropy::internal::encode_lzss_position_distance_64m_range_operations(
          ops, l, payload, ps, desc);
  ASSERT_EQ(enc.details.error,
            entropy::internal::ContextualDynamicRangeEncodeError::none);
  std::vector<LzssTypedToken> decoded(tokens.size()), ds(tokens.size());
  auto dec = context::internal::decode_lzss_position_distance_64m_tokens(
      desc, std::span(payload).first(enc.bytes_committed), p, c, l, decoded,
      ds);
  ASSERT_EQ(dec.error, context::internal::LzssContextualRangeDecodeError::none);
  EXPECT_EQ(reference_oracle::reconstruct(decoded), raw);
}
TEST(PositionDistance64mIndexed, ReachableFarReferences) {
  for (const unsigned length : {5u, 258u}) {
    std::vector<std::byte> raw(67108864, std::byte{});
    for (unsigned i = 0; i < length; ++i)
      raw[i] = raw[raw.size() - length + i] =
          std::byte{static_cast<unsigned char>(1 + i % 255)};
    auto p = params();
    auto l = limits();
    std::vector<std::uint32_t> ws(1048576 + raw.size());
    const auto q =
        query_lzss_position_distance_64m_indexed(raw, p, l, 0, 0, ws);
    ASSERT_EQ(q.error, E::output_too_small);
    std::vector<LzssTypedToken> out(q.details.token_count), scratch(out.size()),
        reference(out.size()), rs(out.size());
    LzssPositionDistance64mParseMetadata metadata{}, ref_metadata{};
    const auto r = tokenize_lzss_position_distance_64m_indexed(
        raw, p, l, out, scratch, ws, metadata);
    ASSERT_EQ(r.error, E::none);
    ASSERT_EQ(tokenize_lzss_position_distance_64m_reference(
                  raw, p, l, reference, rs, ref_metadata)
                  .error,
              E::none);
    ASSERT_EQ(metadata.token_count, ref_metadata.token_count);
    for (std::size_t i = 0; i < out.size(); ++i)
      ASSERT_TRUE(reference_oracle::equal(out[i], reference[i]));
    ASSERT_EQ(out.back().kind, LzssTypedTokenKind::match);
    EXPECT_EQ(out.back().distance, raw.size() - length);
    EXPECT_EQ(out.back().length, length);
    EXPECT_EQ(reference_oracle::reconstruct(out), raw);
  }
}
TEST(PositionDistance64mIndexed, EmptyAndInvalidParameters) {
  failed({}, E::invalid_parameters);
  auto raw = bytes("a");
  for (unsigned i = 0; i < 4; ++i) {
    auto p = params();
    switch (i) {
    case 0:
      p.window_size = 0;
      break;
    case 1:
      p.window_size = 67108865;
      break;
    case 2:
      p.min_match_length = 5;
      break;
    case 3:
      p.flags = 1;
      break;
    }
    failed(raw, E::invalid_parameters, p);
  }
}
TEST(PositionDistance64mIndexed, BothCapacities) {
  auto raw = bytes("aaaaaaaaaa");
  for (unsigned n = 0; n < 2; ++n) {
    failed(raw, E::output_too_small, params(), limits(), n, 2);
    failed(raw, E::output_too_small, params(), limits(), 2, n);
  }
}
TEST(PositionDistance64mIndexed, WorkspaceShortageAndCountOnly) {
  auto raw = bytes("aaaaaa");
  failed(raw, E::output_too_small, params(), limits(), 6, 6, 1048576 + 5);
  auto p = params();
  auto l = limits();
  std::vector<std::uint32_t> ws(1048576 + 6);
  auto q = query_lzss_position_distance_64m_indexed(raw, p, l, 0, 0, ws);
  EXPECT_EQ(q.error, E::output_too_small);
  EXPECT_EQ(q.details.token_count, 2);
  EXPECT_EQ(q.details.raw_size, 6);
}
TEST(PositionDistance64mIndexed, ExactFullCapacityLedger) {
  auto raw = bytes("a");
  auto p = params();
  auto l = limits();
  l.max_block_size = 1;
  std::vector<std::uint32_t> ws(1048576 + 9);
  auto q = query_lzss_position_distance_64m_indexed(raw, p, l, 7, 9, ws, 123);
  ASSERT_EQ(q.error, E::none);
  RecordProperty("indexed_working_bytes",
                 static_cast<int>(q.working_state_bytes));
  EXPECT_EQ(q.aggregate_bytes, raw.size() + 16 * sizeof(LzssTypedToken) +
                                   ws.size() * 4 + q.working_state_bytes + 123);
  l.max_internal_buffered_bytes = q.aggregate_bytes;
  EXPECT_EQ(
      query_lzss_position_distance_64m_indexed(raw, p, l, 7, 9, ws, 123).error,
      E::none);
  --l.max_internal_buffered_bytes;
  failed(raw, E::limit_exceeded, p, l, 7, 9, ws.size(), 123);
}
TEST(PositionDistance64mIndexed, OverflowAndTotal) {
  auto raw = bytes("a");
  auto p = params();
  auto l = limits();
  std::vector<std::uint32_t> ws(65537);
  auto max = std::numeric_limits<std::size_t>::max();
  EXPECT_EQ(
      query_lzss_position_distance_64m_indexed(raw, p, l, max, 1, ws).error,
      E::arithmetic_overflow);
  EXPECT_EQ(
      query_lzss_position_distance_64m_indexed(raw, p, l, 1, max, ws).error,
      E::arithmetic_overflow);
  failed(raw, E::arithmetic_overflow, p, l, 1, 1, 65537, max);
  failed(raw, E::arithmetic_overflow, p, l, 1, 1, 65537, 0,
         std::numeric_limits<std::uint64_t>::max());
}
TEST(PositionDistance64mIndexed, PolicyAndHonestFullFrameBudget) {
  auto raw = bytes("abcdef");
  for (unsigned i = 0; i < 4; ++i) {
    auto l = limits();
    switch (i) {
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
      l.max_lz_distance = 67108863;
      break;
    }
    failed(raw, E::limit_exceeded, params(), l);
  }
  std::vector<std::byte> large(67108864);
  std::vector<std::uint32_t> ws(1048576 + large.size());
  auto q = query_lzss_position_distance_64m_indexed(
      large, params(), limits(), large.size(), large.size(), ws);
  EXPECT_EQ(q.error, E::limit_exceeded);
}
TEST(PositionDistance64mIndexed, FullTokenTailOverlap) {
  auto raw = bytes("a");
  auto p = params();
  auto l = limits();
  std::vector<std::uint32_t> ws(65537);
  std::array<LzssTypedToken, 16> out;
  out.fill(guard);
  LzssPositionDistance64mParseMetadata meta{777, 888};
  auto r = tokenize_lzss_position_distance_64m_indexed(
      raw, p, l, std::span(out).first(10), std::span(out).subspan(9), ws, meta);
  EXPECT_EQ(r.error, E::overlapping_buffers);
  EXPECT_EQ(r.tokens_committed, 0);
  for (auto t : out)
    EXPECT_TRUE(reference_oracle::equal(t, guard));
  EXPECT_EQ(meta.token_count, 777);
}
TEST(PositionDistance64mIndexed, WorkspaceInputAndMetadataAliases) {
  auto p = params();
  auto l = limits();
  std::vector<std::uint32_t> ws(lzss_position_distance_64m_index_heads + 1,
                                123);
  std::array<LzssTypedToken, 2> out, scratch;
  out.fill(guard);
  LzssPositionDistance64mParseMetadata meta{777, 888};
  auto raw = std::as_bytes(std::span(ws)).first(1);
  auto r = tokenize_lzss_position_distance_64m_indexed(raw, p, l, out, scratch,
                                                       ws, meta);
  EXPECT_EQ(r.error, E::overlapping_buffers);
  EXPECT_EQ(ws[0], 123);
  raw = std::as_bytes(std::span(&meta, 1));
  r = tokenize_lzss_position_distance_64m_indexed(raw, p, l, out, scratch, ws,
                                                  meta);
  EXPECT_EQ(r.error, E::overlapping_buffers);
  for (auto t : out)
    EXPECT_TRUE(reference_oracle::equal(t, guard));
  EXPECT_EQ(meta.token_count, 777);
}
TEST(PositionDistance64mIndexed, QueryWorkspaceAlias) {
  std::vector<std::uint32_t> ws(lzss_position_distance_64m_index_heads + 1,
                                123);
  auto raw = std::as_bytes(std::span(ws)).first(1);
  auto q = query_lzss_position_distance_64m_indexed(raw, params(), limits(), 1,
                                                    1, ws);
  EXPECT_EQ(q.error, E::overlapping_buffers);
  for (auto w : ws)
    EXPECT_EQ(w, 123);
}
} // namespace
