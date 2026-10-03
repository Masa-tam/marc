#include "entropy/lzss_position_distance_8m_range_decoder.hpp"
#include "entropy/lzss_position_distance_8m_range_encoder.hpp"
#include "lzss_position_distance_8m_range_vectors.hpp"
#include <algorithm>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
#include <vector>
namespace {
using namespace marc;
using namespace entropy::internal;
using namespace context::internal;
using Error = ContextualDynamicRangeEncodeError;
constexpr auto guard = std::byte{0xa5};
core::DecoderLimits limits() { return {}; }
ModeledOperation symbol(std::uint16_t c, std::uint16_t a, std::uint32_t v) {
  return {ModeledOperationKind::symbol, c, a, v, 0};
}
ModeledOperation extra(std::uint32_t v, std::uint8_t w) {
  return {ModeledOperationKind::bypass_bits, 0, 0, v, w};
}
std::vector<ModeledOperation> literals(std::size_t n, bool all = false) {
  std::vector<ModeledOperation> ops;
  std::uint8_t last = 0;
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
  auto previous = static_cast<std::uint16_t>(ops.empty() ? 0 : 2);
  std::uint8_t lc = 8;
  auto le = length - 3;
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
void guards(std::span<const std::byte> b) {
  EXPECT_TRUE(std::ranges::all_of(b, [](auto x) { return x == guard; }));
}
ContextualDynamicRangeDescriptor sentinel() { return {777, 888, 999}; }
void unchanged(const ContextualDynamicRangeDescriptor &d) {
  EXPECT_EQ(d.decision_count, 777);
  EXPECT_EQ(d.payload_size, 888);
  EXPECT_EQ(d.context_count, 999);
}
void decoded(std::span<const ModeledOperation> ops,
             std::span<const std::byte> payload,
             const ContextualDynamicRangeDescriptor &desc) {
  LzssPositionDistance8mRangeDecoder d;
  ASSERT_EQ(d.begin(desc, payload, limits()).error,
            ContextualDynamicRangeDecodeError::none);
  for (auto expected : ops) {
    ModeledOperation op{};
    ASSERT_EQ(d.decode_next(op).error, ContextualDynamicRangeDecodeError::none);
    EXPECT_EQ(op.kind, expected.kind);
    EXPECT_EQ(op.context_id, expected.context_id);
    EXPECT_EQ(op.alphabet_size, expected.alphabet_size);
    EXPECT_EQ(op.value, expected.value);
    EXPECT_EQ(op.bit_count, expected.bit_count);
  }
  EXPECT_EQ(
      d.finish(static_cast<std::uint32_t>(ops.size()), desc.decision_count)
          .error,
      ContextualDynamicRangeDecodeError::none);
}
template <std::size_t N>
void fixed(const std::array<std::uint8_t, N> &payload, std::uint32_t decisions,
           const std::vector<ModeledOperation> &ops) {
  std::vector<std::byte> out(N + 7, guard), scratch(N + 9, guard);
  auto desc = sentinel();
  const auto q = query_lzss_position_distance_8m_range_encode(
      ops, limits(), out.size(), scratch.size());
  ASSERT_EQ(q.error, Error::none);
  EXPECT_EQ(q.details.operation_count, ops.size());
  EXPECT_EQ(q.details.decision_count, decisions);
  EXPECT_EQ(q.details.payload_size, N);
  auto r = encode_lzss_position_distance_8m_range_operations(ops, limits(), out,
                                                             scratch, desc);
  ASSERT_EQ(r.details.error, Error::none);
  ASSERT_EQ(r.bytes_committed, N);
  EXPECT_EQ(desc.context_count, 47);
  EXPECT_EQ(desc.decision_count, decisions);
  EXPECT_EQ(desc.payload_size, N);
  EXPECT_TRUE(std::ranges::equal(std::span(out).first(N),
                                 std::as_bytes(std::span(payload))));
  EXPECT_TRUE(
      std::ranges::equal(std::span(out).first(N), std::span(scratch).first(N)));
  guards(std::span(out).subspan(N));
  guards(std::span(scratch).subspan(N));
  decoded(ops, std::span(out).first(N), desc);
}
void failure(const std::vector<ModeledOperation> &ops, Error expected) {
  std::array<std::byte, 1024> out, scratch;
  out.fill(guard);
  scratch.fill(guard);
  auto desc = sentinel();
  auto r = encode_lzss_position_distance_8m_range_operations(ops, limits(), out,
                                                             scratch, desc);
  EXPECT_EQ(r.details.error, expected);
  EXPECT_EQ(r.bytes_committed, 0);
  guards(out);
  unchanged(desc);
}
TEST(PositionDistance8mRangeEncoder, HandLiteralCanonicalBytes) {
  fixed(vectors::literal_A, vectors::literal_A_decisions, literals(1));
}
TEST(PositionDistance8mRangeEncoder, AllLiteralContexts) {
  fixed(vectors::all_literals, vectors::all_literals_decisions,
        literals(256, true));
}
TEST(PositionDistance8mRangeEncoder, LiteralModelRescaling) {
  fixed(vectors::literal_rescale, vectors::literal_rescale_decisions,
        literals(70000));
}
TEST(PositionDistance8mRangeEncoder, AllLengthsUpperHalfDistance) {
  std::vector<ModeledOperation> ops;
  for (std::uint32_t n = 3; n <= 258; ++n)
    match(ops, n, 22, 1);
  fixed(vectors::all_lengths, vectors::all_lengths_decisions, ops);
}
TEST(PositionDistance8mRangeEncoder, EveryDistanceClassBoundary) {
  std::vector<ModeledOperation> ops;
  for (std::uint8_t c = 0; c < 24; ++c) {
    match(ops, 258, c, 0);
    if (c && c != 23)
      match(ops, 258, c, (std::uint32_t{1} << c) - 1);
  }
  fixed(vectors::all_distances, vectors::all_distances_decisions, ops);
}
TEST(PositionDistance8mRangeEncoder, AllAdaptiveExtraModelsRescale) {
  std::vector<ModeledOperation> ops;
  for (std::size_t i = 0; i < 33000; ++i)
    match(ops, 5, 23, 0);
  fixed(vectors::distance_rescale, vectors::distance_rescale_decisions, ops);
}
TEST(PositionDistance8mRangeEncoder, IndependentInstancesResetAndRepeat) {
  auto ops = literals(256, true);
  std::array<std::byte, 512> a, b, scratch;
  auto da = sentinel(), db = sentinel();
  auto ra = encode_lzss_position_distance_8m_range_operations(ops, limits(), a,
                                                              scratch, da);
  static_cast<void>(encode_lzss_position_distance_8m_range_operations(
      literals(1), limits(), b, scratch, db));
  auto rb = encode_lzss_position_distance_8m_range_operations(ops, limits(), b,
                                                              scratch, db);
  ASSERT_EQ(ra.details.error, Error::none);
  ASSERT_EQ(rb.details.error, Error::none);
  EXPECT_EQ(ra.bytes_committed, rb.bytes_committed);
  EXPECT_TRUE(std::ranges::equal(std::span(a).first(ra.bytes_committed),
                                 std::span(b).first(rb.bytes_committed)));
}
TEST(PositionDistance8mRangeEncoder, EmptyOperationsCannotCommit) {
  failure({}, Error::empty_operations);
}
TEST(PositionDistance8mRangeEncoder, IncompleteGrammarAtEveryPhase) {
  std::vector<ModeledOperation> ops;
  match(ops, 258, 22, 1);
  for (std::size_t n = 1; n < ops.size(); ++n)
    failure({ops.begin(), ops.begin() + n}, Error::invalid_symbol);
}
TEST(PositionDistance8mRangeEncoder, WrongShapeBeforeModelAccess) {
  auto original = literals(1);
  for (int i = 0; i < 6; ++i) {
    auto ops = original;
    if (i == 0)
      ops[0].kind = static_cast<ModeledOperationKind>(99);
    if (i == 1)
      ops[0].context_id = 65535;
    if (i == 2)
      ops[0].alphabet_size = 65535;
    if (i == 3)
      ops[0].value = 2;
    if (i == 4)
      ops[0].bit_count = 1;
    if (i == 5)
      ops[1].value = 256;
    failure(ops, Error::invalid_symbol);
  }
}
TEST(PositionDistance8mRangeEncoder, InvalidLength259) {
  std::vector<ModeledOperation> ops;
  match(ops, 259, 0, 0);
  failure(ops, Error::invalid_symbol);
}
TEST(PositionDistance8mRangeEncoder, EndpointExtraMustBeZero) {
  std::vector<ModeledOperation> ops;
  match(ops, 5, 23, 1);
  failure(ops, Error::invalid_symbol);
}
TEST(PositionDistance8mRangeEncoder, WrongExtraWidthValueAndUnused) {
  std::vector<ModeledOperation> original;
  match(original, 3, 22, 1);
  for (int i = 0; i < 5; ++i) {
    auto ops = original;
    if (i == 0)
      ops[2].bit_count = 0;
    if (i == 1)
      ops[2].value = 2;
    if (i == 2)
      ops.back().bit_count = 24;
    if (i == 3)
      ops.back().context_id = 1;
    if (i == 4)
      ops.back().alphabet_size = 2;
    failure(ops, Error::invalid_symbol);
  }
}
TEST(PositionDistance8mRangeEncoder, BothCapacitiesAndUntouchedTails) {
  for (std::size_t cap = 0; cap < 6; ++cap)
    for (bool first : {false, true}) {
      std::array<std::byte, 16> out, scratch;
      out.fill(guard);
      scratch.fill(guard);
      auto desc = sentinel();
      auto r = encode_lzss_position_distance_8m_range_operations(
          literals(1), limits(), std::span(out).first(first ? cap : 16),
          std::span(scratch).first(first ? 16 : cap), desc);
      EXPECT_EQ(r.details.error, Error::payload_output_too_small);
      EXPECT_EQ(r.bytes_committed, 0);
      guards(out);
      guards(scratch);
      unchanged(desc);
    }
}
TEST(PositionDistance8mRangeEncoder, EntireUnusedPayloadTailsDisjoint) {
  std::array<std::byte, 32> storage;
  storage.fill(guard);
  auto desc = sentinel();
  auto r = encode_lzss_position_distance_8m_range_operations(
      literals(1), limits(), std::span(storage).first(17),
      std::span(storage).subspan(16), desc);
  EXPECT_EQ(r.details.error, Error::overlapping_buffers);
  EXPECT_EQ(r.bytes_committed, 0);
  guards(storage);
  unchanged(desc);
}
TEST(PositionDistance8mRangeEncoder, InputAndMetadataAliases) {
  auto ops = literals(1);
  auto before = ops;
  std::array<std::byte, 128> scratch;
  auto desc = sentinel();
  auto writable = std::as_writable_bytes(std::span(ops));
  auto r = encode_lzss_position_distance_8m_range_operations(
      ops, limits(), writable, scratch, desc);
  EXPECT_EQ(r.details.error, Error::overlapping_buffers);
  EXPECT_EQ(std::memcmp(ops.data(), before.data(), writable.size()), 0);
  unchanged(desc);
  auto db = std::as_writable_bytes(std::span(&desc, 1));
  r = encode_lzss_position_distance_8m_range_operations(ops, limits(), db,
                                                        scratch, desc);
  EXPECT_EQ(r.details.error, Error::overlapping_buffers);
  unchanged(desc);
}
TEST(PositionDistance8mRangeEncoder, LimitsAliasesAndModelPolicies) {
  auto l = limits();
  auto saved = l;
  auto lb = std::as_writable_bytes(std::span(&l, 1));
  std::array<std::byte, 128> scratch;
  auto desc = sentinel();
  auto r = encode_lzss_position_distance_8m_range_operations(literals(1), l, lb,
                                                             scratch, desc);
  EXPECT_EQ(r.details.error, Error::overlapping_buffers);
  EXPECT_EQ(std::memcmp(&l, &saved, sizeof(l)), 0);
  unchanged(desc);
  for (int i = 0; i < 3; ++i) {
    l = limits();
    if (i == 0)
      l.max_entropy_table_entries = 2598;
    if (i == 1)
      l.max_range_model_total = 32767;
    if (i == 2)
      l.max_compressed_payload_size = 5;
    EXPECT_EQ(
        query_lzss_position_distance_8m_range_encode(literals(1), l, 16, 16)
            .error,
        Error::limit_exceeded);
  }
}
TEST(PositionDistance8mRangeEncoder, ExactAggregateAndOneByteUnder) {
  auto ops = literals(1);
  auto l = limits();
  l.max_block_size = 1;
  auto q = query_lzss_position_distance_8m_range_encode(ops, l, 16, 32, 123);
  ASSERT_EQ(q.error, Error::none);
  EXPECT_EQ(q.working_state_bytes,
            lzss_position_distance_8m_range_encode_working_bytes());
  RecordProperty("encoder_working_bytes",
                 static_cast<int>(q.working_state_bytes));
  EXPECT_EQ(q.aggregate_bytes, ops.size() * sizeof(ModeledOperation) + 16 + 32 +
                                   123 + q.working_state_bytes);
  l.max_internal_buffered_bytes = q.aggregate_bytes;
  std::array<std::byte, 16> out;
  std::array<std::byte, 32> scratch;
  auto desc = sentinel();
  ASSERT_EQ(encode_lzss_position_distance_8m_range_operations(
                ops, l, out, scratch, desc, 123)
                .details.error,
            Error::none);
  --l.max_internal_buffered_bytes;
  out.fill(guard);
  desc = sentinel();
  auto r = encode_lzss_position_distance_8m_range_operations(
      ops, l, out, scratch, desc, 123);
  EXPECT_EQ(r.details.error, Error::limit_exceeded);
  EXPECT_EQ(r.bytes_committed, 0);
  guards(out);
  unchanged(desc);
}
TEST(PositionDistance8mRangeEncoder, NumericCapacityAndRetainedOverflow) {
  auto ops = literals(1);
  auto l = limits();
  auto max = std::numeric_limits<std::size_t>::max();
  EXPECT_EQ(query_lzss_position_distance_8m_range_encode(ops, l, max, 16).error,
            Error::arithmetic_overflow);
  EXPECT_EQ(query_lzss_position_distance_8m_range_encode(ops, l, 16, max).error,
            Error::arithmetic_overflow);
  EXPECT_EQ(
      query_lzss_position_distance_8m_range_encode(ops, l, 16, 16, max).error,
      Error::arithmetic_overflow);
}
TEST(PositionDistance8mRangeEncoder, RejectedLateGrammarPreservesWholeOutput) {
  auto ops = literals(70000);
  ops.back().value = 256;
  std::array<std::byte, 1024> out, scratch;
  out.fill(guard);
  scratch.fill(guard);
  auto desc = sentinel();
  auto r = encode_lzss_position_distance_8m_range_operations(ops, limits(), out,
                                                             scratch, desc);
  EXPECT_EQ(r.details.error, Error::invalid_symbol);
  EXPECT_EQ(r.bytes_committed, 0);
  EXPECT_EQ(r.details.operation_index, ops.size() - 1);
  guards(out);
  guards(scratch);
  unchanged(desc);
}
TEST(PositionDistance8mRangeEncoder, ZeroScratchSeedPreservesDescriptorBytes) {
  // Retained FZ-0060 initial seed: complete fields, output512/scratch0.
  std::vector<ModeledOperation> ops;
  for (auto length : {3u, 4u, 5u, 6u, 8u, 17u, 128u, 258u})
    match(ops, length, 22, 1);
  std::array<std::byte, 512> out;
  out.fill(guard);
  auto desc = sentinel();
  auto object = std::as_writable_bytes(std::span(&desc, 1));
  std::fill(object.begin(), object.end(), std::byte{0xcc});
  desc.decision_count = 777;
  desc.payload_size = 888;
  desc.context_count = 999;
  std::array<std::byte, sizeof(desc)> snapshot{};
  std::memcpy(snapshot.data(), &desc, sizeof(desc));
  auto r = encode_lzss_position_distance_8m_range_operations(ops, limits(), out,
                                                             {}, desc);
  EXPECT_EQ(r.details.error, Error::payload_output_too_small);
  EXPECT_EQ(r.bytes_committed, 0);
  guards(out);
  EXPECT_EQ(std::memcmp(&desc, snapshot.data(), sizeof(desc)), 0);
}
} // namespace
