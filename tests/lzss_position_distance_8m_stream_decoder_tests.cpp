#include "frame/lzss_position_distance_8m_stream_decoder.hpp"
#include "lzss_position_distance_8m_frame_vectors.hpp"
#include "lzss_position_distance_8m_token_vectors.hpp"
#include <algorithm>
#include <gtest/gtest.h>
#include <limits>
#include <random>
#include <vector>
namespace {
using namespace marc;
using namespace frame::internal;
using Token = dictionary::internal::LzssTypedToken;
using Decoder = LzssPositionDistance8mStreamDecoder;
using S = core::StreamStatus;
using C = core::ErrorCode;
constexpr auto end = core::flag_value(core::ProcessFlags::end_input);
constexpr auto guard = std::byte{0xa5};
core::DecoderLimits limits() {
  core::DecoderLimits l{};
  l.max_frame_size = 8388608;
  l.max_block_size = 8388608;
  l.max_lz_distance = 8388608;
  return l;
}
template <class T>
void put(std::vector<std::byte> &b, std::size_t off, T value) {
  for (std::size_t i = 0; i < sizeof(T); ++i)
    b[off + i] = static_cast<std::byte>((value >> (8 * i)) & 255);
}
template <class A> std::vector<std::byte> bytes(const A &a) {
  std::vector<std::byte> b;
  for (auto x : a)
    b.push_back(static_cast<std::byte>(x));
  return b;
}
template <class A> void append(std::vector<std::byte> &b, const A &a) {
  for (auto x : a)
    b.push_back(static_cast<std::byte>(x));
}
template <class A>
std::vector<std::byte> stream(const std::array<std::uint8_t, 80> &prefix,
                              const A &payload, std::uint32_t frame_size,
                              std::uint64_t total) {
  auto b = bytes(frame_vectors::literal_stream_header);
  put(b, 20, frame_size);
  put(b, 40, total);
  append(b, prefix);
  append(b, payload);
  return b;
}
auto literal() {
  return stream(frame_vectors::literal_prefix, token_vectors::literal, 1, 1);
}
auto two() {
  auto b = literal();
  put(b, 40, std::uint64_t{2});
  auto p = bytes(frame_vectors::literal_prefix);
  put(p, 8, std::uint64_t{1});
  append(b, p);
  append(b, token_vectors::literal);
  return b;
}
struct Buffers {
  std::vector<std::byte> serial, raw, scratch;
  std::vector<Token> tokens, token_scratch;
  Buffers(std::size_t p = 100, std::size_t t = 8, std::size_t f = 8)
      : serial(p, guard), raw(f, guard), scratch(f, guard), tokens(t),
        token_scratch(t) {}
};
auto make(Buffers &b, core::DecoderLimits l = limits(),
          std::size_t retained = 0) {
  return Decoder(l, b.serial, b.tokens, b.token_scratch, b.raw, b.scratch,
                 retained);
}
void guards(std::span<const std::byte> b) {
  EXPECT_TRUE(std::ranges::all_of(b, [](auto x) { return x == guard; }));
}
auto query(const Buffers &b, const core::DecoderLimits &l,
           std::size_t extra = 0) {
  return query_lzss_position_distance_8m_stream_workspace(
      l, b.serial.size(), b.tokens.size(), b.token_scratch.size(), b.raw.size(),
      b.scratch.size(), extra);
}
// All fixture bytes are mathematical first-party vectors. This driver varies
// only scheduling; it never calls an encoder/serializer to create the stream.
std::vector<std::byte> drive(Decoder &d, std::span<const std::byte> data,
                             const std::vector<std::size_t> &chunks,
                             std::size_t cap, bool final_on_data = true,
                             std::uint32_t flags = 0) {
  std::vector<std::byte> raw;
  std::size_t offset = 0, steps = 0;
  S status = S::need_input;
  auto call = [&](std::span<const std::byte> in, std::uint32_t f) {
    std::vector<std::byte> out(cap + 3, guard);
    const auto r = d.process(in, std::span(out).first(cap), f);
    EXPECT_LE(r.input_consumed, in.size());
    EXPECT_LE(r.output_produced, cap);
    EXPECT_TRUE(r.status != S::progress || r.input_consumed ||
                r.output_produced);
    guards(std::span(out).subspan(r.output_produced));
    raw.insert(raw.end(), out.begin(), out.begin() + r.output_produced);
    status = r.status;
    EXPECT_LT(++steps, 100000u);
    return r;
  };
  for (auto count : chunks) {
    const auto stop = offset + count;
    EXPECT_LE(stop, data.size());
    do {
      const auto r =
          call(data.subspan(offset, stop - offset),
               flags | (final_on_data && stop == data.size() ? end : 0));
      offset += r.input_consumed;
      if (status == S::error || status == S::end_of_stream)
        return raw;
      if (!r.input_consumed && !r.output_produced && status != S::need_output)
        break;
    } while (offset < stop || status == S::need_output);
  }
  EXPECT_EQ(offset, data.size());
  while (status != S::end_of_stream && status != S::error)
    call({}, end | flags);
  EXPECT_EQ(status, S::end_of_stream);
  return raw;
}
TEST(PositionDistance8mStream, LiteralEveryInputSplit) {
  const auto b = literal();
  for (std::size_t i = 0; i <= b.size(); ++i)
    for (auto cap : {1u, 17u})
      for (bool final : {false, true}) {
        Buffers w;
        auto d = make(w);
        EXPECT_EQ(drive(d, b, {i, b.size() - i}, cap, final),
                  std::vector<std::byte>{std::byte{65}});
      }
}
TEST(PositionDistance8mStream, OneByteInputOutput) {
  auto b = two();
  Buffers w;
  auto d = make(w);
  EXPECT_EQ(drive(d, b, std::vector<std::size_t>(b.size(), 1), 1),
            std::vector<std::byte>(2, std::byte{65}));
}
TEST(PositionDistance8mStream, EmptyKnownStream) {
  auto b = bytes(frame_vectors::literal_stream_header);
  put(b, 40, std::uint64_t{0});
  Buffers w;
  auto d = make(w);
  auto r = d.process(b, {}, 0);
  EXPECT_EQ(r.input_consumed, 112);
  EXPECT_EQ(r.status, S::progress);
  EXPECT_EQ(d.process({}, {}, 0).status, S::need_input);
  EXPECT_EQ(d.process({}, {}, end).status, S::end_of_stream);
}
TEST(PositionDistance8mStream, FinalSuffixWhileDraining) {
  auto b = two();
  Buffers w;
  auto d = make(w);
  std::array<std::byte, 2> out{guard, guard};
  auto r = d.process(b, {}, end);
  ASSERT_EQ(r.status, S::need_output);
  EXPECT_EQ(r.input_consumed, 198);
  r = d.process(std::span(b).subspan(198), std::span(out).first(1), end);
  EXPECT_EQ(r.status, S::need_output);
  EXPECT_EQ(r.input_consumed, 86);
  EXPECT_EQ(r.output_produced, 1);
  EXPECT_EQ(out[0], std::byte{65});
  EXPECT_EQ(out[1], guard);
  r = d.process({}, out, end);
  EXPECT_EQ(r.status, S::end_of_stream);
  EXPECT_EQ(r.output_produced, 1);
}
TEST(PositionDistance8mStream, EndInputOnLaterEmptyCall) {
  auto b = literal();
  Buffers w;
  auto d = make(w);
  std::array<std::byte, 1> out{};
  auto r = d.process(b, out, 0);
  EXPECT_EQ(r.status, S::progress);
  EXPECT_EQ(r.output_produced, 1);
  EXPECT_EQ(d.process({}, out, 0).status, S::need_input);
  EXPECT_EQ(d.process({}, out, end).status, S::end_of_stream);
}
TEST(PositionDistance8mStream, FlushCannotFinishOrBypass) {
  auto b = literal();
  Buffers w;
  auto d = make(w);
  std::array<std::byte, 1> out{guard};
  auto flush = core::flag_value(core::ProcessFlags::flush);
  auto r = d.process(std::span(b).first(197), out, flush);
  EXPECT_EQ(r.output_produced, 0);
  EXPECT_EQ(out[0], guard);
  r = d.process(std::span(b).last(1), out, flush);
  EXPECT_EQ(r.output_produced, 1);
  EXPECT_EQ(r.status, S::progress);
  EXPECT_EQ(d.process({}, out, flush).status, S::need_input);
}
TEST(PositionDistance8mStream, UnsupportedFlagsBeforeProgress) {
  for (auto f :
       {core::flag_value(core::ProcessFlags::reset_block), std::uint32_t{8}}) {
    Buffers w;
    auto d = make(w);
    std::array<std::byte, 8> out;
    out.fill(guard);
    auto r = d.process(literal(), out, f);
    EXPECT_EQ(r.error.code, C::unsupported);
    EXPECT_EQ(r.input_consumed, 0);
    EXPECT_EQ(r.output_produced, 0);
    guards(out);
    guards(w.serial);
  }
}
TEST(PositionDistance8mStream, StrictTrailingNotConsumed) {
  auto b = literal();
  b.push_back(std::byte{7});
  Buffers w;
  auto d = make(w);
  std::array<std::byte, 2> out{guard, guard};
  auto r = d.process(b, out, end);
  EXPECT_EQ(r.status, S::error);
  EXPECT_EQ(r.input_consumed, 198);
  EXPECT_EQ(r.output_produced, 1);
  EXPECT_EQ(r.error.byte_position, 198);
  EXPECT_EQ(out[0], std::byte{65});
  EXPECT_EQ(out[1], guard);
}
TEST(PositionDistance8mStream, EveryTwoFrameTruncation) {
  auto b = two();
  for (std::size_t n = 0; n < b.size(); ++n) {
    Buffers w;
    auto d = make(w);
    std::array<std::byte, 3> out{guard, guard, guard};
    auto r = d.process(std::span(b).first(n), out, end);
    EXPECT_EQ(r.status, S::error);
    EXPECT_EQ(r.input_consumed, n);
    EXPECT_EQ(r.error.byte_position, n);
    EXPECT_EQ(r.output_produced, n >= 198 ? 1u : 0u);
    guards(std::span(out).subspan(r.output_produced));
    if (r.output_produced)
      EXPECT_EQ(out[0], std::byte{65});
  }
}
TEST(PositionDistance8mStream, LateSecondFailurePreservesFirstSameCall) {
  auto b = two();
  b.back() ^= std::byte{1};
  Buffers w;
  auto d = make(w);
  std::array<std::byte, 3> out{guard, guard, guard};
  auto r = d.process(b, out, end);
  EXPECT_EQ(r.status, S::error);
  EXPECT_EQ(r.output_produced, 1);
  EXPECT_EQ(r.input_consumed, b.size());
  EXPECT_EQ(r.error.byte_position, 278);
  EXPECT_EQ(out[0], std::byte{65});
  guards(std::span(out).subspan(1));
}
TEST(PositionDistance8mStream, FinalShortAndRandomChunks) {
  auto b = stream(frame_vectors::short_overlap_prefix,
                  token_vectors::short_overlap, 267, 268);
  auto p = bytes(frame_vectors::literal_prefix);
  put(p, 8, std::uint64_t{1});
  append(b, p);
  append(b, token_vectors::literal);
  for (unsigned seed = 0; seed < 32; ++seed) {
    std::mt19937 rng(seed);
    std::vector<std::size_t> chunks;
    auto left = b.size();
    while (left) {
      auto n = std::min<std::size_t>(left, 1 + rng() % 23);
      chunks.push_back(n);
      left -= n;
    }
    Buffers w(200, 8, 270);
    auto d = make(w);
    auto out = drive(d, b, chunks, 1 + seed % 19);
    ASSERT_EQ(out.size(), 268);
    for (std::size_t i = 0; i < 267; ++i)
      EXPECT_EQ(out[i], static_cast<std::byte>(i < 8 ? 65 : i % 2 ? 65 : 66));
    EXPECT_EQ(out.back(), std::byte{65});
  }
}
TEST(PositionDistance8mStream, EndedStickyEvenMisuse) {
  Buffers w;
  auto d = make(w);
  auto b = literal();
  std::array<std::byte, 1> out{};
  ASSERT_EQ(d.process(b, out, end).status, S::end_of_stream);
  out[0] = guard;
  auto r = d.process(b, out, 999);
  EXPECT_EQ(r.status, S::end_of_stream);
  EXPECT_EQ(r.input_consumed, 0);
  EXPECT_EQ(r.output_produced, 0);
  guards(out);
}
TEST(PositionDistance8mStream, ErrorStickyAndHeaderPosition) {
  Buffers w;
  auto d = make(w);
  auto b = literal();
  b[0] = std::byte{};
  std::array<std::byte, 2> out{guard, guard};
  auto r = d.process(b, out, end);
  EXPECT_EQ(r.error.byte_position, 0);
  EXPECT_EQ(r.input_consumed, 112);
  guards(out);
  auto repeated = d.process(b, out, 999);
  EXPECT_EQ(repeated.error.code, r.error.code);
  EXPECT_EQ(repeated.error.byte_position, r.error.byte_position);
  EXPECT_EQ(repeated.input_consumed, 0);
  guards(out);
}
TEST(PositionDistance8mStream, EveryWorkspaceCapacityBeforePayload) {
  for (int i = 0; i < 5; ++i) {
    Buffers w;
    if (i == 0)
      w.serial.resize(85);
    if (i == 1)
      w.tokens.clear();
    if (i == 2)
      w.token_scratch.clear();
    if (i == 3)
      w.raw.clear();
    if (i == 4)
      w.scratch.clear();
    auto d = make(w);
    std::array<std::byte, 2> out{guard, guard};
    auto r = d.process(literal(), out, end);
    EXPECT_EQ(r.status, S::error);
    EXPECT_EQ(r.error.code, C::limit_exceeded);
    EXPECT_EQ(r.input_consumed, 192);
    EXPECT_EQ(r.error.byte_position, 112);
    guards(out);
  }
}
TEST(PositionDistance8mStream, WorkspaceFullTailOverlap) {
  Buffers w;
  Decoder d(limits(), w.serial, w.tokens, w.token_scratch, w.raw,
            std::span(w.raw).last(1));
  auto r = d.process(literal(), {}, end);
  EXPECT_EQ(r.error.code, C::invalid_argument);
  EXPECT_EQ(r.input_consumed, 0);
  guards(w.serial);
}
TEST(PositionDistance8mStream, CallerBufferAliases) {
  auto b = literal();
  {
    Buffers w;
    auto d = make(w);
    auto r = d.process(b, std::span(b).last(1), end);
    EXPECT_EQ(r.error.code, C::invalid_argument);
    EXPECT_EQ(r.input_consumed, 0);
  }
  {
    Buffers w;
    auto d = make(w);
    auto r = d.process(std::span(w.serial).last(1), {}, end);
    EXPECT_EQ(r.error.code, C::invalid_argument);
    guards(w.serial);
  }
  {
    Buffers w;
    auto d = make(w);
    auto r = d.process(b, std::span(w.scratch).last(1), end);
    EXPECT_EQ(r.error.code, C::invalid_argument);
    guards(w.scratch);
  }
}
TEST(PositionDistance8mStream, LiveOwnerAliasRejected) {
  Buffers w;
  auto d = make(w);
  auto owner = std::as_writable_bytes(std::span(&d, 1));
  auto r = d.process(literal(), owner.last(1), end);
  EXPECT_EQ(r.error.code, C::invalid_argument);
  EXPECT_EQ(r.input_consumed, 0);
  EXPECT_EQ(r.output_produced, 0);
}
TEST(PositionDistance8mStream, ExactBudgetAndActualSizes) {
  Buffers w;
  auto l = limits();
  l.max_block_size = 1;
  const auto q = query(w, l, 123);
  ASSERT_EQ(q.error, C::none);
  EXPECT_EQ(q.owner_bytes, sizeof(Decoder));
  EXPECT_EQ(q.helper_bytes, 5856);
  EXPECT_EQ(q.aggregate_bytes,
            w.serial.size() +
                (w.tokens.size() + w.token_scratch.size()) * sizeof(Token) +
                w.raw.size() + w.scratch.size() + q.owner_bytes +
                q.control_bytes + q.helper_bytes + 123);
  RecordProperty("owner_bytes", static_cast<int>(q.owner_bytes));
  RecordProperty("control_bytes", static_cast<int>(q.control_bytes));
  RecordProperty("helper_bytes", static_cast<int>(q.helper_bytes));
  l.max_internal_buffered_bytes = q.aggregate_bytes;
  auto d = make(w, l, 123);
  std::array<std::byte, 1> out{};
  ASSERT_EQ(d.process(literal(), out, end).status, S::end_of_stream);
  --l.max_internal_buffered_bytes;
  auto bad = make(w, l, 123);
  EXPECT_EQ(bad.process({}, {}, 0).error.code, C::limit_exceeded);
}
TEST(PositionDistance8mStream, SpareSerializedCapacityChargedOnce) {
  Buffers a(86), b(4096);
  auto l = limits();
  l.max_block_size = 1;
  auto qa = query(a, l), qb = query(b, l);
  EXPECT_EQ(qb.aggregate_bytes - qa.aggregate_bytes, 4096 - 86);
  l.max_internal_buffered_bytes = qb.aggregate_bytes;
  auto d = make(b, l);
  std::array<std::byte, 1> out{};
  ASSERT_EQ(d.process(literal(), out, end).status, S::end_of_stream);
  guards(std::span(b.serial).subspan(86));
  guards(std::span(b.raw).subspan(1));
}
TEST(PositionDistance8mStream, NumericCapacityAndRetainedOverflow) {
  auto l = limits();
  auto max = std::numeric_limits<std::size_t>::max();
  EXPECT_EQ(query_lzss_position_distance_8m_stream_workspace(l, max, 1, 1, 1, 1)
                .error,
            C::limit_exceeded);
  EXPECT_EQ(
      query_lzss_position_distance_8m_stream_workspace(l, 86, max, 1, 1, 1)
          .error,
      C::limit_exceeded);
  EXPECT_EQ(
      query_lzss_position_distance_8m_stream_workspace(l, 86, 1, 1, 1, 1, max)
          .error,
      C::limit_exceeded);
  l.max_internal_buffered_bytes = 512u << 20;
  auto q = query_lzss_position_distance_8m_stream_workspace(
      l, 150995029, 8388608, 8388608, 8388608, 8388608);
  ASSERT_EQ(q.error, C::none);
  EXPECT_EQ(q.aggregate_bytes, 369104693 + q.owner_bytes + q.control_bytes);
}
TEST(PositionDistance8mStream, LimitsCopiedAtConstruction) {
  Buffers w;
  auto l = limits();
  auto d = make(w, l);
  l.max_lz_distance = 1;
  std::array<std::byte, 1> out{};
  EXPECT_EQ(d.process(literal(), out, end).status, S::end_of_stream);
}
TEST(PositionDistance8mStream, AllLengthsActualRaw) {
  auto b =
      stream(frame_vectors::all_lengths_prefix, token_vectors::all_lengths,
             token_vectors::all_lengths_raw, token_vectors::all_lengths_raw);
  Buffers w(b.size(), token_vectors::all_lengths_tokens,
            token_vectors::all_lengths_raw);
  auto d = make(w);
  auto out = drive(d, b, {b.size()}, 31);
  EXPECT_EQ(out.size(), token_vectors::all_lengths_raw);
  EXPECT_TRUE(
      std::ranges::all_of(out, [](auto x) { return x == std::byte{65}; }));
}
TEST(PositionDistance8mStream, UpperHalfHistoryActualRaw) {
  auto b = stream(frame_vectors::upper_half_prefix, token_vectors::upper_half,
                  token_vectors::upper_half_raw, token_vectors::upper_half_raw);
  Buffers w(b.size(), token_vectors::upper_half_tokens,
            token_vectors::upper_half_raw);
  auto d = make(w);
  auto out = drive(d, b, {b.size()}, 65536);
  EXPECT_EQ(out.size(), token_vectors::upper_half_raw);
  EXPECT_TRUE(
      std::ranges::all_of(out, [](auto x) { return x == std::byte{65}; }));
}
TEST(PositionDistance8mStream, ExactEightMiBBoundary) {
  auto b = stream(frame_vectors::maximum_history_prefix,
                  token_vectors::maximum_history, 8388608, 8388608);
  Buffers w(b.size(), token_vectors::maximum_history_tokens, 8388608);
  auto d = make(w);
  auto out = drive(d, b, {b.size()}, 65536);
  ASSERT_EQ(out.size(), 8388608);
  EXPECT_TRUE(
      std::ranges::all_of(out, [](auto x) { return x == std::byte{65}; }));
}
TEST(PositionDistance8mStream, LateEightMiBFailurePublishesNothing) {
  auto b = stream(frame_vectors::maximum_history_prefix,
                  token_vectors::maximum_history, 8388608, 8388608);
  b.back() ^= std::byte{1};
  Buffers w(b.size(), token_vectors::maximum_history_tokens, 8388608);
  auto d = make(w);
  std::vector<std::byte> out(8388608, guard);
  auto r = d.process(b, out, end);
  EXPECT_EQ(r.status, S::error);
  EXPECT_EQ(r.output_produced, 0);
  guards(out);
  guards(w.raw);
}
} // namespace
