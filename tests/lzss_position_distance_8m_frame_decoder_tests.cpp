#include "frame/lzss_position_distance_8m_frame_decoder.hpp"
#include "lzss_position_distance_8m_frame_vectors.hpp"
#include "lzss_position_distance_8m_token_vectors.hpp"
#include <algorithm>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
#include <vector>
namespace {
using namespace marc;
using namespace frame::internal;
using namespace dictionary::internal;
using Error = LzssPositionDistance8mFrameDecodeError;
using PrefixError = LzssPositionDistance8mPreflightError;
constexpr auto guard = std::byte{0xa5};
core::DecoderLimits limits() {
  auto l = core::DecoderLimits{};
  l.max_frame_size = 8388608;
  l.max_block_size = 8388608;
  l.max_lz_distance = 8388608;
  return l;
}
TypedContextStreamHeader stream(std::uint32_t raw) {
  TypedContextStreamHeader s{};
  s.frame_size = raw;
  s.original_size = raw;
  s.dictionary = {8388608, 3, 258, 0};
  s.range_model_total = 32768;
  s.context_count = 47;
  s.dictionary_variant = 11;
  s.context_algorithm = 1;
  s.context_variant = 12;
  return s;
}
TypedContextFrameLayout sentinel() {
  TypedContextFrameLayout m{};
  m.serialized_size = 777;
  m.header.sequence = 88;
  m.header.token_count = 99;
  return m;
}
void unchanged(const TypedContextFrameLayout &m) {
  EXPECT_EQ(m.serialized_size, 777);
  EXPECT_EQ(m.header.sequence, 88);
  EXPECT_EQ(m.header.token_count, 99);
  EXPECT_EQ(m.header.uncompressed_size, 0);
  EXPECT_EQ(m.descriptor.context_count, 0);
}
void guards(std::span<const std::byte> b) {
  for (auto v : b)
    EXPECT_EQ(v, guard);
}
template <std::size_t N>
auto make_frame(const std::array<std::uint8_t, 80> &prefix,
                const std::array<std::uint8_t, N> &payload) {
  std::vector<std::byte> out;
  for (auto v : prefix)
    out.push_back(static_cast<std::byte>(v));
  for (auto v : payload)
    out.push_back(static_cast<std::byte>(v));
  return out;
}
template <class T>
void put(std::vector<std::byte> &b, std::size_t offset, T value) {
  for (std::size_t i = 0; i < sizeof(T); ++i)
    b[offset + i] = static_cast<std::byte>((value >> (8 * i)) & 255);
}
#define FRAME(name)                                                            \
  make_frame(frame_vectors::name##_prefix, token_vectors::name)
struct Buffers {
  std::vector<LzssTypedToken> tokens, token_scratch;
  std::vector<std::byte> raw, raw_scratch;
  TypedContextFrameLayout meta{sentinel()};
  Buffers(std::size_t t, std::size_t f)
      : tokens(t + 2), token_scratch(t + 3), raw(f + 2, guard),
        raw_scratch(f + 3, guard) {}
};
auto decode(std::span<const std::byte> input, const TypedContextStreamHeader &s,
            const core::DecoderLimits &l, Buffers &b, std::size_t retained = 0,
            std::uint64_t seq = 0, std::uint64_t committed = 0) {
  const TypedContextFrameValidationContext c{s, l, seq, committed};
  return decode_lzss_position_distance_8m_frame(
      input, c, b.tokens, b.token_scratch, b.raw, b.raw_scratch, b.meta,
      retained);
}
auto query(std::span<const std::byte> input, const TypedContextStreamHeader &s,
           const core::DecoderLimits &l, const Buffers &b,
           std::size_t retained = 0) {
  const TypedContextFrameValidationContext c{s, l, 0, 0};
  return query_lzss_position_distance_8m_frame_decode(
      input, c, b.tokens.size(), b.token_scratch.size(), b.raw.size(),
      b.raw_scratch.size(), retained);
}
void failure(std::span<const std::byte> input,
             const TypedContextStreamHeader &s, Error expected) {
  Buffers b(100, static_cast<std::size_t>(s.frame_size));
  const auto r = decode(input, s, limits(), b);
  EXPECT_EQ(r.error, expected);
  EXPECT_EQ(r.bytes_consumed, 0);
  EXPECT_EQ(r.raw_produced, 0);
  guards(b.raw);
  unchanged(b.meta);
}
TEST(PositionDistance8mFrame, HandStreamHeaderAndLiteral) {
  TypedContextStreamHeader s{};
  std::size_t consumed = 7;
  const auto l = limits();
  ASSERT_EQ(parse_lzss_position_distance_8m_stream_header(
                std::as_bytes(std::span(frame_vectors::literal_stream_header)),
                l, s, consumed),
            PrefixError::none);
  EXPECT_EQ(consumed, 112);
  auto input = FRAME(literal);
  Buffers b(1, 1);
  const auto q = query(input, s, l, b, 123);
  ASSERT_EQ(q.error, Error::none);
  EXPECT_EQ(q.token_requirements.aggregate_bytes,
            input.size() +
                (b.tokens.size() + b.token_scratch.size()) *
                    sizeof(LzssTypedToken) +
                b.raw.size() + b.raw_scratch.size() + q.frame_state_bytes +
                q.token_requirements.working_state_bytes + 123);
  RecordProperty("frame_state_bytes", static_cast<int>(q.frame_state_bytes));
  RecordProperty("token_state_bytes",
                 static_cast<int>(q.token_requirements.working_state_bytes));
  const auto r = decode(input, s, l, b, 123);
  ASSERT_EQ(r.error, Error::none);
  EXPECT_EQ(r.bytes_consumed, 86);
  EXPECT_EQ(r.raw_produced, 1);
  EXPECT_EQ(b.raw[0], std::byte{65});
  EXPECT_EQ(b.meta.serialized_size, 86);
  EXPECT_EQ(b.meta.header.token_count, 1);
  guards(std::span(b.raw).subspan(1));
  guards(std::span(b.raw_scratch).subspan(1));
}
TEST(PositionDistance8mFrame, ShortOverlapRawAndTails) {
  auto input = FRAME(short_overlap);
  auto s = stream(267);
  Buffers b(5, 267);
  const auto r = decode(input, s, limits(), b);
  ASSERT_EQ(r.error, Error::none);
  for (std::size_t i = 0; i < 267; ++i)
    EXPECT_EQ(b.raw[i], static_cast<std::byte>(i < 8 ? 65 : i % 2 ? 65 : 66));
  guards(std::span(b.raw).subspan(267));
  guards(std::span(b.raw_scratch).subspan(267));
}
TEST(PositionDistance8mFrame, AllLengthsActualRaw) {
  auto input = FRAME(all_lengths);
  auto s = stream(token_vectors::all_lengths_raw);
  Buffers b(token_vectors::all_lengths_tokens, s.frame_size);
  ASSERT_EQ(decode(input, s, limits(), b).error, Error::none);
  EXPECT_TRUE(std::ranges::all_of(std::span(b.raw).first(s.frame_size),
                                  [](auto v) { return v == std::byte{65}; }));
}
TEST(PositionDistance8mFrame, UpperHalfHistoryActualRaw) {
  auto input = FRAME(upper_half);
  auto s = stream(token_vectors::upper_half_raw);
  Buffers b(token_vectors::upper_half_tokens, s.frame_size);
  ASSERT_EQ(decode(input, s, limits(), b).error, Error::none);
  EXPECT_EQ(b.tokens[token_vectors::upper_half_tokens - 1].distance, 4194305);
  EXPECT_TRUE(std::ranges::all_of(std::span(b.raw).first(s.frame_size),
                                  [](auto v) { return v == std::byte{65}; }));
  guards(std::span(b.raw).subspan(s.frame_size));
}
TEST(PositionDistance8mFrame, ExactEightMiBBoundaryActualRaw) {
  auto input = FRAME(maximum_history);
  auto s = stream(8388608);
  Buffers b(token_vectors::maximum_history_tokens, s.frame_size);
  const auto r = decode(input, s, limits(), b);
  ASSERT_EQ(r.error, Error::none);
  EXPECT_EQ(r.raw_produced, 8388608);
  EXPECT_TRUE(std::ranges::all_of(std::span(b.raw).first(s.frame_size),
                                  [](auto v) { return v == std::byte{65}; }));
  guards(std::span(b.raw).subspan(s.frame_size));
  guards(std::span(b.raw_scratch).subspan(s.frame_size));
}
TEST(PositionDistance8mFrame, EveryTruncationPreservesRawAndMetadata) {
  auto input = FRAME(short_overlap);
  auto s = stream(267);
  for (std::size_t n = 0; n < input.size(); ++n) {
    Buffers b(5, 267);
    const auto r = decode(std::span(input).first(n), s, limits(), b);
    EXPECT_NE(r.error, Error::none);
    EXPECT_EQ(r.bytes_consumed, 0);
    guards(b.raw);
    guards(b.raw_scratch);
    unchanged(b.meta);
  }
}
TEST(PositionDistance8mFrame, TrailingAndPrefixOnlyRejected) {
  auto input = FRAME(literal);
  failure(std::span(input).first(80), stream(1), Error::truncated_frame);
  input.push_back(std::byte{});
  failure(input, stream(1), Error::trailing_frame);
}
TEST(PositionDistance8mFrame, QueryCannotValidateCanonicalPayload) {
  auto input = FRAME(literal);
  input.back() ^= std::byte{1};
  auto s = stream(1);
  Buffers b(1, 1);
  EXPECT_EQ(query(input, s, limits(), b).error, Error::none);
  EXPECT_EQ(decode(input, s, limits(), b).error, Error::token_error);
  guards(b.raw);
  guards(b.raw_scratch);
  unchanged(b.meta);
}
TEST(PositionDistance8mFrame, HistoryAndLengthFailuresCannotPublish) {
  failure(FRAME(before_history), stream(3), Error::token_error);
  failure(FRAME(history_overrun), stream(4), Error::token_error);
  failure(FRAME(endpoint), stream(4), Error::token_error);
  failure(FRAME(invalid_length), stream(260), Error::token_error);
}
TEST(PositionDistance8mFrame, ExactRawSizeAndCountFinish) {
  auto input = FRAME(declared_long);
  put<std::uint32_t>(input, 16, 5);
  failure(input, stream(5), Error::token_error);
  input = FRAME(raw_overrun);
  put<std::uint32_t>(input, 16, 3);
  failure(input, stream(3), Error::token_error);
  input = FRAME(extra_token);
  put<std::uint32_t>(input, 20, 1);
  failure(input, stream(2), Error::token_error);
}
TEST(PositionDistance8mFrame, BadHeaderIdentityReservedAndCounts) {
  auto original = FRAME(literal);
  for (auto offset : {0u, 4u, 6u, 16u, 20u, 24u, 28u, 36u, 40u, 44u, 48u, 63u,
                      64u, 68u, 72u, 74u, 76u, 79u}) {
    auto input = original;
    input[offset] ^= std::byte{1};
    Buffers b(1, 1);
    EXPECT_NE(decode(input, stream(1), limits(), b).error, Error::none);
    guards(b.raw);
    unchanged(b.meta);
  }
  auto s = stream(1);
  s.dictionary_variant = 10;
  failure(original, s, Error::preflight_error);
  s = stream(1);
  s.context_variant = 11;
  failure(original, s, Error::preflight_error);
}
TEST(PositionDistance8mFrame, SequenceFinalShortAndEarlierCommit) {
  auto input = FRAME(literal);
  put<std::uint64_t>(input, 8, 1);
  auto s = stream(2);
  s.original_size = 3;
  Buffers b(1, 1);
  EXPECT_EQ(decode(input, s, limits(), b, 0, 1, 2).error, Error::none);
  EXPECT_EQ(b.raw[0], std::byte{65});
  Buffers bad(1, 1);
  EXPECT_EQ(decode(input, s, limits(), bad, 0, 0, 2).error,
            Error::preflight_error);
  guards(bad.raw);
  unchanged(bad.meta);
}
TEST(PositionDistance8mFrame, LaterFailurePreservesPreviousFrame) {
  auto first = FRAME(literal);
  auto second = first;
  put<std::uint64_t>(second, 8, 1);
  second.back() ^= std::byte{1};
  auto s = stream(1);
  s.original_size = 2;
  Buffers earlier(1, 1), later(1, 1);
  ASSERT_EQ(decode(first, s, limits(), earlier).error, Error::none);
  EXPECT_EQ(decode(second, s, limits(), later, 0, 1, 1).error,
            Error::token_error);
  EXPECT_EQ(earlier.raw[0], std::byte{65});
  EXPECT_EQ(earlier.meta.header.sequence, 0);
  guards(later.raw);
  unchanged(later.meta);
}
TEST(PositionDistance8mFrame, AllFourStorageCapacities) {
  auto input = FRAME(short_overlap);
  const auto s = stream(267);
  const auto l = limits();
  for (int which = 0; which < 4; ++which) {
    Buffers b(5, 267);
    if (which == 0)
      b.tokens.resize(4);
    if (which == 1)
      b.token_scratch.resize(4);
    if (which == 2)
      b.raw.resize(266);
    if (which == 3)
      b.raw_scratch.resize(266);
    EXPECT_EQ(decode(input, s, l, b).error, Error::storage_too_small);
    guards(b.raw);
    guards(b.raw_scratch);
    unchanged(b.meta);
  }
}
TEST(PositionDistance8mFrame, ExactAggregateBudgetAndFullTails) {
  auto input = FRAME(literal);
  auto s = stream(1);
  Buffers b(1, 1);
  auto l = limits();
  const auto q = query(input, s, l, b, 123);
  ASSERT_EQ(q.error, Error::none);
  l.max_block_size = 1;
  l.max_internal_buffered_bytes = q.token_requirements.aggregate_bytes;
  EXPECT_EQ(query(input, s, l, b, 123).error, Error::none);
  ASSERT_EQ(decode(input, s, l, b, 123).error, Error::none);
  --l.max_internal_buffered_bytes;
  Buffers bad(1, 1);
  EXPECT_EQ(decode(input, s, l, bad, 123).error, Error::limit_exceeded);
  guards(bad.raw);
  unchanged(bad.meta);
  ++l.max_internal_buffered_bytes;
  bad.raw.push_back(guard);
  EXPECT_EQ(decode(input, s, l, bad, 123).error, Error::limit_exceeded);
}
TEST(PositionDistance8mFrame, CapacityAndRetainedOverflow) {
  auto input = FRAME(literal);
  auto s = stream(1);
  auto l = limits();
  TypedContextFrameValidationContext c{s, l, 0, 0};
  const auto max = std::numeric_limits<std::size_t>::max();
  EXPECT_EQ(query_lzss_position_distance_8m_frame_decode(input, c, max, 1, 1, 1)
                .error,
            Error::arithmetic_overflow);
  EXPECT_EQ(query_lzss_position_distance_8m_frame_decode(input, c, 1, 1, max, 1)
                .error,
            Error::arithmetic_overflow);
  EXPECT_EQ(
      query_lzss_position_distance_8m_frame_decode(input, c, 1, 1, 1, 1, max)
          .error,
      Error::arithmetic_overflow);
}
TEST(PositionDistance8mFrame, PolicyLimitsAndExpansion) {
  auto input = FRAME(short_overlap);
  auto s = stream(267);
  for (int which = 0; which < 6; ++which) {
    auto l = limits();
    switch (which) {
    case 0:
      l.max_block_size = 266;
      break;
    case 1:
      l.max_compressed_payload_size = 1;
      break;
    case 2:
      l.max_entropy_table_entries = 2598;
      break;
    case 3:
      l.max_range_model_total = 32767;
      break;
    case 4:
      l.max_total_output_size = 266;
      l.max_frame_size = 266;
      break;
    case 5:
      l.max_expansion_ratio = 1;
      l.expansion_slack = 0;
      break;
    }
    Buffers b(5, 267);
    EXPECT_NE(decode(input, s, l, b).error, Error::none);
    guards(b.raw);
    unchanged(b.meta);
  }
}
TEST(PositionDistance8mFrame, RawBuffersAndFullTailsCannotOverlap) {
  auto input = FRAME(literal);
  auto s = stream(1);
  auto l = limits();
  TypedContextFrameValidationContext c{s, l, 0, 0};
  Buffers b(1, 1);
  std::vector<std::byte> storage(5, guard);
  EXPECT_EQ(decode_lzss_position_distance_8m_frame(
                input, c, b.tokens, b.token_scratch,
                std::span(storage).first(3), std::span(storage).subspan(2),
                b.meta)
                .error,
            Error::overlapping_buffers);
  guards(storage);
  unchanged(b.meta);
}
TEST(PositionDistance8mFrame, TypedRawAndTypedTypedAliases) {
  auto input = FRAME(literal);
  auto s = stream(1);
  auto l = limits();
  TypedContextFrameValidationContext c{s, l, 0, 0};
  Buffers b(1, 1);
  EXPECT_EQ(decode_lzss_position_distance_8m_frame(input, c, b.tokens, b.tokens,
                                                   b.raw, b.raw_scratch, b.meta)
                .error,
            Error::overlapping_buffers);
  guards(b.raw);
  unchanged(b.meta);
  auto raw = std::as_writable_bytes(std::span(b.tokens)).first(1);
  EXPECT_EQ(decode_lzss_position_distance_8m_frame(
                input, c, b.tokens, b.token_scratch, raw, b.raw_scratch, b.meta)
                .error,
            Error::overlapping_buffers);
  unchanged(b.meta);
}
TEST(PositionDistance8mFrame, InputCannotAliasRawOrScratch) {
  auto input = FRAME(literal);
  auto original = input;
  auto s = stream(1);
  auto l = limits();
  TypedContextFrameValidationContext c{s, l, 0, 0};
  Buffers b(1, 1);
  EXPECT_EQ(decode_lzss_position_distance_8m_frame(
                input, c, b.tokens, b.token_scratch, std::span(input).first(1),
                b.raw_scratch, b.meta)
                .error,
            Error::overlapping_buffers);
  EXPECT_EQ(input, original);
  unchanged(b.meta);
  EXPECT_EQ(decode_lzss_position_distance_8m_frame(
                input, c, b.tokens, b.token_scratch, b.raw,
                std::span(input).first(1), b.meta)
                .error,
            Error::overlapping_buffers);
  EXPECT_EQ(input, original);
  guards(b.raw);
}
TEST(PositionDistance8mFrame, MetadataAndConfigurationAliases) {
  auto input = FRAME(literal);
  auto s = stream(1);
  auto l = limits();
  TypedContextFrameValidationContext c{s, l, 0, 0};
  Buffers b(1, 1);
  auto meta = std::as_writable_bytes(std::span(&b.meta, 1)).first(1);
  EXPECT_EQ(decode_lzss_position_distance_8m_frame(input, c, b.tokens,
                                                   b.token_scratch, meta,
                                                   b.raw_scratch, b.meta)
                .error,
            Error::overlapping_buffers);
  unchanged(b.meta);
  auto config = std::as_writable_bytes(std::span(&s, 1)).first(1);
  EXPECT_EQ(decode_lzss_position_distance_8m_frame(input, c, b.tokens,
                                                   b.token_scratch, config,
                                                   b.raw_scratch, b.meta)
                .error,
            Error::overlapping_buffers);
  EXPECT_EQ(s.frame_size, 1);
  unchanged(b.meta);
}
TEST(PositionDistance8mFrame, MetadataOverlapsLiveSerializedOwner) {
  struct Owner {
    TypedContextFrameLayout layout{};
    std::array<std::byte, 86> tail{};
  } owner;
  auto input = FRAME(literal);
  auto storage = std::as_writable_bytes(std::span(&owner, 1));
  ASSERT_GE(storage.size(), input.size());
  std::copy(input.begin(), input.end(), storage.begin());
  const std::vector<std::byte> before(storage.begin(), storage.end());
  auto s = stream(1);
  auto l = limits();
  TypedContextFrameValidationContext c{s, l, 0, 0};
  Buffers b(1, 1);
  EXPECT_EQ(decode_lzss_position_distance_8m_frame(
                storage.first(86), c, b.tokens, b.token_scratch, b.raw,
                b.raw_scratch, owner.layout)
                .error,
            Error::overlapping_buffers);
  EXPECT_TRUE(std::equal(storage.begin(), storage.end(), before.begin()));
  guards(b.raw);
}
TEST(PositionDistance8mFrame, LateLargeFrameFailurePreservesFullOutput) {
  auto input = FRAME(maximum_history);
  input.back() ^= std::byte{1};
  auto s = stream(8388608);
  Buffers b(token_vectors::maximum_history_tokens, s.frame_size);
  EXPECT_EQ(decode(input, s, limits(), b).error, Error::token_error);
  EXPECT_TRUE(std::ranges::all_of(b.raw, [](auto v) { return v == guard; }));
  EXPECT_TRUE(std::ranges::all_of(b.raw_scratch, [](auto v) { return v == guard; }));
  unchanged(b.meta);
  EXPECT_EQ(b.token_scratch[0].literal, 65);
}
TEST(PositionDistance8mFrame, LargeFullCapacitiesAreChargedWithoutAllocation) {
  auto input = FRAME(literal);
  auto s = stream(1);
  auto l = limits();
  TypedContextFrameValidationContext c{s, l, 0, 0};
  constexpr std::size_t capacity = 8388608;
  EXPECT_EQ(query_lzss_position_distance_8m_frame_decode(input, c, capacity,
               capacity, capacity, capacity).error, Error::limit_exceeded);
  l.max_internal_buffered_bytes = 536870912;
  const auto q = query_lzss_position_distance_8m_frame_decode(input, c, capacity,
               capacity, capacity, capacity);
  ASSERT_EQ(q.error, Error::none);
  EXPECT_EQ(q.token_requirements.aggregate_bytes, input.size() +
      2 * capacity * sizeof(LzssTypedToken) + 2 * capacity +
      q.frame_state_bytes + q.token_requirements.working_state_bytes);
}
} // namespace
