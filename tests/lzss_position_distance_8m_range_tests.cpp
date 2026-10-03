#include "entropy/lzss_position_distance_8m_range_decoder.hpp"
#include "lzss_position_distance_8m_range_vectors.hpp"
#include <cstring>
#include <gtest/gtest.h>
#include <vector>
namespace marc::entropy::internal {
struct LzssPositionDistance8mRangeDecoderTestAccess {
  static auto &state(LzssPositionDistance8mRangeDecoder &d) { return d.state_; }
};
} // namespace marc::entropy::internal
namespace {
using namespace marc;
using namespace entropy::internal;
using namespace context::internal;
using Error = ContextualDynamicRangeDecodeError;
using Decoder = LzssPositionDistance8mRangeDecoder;
using Access = LzssPositionDistance8mRangeDecoderTestAccess;
core::DecoderLimits limits() {
  auto l = core::DecoderLimits{};
  l.max_entropy_table_entries = 2599;
  return l;
}
template <std::size_t N> auto bytes(const std::array<std::uint8_t, N> &a) {
  return std::as_bytes(std::span(a));
}
auto descriptor(std::size_t n, std::uint32_t decisions) {
  return ContextualDynamicRangeDescriptor{decisions,
                                          static_cast<std::uint32_t>(n), 47};
}
ModeledOperation symbol(std::uint16_t c, std::uint16_t a, std::uint32_t v) {
  return {ModeledOperationKind::symbol, c, a, v, 0};
}
ModeledOperation extra(std::uint32_t v, std::uint8_t w) {
  return {ModeledOperationKind::bypass_bits, 0, 0, v, w};
}
void equal(const ModeledOperation &a, const ModeledOperation &b) {
  EXPECT_EQ(a.kind, b.kind);
  EXPECT_EQ(a.context_id, b.context_id);
  EXPECT_EQ(a.alphabet_size, b.alphabet_size);
  EXPECT_EQ(a.value, b.value);
  EXPECT_EQ(a.bit_count, b.bit_count);
}
std::vector<ModeledOperation> literals(std::size_t n, bool all) {
  std::vector<ModeledOperation> ops;
  std::uint8_t last{};
  for (std::size_t i = 0; i < n; ++i) {
    auto v = all ? static_cast<std::uint8_t>(i) : std::uint8_t{65};
    ops.push_back(symbol(i ? 1 : 0, 2, 0));
    ops.push_back(
        symbol(i ? static_cast<std::uint16_t>(4 + (last >> 5)) : 3, 256, v));
    last = v;
  }
  return ops;
}
void match(std::vector<ModeledOperation> &ops, std::uint32_t length,
           std::uint8_t dc, std::uint32_t de) {
  const auto previous = static_cast<std::uint16_t>(ops.empty() ? 0 : 2);
  std::uint8_t lc = 8;
  std::uint32_t le = length - 3;
  if (length >= 5) {
    lc = 0;
    while ((std::uint32_t{1} << (lc + 1)) <= length - 4)
      ++lc;
    le = length - 4 - (std::uint32_t{1} << lc);
  }
  ops.push_back(symbol(previous, 2, 1));
  ops.push_back(symbol(12 + previous, 9, lc));
  if (lc)
    ops.push_back(extra(le, lc == 8 ? 1 : lc));
  ops.push_back(symbol(15 + lc, 24, dc));
  if (dc)
    ops.push_back(extra(de, dc));
}
template <std::size_t N>
void run(const std::array<std::uint8_t, N> &payload, std::uint32_t decisions,
         const std::vector<ModeledOperation> &ops) {
  Decoder d;
  ASSERT_EQ(d.begin(descriptor(N, decisions), bytes(payload), limits()).error,
            Error::none);
  for (const auto &expected : ops) {
    ModeledOperation op{};
    ASSERT_EQ(d.decode_next(op).error, Error::none);
    equal(op, expected);
  }
  EXPECT_EQ(d.finish(static_cast<std::uint32_t>(ops.size()), decisions).error,
            Error::none);
  EXPECT_EQ(d.finish(static_cast<std::uint32_t>(ops.size()), decisions).error,
            Error::already_finished);
}
TEST(PositionDistance8mRange, HandLiteral) {
  run(vectors::literal_A, vectors::literal_A_decisions, literals(1, false));
  RecordProperty("range_state_bytes", static_cast<int>(sizeof(Decoder)));
  EXPECT_EQ(sizeof(Decoder), sizeof(LzssPositionDistance8mRangeState));
}
TEST(PositionDistance8mRange, AllLiterals) {
  run(vectors::all_literals, vectors::all_literals_decisions,
      literals(256, true));
}
TEST(PositionDistance8mRange, LiteralRescaling) {
  run(vectors::literal_rescale, vectors::literal_rescale_decisions,
      literals(70000, false));
}
TEST(PositionDistance8mRange, AllLengthsAndUpperHalfDistance) {
  std::vector<ModeledOperation> ops;
  for (std::uint32_t n = 3; n <= 258; ++n)
    match(ops, n, 22, 1);
  run(vectors::all_lengths, vectors::all_lengths_decisions, ops);
}
TEST(PositionDistance8mRange, AllDistanceClassesAndExtras) {
  std::vector<ModeledOperation> ops;
  for (std::uint8_t c = 0; c <= 23; ++c) {
    match(ops, 258, c, 0);
    if (c && c != 23)
      match(ops, 258, c, (std::uint32_t{1} << c) - 1);
  }
  run(vectors::all_distances, vectors::all_distances_decisions, ops);
}
TEST(PositionDistance8mRange, EveryDistanceModelRescales) {
  std::vector<ModeledOperation> ops;
  for (int i = 0; i < 33000; ++i)
    match(ops, 5, 23, 0);
  run(vectors::distance_rescale, vectors::distance_rescale_decisions, ops);
}
TEST(PositionDistance8mRange, NotStartedOutputInvariant) {
  Decoder d;
  auto sentinel = symbol(21, 777, 123);
  auto op = sentinel;
  EXPECT_EQ(d.decode_next(op).error, Error::not_started);
  equal(op, sentinel);
  EXPECT_EQ(d.finish(0, 0).error, Error::not_started);
}
TEST(PositionDistance8mRange, DescriptorAndCallerLimits) {
  auto original = descriptor(vectors::literal_A.size(), 2);
  for (int which = 0; which < 7; ++which) {
    auto desc = original;
    auto l = limits();
    switch (which) {
    case 0:
      desc.context_count = 46;
      break;
    case 1:
      desc.decision_count = 0;
      break;
    case 2:
      desc.payload_size = 4;
      break;
    case 3:
      l.max_entropy_table_entries = 2598;
      break;
    case 4:
      l.max_range_model_total = 32767;
      break;
    case 5:
      l.max_compressed_payload_size = 5;
      break;
    case 6:
      l.max_block_size = 1;
      l.max_internal_buffered_bytes =
          sizeof(Decoder) + vectors::literal_A.size() - 1;
      break;
    }
    Decoder d;
    EXPECT_EQ(d.begin(desc, bytes(vectors::literal_A), l).error,
              Error::invalid_descriptor);
    auto op = symbol(0, 7, 999);
    const auto before = op;
    EXPECT_EQ(d.decode_next(op).error, Error::invalid_descriptor);
    equal(op, before);
  }
  Decoder d;
  auto l = limits();
  l.max_block_size = 1;
  l.max_internal_buffered_bytes = sizeof(Decoder) + vectors::literal_A.size();
  EXPECT_EQ(d.begin(original, bytes(vectors::literal_A), l).error, Error::none);
  auto desc = original;
  ++desc.payload_size;
  EXPECT_EQ(d.begin(desc, bytes(vectors::literal_A), limits()).error,
            Error::payload_size_mismatch);
}
TEST(PositionDistance8mRange, EveryTruncationTerminates) {
  auto p = bytes(vectors::all_distances);
  for (std::size_t n = 0; n < p.size(); ++n) {
    Decoder d;
    auto r = d.begin(descriptor(n, vectors::all_distances_decisions),
                     p.first(n), limits());
    ModeledOperation op{};
    for (std::uint32_t k = 0;
         r.error == Error::none && k < vectors::all_distances_events; ++k)
      r = d.decode_next(op);
    if (r.error == Error::none)
      r = d.finish(vectors::all_distances_events,
                   vectors::all_distances_decisions);
    EXPECT_NE(r.error, Error::none);
  }
}
TEST(PositionDistance8mRange, EveryLiteralBitCorruptionDetected) {
  for (std::size_t bit = 0; bit < 48; ++bit) {
    auto p = vectors::literal_A;
    p[bit / 8] ^= static_cast<std::uint8_t>(1u << (bit % 8));
    Decoder d;
    auto r = d.begin(descriptor(p.size(), 2), bytes(p), limits());
    ModeledOperation op{};
    bool changed = false;
    for (int i = 0; i < 2 && r.error == Error::none; ++i) {
      r = d.decode_next(op);
      if (r.error == Error::none && i == 1 && op.value != 65)
        changed = true;
    }
    if (r.error == Error::none)
      r = d.finish(2, 2);
    EXPECT_TRUE(changed || r.error != Error::none);
  }
}
TEST(PositionDistance8mRange, NoncanonicalSameDecodedOperations) {
  auto p = vectors::literal_A;
  ++p.back();
  Decoder d;
  ASSERT_EQ(d.begin(descriptor(p.size(), 2), bytes(p), limits()).error,
            Error::none);
  for (auto expected : literals(1, false)) {
    ModeledOperation op{};
    ASSERT_EQ(d.decode_next(op).error, Error::none);
    equal(op, expected);
  }
  EXPECT_EQ(d.finish(2, 2).error, Error::invalid_interval);
}
template <std::size_t N>
void invalid(const std::array<std::uint8_t, N> &p, std::uint32_t decisions,
             unsigned valid) {
  Decoder d;
  ASSERT_EQ(d.begin(descriptor(N, decisions), bytes(p), limits()).error,
            Error::none);
  ModeledOperation op{};
  for (unsigned i = 0; i < valid; ++i)
    ASSERT_EQ(d.decode_next(op).error, Error::none);
  auto sentinel = symbol(99, 777, 999);
  op = sentinel;
  EXPECT_EQ(d.decode_next(op).error, Error::invalid_interval);
  equal(op, sentinel);
  EXPECT_EQ(d.decode_next(op).error, Error::invalid_interval);
  equal(op, sentinel);
}
TEST(PositionDistance8mRange, InvalidLengthAndDistanceOutputInvariant) {
  invalid(vectors::invalid_length, vectors::invalid_length_decisions, 2);
  invalid(vectors::invalid_distance, vectors::invalid_distance_decisions, 3);
}
TEST(PositionDistance8mRange, IncompleteGrammarAndCountMismatch) {
  Decoder d;
  ASSERT_EQ(d.begin(descriptor(vectors::incomplete_kind.size(), 1),
                    bytes(vectors::incomplete_kind), limits())
                .error,
            Error::none);
  ModeledOperation op{};
  ASSERT_EQ(d.decode_next(op).error, Error::none);
  EXPECT_EQ(d.finish(1, 1).error, Error::count_mismatch);
  ASSERT_EQ(d.begin(descriptor(vectors::literal_A.size(), 2),
                    bytes(vectors::literal_A), limits())
                .error,
            Error::none);
  ASSERT_EQ(d.decode_next(op).error, Error::none);
  ASSERT_EQ(d.decode_next(op).error, Error::none);
  EXPECT_EQ(d.finish(3, 2).error, Error::count_mismatch);
}
TEST(PositionDistance8mRange, DecisionExhaustionInvariant) {
  Decoder d;
  ASSERT_EQ(d.begin(descriptor(vectors::literal_A.size(), 1),
                    bytes(vectors::literal_A), limits())
                .error,
            Error::none);
  ModeledOperation op{};
  ASSERT_EQ(d.decode_next(op).error, Error::none);
  auto before = op;
  EXPECT_EQ(d.decode_next(op).error, Error::decision_count_exceeded);
  equal(op, before);
}
TEST(PositionDistance8mRange, TrailingPayload) {
  std::vector<std::byte> p(bytes(vectors::literal_A).begin(),
                           bytes(vectors::literal_A).end());
  p.push_back(std::byte{});
  Decoder d;
  ASSERT_EQ(d.begin(descriptor(p.size(), 2), p, limits()).error, Error::none);
  ModeledOperation op{};
  ASSERT_EQ(d.decode_next(op).error, Error::none);
  ASSERT_EQ(d.decode_next(op).error, Error::none);
  EXPECT_EQ(d.finish(2, 2).error, Error::trailing_payload);
}
TEST(PositionDistance8mRange, OutputPayloadAliasAndRestart) {
  ModeledOperation storage{};
  auto p = std::as_writable_bytes(std::span(&storage, 1));
  std::memcpy(p.data(), vectors::literal_A.data(), vectors::literal_A.size());
  Decoder d;
  ASSERT_EQ(d.begin(descriptor(6, 2), p.first(6), limits()).error, Error::none);
  const auto before = storage;
  EXPECT_EQ(d.decode_next(storage).error, Error::invalid_descriptor);
  equal(storage, before);
  ModeledOperation op{};
  EXPECT_EQ(d.decode_next(op).error, Error::none);
  ASSERT_EQ(
      d.begin(descriptor(6, 2), bytes(vectors::literal_A), limits()).error,
      Error::none);
  EXPECT_EQ(d.decode_next(op).error, Error::none);
  EXPECT_EQ(d.decode_next(op).error, Error::none);
  EXPECT_EQ(d.finish(2, 2).error, Error::none);
}
TEST(PositionDistance8mRange, BeginStateAliasRejectedBeforeReset) {
  Decoder d;
  const auto &s = Access::state(d);
  const auto before = s.frequencies;
  auto p = std::as_bytes(std::span(&s, 1)).first(6);
  EXPECT_EQ(d.begin(descriptor(6, 2), p, limits()).error,
            Error::invalid_descriptor);
  EXPECT_EQ(Access::state(d).frequencies, before);
}
TEST(PositionDistance8mRange, ModelInvariantFinish) {
  for (int which = 0; which < 2; ++which) {
    Decoder d;
    ASSERT_EQ(
        d.begin(descriptor(6, 2), bytes(vectors::literal_A), limits()).error,
        Error::none);
    ModeledOperation op{};
    ASSERT_EQ(d.decode_next(op).error, Error::none);
    ASSERT_EQ(d.decode_next(op).error, Error::none);
    auto &s = Access::state(d);
    if (which)
      s.totals[46] = 32768;
    else
      s.frequencies.back() = 0;
    EXPECT_EQ(d.finish(2, 2).error, Error::invalid_model);
  }
}
TEST(PositionDistance8mRange, GrammarUpdateFailureInvariant) {
  LzssPositionDistance8mFieldState s{};
  auto bad = symbol(1, 2, 0);
  EXPECT_EQ(lzss_position_distance_8m_accept(s, bad),
            LzssFieldContextError::unexpected_context);
  EXPECT_EQ(s.phase, LzssPositionDistance8mFieldState::Phase::kind);
  EXPECT_EQ(s.previous_kind, 0);
}
TEST(PositionDistance8mRange, MixedLiteralAndMatchContexts) {
  std::vector<ModeledOperation> ops = {
      symbol(0, 2, 0),    symbol(3, 256, 65), symbol(1, 2, 1),
      symbol(13, 9, 8),   extra(0, 1),        symbol(23, 24, 1),
      extra(1, 1),        symbol(2, 2, 0),    symbol(6, 256, 255),
      symbol(1, 2, 1),    symbol(13, 9, 7),   extra(126, 7),
      symbol(22, 24, 22), extra(1, 22),       symbol(2, 2, 0),
      symbol(11, 256, 0), symbol(1, 2, 1),    symbol(13, 9, 0),
      symbol(15, 24, 23), extra(0, 23)};
  run(vectors::mixed, vectors::mixed_decisions, ops);
}
TEST(PositionDistance8mRange, GroupedDistanceDecisionBudgetInvariant) {
  Decoder d;
  ASSERT_EQ(d.begin(descriptor(vectors::invalid_distance.size(), 25),
                    bytes(vectors::invalid_distance), limits())
                .error,
            Error::none);
  ModeledOperation op{};
  for (int i = 0; i < 3; ++i)
    ASSERT_EQ(d.decode_next(op).error, Error::none);
  auto before = symbol(99, 777, 999);
  op = before;
  const auto counts = Access::state(d).decision_count;
  EXPECT_EQ(d.decode_next(op).error, Error::decision_count_exceeded);
  equal(op, before);
  EXPECT_EQ(Access::state(d).decision_count, counts);
}
} // namespace
