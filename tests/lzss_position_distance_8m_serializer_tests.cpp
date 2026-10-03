#include "frame/lzss_position_distance_8m_serializer.hpp"
#include "lzss_position_distance_8m_frame_vectors.hpp"
#include <algorithm>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
#include <vector>
namespace {
using namespace marc;
using namespace frame::internal;
using E = LzssPositionDistance8mSerializeError;
using V = LzssPositionDistance8mPreflightError;
constexpr auto guard = std::byte{0xa5};
core::DecoderLimits limits() {
  core::DecoderLimits l{};
  l.max_block_size = 8388608;
  l.max_internal_buffered_bytes = 512u << 20;
  return l;
}
TypedContextStreamHeader stream(std::uint32_t f = 1,
                                std::uint64_t original = 1) {
  TypedContextStreamHeader s{};
  s.frame_size = f;
  s.original_size = original;
  s.dictionary = {8388608, 3, 258, 0};
  s.range_model_total = 32768;
  s.context_count = 47;
  s.dictionary_variant = 11;
  s.context_algorithm = 1;
  s.context_variant = 12;
  return s;
}
template <class T> T read(std::span<const std::uint8_t> b, std::size_t pos) {
  T value = 0;
  for (std::size_t i = 0; i < sizeof(T); ++i)
    value |= T(b[pos + i]) << (8 * i);
  return value;
}
TypedContextFrameLayout layout(std::span<const std::uint8_t> b) {
  TypedContextFrameLayout x{};
  auto &h = x.header;
  h.flags = read<std::uint16_t>(b, 6);
  h.sequence = read<std::uint64_t>(b, 8);
  h.uncompressed_size = read<std::uint32_t>(b, 16);
  h.token_count = read<std::uint32_t>(b, 20);
  h.event_count = read<std::uint32_t>(b, 24);
  h.decision_count = read<std::uint32_t>(b, 28);
  h.payload_size = read<std::uint32_t>(b, 32);
  h.descriptor_size = read<std::uint32_t>(b, 36);
  h.context_side_data_size = read<std::uint32_t>(b, 40);
  h.checksum_trailer_size = read<std::uint32_t>(b, 44);
  x.descriptor = {read<std::uint32_t>(b, 64), read<std::uint32_t>(b, 68),
                  read<std::uint16_t>(b, 72)};
  x.serialized_size = 80 + h.payload_size;
  return x;
}
bool counts_ok(const TypedContextFrameHeader &h) {
  const std::uint64_t f = h.uncompressed_size, t = h.token_count,
                      e = h.event_count, d = h.decision_count,
                      p = h.payload_size;
  return t && t <= f && e >= 2 * t && e <= std::min(2 * f, 5 * t) && d >= e &&
         d <= std::min(9 * f, 33 * t) && p >= 5 &&
         p <= std::min(2 * d + 5, 18 * f + 5);
}
void failure_stream(const TypedContextStreamHeader &s, E expected,
                    core::DecoderLimits l = limits(), std::size_t n = 128,
                    std::size_t retained = 0) {
  std::vector<std::byte> out(n, guard);
  std::size_t written = 77;
  auto r = serialize_lzss_position_distance_8m_stream_header(s, l, out, written,
                                                             retained);
  EXPECT_EQ(r.error, expected);
  EXPECT_EQ(r.bytes_committed, 0);
  EXPECT_EQ(written, 77);
  for (auto b : out)
    EXPECT_EQ(b, guard);
}
void failure_prefix(const TypedContextFrameLayout &x,
                    const TypedContextFrameValidationContext &c, E expected,
                    V v = V::none, std::size_t n = 96,
                    std::size_t retained = 0) {
  std::vector<std::byte> out(n, guard);
  std::size_t written = 77;
  auto r = serialize_lzss_position_distance_8m_frame_prefix(x, c, out, written,
                                                            retained);
  EXPECT_EQ(r.error, expected);
  if (v != V::none)
    EXPECT_EQ(r.validation_error, v);
  EXPECT_EQ(r.bytes_committed, 0);
  EXPECT_EQ(written, 77);
  for (auto b : out)
    EXPECT_EQ(b, guard);
}
void fixed(std::span<const std::uint8_t> b) {
  auto x = layout(b);
  auto s = stream(x.header.uncompressed_size, x.header.uncompressed_size);
  auto l = limits();
  TypedContextFrameValidationContext c{s, l, 0, 0};
  if (!counts_ok(x.header)) {
    failure_prefix(x, c, E::validation_error, V::contradictory_counts);
    return;
  }
  std::array<std::byte, 97> out;
  out.fill(guard);
  std::size_t written = 77;
  auto r = serialize_lzss_position_distance_8m_frame_prefix(x, c, out, written);
  ASSERT_EQ(r.error, E::none);
  ASSERT_EQ(r.bytes_committed, 80);
  EXPECT_EQ(written, 80);
  for (std::size_t i = 0; i < 80; ++i)
    EXPECT_EQ(out[i], std::byte{b[i]});
  for (std::size_t i = 80; i < out.size(); ++i)
    EXPECT_EQ(out[i], guard);
  TypedContextFrameLayout parsed{};
  LzssPositionDistance8mFrameRequirements req{};
  ASSERT_EQ(preflight_lzss_position_distance_8m_frame_prefix(
                std::span(out).first(80), c, parsed, req),
            V::none);
  EXPECT_EQ(parsed.serialized_size, x.serialized_size);
  EXPECT_EQ(req.token_count, x.header.token_count);
}
TEST(PositionDistance8mSerializer, LiteralHeaderExact) {
  auto s = stream();
  auto l = limits();
  std::array<std::byte, 129> out;
  out.fill(guard);
  std::size_t written = 77;
  auto r =
      serialize_lzss_position_distance_8m_stream_header(s, l, out, written);
  ASSERT_EQ(r.error, E::none);
  EXPECT_EQ(r.bytes_committed, 112);
  EXPECT_EQ(written, 112);
  for (std::size_t i = 0; i < 112; ++i)
    EXPECT_EQ(out[i], std::byte{frame_vectors::literal_stream_header[i]});
  for (std::size_t i = 112; i < out.size(); ++i)
    EXPECT_EQ(out[i], guard);
}
TEST(PositionDistance8mSerializer, EmptyOriginalAndHeaderLimits) {
  for (auto original :
       {std::uint64_t{0}, std::uint64_t{1}, std::uint64_t{8388609},
        limits().max_total_output_size}) {
    auto s = stream(8388608, original);
    auto l = limits();
    std::array<std::byte, 112> out{};
    std::size_t written = 77;
    auto r =
        serialize_lzss_position_distance_8m_stream_header(s, l, out, written);
    ASSERT_EQ(r.error, E::none);
    TypedContextStreamHeader parsed{};
    std::size_t consumed = 99;
    ASSERT_EQ(
        parse_lzss_position_distance_8m_stream_header(out, l, parsed, consumed),
        V::none);
    EXPECT_EQ(parsed.original_size, original);
    EXPECT_EQ(consumed, 112);
  }
}
TEST(PositionDistance8mSerializer, FourteenIndependentPrefixes) {
  for (auto b : {std::span(frame_vectors::literal_prefix),
                 std::span(frame_vectors::short_overlap_prefix),
                 std::span(frame_vectors::all_lengths_prefix),
                 std::span(frame_vectors::upper_half_prefix),
                 std::span(frame_vectors::maximum_history_prefix),
                 std::span(frame_vectors::before_history_prefix),
                 std::span(frame_vectors::history_overrun_prefix),
                 std::span(frame_vectors::window_overrun_prefix),
                 std::span(frame_vectors::raw_overrun_prefix),
                 std::span(frame_vectors::declared_short_prefix),
                 std::span(frame_vectors::declared_long_prefix),
                 std::span(frame_vectors::extra_token_prefix),
                 std::span(frame_vectors::invalid_length_prefix),
                 std::span(frame_vectors::endpoint_prefix)})
    fixed(b);
}
TEST(PositionDistance8mSerializer, NonzeroSequenceFinalShortFrame) {
  auto s = stream(17, 22);
  auto l = limits();
  auto x = layout(frame_vectors::literal_prefix);
  x.header.sequence = 1;
  x.header.uncompressed_size = 5;
  x.header.token_count = 5;
  x.header.event_count = 10;
  x.header.decision_count = 10;
  x.descriptor.decision_count = 10;
  std::array<std::byte, 80> out{};
  std::size_t written = 77;
  auto r = serialize_lzss_position_distance_8m_frame_prefix(x, {s, l, 1, 17},
                                                            out, written);
  ASSERT_EQ(r.error, E::none);
  EXPECT_EQ(out[8], std::byte{1});
  EXPECT_EQ(out[16], std::byte{5});
  TypedContextFrameLayout parsed{};
  LzssPositionDistance8mFrameRequirements req{};
  EXPECT_EQ(preflight_lzss_position_distance_8m_frame_prefix(out, {s, l, 1, 17},
                                                             parsed, req),
            V::none);
}
TEST(PositionDistance8mSerializer, EveryStreamCapacityShortage) {
  auto s = stream();
  for (std::size_t n = 0; n < 112; ++n)
    failure_stream(s, E::output_too_small, limits(), n);
}
TEST(PositionDistance8mSerializer, EveryPrefixCapacityShortage) {
  auto s = stream();
  auto l = limits();
  auto x = layout(frame_vectors::literal_prefix);
  for (std::size_t n = 0; n < 80; ++n)
    failure_prefix(x, {s, l, 0, 0}, E::output_too_small, V::none, n);
}
TEST(PositionDistance8mSerializer, StreamIdentitiesParameters) {
  for (unsigned i = 0; i < 12; ++i) {
    auto s = stream();
    switch (i) {
    case 0:
      s.dictionary_variant = 10;
      break;
    case 1:
      s.context_algorithm = 2;
      break;
    case 2:
      s.context_variant = 11;
      break;
    case 3:
      s.context_count = 46;
      break;
    case 4:
      s.range_model_total = 32767;
      break;
    case 5:
      s.frame_size = 0;
      break;
    case 6:
      s.frame_size = 8388609;
      break;
    case 7:
      s.dictionary.window_size = 0;
      break;
    case 8:
      s.dictionary.window_size = 8388609;
      break;
    case 9:
      s.dictionary.min_match_length = 5;
      break;
    case 10:
      s.dictionary.max_match_length = 259;
      break;
    case 11:
      s.dictionary.flags = 1;
      break;
    }
    failure_stream(s, E::validation_error);
  }
}
TEST(PositionDistance8mSerializer, StreamPolicy) {
  auto s = stream();
  for (unsigned i = 0; i < 5; ++i) {
    auto l = limits();
    switch (i) {
    case 0:
      l.max_lz_distance = 8388607;
      break;
    case 1:
      l.max_entropy_table_entries = 2598;
      break;
    case 2:
      l.max_range_model_total = 32767;
      break;
    case 3:
      l.max_total_output_size = 0;
      break;
    case 4:
      l.max_internal_buffered_bytes = 5487;
      l.max_block_size = 1;
      break;
    }
    failure_stream(s, E::limit_exceeded, l);
  }
}
TEST(PositionDistance8mSerializer, PrefixUnsupportedFeatures) {
  auto s = stream();
  auto l = limits();
  for (unsigned i = 0; i < 3; ++i) {
    auto x = layout(frame_vectors::literal_prefix);
    switch (i) {
    case 0:
      x.header.flags = 1;
      break;
    case 1:
      x.header.context_side_data_size = 1;
      break;
    case 2:
      x.header.checksum_trailer_size = 4;
      break;
    }
    failure_prefix(x, {s, l, 0, 0}, E::validation_error,
                   V::unsupported_feature);
  }
}
TEST(PositionDistance8mSerializer, PrefixCountAndDescriptorErrors) {
  auto s = stream();
  auto l = limits();
  for (unsigned i = 0; i < 9; ++i) {
    auto x = layout(frame_vectors::literal_prefix);
    switch (i) {
    case 0:
      x.header.token_count = 0;
      break;
    case 1:
      x.header.event_count = 3;
      break;
    case 2:
      x.header.decision_count = 1;
      break;
    case 3:
      x.header.payload_size = 4;
      break;
    case 4:
      x.header.descriptor_size = 15;
      break;
    case 5:
      x.descriptor.decision_count = 3;
      break;
    case 6:
      x.descriptor.payload_size = 7;
      break;
    case 7:
      x.descriptor.context_count = 46;
      break;
    case 8:
      x.serialized_size = 87;
      break;
    }
    failure_prefix(x, {s, l, 0, 0}, E::validation_error);
  }
}
TEST(PositionDistance8mSerializer, PrefixPositionAndRaw) {
  auto s = stream(17, 22);
  auto l = limits();
  auto x = layout(frame_vectors::literal_prefix);
  failure_prefix(x, {s, l, 0, 0}, E::validation_error,
                 V::unexpected_frame_size);
  s = stream();
  x.header.sequence = 1;
  failure_prefix(x, {s, l, 0, 0}, E::validation_error, V::unexpected_sequence);
  x.header.sequence = 0;
  failure_prefix(x, {s, l, 0, 1}, E::validation_error, V::unexpected_sequence);
  x.header.sequence = 1;
  failure_prefix(x, {s, l, 1, 1}, E::validation_error,
                 V::unexpected_frame_size);
  x.header.sequence = 0;
  s.original_size = 0;
  failure_prefix(x, {s, l, 0, 0}, E::validation_error,
                 V::unexpected_frame_size);
}
TEST(PositionDistance8mSerializer, PrefixPayloadAndBlockPolicy) {
  auto x = layout(frame_vectors::literal_prefix);
  auto s = stream();
  auto l = limits();
  l.max_compressed_payload_size = 5;
  failure_prefix(x, {s, l, 0, 0}, E::limit_exceeded);
  auto large = layout(frame_vectors::all_lengths_prefix);
  s = stream(large.header.uncompressed_size, large.header.uncompressed_size);
  l = limits();
  l.max_block_size = large.header.uncompressed_size - 1;
  failure_prefix(large, {s, l, 0, 0}, E::limit_exceeded);
}
TEST(PositionDistance8mSerializer, ExactStreamLedger) {
  auto s = stream();
  auto l = limits();
  l.max_block_size = 1;
  auto q = query_lzss_position_distance_8m_stream_serialize(s, l, 8192, 123);
  ASSERT_EQ(q.error, E::none);
  RecordProperty("stream_serializer_working_bytes",
                 static_cast<int>(q.working_state_bytes));
  EXPECT_EQ(q.aggregate_bytes, 8192 + 123 + q.working_state_bytes);
  l.max_internal_buffered_bytes = q.aggregate_bytes;
  EXPECT_EQ(
      query_lzss_position_distance_8m_stream_serialize(s, l, 8192, 123).error,
      E::none);
  --l.max_internal_buffered_bytes;
  failure_stream(s, E::limit_exceeded, l, 8192, 123);
}
TEST(PositionDistance8mSerializer,
     ExactPrefixLedgerAndSeparateDecoderAdmission) {
  auto s = stream();
  auto x = layout(frame_vectors::literal_prefix);
  auto l = limits();
  l.max_block_size = 1;
  auto q = query_lzss_position_distance_8m_prefix_serialize(x, {s, l, 0, 0},
                                                            8192, 123);
  ASSERT_EQ(q.error, E::none);
  RecordProperty("prefix_serializer_working_bytes",
                 static_cast<int>(q.working_state_bytes));
  EXPECT_EQ(q.aggregate_bytes, 8192 + 123 + q.working_state_bytes);
  l.max_internal_buffered_bytes = q.aggregate_bytes;
  EXPECT_EQ(query_lzss_position_distance_8m_prefix_serialize(x, {s, l, 0, 0},
                                                             8192, 123)
                .error,
            E::none);
  --l.max_internal_buffered_bytes;
  failure_prefix(x, {s, l, 0, 0}, E::limit_exceeded, V::none, 8192, 123);
  l = limits();
  l.max_block_size = 1;
  l.max_internal_buffered_bytes = 5500;
  EXPECT_EQ(
      query_lzss_position_distance_8m_prefix_serialize(x, {s, l, 0, 0}, 80)
          .error,
      E::limit_exceeded);
}
TEST(PositionDistance8mSerializer, Overflow) {
  auto s = stream();
  auto l = limits();
  auto x = layout(frame_vectors::literal_prefix);
  auto max = std::numeric_limits<std::size_t>::max();
  EXPECT_EQ(query_lzss_position_distance_8m_stream_serialize(s, l, max).error,
            E::arithmetic_overflow);
  EXPECT_EQ(
      query_lzss_position_distance_8m_prefix_serialize(x, {s, l, 0, 0}, max)
          .error,
      E::arithmetic_overflow);
  failure_stream(s, E::arithmetic_overflow, l, 128, max);
  failure_prefix(x, {s, l, 0, 0}, E::arithmetic_overflow, V::none, 96, max);
}
TEST(PositionDistance8mSerializer, FullUnusedTailMetadataAlias) {
  auto s = stream();
  auto l = limits();
  std::array<std::size_t, 32> words;
  words.fill(77);
  auto out = std::as_writable_bytes(std::span(words));
  auto r = serialize_lzss_position_distance_8m_stream_header(s, l, out,
                                                             words.back());
  EXPECT_EQ(r.error, E::overlapping_buffers);
  EXPECT_EQ(r.bytes_committed, 0);
  for (auto n : words)
    EXPECT_EQ(n, 77);
  auto x = layout(frame_vectors::literal_prefix);
  r = serialize_lzss_position_distance_8m_frame_prefix(x, {s, l, 0, 0}, out,
                                                       words.back());
  EXPECT_EQ(r.error, E::overlapping_buffers);
  for (auto n : words)
    EXPECT_EQ(n, 77);
}
TEST(PositionDistance8mSerializer, InputOutputAlias) {
  auto s = stream();
  auto l = limits();
  std::array<std::byte, sizeof(s)> original{};
  std::memcpy(original.data(), &s, sizeof(s));
  std::size_t written = 77;
  auto r = serialize_lzss_position_distance_8m_stream_header(
      s, l, std::as_writable_bytes(std::span(&s, 1)), written);
  EXPECT_EQ(r.error, E::overlapping_buffers);
  EXPECT_EQ(std::memcmp(original.data(), &s, sizeof(s)), 0);
  EXPECT_EQ(written, 77);
}
TEST(PositionDistance8mSerializer, PrefixMetadataAliasesInput) {
  auto s = stream();
  auto l = limits();
  auto x = layout(frame_vectors::literal_prefix);
  std::array<std::byte, 96> out;
  out.fill(guard);
  auto r = serialize_lzss_position_distance_8m_frame_prefix(
      x, {s, l, 0, 0}, out, x.serialized_size);
  EXPECT_EQ(r.error, E::overlapping_buffers);
  EXPECT_EQ(x.serialized_size, 86);
  for (auto b : out)
    EXPECT_EQ(b, guard);
}
TEST(PositionDistance8mSerializer, LiveLimitsAlias) {
  auto s = stream();
  auto l = limits();
  std::array<std::byte, sizeof(l)> original{};
  std::memcpy(original.data(), &l, sizeof(l));
  std::size_t written = 77;
  auto r = serialize_lzss_position_distance_8m_stream_header(
      s, l, std::as_writable_bytes(std::span(&l, 1)), written);
  EXPECT_EQ(r.error, E::overlapping_buffers);
  EXPECT_EQ(std::memcmp(original.data(), &l, sizeof(l)), 0);
  auto x = layout(frame_vectors::literal_prefix);
  r = serialize_lzss_position_distance_8m_frame_prefix(
      x, {s, l, 0, 0}, std::as_writable_bytes(std::span(&l, 1)), written);
  EXPECT_EQ(r.error, E::overlapping_buffers);
  EXPECT_EQ(std::memcmp(original.data(), &l, sizeof(l)), 0);
}
TEST(PositionDistance8mSerializer, RepeatedCallsDeterministic) {
  auto s = stream();
  auto l = limits();
  auto x = layout(frame_vectors::literal_prefix);
  std::array<std::byte, 129> a, b;
  a.fill(guard);
  b.fill(guard);
  std::size_t wa = 77, wb = 88;
  ASSERT_EQ(
      serialize_lzss_position_distance_8m_stream_header(s, l, a, wa).error,
      E::none);
  ASSERT_EQ(
      serialize_lzss_position_distance_8m_stream_header(s, l, b, wb).error,
      E::none);
  EXPECT_EQ(a, b);
  a.fill(guard);
  b.fill(guard);
  ASSERT_EQ(
      serialize_lzss_position_distance_8m_frame_prefix(x, {s, l, 0, 0}, a, wa)
          .error,
      E::none);
  ASSERT_EQ(
      serialize_lzss_position_distance_8m_frame_prefix(x, {s, l, 0, 0}, b, wb)
          .error,
      E::none);
  EXPECT_EQ(a, b);
}
} // namespace
