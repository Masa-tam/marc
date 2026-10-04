#include "dictionary/lzss_position_distance_16m_compact.hpp"
#include "lzss_position_distance_16m_reference_oracle.hpp"
#include <array>
#include <gtest/gtest.h>
#include <limits>
#include <string_view>

namespace {
using namespace marc;
using namespace dictionary::internal;
using E = LzssPositionDistance16mParseError;
LzssParameters parameters() { return {16777216, 3, 258, 0}; }
core::DecoderLimits limits() {
  core::DecoderLimits l{};
  l.max_block_size = 16777216;
  return l;
}
// Independent explicit byte serialization, not the production compact writer.
std::vector<std::byte> serialize(std::span<const LzssTypedToken> tokens) {
  std::vector<std::byte> out;
  for (const auto &t : tokens) {
    if (t.kind == LzssTypedTokenKind::literal) {
      out.push_back(std::byte{0});
      out.push_back(std::byte{t.literal});
    } else {
      out.push_back(std::byte{1});
      for (auto v : {t.distance, t.length})
        for (unsigned shift = 0; shift < 32; shift += 8)
          out.push_back(std::byte{static_cast<unsigned char>(v >> shift)});
    }
  }
  return out;
}
void check(std::span<const std::byte> raw, LzssParameters p = parameters()) {
  auto l = limits();
  const auto tokens = reference_oracle::parse(raw, p);
  const auto expected = serialize(tokens);
  ASSERT_LE(expected.size(), 2 * raw.size());
  std::vector<std::uint32_t> index(
      lzss_position_distance_16m_index_heads + raw.size() + 3, 73);
  auto q = query_lzss_position_distance_16m_compact(raw, p, l, 0, index);
  ASSERT_EQ(q.error, E::output_too_small);
  EXPECT_EQ(q.byte_count, expected.size());
  EXPECT_EQ(q.details.token_count, tokens.size());
  EXPECT_EQ(q.details.raw_size, raw.size());
  std::vector<std::byte> bytes(expected.size() + 3, std::byte{0xa5});
  auto written =
      write_lzss_position_distance_16m_compact_private(raw, p, l, bytes, index);
  ASSERT_EQ(written.error, E::none);
  EXPECT_EQ(written.byte_count, expected.size());
  EXPECT_TRUE(std::equal(expected.begin(), expected.end(), bytes.begin()));
  for (std::size_t i = expected.size(); i < bytes.size(); ++i)
    EXPECT_EQ(bytes[i], std::byte{0xa5});
  for (std::size_t i = lzss_position_distance_16m_index_heads + raw.size();
       i < index.size(); ++i)
    EXPECT_EQ(index[i], 73u);
  LzssPositionDistance16mCompactReader reader(
      std::span<const std::byte>(bytes).first(expected.size()));
  for (auto t : tokens) {
    LzssTypedToken decoded{};
    ASSERT_TRUE(reader.read(decoded));
    EXPECT_TRUE(reference_oracle::equal(t, decoded));
  }
  EXPECT_TRUE(reader.empty());
  auto exact = l;
  exact.max_block_size = raw.size();
  exact.max_internal_buffered_bytes = written.aggregate_bytes;
  EXPECT_EQ(write_lzss_position_distance_16m_compact_private(raw, p, exact,
                                                             bytes, index)
                .error,
            E::none);
  --exact.max_internal_buffered_bytes;
  std::fill(bytes.begin(), bytes.end(), std::byte{0xa5});
  std::fill(index.begin(), index.end(), 73);
  EXPECT_EQ(write_lzss_position_distance_16m_compact_private(raw, p, exact,
                                                             bytes, index)
                .error,
            E::limit_exceeded);
  EXPECT_TRUE(
      std::ranges::all_of(bytes, [](auto b) { return b == std::byte{0xa5}; }));
  EXPECT_TRUE(std::ranges::all_of(index, [](auto b) { return b == 73; }));
  auto small = std::span<std::byte>(bytes).first(expected.size() - 1);
  EXPECT_EQ(
      write_lzss_position_distance_16m_compact_private(raw, p, l, small, index)
          .error,
      E::output_too_small);
  // Failed private writes are discardable, never a valid complete token view.
}
TEST(PositionDistance16mCompact, EveryByte) {
  for (unsigned b = 0; b < 256; ++b) {
    std::array raw{std::byte{static_cast<unsigned char>(b)}};
    check(raw);
  }
}
TEST(PositionDistance16mCompact, TailsAndLongestNearestOverlap) {
  for (auto s :
       {"aaaaa", "aaaaaaaaaa", "abcde0abcde1abcde2", "abcdefghXabcdeYabcdefghZ",
        "abcdef012345abcdef012345abcdef012345"}) {
    std::vector<std::byte> raw;
    for (auto b : std::string_view(s))
      raw.push_back(std::byte{static_cast<unsigned char>(b)});
    check(raw);
    for (unsigned w : {1, 3, 9, 10, 11, 12}) {
      auto p = parameters();
      p.window_size = w;
      check(raw, p);
    }
  }
  for (unsigned n : {2, 3, 4, 5, 6, 257, 258, 259, 260, 516, 517})
    check(std::vector<std::byte>(n, std::byte{65}));
}
TEST(PositionDistance16mCompact, MaximumLength) {
  for (unsigned n : {3, 4, 5, 17, 257, 258}) {
    auto p = parameters();
    p.max_match_length = n;
    check(std::vector<std::byte>(520, std::byte{65}), p);
  }
}
TEST(PositionDistance16mCompact, ExhaustiveBinaryAndRandom) {
  for (unsigned n = 1; n <= 8; ++n)
    for (unsigned m = 0; m < (1u << n); ++m) {
      std::vector<std::byte> raw(n);
      for (unsigned i = 0; i < n; ++i)
        raw[i] = std::byte{static_cast<unsigned char>((m >> i) & 1)};
      check(raw);
    }
  std::uint32_t state = 1483;
  for (unsigned n : {15, 64, 129, 256}) {
    std::vector<std::byte> raw(n);
    for (auto &b : raw) {
      state ^= state << 13;
      state ^= state >> 17;
      state ^= state << 5;
      b = std::byte{static_cast<unsigned char>(state)};
    }
    check(raw);
  }
}
TEST(PositionDistance16mCompact, ReaderAtomicSyntaxFailures) {
  const LzssTypedToken guard{LzssTypedTokenKind::literal, 7, 8, 9};
  for (unsigned tag = 0; tag < 256; ++tag) {
    std::array<std::byte, 9> raw{};
    raw[0] = std::byte{static_cast<unsigned char>(tag)};
    for (std::size_t n = 0; n <= 9; ++n) {
      LzssPositionDistance16mCompactReader reader(
          std::span<const std::byte>(raw).first(n));
      auto output = guard;
      const auto valid = (tag == 0 && n >= 2) || (tag == 1 && n >= 9);
      EXPECT_EQ(reader.read(output), valid);
      if (!valid) {
        EXPECT_EQ(reader.position(), 0);
        EXPECT_TRUE(reference_oracle::equal(output, guard));
      }
    }
  }
}
TEST(PositionDistance16mCompact, ReaderLittleEndianAndGrammarNeutral) {
  const std::array raw{std::byte{1},    std::byte{0x78}, std::byte{0x56},
                       std::byte{0x34}, std::byte{0x12}, std::byte{3},
                       std::byte{0},    std::byte{0},    std::byte{0}};
  LzssPositionDistance16mCompactReader reader(raw);
  LzssTypedToken t{};
  ASSERT_TRUE(reader.read(t));
  EXPECT_EQ(t.distance, 0x12345678u);
  EXPECT_EQ(t.length, 3u);
  EXPECT_EQ(t.literal, 0);
  EXPECT_TRUE(reader.empty());
  const auto old = t;
  EXPECT_FALSE(reader.read(t));
  EXPECT_TRUE(reference_oracle::equal(t, old));
  // Syntax only: the downstream sixteen-MiB validator rejects invalid history.
}
TEST(PositionDistance16mCompact, AdmissionPrecedesMutation) {
  auto p = parameters();
  auto l = limits();
  std::array raw{std::byte{65}};
  std::array<std::byte, 2> out{std::byte{0xa5}, std::byte{0xa5}};
  std::vector<std::uint32_t> index(lzss_position_distance_16m_index_heads + 1,
                                   73);
  auto bad = [&](E e, std::size_t retained = 0, std::uint64_t committed = 0) {
    EXPECT_EQ(write_lzss_position_distance_16m_compact_private(
                  raw, p, l, out, index, retained, committed)
                  .error,
              e);
    EXPECT_TRUE(
        std::ranges::all_of(out, [](auto b) { return b == std::byte{0xa5}; }));
    EXPECT_TRUE(std::ranges::all_of(index, [](auto b) { return b == 73; }));
  };
  p.window_size = 0;
  bad(E::invalid_parameters);
  p = parameters();
  bad(E::arithmetic_overflow, std::numeric_limits<std::size_t>::max());
  bad(E::arithmetic_overflow, 0, std::numeric_limits<std::uint64_t>::max());
  l.max_internal_buffered_bytes = 1;
  bad(E::limit_exceeded);
  l = limits();
  EXPECT_EQ(write_lzss_position_distance_16m_compact_private(
                raw, p, l, out, std::span(index).first(index.size() - 1))
                .error,
            E::output_too_small);
  EXPECT_EQ(
      write_lzss_position_distance_16m_compact_private({}, p, l, out, index)
          .error,
      E::invalid_parameters);
}
TEST(PositionDistance16mCompact, FullRegionAliasesRejected) {
  auto p = parameters();
  auto l = limits();
  std::vector<std::uint32_t> index(lzss_position_distance_16m_index_heads + 2,
                                   73);
  std::array<std::byte, 4> raw{std::byte{65}, std::byte{65}, std::byte{65},
                               std::byte{65}};
  EXPECT_EQ(write_lzss_position_distance_16m_compact_private(
                std::span(raw).first(1), p, l, raw, index)
                .error,
            E::overlapping_buffers);
  auto ib = std::as_writable_bytes(std::span(index));
  EXPECT_EQ(write_lzss_position_distance_16m_compact_private(
                std::span(raw).first(1), p, l, ib, index)
                .error,
            E::overlapping_buffers);
  auto pb = std::as_writable_bytes(std::span(&p, 1));
  EXPECT_EQ(write_lzss_position_distance_16m_compact_private(
                std::span(raw).first(1), p, l, pb, index)
                .error,
            E::overlapping_buffers);
  EXPECT_TRUE(std::ranges::all_of(index, [](auto b) { return b == 73; }));
  EXPECT_EQ(raw[0], std::byte{65});
}
} // namespace
