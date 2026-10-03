#include "context/lzss_position_distance_8m_mapper.hpp"
#include "entropy/lzss_position_distance_8m_range_decoder.hpp"
#include "entropy/lzss_position_distance_8m_range_encoder.hpp"
#include "lzss_position_distance_8m_mapper_oracle.hpp"
#include "lzss_position_distance_8m_token_vectors.hpp"
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
namespace {
using namespace marc;
using namespace context::internal;
using namespace dictionary::internal;
using namespace mapper_oracle;
using E = LzssFieldContextError;
core::DecoderLimits limits() {
  core::DecoderLimits l{};
  l.max_block_size = 8388608;
  return l;
}
LzssParameters params() { return {8388608, 3, 258, 0}; }
constexpr ModeledOperation guard{ModeledOperationKind::bypass_bits, 77, 88, 99,
                                 11};
void same(const ModeledOperation &a, const ModeledOperation &b) {
  EXPECT_TRUE(equal(a, b));
}
void failed(std::span<const LzssTypedToken> t,
            const LzssFieldContextValidationContext &c, E e,
            LzssParameters p = params(), core::DecoderLimits l = limits(),
            std::size_t oc = 2048, std::size_t sc = 2048,
            std::size_t retained = 0) {
  std::vector<ModeledOperation> out(oc, guard), scratch(sc, guard);
  LzssFieldContextResult meta{};
  std::memset(&meta, 0xcc, sizeof(meta));
  std::array<std::byte, sizeof(meta)> original{};
  std::memcpy(original.data(), &meta, sizeof(meta));
  const auto r = map_lzss_position_distance_8m_tokens(t, p, c, l, out, scratch,
                                                      meta, retained);
  EXPECT_EQ(r.details.error, e);
  EXPECT_EQ(r.operations_committed, 0);
  EXPECT_EQ(std::memcmp(&meta, original.data(), sizeof(meta)), 0);
  for (auto op : out)
    same(op, guard);
}
template <std::size_t N>
void roundtrip(const std::vector<LzssTypedToken> &t,
               const std::array<std::uint8_t, N> &fixed, std::uint32_t T,
               std::uint32_t Ecount, std::uint32_t D, std::uint32_t F) {
  const auto p = params();
  const auto l = limits();
  auto c = counts(t);
  ASSERT_EQ(c.declared_token_count, T);
  ASSERT_EQ(c.declared_event_count, Ecount);
  ASSERT_EQ(c.declared_decision_count, D);
  ASSERT_EQ(c.declared_raw_size, F);
  auto oracle = fields(t);
  std::vector<ModeledOperation> out(Ecount + 7, guard),
      scratch(Ecount + 9, guard);
  LzssFieldContextResult meta{};
  const auto q = query_lzss_position_distance_8m_map(t, p, c, l, out.size(),
                                                     scratch.size(), 123);
  ASSERT_EQ(q.error, E::none);
  EXPECT_EQ(q.aggregate_bytes,
            t.size() * sizeof(LzssTypedToken) +
                (out.size() + scratch.size()) * sizeof(ModeledOperation) +
                q.working_state_bytes + 123);
  const auto r =
      map_lzss_position_distance_8m_tokens(t, p, c, l, out, scratch, meta, 123);
  ASSERT_EQ(r.details.error, E::none);
  ASSERT_EQ(r.operations_committed, Ecount);
  EXPECT_EQ(meta.raw_size, F);
  for (std::size_t i = 0; i < oracle.size(); ++i) {
    same(out[i], oracle[i]);
    same(scratch[i], oracle[i]);
  }
  for (std::size_t i = Ecount; i < out.size(); ++i)
    same(out[i], guard);
  for (std::size_t i = Ecount; i < scratch.size(); ++i)
    same(scratch[i], guard);
  std::vector<std::byte> bytes(N + 7, std::byte{0xa5}),
      byte_scratch(N + 9, std::byte{0xa5});
  entropy::internal::ContextualDynamicRangeDescriptor desc{};
  const auto encoded =
      entropy::internal::encode_lzss_position_distance_8m_range_operations(
          std::span(out).first(Ecount), l, bytes, byte_scratch, desc);
  ASSERT_EQ(encoded.details.error,
            entropy::internal::ContextualDynamicRangeEncodeError::none);
  ASSERT_EQ(encoded.bytes_committed, N);
  for (std::size_t i = 0; i < N; ++i)
    EXPECT_EQ(bytes[i], std::byte{fixed[i]});
  entropy::internal::LzssPositionDistance8mRangeDecoder decoder;
  ASSERT_EQ(decoder.begin(desc, std::span(bytes).first(N), l).error,
            entropy::internal::ContextualDynamicRangeDecodeError::none);
  for (auto op : oracle) {
    ModeledOperation got{};
    ASSERT_EQ(decoder.decode_next(got).error,
              entropy::internal::ContextualDynamicRangeDecodeError::none);
    same(got, op);
  }
  ASSERT_EQ(decoder.finish(Ecount, D).error,
            entropy::internal::ContextualDynamicRangeDecodeError::none);
  std::vector<LzssTypedToken> decoded(T), token_scratch(T);
  const auto decoded_result = decode_lzss_position_distance_8m_tokens(
      desc, std::span(bytes).first(N), p, c, l, decoded, token_scratch);
  ASSERT_EQ(decoded_result.error, LzssContextualRangeDecodeError::none);
  for (std::size_t i = 0; i < T; ++i) {
    EXPECT_EQ(decoded[i].kind, t[i].kind);
    EXPECT_EQ(decoded[i].literal, t[i].literal);
    EXPECT_EQ(decoded[i].distance, t[i].distance);
    EXPECT_EQ(decoded[i].length, t[i].length);
  }
}
TEST(PositionDistance8mMapper, LiteralFixed) {
  roundtrip({literal()}, token_vectors::literal, 1, 2, 2, 1);
}
TEST(PositionDistance8mMapper, ShortOverlapFixed) {
  roundtrip({literal(), match(1, 3), match(1, 4), literal(66), match(2, 258)},
            token_vectors::short_overlap, 5, 17, 23, 267);
}
TEST(PositionDistance8mMapper, AllLengthsFixed) {
  std::vector<LzssTypedToken> t{literal()};
  for (unsigned n = 3; n <= 258; ++n)
    t.push_back(match(1, n));
  roundtrip(t, token_vectors::all_lengths, 257, 1025, 2303, 33409);
}
TEST(PositionDistance8mMapper, UpperHalfFixed) {
  std::vector<LzssTypedToken> t{literal()};
  unsigned raw = 1;
  while (raw < 4194305) {
    auto n = std::min(258u, 4194305 - raw);
    if (n < 3) {
      t.push_back(literal());
      ++raw;
    } else {
      t.push_back(match(1, n));
      raw += n;
    }
  }
  t.push_back(match(4194305, 3));
  roundtrip(t, token_vectors::upper_half, token_vectors::upper_half_tokens,
            token_vectors::upper_half_events,
            token_vectors::upper_half_decisions, token_vectors::upper_half_raw);
}
TEST(PositionDistance8mMapper, MaximumHistoryFixed) {
  std::vector<LzssTypedToken> t{literal()};
  for (unsigned i = 0; i < 32513; ++i)
    t.push_back(match(1, 258));
  t.push_back(match(1, 253));
  roundtrip(t, token_vectors::maximum_history,
            token_vectors::maximum_history_tokens,
            token_vectors::maximum_history_events,
            token_vectors::maximum_history_decisions,
            token_vectors::maximum_history_raw);
}
TEST(PositionDistance8mMapper, EveryLiteralContext) {
  std::vector<LzssTypedToken> t;
  for (unsigned b = 0; b < 256; ++b)
    t.push_back(literal(b));
  const auto p = params();
  const auto l = limits();
  auto c = counts(t);
  auto oracle = fields(t);
  std::vector<ModeledOperation> out(oracle.size()), scratch(oracle.size());
  LzssFieldContextResult meta{};
  auto r = map_lzss_position_distance_8m_tokens(t, p, c, l, out, scratch, meta);
  ASSERT_EQ(r.details.error, E::none);
  for (std::size_t i = 0; i < out.size(); ++i)
    same(out[i], oracle[i]);
}
TEST(PositionDistance8mMapper, BeforeHistory) {
  std::vector<LzssTypedToken> t{match(1, 3)};
  failed(t, counts(t), E::invalid_token);
}
TEST(PositionDistance8mMapper, HistoryAndWindowOverrun) {
  std::vector<LzssTypedToken> t{literal(), match(2, 3)};
  failed(t, counts(t), E::invalid_token);
  t = {literal(), literal(), match(2, 3)};
  auto p = params();
  p.window_size = 1;
  failed(t, counts(t), E::invalid_token, p);
}
TEST(PositionDistance8mMapper, InvalidTokenFields) {
  std::vector<LzssTypedToken> t{literal(), match(1, 3)};
  auto c = counts(t);
  for (auto bad :
       {LzssTypedToken{LzssTypedTokenKind::match, 1, 1, 3},
        LzssTypedToken{LzssTypedTokenKind::match, 0, 0, 3},
        LzssTypedToken{static_cast<LzssTypedTokenKind>(255), 0, 1, 3},
        match(1, 2), match(1, 259),
        LzssTypedToken{LzssTypedTokenKind::literal, 65, 1, 0}}) {
    t[1] = bad;
    failed(t, c, E::invalid_token);
  }
}
TEST(PositionDistance8mMapper, RawSizeExact) {
  std::vector<LzssTypedToken> t{literal(), match(1, 3)};
  auto c = counts(t);
  ++c.declared_raw_size;
  failed(t, c, E::raw_size_mismatch);
  c.declared_raw_size = 3;
  failed(t, c, E::invalid_token);
}
TEST(PositionDistance8mMapper, DeclaredCounts) {
  std::vector<LzssTypedToken> t{literal(), match(1, 3)};
  auto c = counts(t);
  ++c.declared_token_count;
  failed(t, c, E::token_count_mismatch);
  c = counts(t);
  --c.declared_event_count;
  failed(t, c, E::event_count_mismatch);
  c = counts(t);
  ++c.declared_decision_count;
  failed(t, c, E::decision_count_mismatch);
}
TEST(PositionDistance8mMapper, InvalidCountsAndParameters) {
  std::vector<LzssTypedToken> t{literal()};
  auto c = counts(t);
  c.declared_raw_size = 0;
  failed(t, c, E::invalid_parameters);
  c = counts(t);
  c.declared_event_count = 3;
  failed(t, c, E::invalid_parameters);
  auto p = params();
  p.flags = 1;
  failed(t, counts(t), E::invalid_parameters, p);
  p = params();
  p.window_size = 8388609;
  failed(t, counts(t), E::invalid_parameters, p);
}
TEST(PositionDistance8mMapper, BothCapacities) {
  std::vector<LzssTypedToken> t{literal()};
  for (std::size_t n = 0; n < 2; ++n) {
    failed(t, counts(t), E::output_too_small, params(), limits(), n, 2);
    failed(t, counts(t), E::output_too_small, params(), limits(), 2, n);
  }
}
TEST(PositionDistance8mMapper, ExactLedgerAndRetained) {
  std::vector<LzssTypedToken> t{literal()};
  auto p = params();
  auto l = limits();
  l.max_block_size = 1;
  auto c = counts(t);
  auto q = query_lzss_position_distance_8m_map(t, p, c, l, 7, 9, 123);
  ASSERT_EQ(q.error, E::none);
  RecordProperty("mapper_working_bytes",
                 static_cast<int>(q.working_state_bytes));
  l.max_internal_buffered_bytes = q.aggregate_bytes;
  EXPECT_EQ(query_lzss_position_distance_8m_map(t, p, c, l, 7, 9, 123).error,
            E::none);
  --l.max_internal_buffered_bytes;
  failed(t, c, E::limit_exceeded, p, l, 7, 9, 123);
}
TEST(PositionDistance8mMapper, NumericOverflow) {
  std::vector<LzssTypedToken> t{literal()};
  auto p = params();
  auto l = limits();
  auto c = counts(t);
  const auto max = std::numeric_limits<std::size_t>::max();
  EXPECT_EQ(query_lzss_position_distance_8m_map(t, p, c, l, max, 2).error,
            E::arithmetic_overflow);
  EXPECT_EQ(query_lzss_position_distance_8m_map(t, p, c, l, 2, max).error,
            E::arithmetic_overflow);
  failed(t, c, E::arithmetic_overflow, p, l, 2, 2, max);
  c.output_already_committed = std::numeric_limits<std::uint64_t>::max();
  failed(t, c, E::arithmetic_overflow);
}
TEST(PositionDistance8mMapper, Policies) {
  std::vector<LzssTypedToken> t{literal(), match(1, 3)};
  for (unsigned which = 0; which < 6; ++which) {
    auto l = limits();
    switch (which) {
    case 0:
      l.max_block_size = 3;
      break;
    case 1:
      l.max_frame_size = 3;
      l.max_block_size = 3;
      break;
    case 2:
      l.max_total_output_size = 3;
      break;
    case 3:
      l.max_entropy_table_entries = 2598;
      break;
    case 4:
      l.max_range_model_total = 32767;
      break;
    case 5:
      l.max_lz_distance = 8388607;
      break;
    }
    failed(t, counts(t), E::limit_exceeded, params(), l);
  }
}
TEST(PositionDistance8mMapper, FullUnusedTailOverlap) {
  std::vector<LzssTypedToken> t{literal()};
  auto p = params();
  auto l = limits();
  auto c = counts(t);
  std::array<ModeledOperation, 16> ops;
  ops.fill(guard);
  LzssFieldContextResult meta{};
  const auto r = map_lzss_position_distance_8m_tokens(
      t, p, c, l, std::span(ops).first(10), std::span(ops).subspan(9), meta);
  EXPECT_EQ(r.details.error, E::overlapping_buffers);
  EXPECT_EQ(r.operations_committed, 0);
  for (auto op : ops)
    same(op, guard);
}
TEST(PositionDistance8mMapper, MetadataAliasesInput) {
  std::vector<LzssTypedToken> t{literal()};
  auto p = params();
  auto l = limits();
  auto c = counts(t);
  std::array<ModeledOperation, 2> out, scratch;
  out.fill(guard);
  scratch.fill(guard);
  LzssFieldContextResult meta{};
  std::memset(&meta, 0xcc, sizeof(meta));
  auto *bytes = reinterpret_cast<std::byte *>(&meta);
  auto *tokens = reinterpret_cast<LzssTypedToken *>(bytes);
  const auto r = map_lzss_position_distance_8m_tokens(
      std::span<const LzssTypedToken>(tokens, 1), p, c, l, out, scratch, meta);
  EXPECT_EQ(r.details.error, E::overlapping_buffers);
  for (auto op : out)
    same(op, guard);
  for (auto b : std::as_bytes(std::span(&meta, 1)))
    EXPECT_EQ(b, std::byte{0xcc});
}
TEST(PositionDistance8mMapper, LateInvalidPreservesWholeOutput) {
  std::vector<LzssTypedToken> t(70000, literal());
  auto c = counts(t);
  t.back().length = 1;
  failed(t, c, E::invalid_token, params(), limits(), 140007, 140009);
}
TEST(PositionDistance8mMapper, EndpointHistoryRejected) {
  std::vector<LzssTypedToken> t{literal(), match(8388608, 3)};
  failed(t, counts(t), E::invalid_token);
}
TEST(PositionDistance8mMapper, EveryReachableDistanceClassAndReset) {
  std::vector<LzssTypedToken> t{literal()};
  unsigned raw = 1;
  for (unsigned dc = 0; dc <= 22; ++dc) {
    const unsigned distance = (1u << dc) + (dc ? 1 : 0);
    while (raw < distance) {
      const unsigned n = std::min(258u, distance - raw);
      if (n < 3) {
        t.push_back(literal());
        ++raw;
      } else {
        t.push_back(match(1, n));
        raw += n;
      }
    }
    t.push_back(match(distance, 5));
    raw += 5;
  }
  auto p = params();
  auto l = limits();
  auto c = counts(t);
  auto oracle = fields(t);
  std::vector<ModeledOperation> out(oracle.size()), scratch(oracle.size());
  LzssFieldContextResult meta{};
  for (unsigned repeat = 0; repeat < 2; ++repeat) {
    const auto r =
        map_lzss_position_distance_8m_tokens(t, p, c, l, out, scratch, meta);
    ASSERT_EQ(r.details.error, E::none);
    for (std::size_t i = 0; i < out.size(); ++i)
      same(out[i], oracle[i]);
  }
}
TEST(PositionDistance8mMapper, WritableAliasesLiveConfiguration) {
  std::vector<LzssTypedToken> t{literal()};
  auto p = params();
  auto l = limits();
  auto c = counts(t);
  std::array<ModeledOperation, 2> scratch;
  scratch.fill(guard);
  LzssFieldContextResult meta{};
  std::array<std::byte, sizeof(l)> original{};
  std::memcpy(original.data(), &l, sizeof(l));
  // No operation is accessed: complete overlap detection precedes validation.
  auto *op = reinterpret_cast<ModeledOperation *>(&l);
  const auto r = map_lzss_position_distance_8m_tokens(
      t, p, c, l, std::span(op, 2), scratch, meta);
  EXPECT_EQ(r.details.error, E::overlapping_buffers);
  EXPECT_EQ(r.operations_committed, 0);
  EXPECT_EQ(std::memcmp(original.data(), &l, sizeof(l)), 0);
  for (auto v : scratch)
    same(v, guard);
}
} // namespace
