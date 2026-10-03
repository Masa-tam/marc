#include "frame/lzss_position_distance_8m_stream_decoder.hpp"
#include "frame/lzss_position_distance_8m_stream_encoder.hpp"
#include "lzss_position_distance_8m_stream_encode_vectors.hpp"
#include <algorithm>
#include <gtest/gtest.h>
#include <limits>
#include <random>
#include <vector>
namespace {
using namespace marc;
using namespace frame::internal;
using Encoder = LzssPositionDistance8mStreamEncoder;
using Caps = LzssPositionDistance8mStreamEncodeCapacities;
using Token = dictionary::internal::LzssTypedToken;
using Op = context::internal::ModeledOperation;
using S = core::StreamStatus;
using C = core::ErrorCode;
constexpr auto end = core::flag_value(core::ProcessFlags::end_input);
constexpr auto flush = core::flag_value(core::ProcessFlags::flush);
constexpr auto guard = std::byte{0xa5};
template <class A> auto bytes(const A &a) {
  std::vector<std::byte> b;
  for (auto x : a)
    b.push_back(static_cast<std::byte>(x));
  return b;
}
void guards(std::span<const std::byte> b) {
  EXPECT_TRUE(std::ranges::all_of(b, [](auto x) { return x == guard; }));
}
struct Fixture {
  core::DecoderLimits limits{};
  TypedContextStreamHeader stream{};
  std::vector<std::byte> raw, publication, frame, payload;
  std::vector<Token> tokens, ts;
  std::vector<Op> ops, os;
  std::vector<std::uint32_t> index;
  explicit Fixture(std::size_t f = 8, std::size_t original = 16)
      : raw(f, guard), publication(18 * f + 85, guard),
        frame(publication.size(), guard), payload(18 * f + 5, guard), tokens(f),
        ts(f), ops(2 * f), os(2 * f), index(65536 + f) {
    limits.max_block_size = 65536;
    stream.frame_size = static_cast<std::uint32_t>(f);
    stream.original_size = original;
    stream.dictionary = {8388608, 3, 258, 0};
    stream.range_model_total = 32768;
    stream.context_count = 47;
    stream.dictionary_variant = 11;
    stream.context_algorithm = 1;
    stream.context_variant = 12;
  }
  auto workspace() {
    return LzssPositionDistance8mFrameEncodeWorkspace{
        tokens, ts, index, ops, os, frame, payload};
  }
  auto capacities(std::size_t in = 0, std::size_t out = 0) const {
    return Caps{raw.size(),
                publication.size(),
                tokens.size(),
                ts.size(),
                index.size(),
                ops.size(),
                os.size(),
                frame.size(),
                payload.size(),
                in,
                out};
  }
  auto query(std::size_t in = 0, std::size_t out = 0,
             std::size_t extra = 0) const {
    return query_lzss_position_distance_8m_stream_encode_workspace(
        limits, capacities(in, out), extra);
  }
  auto make(std::size_t extra = 0) {
    return Encoder(stream, limits, raw, publication, workspace(), extra);
  }
};
struct Outcome {
  std::vector<std::byte> wire;
  core::ProcessResult last{};
  std::size_t consumed{}, calls{};
};
Outcome drive(Encoder &e, std::span<const std::byte> raw,
              const std::vector<std::size_t> &chunks, std::size_t capacity,
              bool final = true, std::uint32_t flags = 0) {
  Outcome o;
  auto call = [&](std::span<const std::byte> input, std::uint32_t f) {
    std::vector<std::byte> output(capacity + 3, guard);
    o.last = e.process(input, std::span(output).first(capacity), f);
    EXPECT_LE(o.last.input_consumed, input.size());
    EXPECT_LE(o.last.output_produced, capacity);
    EXPECT_TRUE(o.last.status != S::progress || o.last.input_consumed ||
                o.last.output_produced);
    guards(std::span(output).subspan(o.last.output_produced));
    o.wire.insert(o.wire.end(), output.begin(),
                  output.begin() + o.last.output_produced);
    o.consumed += o.last.input_consumed;
    EXPECT_LT(++o.calls, 100000u);
  };
  for (auto n : chunks) {
    const auto stop = o.consumed + n;
    EXPECT_LE(stop, raw.size());
    do {
      call(raw.subspan(o.consumed, stop - o.consumed),
           flags | (final && stop == raw.size() ? end : 0));
      if (o.last.status == S::error || o.last.status == S::end_of_stream)
        return o;
      if (!o.last.input_consumed && !o.last.output_produced &&
          o.last.status != S::need_output)
        break;
    } while (o.consumed < stop || o.last.status == S::need_output);
  }
  EXPECT_EQ(o.consumed, raw.size());
  while (o.last.status != S::error && o.last.status != S::end_of_stream &&
         o.calls < 100000)
    call({}, end | flags);
  return o;
}
void sticky(Encoder &e, const core::ProcessResult &before) {
  std::array<std::byte, 3> out{guard, guard, guard};
  auto r = e.process(out, out, 0xffffffffu);
  EXPECT_EQ(r.status, before.status);
  EXPECT_EQ(r.input_consumed, 0u);
  EXPECT_EQ(r.output_produced, 0u);
  EXPECT_EQ(r.error.code, before.error.code);
  EXPECT_EQ(r.error.byte_position, before.error.byte_position);
  guards(out);
}
void consume(std::span<const std::byte> wire,
             std::span<const std::byte> expected, bool malformed = false) {
  Fixture f;
  std::vector<std::byte> serial(256, guard), raw(8, guard), scratch(8, guard);
  std::vector<Token> tokens(8), ts(8);
  LzssPositionDistance8mStreamDecoder d(f.limits, serial, tokens, ts, raw,
                                        scratch);
  std::vector<std::byte> decoded;
  std::size_t offset = 0, steps = 0;
  core::ProcessResult r;
  do {
    std::array<std::byte, 5> output{guard, guard, guard, guard, guard};
    auto input =
        wire.subspan(offset, std::min<std::size_t>(7, wire.size() - offset));
    r = d.process(input, std::span(output).first(2),
                  offset + input.size() == wire.size() ? end : 0);
    ASSERT_LE(r.input_consumed, input.size());
    ASSERT_LE(r.output_produced, 2u);
    guards(std::span(output).subspan(r.output_produced));
    offset += r.input_consumed;
    decoded.insert(decoded.end(), output.begin(),
                   output.begin() + r.output_produced);
    ASSERT_TRUE(r.status != S::progress || r.input_consumed ||
                r.output_produced);
    ASSERT_LT(++steps, 10000u);
  } while (r.status != S::error && r.status != S::end_of_stream);
  EXPECT_EQ(r.status, malformed ? S::error : S::end_of_stream);
  EXPECT_EQ(decoded, std::vector<std::byte>(expected.begin(), expected.end()));
}
TEST(PositionDistance8mStreamEncoder, IndependentEverySmallSplit) {
  for (const auto &v : tests::stream_encode_vectors::vectors)
    if (!v.bad) {
      auto raw = bytes(v.raw), wire = bytes(v.wire);
      SCOPED_TRACE(v.name);
      for (std::size_t split = 0; split <= raw.size(); ++split)
        for (auto cap : {1u, 17u, 200u})
          for (bool final : {false, true}) {
            Fixture f(v.frame_size, raw.size());
            auto e = f.make();
            auto o = drive(e, raw, {split, raw.size() - split}, cap, final);
            EXPECT_EQ(o.last.status, S::end_of_stream);
            EXPECT_EQ(o.wire, wire);
            sticky(e, o.last);
          }
    }
}
TEST(PositionDistance8mStreamEncoder, OneByteAndFlushNeutral) {
  for (const auto &v : tests::stream_encode_vectors::vectors)
    if (!v.bad) {
      auto raw = bytes(v.raw);
      Fixture f(v.frame_size, raw.size());
      auto e = f.make();
      auto o = drive(e, raw, std::vector<std::size_t>(raw.size(), 1), 1, true,
                     flush);
      EXPECT_EQ(o.last.status, S::end_of_stream);
      EXPECT_EQ(o.wire, bytes(v.wire));
    }
}
TEST(PositionDistance8mStreamEncoder, IndependentConsumerDifferential) {
  for (const auto &v : tests::stream_encode_vectors::vectors) {
    auto raw = bytes(v.raw), wire = bytes(v.wire);
    if (!v.bad) {
      Fixture f(v.frame_size, raw.size());
      auto e = f.make();
      auto o = drive(e, raw, {raw.size()}, 3);
      ASSERT_EQ(o.wire, wire);
    }
    consume(wire, raw, v.bad);
  }
}
TEST(PositionDistance8mStreamEncoder, RandomChunkingAndConsumer) {
  std::mt19937 rng(1433);
  for (std::size_t n : {1u, 7u, 8u, 9u, 16u, 17u, 63u, 258u, 513u}) {
    std::vector<std::byte> raw(n);
    for (auto &b : raw)
      b = static_cast<std::byte>(rng() % 7);
    Fixture full(8, n);
    auto a = full.make();
    auto baseline = drive(a, raw, {n}, 2048);
    ASSERT_EQ(baseline.last.status, S::end_of_stream);
    consume(baseline.wire, raw);
    for (int run = 0; run < 8; ++run) {
      std::vector<std::size_t> chunks;
      std::size_t left = n;
      while (left) {
        auto size = std::min<std::size_t>(left, 1 + rng() % 11);
        chunks.push_back(size);
        left -= size;
      }
      Fixture f(8, n);
      auto e = f.make();
      auto o = drive(e, raw, chunks, 1 + rng() % 31, run % 2, flush);
      EXPECT_EQ(o.last.status, S::end_of_stream);
      EXPECT_EQ(o.wire, baseline.wire);
    }
  }
}
TEST(PositionDistance8mStreamEncoder, ZeroCapacityPendingHeader) {
  Fixture f(1, 1);
  auto e = f.make();
  std::array<std::byte, 1> input{std::byte{65}};
  auto r = e.process(input, {}, end);
  EXPECT_EQ(r.status, S::need_output);
  EXPECT_EQ(r.input_consumed, 0u);
  EXPECT_EQ(r.output_produced, 0u);
  auto o = drive(e, input, {1}, 1);
  EXPECT_EQ(o.last.status, S::end_of_stream);
  EXPECT_EQ(o.wire, bytes(tests::stream_encode_vectors::repeat_f1_n1_wire));
}
TEST(PositionDistance8mStreamEncoder, PrivateFramePendingZeroCapacity) {
  Fixture f(1, 1);
  auto e = f.make();
  std::array<std::byte, 112> header{};
  auto r = e.process({}, header, 0);
  ASSERT_EQ(r.output_produced, 112u);
  ASSERT_EQ(r.status, S::progress);
  std::array<std::byte, 1> input{std::byte{65}};
  r = e.process(input, {}, end);
  EXPECT_EQ(r.input_consumed, 1u);
  EXPECT_EQ(r.output_produced, 0u);
  EXPECT_EQ(r.status, S::need_output);
  auto saved = f.publication;
  r = e.process({}, {}, end);
  EXPECT_EQ(r.status, S::need_output);
  EXPECT_EQ(f.publication, saved);
  auto o = drive(e, {}, {}, 1);
  EXPECT_EQ(o.last.status, S::end_of_stream);
  auto wire = bytes(tests::stream_encode_vectors::repeat_f1_n1_wire);
  EXPECT_EQ(o.wire, std::vector<std::byte>(wire.begin() + 112, wire.end()));
}
TEST(PositionDistance8mStreamEncoder, ExactOriginalAwaitsEmptyEndConfirmation) {
  Fixture f(1, 1);
  auto e = f.make();
  std::array<std::byte, 1> in{std::byte{65}};
  std::array<std::byte, 300> out{};
  auto r = e.process(in, out, 0);
  EXPECT_EQ(r.status, S::progress);
  EXPECT_EQ(r.input_consumed, 1u);
  EXPECT_EQ(r.output_produced, 198u);
  r = e.process({}, {}, 0);
  EXPECT_EQ(r.status, S::need_input);
  r = e.process({}, {}, end);
  EXPECT_EQ(r.status, S::end_of_stream);
  sticky(e, r);
}
TEST(PositionDistance8mStreamEncoder, EmptyAllBorrowedCapacitiesZero) {
  Fixture f(1, 0);
  f.raw.clear();
  f.publication.clear();
  f.tokens.clear();
  f.ts.clear();
  f.index.clear();
  f.ops.clear();
  f.os.clear();
  f.frame.clear();
  f.payload.clear();
  auto e = f.make();
  auto o = drive(e, {}, {}, 1);
  EXPECT_EQ(o.last.status, S::end_of_stream);
  EXPECT_EQ(o.wire, bytes(tests::stream_encode_vectors::repeat_f1_n0_wire));
}
TEST(PositionDistance8mStreamEncoder, PrematureEndNeverInventsShortFrame) {
  Fixture f(8, 16);
  auto e = f.make();
  std::vector<std::byte> input(3, std::byte{65});
  auto o = drive(e, input, {3}, 300);
  EXPECT_EQ(o.last.status, S::error);
  EXPECT_EQ(o.last.error.code, C::malformed_stream);
  EXPECT_EQ(o.consumed, 3u);
  EXPECT_EQ(o.wire.size(), 112u);
  guards(f.publication);
  sticky(e, o.last);
}
TEST(PositionDistance8mStreamEncoder, PrematureEndAfterValidFrame) {
  Fixture f(8, 16);
  auto e = f.make();
  std::vector<std::byte> input(11, std::byte{65});
  auto o = drive(e, input, {11}, 300);
  EXPECT_EQ(o.last.error.code, C::malformed_stream);
  EXPECT_EQ(o.consumed, 11u);
  auto expected = bytes(tests::stream_encode_vectors::repeat_f8_n16_wire);
  const auto first = 112 + 80 + std::to_integer<unsigned>(expected[112 + 32]);
  EXPECT_EQ(o.wire,
            std::vector<std::byte>(expected.begin(), expected.begin() + first));
  sticky(e, o.last);
}
TEST(PositionDistance8mStreamEncoder,
     ExtraInputUnconsumedAfterDeclaredOriginal) {
  Fixture f(1, 1);
  auto e = f.make();
  std::array<std::byte, 2> input{std::byte{65}, std::byte{66}};
  auto o = drive(e, input, {2}, 300);
  EXPECT_EQ(o.last.error.code, C::malformed_stream);
  EXPECT_EQ(o.consumed, 1u);
  EXPECT_EQ(o.wire, bytes(tests::stream_encode_vectors::repeat_f1_n1_wire));
  sticky(e, o.last);
}
TEST(PositionDistance8mStreamEncoder,
     InputAfterLatchedEndRejectedBeforePendingDrain) {
  Fixture f(1, 1);
  auto e = f.make();
  std::array<std::byte, 112> header{};
  static_cast<void>(e.process({}, header, 0));
  std::array<std::byte, 1> input{std::byte{65}};
  auto r = e.process(input, {}, end);
  ASSERT_EQ(r.status, S::need_output);
  std::array<std::byte, 300> out;
  out.fill(guard);
  r = e.process(input, out, end);
  EXPECT_EQ(r.error.code, C::malformed_stream);
  EXPECT_EQ(r.input_consumed, 0u);
  EXPECT_EQ(r.output_produced, 0u);
  guards(out);
  sticky(e, r);
}
void failed_second(bool operations) {
  Fixture f(8, 16);
  if (operations) {
    f.ops.resize(6);
    f.os.resize(6);
  } else {
    f.tokens.resize(2);
    f.ts.resize(2);
  }
  auto e = f.make();
  std::vector<std::byte> input(16, std::byte{65});
  for (std::size_t i = 8; i < 16; ++i)
    input[i] = static_cast<std::byte>(i);
  auto o = drive(e, input, {16}, 2048);
  EXPECT_EQ(o.last.status, S::error);
  EXPECT_EQ(o.last.error.code, C::limit_exceeded);
  EXPECT_EQ(o.consumed, 16u);
  EXPECT_EQ(o.last.error.byte_position, 8u);
  auto wire = bytes(tests::stream_encode_vectors::repeat_f8_n16_wire);
  const auto first = 112 + 80 + std::to_integer<unsigned>(wire[112 + 32]);
  EXPECT_EQ(o.wire, std::vector<std::byte>(wire.begin(), wire.begin() + first));
  sticky(e, o.last);
}
TEST(PositionDistance8mStreamEncoder,
     FailedSecondTokenCapacityPublishesOnlyFirst) {
  failed_second(false);
}
TEST(PositionDistance8mStreamEncoder,
     FailedSecondOperationCapacityPublishesOnlyFirst) {
  failed_second(true);
}
TEST(PositionDistance8mStreamEncoder, FrameStorageFailurePublishesHeaderOnly) {
  for (int stage = 0; stage < 3; ++stage) {
    Fixture f(8, 8);
    if (stage == 0)
      f.publication.resize(80);
    if (stage == 1)
      f.frame.resize(80);
    if (stage == 2)
      f.payload.resize(1);
    auto e = f.make();
    std::vector<std::byte> input(8, std::byte{65});
    auto o = drive(e, input, {8}, 300);
    EXPECT_EQ(o.last.error.code, C::limit_exceeded);
    EXPECT_EQ(o.wire.size(), 112u);
    guards(f.publication);
    sticky(e, o.last);
  }
}
TEST(PositionDistance8mStreamEncoder,
     LaterFrameStorageFailureLeavesSlotUnchanged) {
  const auto first_payload =
      tests::stream_encode_vectors::repeat_f8_n16_wire[112 + 32];
  for (int which = 0; which < 3; ++which) {
    Fixture f(8, 16);
    if (which == 0)
      f.publication.resize(80 + first_payload);
    if (which == 1)
      f.frame.resize(80 + first_payload);
    if (which == 2)
      f.payload.resize(first_payload);
    auto e = f.make();
    std::vector<std::byte> first(8, std::byte{65});
    std::array<std::byte, 512> output;
    output.fill(guard);
    auto r = e.process(first, output, 0);
    ASSERT_EQ(r.status, S::progress);
    ASSERT_EQ(r.input_consumed, 8u);
    ASSERT_EQ(r.output_produced, 112 + 80 + first_payload);
    guards(std::span(output).subspan(r.output_produced));
    auto before = f.publication;
    std::array<std::byte, 8> second{};
    for (std::size_t i = 0; i < 8; ++i)
      second[i] = static_cast<std::byte>(i);
    output.fill(guard);
    r = e.process(second, output, end);
    EXPECT_EQ(r.error.code, C::limit_exceeded);
    EXPECT_EQ(r.error.byte_position, 8u);
    EXPECT_EQ(r.input_consumed, 8u);
    EXPECT_EQ(r.output_produced, 0u);
    guards(output);
    EXPECT_EQ(f.publication, before);
    sticky(e, r);
  }
}
TEST(PositionDistance8mStreamEncoder,
     CallBudgetFailureInterruptsValidatedDrain) {
  Fixture f(1, 1);
  const auto q = f.query(0, 112);
  f.limits.max_internal_buffered_bytes = q.aggregate_bytes;
  auto e = f.make();
  std::array<std::byte, 112> header{};
  auto r = e.process({}, header, 0);
  ASSERT_EQ(r.output_produced, 112u);
  std::array<std::byte, 1> in{std::byte{65}}, part{};
  r = e.process(in, part, end);
  ASSERT_EQ(r.status, S::need_output);
  ASSERT_EQ(r.input_consumed, 1u);
  ASSERT_EQ(r.output_produced, 1u);
  auto slot = f.publication;
  std::array<std::byte, 113> output;
  output.fill(guard);
  r = e.process({}, output, end);
  EXPECT_EQ(r.error.code, C::limit_exceeded);
  EXPECT_EQ(r.input_consumed, 0u);
  EXPECT_EQ(r.output_produced, 0u);
  guards(output);
  EXPECT_EQ(f.publication, slot);
  sticky(e, r);
}
TEST(PositionDistance8mStreamEncoder, UnsupportedFlagsBeforeWrites) {
  for (auto flags :
       {core::flag_value(core::ProcessFlags::reset_block), 8u, 0xffffffffu}) {
    Fixture f;
    auto e = f.make();
    std::array<std::byte, 1> in{};
    std::array<std::byte, 300> out;
    out.fill(guard);
    auto r = e.process(in, out, flags);
    EXPECT_EQ(r.error.code, C::unsupported);
    EXPECT_EQ(r.input_consumed, 0u);
    EXPECT_EQ(r.output_produced, 0u);
    guards(out);
    sticky(e, r);
  }
}
TEST(PositionDistance8mStreamEncoder, ConstructorInvalidConfigurationSticky) {
  for (int kind = 0; kind < 5; ++kind) {
    Fixture f;
    if (kind == 0)
      f.stream.dictionary_variant = 10;
    if (kind == 1)
      f.stream.frame_size = 0;
    if (kind == 2)
      f.stream.original_size = std::numeric_limits<std::uint64_t>::max();
    if (kind == 3)
      f.stream.context_count = 46;
    if (kind == 4)
      f.limits.max_internal_buffered_bytes = 0;
    auto e = f.make();
    std::array<std::byte, 300> out;
    out.fill(guard);
    auto r = e.process({}, out, 0);
    EXPECT_EQ(r.status, S::error);
    EXPECT_EQ(r.output_produced, 0u);
    guards(out);
    sticky(e, r);
  }
}
TEST(PositionDistance8mStreamEncoder, RawCapacityExactFinalShort) {
  Fixture f(8, 1);
  f.raw.resize(1);
  auto e = f.make();
  std::array<std::byte, 1> in{std::byte{65}};
  auto o = drive(e, in, {1}, 3);
  EXPECT_EQ(o.last.status, S::end_of_stream);
  EXPECT_EQ(o.wire, bytes(tests::stream_encode_vectors::repeat_f8_n1_wire));
  Fixture bad(8, 9);
  bad.raw.resize(7);
  auto b = bad.make();
  auto r = b.process({}, {}, 0);
  EXPECT_EQ(r.error.code, C::limit_exceeded);
  sticky(b, r);
}
TEST(PositionDistance8mStreamEncoder, FullBorrowedOverlapRejected) {
  Fixture f;
  auto w = f.workspace();
  w.payload_scratch = f.publication;
  Encoder e(f.stream, f.limits, f.raw, f.publication, w);
  auto r = e.process({}, {}, 0);
  EXPECT_EQ(r.error.code, C::invalid_argument);
  guards(f.publication);
  sticky(e, r);
}
TEST(PositionDistance8mStreamEncoder, CallInputOutputOverlapBeforeWrites) {
  Fixture f;
  auto e = f.make();
  std::array<std::byte, 300> b;
  b.fill(guard);
  auto r = e.process(std::span(b).last(1), b, 0);
  EXPECT_EQ(r.error.code, C::invalid_argument);
  guards(b);
  EXPECT_EQ(r.input_consumed, 0u);
  EXPECT_EQ(r.output_produced, 0u);
  sticky(e, r);
}
TEST(PositionDistance8mStreamEncoder, CallAliasesEveryBorrowedStorage) {
  for (int which = 0; which < 9; ++which)
    for (bool output : {false, true}) {
      Fixture f;
      auto e = f.make();
      std::span<std::byte> region;
      switch (which) {
      case 0:
        region = f.raw;
        break;
      case 1:
        region = f.publication;
        break;
      case 2:
        region = std::as_writable_bytes(std::span(f.tokens));
        break;
      case 3:
        region = std::as_writable_bytes(std::span(f.ts));
        break;
      case 4:
        region = std::as_writable_bytes(std::span(f.index));
        break;
      case 5:
        region = std::as_writable_bytes(std::span(f.ops));
        break;
      case 6:
        region = std::as_writable_bytes(std::span(f.os));
        break;
      case 7:
        region = f.frame;
        break;
      case 8:
        region = f.payload;
        break;
      }
      std::vector<std::byte> before(region.begin(), region.end());
      auto r = output ? e.process({}, region, 0) : e.process(region, {}, 0);
      EXPECT_EQ(r.error.code, C::invalid_argument);
      EXPECT_EQ(r.input_consumed, 0u);
      EXPECT_EQ(r.output_produced, 0u);
      EXPECT_TRUE(std::equal(region.begin(), region.end(), before.begin()));
      sticky(e, r);
    }
}
TEST(PositionDistance8mStreamEncoder, ExactNumericBudgetAndPerCallLimit) {
  Fixture f(1, 1);
  auto q = f.query(1, 198, 17);
  ASSERT_EQ(q.error, C::none);
  EXPECT_EQ(q.owner_bytes, sizeof(Encoder));
  EXPECT_EQ(q.aggregate_bytes, q.buffer_bytes + q.owner_bytes +
                                   q.control_bytes + q.helper_bytes + 17 + 199);
  RecordProperty("owner_bytes", q.owner_bytes);
  RecordProperty("control_bytes", q.control_bytes);
  RecordProperty("helper_bytes", q.helper_bytes);
  f.limits.max_internal_buffered_bytes = q.aggregate_bytes;
  auto e = f.make(17);
  std::array<std::byte, 1> in{std::byte{65}};
  std::array<std::byte, 198> out;
  auto r = e.process(in, out, end);
  EXPECT_EQ(r.status, S::end_of_stream);
  EXPECT_EQ(r.output_produced, 198u);
  Fixture bad(1, 1);
  bad.limits.max_internal_buffered_bytes = q.aggregate_bytes - 1;
  auto b = bad.make(17);
  out.fill(guard);
  r = b.process(in, out, end);
  EXPECT_EQ(r.error.code, C::limit_exceeded);
  EXPECT_EQ(r.input_consumed, 0u);
  EXPECT_EQ(r.output_produced, 0u);
  guards(out);
  sticky(b, r);
}
TEST(PositionDistance8mStreamEncoder, UnusedBorrowedAndCallCapacitiesCount) {
  Fixture f(1, 1);
  auto base = f.query();
  f.payload.resize(f.payload.size() + 101);
  auto larger = f.query();
  EXPECT_EQ(larger.aggregate_bytes, base.aggregate_bytes + 101);
  auto q = f.query(100, 300);
  f.limits.max_internal_buffered_bytes = q.aggregate_bytes - 1;
  auto e = f.make();
  std::array<std::byte, 100> in{};
  std::array<std::byte, 300> out;
  out.fill(guard);
  auto r = e.process(in, out, 0);
  EXPECT_EQ(r.error.code, C::limit_exceeded);
  EXPECT_EQ(r.input_consumed, 0u);
  EXPECT_EQ(r.output_produced, 0u);
  guards(out);
}
TEST(PositionDistance8mStreamEncoder, ConstructorBaseBudgetOneBelow) {
  Fixture f;
  auto q = f.query();
  f.limits.max_internal_buffered_bytes = q.aggregate_bytes - 1;
  auto e = f.make();
  auto r = e.process({}, {}, 0);
  EXPECT_EQ(r.error.code, C::limit_exceeded);
  sticky(e, r);
}
TEST(PositionDistance8mStreamEncoder, NumericOverflowWithoutAllocation) {
  Fixture f;
  const auto max = std::numeric_limits<std::size_t>::max();
  for (int which = 0; which < 8; ++which) {
    auto c = f.capacities();
    std::size_t extra = 0;
    switch (which) {
    case 0:
      c.tokens = max;
      break;
    case 1:
      c.operation_scratch = max;
      break;
    case 2:
      c.index_entries = max;
      break;
    case 3:
      c.raw_bytes = max;
      break;
    case 4:
      c.input_bytes = max;
      break;
    case 5:
      c.output_bytes = max;
      break;
    case 6:
      extra = max;
      break;
    case 7:
      c.publication_bytes = max / 2;
      c.frame_bytes = max / 2;
      break;
    }
    EXPECT_EQ(query_lzss_position_distance_8m_stream_encode_workspace(f.limits,
                                                                      c, extra)
                  .error,
              C::limit_exceeded);
  }
}
TEST(PositionDistance8mStreamEncoder, UniversalEightMiBRejectedNumerically) {
  Fixture f;
  constexpr std::size_t n = 8388608;
  Caps c{n,     18 * n + 85, n,          n, 65536 + n, 2 * n,
         2 * n, 18 * n + 85, 18 * n + 5, 0, 0};
  auto q = query_lzss_position_distance_8m_stream_encode_workspace(f.limits, c);
  EXPECT_EQ(q.error, C::limit_exceeded);
  EXPECT_GT(q.aggregate_bytes, 512u << 20);
  RecordProperty("universal_buffer_bytes", q.buffer_bytes);
  RecordProperty("universal_aggregate_bytes", q.aggregate_bytes);
}
} // namespace
