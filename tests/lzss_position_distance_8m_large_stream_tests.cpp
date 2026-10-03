#include "core/checked_math.hpp"
#include "frame/lzss_position_distance_8m_stream_decoder.hpp"
#include "frame/lzss_position_distance_8m_stream_encoder.hpp"
#include "lzss_position_distance_8m_frame_encode_vectors.hpp"
#include "lzss_position_distance_8m_frame_vectors.hpp"
#include "lzss_position_distance_8m_large_frame_vectors.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <vector>
namespace {
using namespace marc;
using namespace frame::internal;
using Token = dictionary::internal::LzssTypedToken;
using Op = context::internal::ModeledOperation;
using Encoder = LzssPositionDistance8mStreamEncoder;
using Decoder = LzssPositionDistance8mStreamDecoder;
using C = core::ErrorCode;
using S = core::StreamStatus;
constexpr auto end = core::flag_value(core::ProcessFlags::end_input);
constexpr auto flush = core::flag_value(core::ProcessFlags::flush);
constexpr auto guard = std::byte{0xa5};
constexpr std::size_t policy = 512u << 20;
constexpr std::size_t fixture_bytes =
    sizeof(large_frame_vectors::repeat1m) +
    sizeof(large_frame_vectors::repeat4m) +
    sizeof(large_frame_vectors::repeat8m) +
    sizeof(frame_vectors::literal_stream_header) +
    sizeof(frame_encode_vectors::repeat_frame);
std::size_t sum(std::initializer_list<std::size_t> values) {
  std::size_t result{};
  for (auto n : values)
    if (!core::checked_add(result, n, result))
      std::abort();
  return result;
}
std::size_t mul(std::size_t a, std::size_t b) {
  std::size_t result{};
  if (!core::checked_multiply(a, b, result))
    std::abort();
  return result;
}
template <class T> std::size_t capacity_bytes(const std::vector<T> &v) {
  return mul(v.capacity(), sizeof(T));
}
template <class T> void put(std::vector<std::byte> &v, std::size_t off, T x) {
  for (std::size_t i = 0; i < sizeof(T); ++i)
    v[off + i] = static_cast<std::byte>((x >> (8 * i)) & 255);
}
std::size_t field(std::span<const std::uint8_t> v, std::size_t off) {
  std::size_t x{};
  for (unsigned i = 0; i < 4; ++i)
    x |= static_cast<std::size_t>(v[off + i]) << (8 * i);
  return x;
}
struct Recipe {
  const char *name;
  std::span<const std::uint8_t> frame;
  std::size_t f, t, e, p;
};
template <class A> Recipe recipe(const char *name, const A &a) {
  return {name, a, field(a, 16), field(a, 20), field(a, 24), field(a, 32)};
}
// Concrete conservative outer controls, separate from persistent controller
// copies and the production encoder/decoder owner/control/helper reservations.
struct CallControls {
  LzssPositionDistance8mStreamEncodeCapacities capacities;
  LzssPositionDistance8mStreamEncodeRequirements encode_query;
  LzssPositionDistance8mStreamRequirements decode_query;
  core::ProcessResult result;
  Recipe recipe_copy;
  std::span<const std::byte> input;
  std::span<std::byte> output;
  std::array<std::size_t, 8> scalars;
};
struct EncodeBuffers {
  std::vector<std::byte> raw, publication, frame, payload;
  std::vector<Token> tokens, ts;
  std::vector<Op> ops, os;
  std::vector<std::uint32_t> index;
  explicit EncodeBuffers(const Recipe &r)
      : raw(r.f, guard), publication(80 + r.p + 16, guard),
        frame(publication.size(), guard), payload(r.p, guard), tokens(r.t),
        ts(r.t), ops(r.e), os(r.e), index(65536 + r.f) {}
  auto workspace() {
    return LzssPositionDistance8mFrameEncodeWorkspace{
        tokens, ts, index, ops, os, frame, payload};
  }
  auto caps(std::size_t i, std::size_t o) const {
    return LzssPositionDistance8mStreamEncodeCapacities{raw.size(),
                                                        publication.size(),
                                                        tokens.size(),
                                                        ts.size(),
                                                        index.size(),
                                                        ops.size(),
                                                        os.size(),
                                                        frame.size(),
                                                        payload.size(),
                                                        i,
                                                        o};
  }
  std::size_t spares() const {
    return sum({capacity_bytes(raw) - raw.size(),
                capacity_bytes(publication) - publication.size(),
                capacity_bytes(frame) - frame.size(),
                capacity_bytes(payload) - payload.size(),
                capacity_bytes(tokens) - mul(tokens.size(), sizeof(Token)),
                capacity_bytes(ts) - mul(ts.size(), sizeof(Token)),
                capacity_bytes(ops) - mul(ops.size(), sizeof(Op)),
                capacity_bytes(os) - mul(os.size(), sizeof(Op)),
                capacity_bytes(index) - mul(index.size(), 4)});
  }
};
struct DecodeBuffers {
  std::vector<std::byte> serial, raw, scratch;
  std::vector<Token> tokens, ts;
  explicit DecodeBuffers(const Recipe &r)
      : serial(80 + r.p + 16, guard), raw(r.f, guard), scratch(r.f, guard),
        tokens(r.t), ts(r.t) {}
  std::size_t spares() const {
    return sum({capacity_bytes(serial) - serial.size(),
                capacity_bytes(raw) - raw.size(),
                capacity_bytes(scratch) - scratch.size(),
                capacity_bytes(tokens) - mul(tokens.size(), sizeof(Token)),
                capacity_bytes(ts) - mul(ts.size(), sizeof(Token))});
  }
};
struct Controller {
  Recipe r;
  core::DecoderLimits limits{};
  TypedContextStreamHeader header{};
  std::vector<std::byte> input, expected, wire, step;
  LzssPositionDistance8mStreamEncodeRequirements eq{};
  LzssPositionDistance8mStreamRequirements dq{};
  core::ProcessResult result{};
  std::size_t written{}, offset{}, stop{}, calls{}, decoded{}, encode_peak{},
      decode_peak{}, external{}, capacity{}, full_frames{}, tail{};
  std::uint32_t rng{1434};
  Controller(Recipe value, std::size_t final = 0, bool failed_second = false)
      : r(value), full_frames(failed_second ? 1 : 2), tail(final) {
    limits.max_frame_size = 8388608;
    limits.max_block_size = 8388608;
    limits.max_lz_distance = 8388608;
    limits.max_internal_buffered_bytes = policy;
    header.frame_size = static_cast<std::uint32_t>(r.f);
    header.original_size = sum({mul(2, r.f), tail});
    header.dictionary = {8388608, 3, 258, 0};
    header.dictionary_variant = 11;
    header.context_algorithm = 1;
    header.context_variant = 12;
    header.context_count = 47;
    header.range_model_total = 32768;
  }
  std::size_t raw_size() const { return sum({mul(2, r.f), tail}); }
  std::size_t wire_size() const {
    return sum({112, mul(full_frames, r.frame.size()),
                tail ? frame_encode_vectors::repeat_frame.size() : 0});
  }
  std::size_t owners() const {
    return sum({capacity_bytes(input), capacity_bytes(expected),
                capacity_bytes(wire), capacity_bytes(step), sizeof(Controller),
                sizeof(CallControls), fixture_bytes});
  }
  auto numeric(std::size_t cap) const {
    return LzssPositionDistance8mStreamEncodeCapacities{
        r.f, 80 + r.p + 16, r.t, r.t,        65536 + r.f, r.e,
        r.e, 80 + r.p + 16, r.p, raw_size(), cap};
  }
  void prepare(std::size_t cap, bool failed_second) {
    capacity = cap;
    // Query all prospective owned buffers before allocating any raw/workspace.
    external = sum({raw_size(), wire_size(), wire_size() + 64, cap + 3,
                    sizeof(Controller), sizeof(CallControls),
                    sizeof(EncodeBuffers), fixture_bytes});
    eq = query_lzss_position_distance_8m_stream_encode_workspace(
        limits, numeric(cap), external);
    ASSERT_EQ(eq.error, C::none);
    ASSERT_LE(eq.aggregate_bytes, policy);
    input = std::vector<std::byte>(raw_size(), std::byte{65});
    expected = std::vector<std::byte>(wire_size());
    wire = std::vector<std::byte>(wire_size() + 64, guard);
    step = std::vector<std::byte>(cap + 3, guard);
    ASSERT_LE(sum({owners(), sizeof(EncodeBuffers)}), external);
    for (std::size_t i = 0; i < 112; ++i)
      expected[i] =
          static_cast<std::byte>(frame_vectors::literal_stream_header[i]);
    put(expected, 20, header.frame_size);
    put(expected, 40, header.original_size);
    for (std::size_t seq = 0; seq < full_frames; ++seq) {
      const auto at = 112 + seq * r.frame.size();
      for (std::size_t i = 0; i < r.frame.size(); ++i)
        expected[at + i] = static_cast<std::byte>(r.frame[i]);
      put(expected, at + 8, static_cast<std::uint64_t>(seq));
    }
    if (tail) {
      ASSERT_EQ(tail, 32u);
      const auto at = 112 + full_frames * r.frame.size();
      for (std::size_t i = 0; i < frame_encode_vectors::repeat_frame.size();
           ++i)
        expected[at + i] =
            static_cast<std::byte>(frame_encode_vectors::repeat_frame[i]);
      put(expected, at + 8, static_cast<std::uint64_t>(full_frames));
    }
    if (failed_second)
      for (std::size_t i = r.f; i < input.size(); ++i)
        input[i] = static_cast<std::byte>((i - r.f) % 251);
  }
  std::size_t next_stop(int mode) {
    if (mode == 0)
      return input.size();
    if (mode == 1) {
      for (auto boundary :
           {r.f - 1, r.f, r.f + 1, mul(2, r.f) - 1, mul(2, r.f), input.size()})
        if (boundary > offset)
          return boundary;
    }
    rng = rng * 1664525u + 1013904223u;
    return std::min(input.size(), sum({offset, 1 + (rng % 65537)}));
  }
  void encode(int mode, bool failed_second = false) {
    std::fill(wire.begin(), wire.end(), guard);
    written = offset = calls = 0;
    stop = next_stop(mode);
    // Destruction of this entire scope releases encoder-owned buffers before
    // consumer construction. No clear()/logical-last-use owner subtraction.
    {
      EncodeBuffers b(r);
      external = sum({owners(), sizeof(EncodeBuffers), b.spares()});
      eq = query_lzss_position_distance_8m_stream_encode_workspace(
          limits, b.caps(input.size(), capacity), external);
      ASSERT_EQ(eq.error, C::none);
      ASSERT_LE(eq.aggregate_bytes, policy);
      encode_peak = std::max(encode_peak, eq.aggregate_bytes);
      limits.max_internal_buffered_bytes = eq.aggregate_bytes;
      Encoder e(header, limits, b.raw, b.publication, b.workspace(), external);
      do {
        std::fill(step.begin(), step.end(), guard);
        const auto flags =
            flush |
            ((mode != 1 && stop == input.size()) || offset == input.size() ? end
                                                                           : 0);
        result = e.process(std::span(input).subspan(offset, stop - offset),
                           std::span(step).first(capacity), flags);
        ASSERT_LE(result.input_consumed, stop - offset);
        ASSERT_LE(result.output_produced, capacity);
        ASSERT_TRUE(result.status != S::progress || result.input_consumed ||
                    result.output_produced);
        ASSERT_TRUE(
            std::ranges::all_of(std::span(step).subspan(result.output_produced),
                                [](auto x) { return x == guard; }));
        ASSERT_LE(sum({written, result.output_produced}), wire.size());
        std::copy_n(step.begin(), result.output_produced,
                    wire.begin() + written);
        written += result.output_produced;
        offset += result.input_consumed;
        if (offset == stop && offset < input.size())
          stop = next_stop(mode);
        ASSERT_LT(++calls, 100000u);
      } while (result.status != S::error && result.status != S::end_of_stream);
      EXPECT_EQ(result.status, failed_second ? S::error : S::end_of_stream);
      EXPECT_EQ(result.error.code, failed_second ? C::limit_exceeded : C::none);
      EXPECT_EQ(offset, input.size());
      EXPECT_EQ(written, expected.size());
      EXPECT_TRUE(std::equal(expected.begin(), expected.end(), wire.begin()));
      EXPECT_TRUE(std::ranges::all_of(std::span(wire).subspan(written),
                                      [](auto x) { return x == guard; }));
      if (failed_second)
        EXPECT_EQ(result.error.byte_position, r.f);
      auto repeat = e.process({}, {}, 0xffffffffu);
      EXPECT_EQ(repeat.status, result.status);
      EXPECT_EQ(repeat.error.code, result.error.code);
      EXPECT_EQ(repeat.error.byte_position, result.error.byte_position);
      EXPECT_EQ(repeat.input_consumed, 0u);
      EXPECT_EQ(repeat.output_produced, 0u);
    }
  }
  void decode(bool malformed = false, std::size_t successful = 0,
              int mode = 0) {
    // No EncodeBuffers/Encoder object survives here. External wire/raw/expected
    // owners remain counted, with fixed conservative call extents included.
    // The replacement expression can briefly own BOTH old and new step
    // buffers. Reserve both before allocation; subtract the old capacity only
    // after the completed move assignment actually destroys its allocation.
    external =
        sum({owners(), 65537 + 3, sizeof(DecodeBuffers), written, 65537});
    dq = query_lzss_position_distance_8m_stream_workspace(
        limits, 80 + r.p + 16, r.t, r.t, r.f, r.f, external);
    ASSERT_EQ(dq.error, C::none);
    ASSERT_LE(dq.aggregate_bytes, policy);
    decode_peak = std::max(decode_peak, dq.aggregate_bytes);
    step = std::vector<std::byte>(65537 + 3, guard);
    {
      DecodeBuffers b(r);
      external =
          sum({owners(), sizeof(DecodeBuffers), b.spares(), written, 65537});
      dq = query_lzss_position_distance_8m_stream_workspace(
          limits, b.serial.size(), b.tokens.size(), b.ts.size(), b.raw.size(),
          b.scratch.size(), external);
      ASSERT_EQ(dq.error, C::none);
      ASSERT_LE(dq.aggregate_bytes, policy);
      decode_peak = std::max(decode_peak, dq.aggregate_bytes);
      Decoder d(limits, b.serial, b.tokens, b.ts, b.raw, b.scratch, external);
      offset = decoded = calls = 0;
      do {
        std::fill(step.begin(), step.end(), guard);
        const auto n =
            std::min<std::size_t>(mode ? 7 : written, written - offset);
        const auto cap = calls == 0 ? 0u : 65537u;
        result = d.process(std::span(wire).subspan(offset, n),
                           std::span(step).first(cap),
                           offset + n == written ? end : 0);
        ASSERT_LE(result.input_consumed, n);
        ASSERT_LE(result.output_produced, cap);
        ASSERT_TRUE(result.status != S::progress || result.input_consumed ||
                    result.output_produced);
        ASSERT_TRUE(
            std::ranges::all_of(std::span(step).subspan(result.output_produced),
                                [](auto x) { return x == guard; }));
        ASSERT_LE(sum({decoded, result.output_produced}), input.size());
        ASSERT_TRUE(std::equal(step.begin(),
                               step.begin() + result.output_produced,
                               input.begin() + decoded));
        offset += result.input_consumed;
        decoded += result.output_produced;
        ASSERT_LT(++calls, 100000u);
      } while (result.status != S::error && result.status != S::end_of_stream);
      EXPECT_EQ(result.status, malformed ? S::error : S::end_of_stream);
      EXPECT_EQ(decoded, malformed ? successful : input.size());
      auto repeat = d.process({}, {}, 0xffffffffu);
      EXPECT_EQ(repeat.status, result.status);
      EXPECT_EQ(repeat.error.code, result.error.code);
      EXPECT_EQ(repeat.error.byte_position, result.error.byte_position);
      EXPECT_EQ(repeat.input_consumed, 0u);
      EXPECT_EQ(repeat.output_produced, 0u);
    }
  }
  void report() const {
    ::testing::Test::RecordProperty("F", r.f);
    ::testing::Test::RecordProperty("raw_bytes", input.size());
    ::testing::Test::RecordProperty("wire_bytes", written);
    ::testing::Test::RecordProperty("encode_peak", encode_peak);
    ::testing::Test::RecordProperty("decode_peak", decode_peak);
    ::testing::Test::RecordProperty("controller_bytes", sizeof(Controller));
    ::testing::Test::RecordProperty("call_controls_bytes",
                                    sizeof(CallControls));
    ::testing::Test::RecordProperty("fixture_bytes", fixture_bytes);
  }
  void artifact(const char *suffix) const {
    const auto *directory = std::getenv("MARC_LARGE_STREAM_ARTIFACT_DIR");
    if (!directory)
      return;
    const auto path = std::filesystem::path(directory) /
                      (std::string(r.name) + suffix + ".marc");
    ASSERT_FALSE(std::filesystem::exists(path));
    std::ofstream file(path, std::ios::binary);
    ASSERT_TRUE(file.good());
    file.write(reinterpret_cast<const char *>(wire.data()),
               static_cast<std::streamsize>(written));
    ASSERT_TRUE(file.good());
  }
};
void success(Recipe r, std::size_t tail) {
  for (int mode = 0; mode < 3; ++mode) {
    Controller c(r, tail);
    const auto cap = mode == 0   ? c.wire_size() + 64
                     : mode == 1 ? (r.f == (1u << 20) ? 1u : 17u)
                                 : 257u;
    c.prepare(cap, false);
    ASSERT_FALSE(::testing::Test::HasFatalFailure());
    c.encode(mode);
    ASSERT_FALSE(::testing::Test::HasFatalFailure());
    if (mode == 0)
      c.artifact(tail ? "-short" : "-exact");
    c.decode(false, 0, mode != 0);
    ASSERT_FALSE(::testing::Test::HasFatalFailure());
    if (mode == 0)
      c.report();
  }
}
TEST(PositionDistance8mLargeStream, OneMiBExactTwoFrames) {
  success(recipe("repeat1m", large_frame_vectors::repeat1m), 0);
}
TEST(PositionDistance8mLargeStream, FourMiBExactTwoFrames) {
  success(recipe("repeat4m", large_frame_vectors::repeat4m), 0);
}
TEST(PositionDistance8mLargeStream, EightMiBExactTwoFrames) {
  success(recipe("repeat8m", large_frame_vectors::repeat8m), 0);
}
TEST(PositionDistance8mLargeStream, OneMiBFinalShortFrame) {
  success(recipe("repeat1m", large_frame_vectors::repeat1m), 32);
}
TEST(PositionDistance8mLargeStream, FourMiBFinalShortFrame) {
  success(recipe("repeat4m", large_frame_vectors::repeat4m), 32);
}
TEST(PositionDistance8mLargeStream, EightMiBFinalShortFrame) {
  success(recipe("repeat8m", large_frame_vectors::repeat8m), 32);
}
TEST(PositionDistance8mLargeStream, FailedSecondEightMiBFrameNeverPublishes) {
  for (int mode : {0, 2}) {
    Controller c(recipe("repeat8m", large_frame_vectors::repeat8m), 0, true);
    c.prepare(mode ? 17 : c.wire_size() + 64, true);
    ASSERT_FALSE(::testing::Test::HasFatalFailure());
    c.encode(mode, true);
    ASSERT_FALSE(::testing::Test::HasFatalFailure());
    c.decode(true, c.r.f, 1);
    ASSERT_FALSE(::testing::Test::HasFatalFailure());
    if (!mode) {
      c.artifact("-failed-second");
      c.report();
    }
  }
}
TEST(PositionDistance8mLargeStream,
     MalformedSecondLargeFramePreservesFirstRaw) {
  for (bool truncation : {false, true}) {
    Controller c(recipe("repeat8m", large_frame_vectors::repeat8m));
    c.prepare(c.wire_size() + 64, false);
    ASSERT_FALSE(::testing::Test::HasFatalFailure());
    c.encode(0);
    ASSERT_FALSE(::testing::Test::HasFatalFailure());
    if (truncation)
      --c.written;
    else
      put(c.wire, 112 + c.r.frame.size() + 8, std::uint64_t{9});
    c.decode(true, c.r.f, 1);
    ASSERT_FALSE(::testing::Test::HasFatalFailure());
    if (!truncation)
      c.report();
  }
}
TEST(PositionDistance8mLargeStream,
     NumericLargeAdmissionOneBelowWithoutAllocation) {
  Controller c(recipe("repeat8m", large_frame_vectors::repeat8m), 32);
  const auto external = sum({c.raw_size(), c.wire_size(), c.wire_size() + 64,
                             260, sizeof(Controller), sizeof(CallControls),
                             sizeof(EncodeBuffers), fixture_bytes});
  auto caps = c.numeric(257);
  auto q = query_lzss_position_distance_8m_stream_encode_workspace(
      c.limits, caps, external);
  ASSERT_EQ(q.error, C::none);
  ASSERT_LE(q.aggregate_bytes, policy);
  c.limits.max_internal_buffered_bytes = q.aggregate_bytes;
  EXPECT_EQ(query_lzss_position_distance_8m_stream_encode_workspace(
                c.limits, caps, external)
                .error,
            C::none);
  --c.limits.max_internal_buffered_bytes;
  EXPECT_EQ(query_lzss_position_distance_8m_stream_encode_workspace(
                c.limits, caps, external)
                .error,
            C::limit_exceeded);
  EXPECT_TRUE(c.input.empty() && c.expected.empty() && c.wire.empty() &&
              c.step.empty());
  RecordProperty("prospective_peak", q.aggregate_bytes);
  RecordProperty("controller_bytes", sizeof(Controller));
  RecordProperty("call_controls_bytes", sizeof(CallControls));
}
} // namespace
