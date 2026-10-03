#include "frame/lzss_position_distance_8m_prepared_stream_encoder.hpp"
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
using Encoder = LzssPositionDistance8mPreparedStreamEncoder;
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
  LzssPositionDistance8mExactStreamAllocator allocator;
  explicit Fixture(std::size_t f = 8, std::size_t original = 16) {
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
  auto make(std::size_t extra = 65536) {
    return Encoder(stream, limits, allocator, extra);
  }
};
struct Outcome {
  std::vector<std::byte> wire;
  core::ProcessResult last{};
  std::size_t consumed{}, calls{};
};
template <class E>
Outcome drive(E &e, std::span<const std::byte> raw,
              const std::vector<std::size_t> &chunks, std::size_t capacity,
              bool final = true, std::uint32_t flags = 0) {
  Outcome o;
  o.wire.reserve(raw.size() * 128 + 112);
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
             std::span<const std::byte> expected, bool malformed = false,
             std::size_t retained = 0) {
  Fixture f;
  std::vector<std::byte> serial(256, guard), raw(8, guard), scratch(8, guard);
  std::vector<Token> tokens(8), ts(8);
  LzssPositionDistance8mStreamDecoder d(f.limits, serial, tokens, ts, raw,
                                        scratch, retained + 4096);
  std::vector<std::byte> decoded;
  decoded.reserve(expected.size());
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

class FaultAllocator final : public LzssPositionDistance8mStreamAllocator {
public:
  struct Record {
    void *p{};
    std::size_t bytes{};
  };
  std::array<Record, 32> live{};
  std::size_t calls{}, fail_at{}, wrong_at{}, short_at{}, live_bytes{}, peak{},
      deleted{};
  LzssPositionDistance8mExactStreamAllocator exact;
  auto controls() const noexcept
      -> LzssPositionDistance8mAllocatorControls override {
    return {this, sizeof(*this), 256};
  }
  template <class T> auto obtain(std::size_t n) noexcept {
    ++calls;
    if (calls == fail_at)
      return LzssPositionDistance8mOwnedBlock<T>{};
    if (calls == wrong_at)
      ++n;
    if (calls == short_at)
      --n;
    auto p = new (std::nothrow) T[n]{};
    if (!p)
      return LzssPositionDistance8mOwnedBlock<T>{};
    for (auto &r : live)
      if (!r.p) {
        r = {p, n * sizeof(T)};
        live_bytes += r.bytes;
        peak = std::max(peak, live_bytes);
        break;
      }
    return LzssPositionDistance8mOwnedBlock<T>{p, n};
  }
  auto tokens(std::size_t n) noexcept
      -> LzssPositionDistance8mOwnedTokens override {
    return obtain<Token>(n);
  }
  auto bytes(std::size_t n) noexcept
      -> LzssPositionDistance8mOwnedBytes override {
    return obtain<std::byte>(n);
  }
  auto indices(std::size_t n) noexcept
      -> LzssPositionDistance8mOwnedIndex override {
    return obtain<std::uint32_t>(n);
  }
  template <class T>
  void destroy(LzssPositionDistance8mOwnedBlock<T> &b) noexcept {
    auto p = b.data;
    delete[] p;
    b = {};
    if (p)
      for (auto &r : live)
        if (r.p == p) {
          live_bytes -= r.bytes;
          r = {};
          ++deleted;
          break;
        }
  }
  void release(LzssPositionDistance8mOwnedTokens &b) noexcept override {
    destroy(b);
  }
  void release(LzssPositionDistance8mOwnedBytes &b) noexcept override {
    destroy(b);
  }
  void release(LzssPositionDistance8mOwnedIndex &b) noexcept override {
    destroy(b);
  }
};
std::vector<std::byte> reference(std::span<const std::byte> input,
                                 std::size_t frame) {
  Fixture c(frame, input.size());
  std::vector<std::byte> raw(frame), pub(18 * frame + 85), fb(pub.size()),
      pb(18 * frame + 5);
  std::vector<Token> t(frame), ts(frame);
  std::vector<Op> op(2 * frame), os(2 * frame);
  std::vector<std::uint32_t> index(65536 + frame);
  LzssPositionDistance8mStreamEncoder old(c.stream, c.limits, raw, pub,
                                          {t, ts, index, op, os, fb, pb},
                                          4096 + input.size());
  std::vector<std::byte> wire;
  std::array<std::byte, 113> output{};
  std::size_t used{};
  for (std::size_t i = 0; i < 100000; ++i) {
    auto r = old.process(input.subspan(used), output, end);
    used += r.input_consumed;
    wire.insert(wire.end(), output.begin(), output.begin() + r.output_produced);
    EXPECT_NE(r.status, S::error);
    if (r.status == S::end_of_stream)
      return wire;
  }
  ADD_FAILURE() << "reference did not finish";
  return wire;
}
TEST(PositionDistance8mPreparedStream, IndependentEverySmallSplit) {
  for (const auto &v : tests::stream_encode_vectors::vectors)
    if (!v.bad) {
      auto raw = bytes(v.raw), wire = bytes(v.wire);
      SCOPED_TRACE(v.name);
      for (std::size_t split = 0; split <= raw.size(); ++split)
        for (auto cap : {1u, 17u, 200u})
          for (bool final : {false, true}) {
            Fixture f(v.frame_size, raw.size());
            auto e = f.make();
            auto o =
                drive(e, raw, {split, raw.size() - split}, cap, final, flush);
            EXPECT_EQ(o.last.status, S::end_of_stream);
            EXPECT_EQ(o.wire, wire);
            sticky(e, o.last);
          }
    }
}
TEST(PositionDistance8mPreparedStream, OneByteAndReferenceMultiframe) {
  std::vector<std::byte> raw(97);
  for (std::size_t i = 0; i < raw.size(); ++i)
    raw[i] = std::byte(i * 71);
  auto expected = reference(raw, 8);
  Fixture f(8, raw.size());
  Outcome o;
  {
    auto e =
        f.make(expected.capacity() + raw.capacity() + raw.size() * 128 + 112);
    o = drive(e, raw, std::vector<std::size_t>(raw.size(), 1), 1);
  }
  EXPECT_EQ(o.last.status, S::end_of_stream);
  EXPECT_EQ(o.wire, expected);
  consume(o.wire, raw, false,
          o.wire.capacity() + expected.capacity() + raw.capacity());
}
TEST(PositionDistance8mPreparedStream, EmptyHeaderWithoutAllocation) {
  Fixture f(8, 0);
  FaultAllocator a;
  {
    Encoder e(f.stream, f.limits, a, 4096);
    auto o = drive(e, {}, {}, 1);
    EXPECT_EQ(o.wire.size(), 112u);
    EXPECT_EQ(o.last.status, S::end_of_stream);
    EXPECT_EQ(a.calls, 0u);
    sticky(e, o.last);
  }
  EXPECT_EQ(a.live_bytes, 0u);
}
TEST(PositionDistance8mPreparedStream, InitialAllocationFailures) {
  for (auto stage : {1u, 2u}) {
    Fixture f;
    FaultAllocator a;
    a.fail_at = stage;
    {
      Encoder e(f.stream, f.limits, a, 4096);
      std::array<std::byte, 8> out;
      out.fill(guard);
      auto r = e.process({}, out, end);
      EXPECT_EQ(r.error.code, C::out_of_memory);
      EXPECT_EQ(r.output_produced, 0u);
      guards(out);
      sticky(e, r);
      EXPECT_EQ(a.calls, stage);
    }
    EXPECT_EQ(a.live_bytes, 0u);
  }
}
TEST(PositionDistance8mPreparedStream, InitialInclusiveBudgetBeforeAllocation) {
  Fixture f;
  FaultAllocator a;
  const auto total =
      Encoder::working_bytes() + sizeof(a) + 256 + 8 + 4 * (65536 + 8);
  f.limits.max_internal_buffered_bytes = total - 1;
  {
    Encoder e(f.stream, f.limits, a);
    auto r = e.process({}, {}, 0);
    EXPECT_EQ(r.error.code, C::limit_exceeded);
    EXPECT_EQ(a.calls, 0u);
  }
  f.limits.max_internal_buffered_bytes = total;
  {
    Encoder e(f.stream, f.limits, a);
    auto r = e.process({}, {}, 0);
    EXPECT_EQ(r.status, S::need_output);
    EXPECT_EQ(a.calls, 2u);
  }
  EXPECT_EQ(a.live_bytes, 0u);
}
TEST(PositionDistance8mPreparedStream,
     EveryGenerationAllocationFailureRetainsEarlierOutput) {
  std::array<std::byte, 16> raw{};
  raw.fill(std::byte{65});
  auto expected = reference(raw, 8);
  const auto first = (expected.size() - 112) / 2;
  for (std::size_t stage = 3; stage <= 12; ++stage) {
    Fixture f;
    FaultAllocator a;
    a.fail_at = stage;
    {
      Encoder e(f.stream, f.limits, a, 4096);
      std::array<std::byte, 1024> out;
      out.fill(guard);
      const auto r = e.process(raw, out, end);
      EXPECT_EQ(r.status, S::error);
      EXPECT_EQ(r.error.code, C::out_of_memory);
      EXPECT_EQ(r.input_consumed, stage <= 7 ? 8u : 16u);
      EXPECT_EQ(r.output_produced, stage <= 7 ? 112u : 112 + first);
      EXPECT_EQ(r.error.byte_position, stage <= 7 ? 0u : 8u);
      EXPECT_TRUE(std::equal(out.begin(), out.begin() + r.output_produced,
                             expected.begin()));
      guards(std::span(out).subspan(r.output_produced));
      EXPECT_EQ(a.calls, stage);
      sticky(e, r);
      EXPECT_EQ(std::count_if(a.live.begin(), a.live.end(),
                              [](auto &r) { return r.p != nullptr; }),
                stage <= 7 ? 2 : 7);
    }
    EXPECT_EQ(a.live_bytes, 0u);
  }
}
TEST(PositionDistance8mPreparedStream,
     PendingBlocksNextGenerationUntilFullyDrained) {
  Fixture f;
  FaultAllocator a;
  Encoder e(f.stream, f.limits, a, 4096);
  std::array<std::byte, 16> raw{};
  std::array<std::byte, 112> header{};
  auto r = e.process(raw, header, end);
  ASSERT_EQ(r.input_consumed, 8u);
  ASSERT_EQ(r.status, S::need_output);
  ASSERT_EQ(a.calls, 7u);
  for (unsigned i = 0; i < 10; ++i) {
    std::array<std::byte, 1> out{};
    r = e.process(std::span(raw).subspan(8), out, end);
    EXPECT_EQ(r.input_consumed, 0u);
    EXPECT_EQ(a.calls, 7u);
  }
  auto o = drive(e, std::span(raw).subspan(8), {8}, 1);
  EXPECT_EQ(o.last.status, S::end_of_stream);
  EXPECT_EQ(a.calls, 12u);
}
TEST(PositionDistance8mPreparedStream, ZeroOutputAndFinalSuffix) {
  Fixture f;
  auto e = f.make();
  std::array<std::byte, 16> raw{};
  auto r = e.process(raw, {}, end);
  EXPECT_EQ(r.status, S::need_output);
  EXPECT_EQ(r.input_consumed, 0u);
  auto o = drive(e, raw, {raw.size()}, 17);
  EXPECT_EQ(o.last.status, S::end_of_stream);
  EXPECT_EQ(o.wire, reference(raw, 8));
}
TEST(PositionDistance8mPreparedStream, EarlyFinalInputKeepsValidFrames) {
  std::array<std::byte, 11> raw{};
  Fixture f(8, 16);
  Outcome o;
  {
    auto e = f.make();
    o = drive(e, raw, {raw.size()}, 1024);
    sticky(e, o.last);
  }
  EXPECT_EQ(o.last.error.code, C::malformed_stream);
  EXPECT_EQ(o.consumed, 11u);
  EXPECT_EQ(o.last.error.byte_position, 11u);
  auto expected = reference(std::span(raw).first(8), 8);
  EXPECT_EQ(o.wire.size(), expected.size());
  consume(o.wire, std::span(raw).first(8), true,
          o.wire.capacity() + expected.capacity() + raw.size());
}
TEST(PositionDistance8mPreparedStream, ExcessInputUnconsumedAndWaitingEnd) {
  Fixture f(8, 8);
  auto e = f.make();
  std::array<std::byte, 9> in{};
  std::array<std::byte, 1024> out;
  out.fill(guard);
  auto r = e.process(in, out, end);
  EXPECT_EQ(r.error.code, C::malformed_stream);
  EXPECT_EQ(r.input_consumed, 8u);
  guards(std::span(out).subspan(r.output_produced));
  sticky(e, r);
  Fixture g(8, 8);
  auto d = g.make();
  r = d.process(std::span(in).first(8), out, 0);
  EXPECT_EQ(r.status, S::progress);
  r = d.process({}, out, flush);
  EXPECT_EQ(r.status, S::need_input);
  r = d.process({}, out, end);
  EXPECT_EQ(r.status, S::end_of_stream);
  sticky(d, r);
}
TEST(PositionDistance8mPreparedStream, UnsupportedAndCallAliases) {
  for (auto flags :
       {core::flag_value(core::ProcessFlags::reset_block), 0xffffffffu}) {
    Fixture f;
    auto e = f.make();
    std::array<std::byte, 4> out;
    out.fill(guard);
    auto r = e.process({}, out, flags);
    EXPECT_EQ(r.error.code, C::unsupported);
    guards(out);
    sticky(e, r);
  }
  Fixture f;
  auto e = f.make();
  std::array<std::byte, 4> out;
  out.fill(guard);
  auto r = e.process(out, out, 0);
  EXPECT_EQ(r.error.code, C::invalid_argument);
  guards(out);
  sticky(e, r);
}
TEST(PositionDistance8mPreparedStream, FullGenerationAndOwnerAliasGuards) {
  for (std::size_t slot = 0; slot < 7; ++slot) {
    Fixture f;
    FaultAllocator a;
    Encoder e(f.stream, f.limits, a, 4096);
    std::array<std::byte, 8> raw{};
    std::array<std::byte, 112> out{};
    auto r = e.process(raw, out, 0);
    ASSERT_EQ(r.status, S::need_output);
    auto rec = a.live[slot];
    ASSERT_NE(rec.p, nullptr);
    auto p = static_cast<std::byte *>(rec.p) + rec.bytes - 1;
    const auto before = *p;
    r = e.process({}, std::span(p, 1), 0);
    EXPECT_EQ(r.error.code, C::invalid_argument);
    EXPECT_EQ(*p, before);
    sticky(e, r);
  }
  Fixture f;
  auto e = f.make();
  auto p = reinterpret_cast<std::byte *>(&e);
  auto r = e.process({}, std::span(p, 1), 0);
  EXPECT_EQ(r.error.code, C::invalid_argument);
}
TEST(PositionDistance8mPreparedStream, OversizedReceiptsNeverPublishFailedFrame) {
  std::array<std::byte, 16> data{};
  auto expected = reference(data, 8);
  const auto first = (expected.size() - 112) / 2;
  for (std::size_t stage = 1; stage <= 12; ++stage) {
    Fixture f;
    FaultAllocator a;
    a.wrong_at = stage;
    std::array<std::byte, 16> raw{};
    {
      Encoder e(f.stream, f.limits, a, 4096);
      std::array<std::byte, 1024> out;
      out.fill(guard);
      auto r = e.process(raw, out, end);
      EXPECT_EQ(r.error.code, C::limit_exceeded);
      EXPECT_EQ(r.output_produced, stage <= 2   ? 0u
                                   : stage <= 7 ? 112u
                                                : 112 + first);
      guards(std::span(out).subspan(r.output_produced));
      sticky(e, r);
    }
    EXPECT_EQ(a.live_bytes, 0u);
  }
}
TEST(PositionDistance8mPreparedStream,
     PayloadPolicyFailureAndInvalidConfiguration) {
  Fixture f;
  f.limits.max_compressed_payload_size = 1;
  FaultAllocator a;
  {
    Encoder e(f.stream, f.limits, a, 4096);
    std::array<std::byte, 8> raw{};
    std::array<std::byte, 512> out;
    out.fill(guard);
    auto r = e.process(raw, out, end);
    EXPECT_EQ(r.error.code, C::limit_exceeded);
    EXPECT_EQ(r.output_produced, 112u);
    guards(std::span(out).subspan(r.output_produced));
    EXPECT_EQ(a.calls, 4u);
  }
  EXPECT_EQ(a.live_bytes, 0u);
  Fixture g;
  g.stream.dictionary_variant = 0;
  FaultAllocator b;
  Encoder e(g.stream, g.limits, b);
  auto r = e.process({}, {}, end);
  EXPECT_EQ(r.error.code, C::invalid_argument);
  EXPECT_EQ(b.calls, 0u);
}
TEST(PositionDistance8mPreparedStream, PendingCallCapacityAdmission) {
  Fixture f;
  FaultAllocator a;
  std::array<std::byte, 8> raw{};
  std::array<std::byte, 112> out{};
  f.limits.max_internal_buffered_bytes = 1024 * 1024;
  Encoder e(f.stream, f.limits, a);
  auto r = e.process(raw, out, 0);
  ASSERT_EQ(r.status, S::need_output);
  std::vector<std::byte> huge(f.limits.max_internal_buffered_bytes, guard);
  r = e.process({}, huge, 0);
  EXPECT_EQ(r.error.code, C::limit_exceeded);
  EXPECT_EQ(r.output_produced, 0u);
  guards(huge);
  EXPECT_EQ(a.calls, 7u);
  sticky(e, r);
}
TEST(PositionDistance8mPreparedStream, RandomizedChunkingReferenceEquality) {
  std::mt19937 rng(1443);
  for (unsigned trial = 0; trial < 40; ++trial) {
    std::vector<std::byte> raw(rng() % 140);
    for (auto &x : raw)
      x = std::byte(rng() % ((trial % 2) ? 256 : 3));
    const auto frame = 1 + rng() % 31;
    auto expected = reference(raw, frame);
    Fixture f(frame, raw.size());
    auto e =
        f.make(expected.capacity() + raw.capacity() + raw.size() * 128 + 112);
    std::vector<std::size_t> chunks;
    std::size_t rest = raw.size();
    while (rest) {
      auto n = std::min<std::size_t>(rest, 1 + rng() % 13);
      chunks.push_back(n);
      rest -= n;
    }
    auto o =
        drive(e, raw, chunks, 1 + rng() % 43, trial % 2, trial % 2 ? flush : 0);
    EXPECT_EQ(o.last.status, S::end_of_stream);
    EXPECT_EQ(o.wire, expected);
  }
  std::cout << "prepared_stream working=" << Encoder::working_bytes()
            << " object=" << sizeof(Encoder) << "\n";
}
TEST(PositionDistance8mPreparedStream, ReplacementInclusivePeakBudget) {
  Fixture f;
  std::array<std::byte, 16> raw{};
  std::array<std::byte, 1024> out{};
  std::size_t peak{};
  {
    FaultAllocator a;
    {
      Encoder e(f.stream, f.limits, a, 4096);
      auto r = e.process(raw, out, end);
      ASSERT_EQ(r.status, S::end_of_stream);
      peak = a.peak;
    }
    EXPECT_EQ(a.live_bytes, 0u);
  }
  const auto exact = peak + Encoder::working_bytes() + sizeof(FaultAllocator) +
                     256 + 4096 + raw.size() + out.size();
  for (bool fits : {false, true}) {
    FaultAllocator a;
    f.limits.max_internal_buffered_bytes = exact - (fits ? 0 : 1);
    out.fill(guard);
    {
      Encoder e(f.stream, f.limits, a, 4096);
      auto r = e.process(raw, out, end);
      EXPECT_EQ(r.status, fits ? S::end_of_stream : S::error);
      if (!fits) {
        EXPECT_EQ(r.error.code, C::limit_exceeded);
        EXPECT_EQ(r.input_consumed, 16u);
        EXPECT_GT(r.output_produced, 112u);
        EXPECT_LT(r.output_produced, 300u);
        sticky(e, r);
      }
      guards(std::span(out).subspan(r.output_produced));
    }
    EXPECT_EQ(a.live_bytes, 0u);
  }
  std::cout << "prepared_stream replacement_peak=" << peak << " admitted=" << exact
            << "\n";
}
TEST(PositionDistance8mPreparedStream, UndersizedReceiptsAndExternalOverflow) {
  for (std::size_t stage = 1; stage <= 12; ++stage) {
    Fixture f;
    FaultAllocator a;
    a.short_at = stage;
    {
      Encoder e(f.stream, f.limits, a, 4096);
      std::array<std::byte, 16> raw{};
      std::array<std::byte, 1024> out;
      out.fill(guard);
      auto r = e.process(raw, out, end);
      EXPECT_EQ(r.error.code, C::limit_exceeded);
      guards(std::span(out).subspan(r.output_produced));
      sticky(e, r);
    }
    EXPECT_EQ(a.live_bytes, 0u);
  }
  Fixture f;
  FaultAllocator a;
  Encoder e(f.stream, f.limits, a, std::numeric_limits<std::size_t>::max());
  auto r = e.process({}, {}, 0);
  EXPECT_EQ(r.error.code, C::limit_exceeded);
  EXPECT_EQ(a.calls, 0u);
}
TEST(PositionDistance8mPreparedStream, PrefixPolicyAndInputAfterLatchedEnd) {
  Fixture f(267, 267);
  f.limits.max_expansion_ratio = 1;
  f.limits.expansion_slack = 0;
  FaultAllocator a;
  {
    Encoder e(f.stream, f.limits, a, 4096);
    std::array<std::byte, 267> raw{};
    std::array<std::byte, 512> out;
    out.fill(guard);
    auto r = e.process(raw, out, end);
    EXPECT_EQ(r.error.code, C::limit_exceeded);
    EXPECT_EQ(r.output_produced, 112u);
    EXPECT_EQ(a.calls, 4u);
    guards(std::span(out).subspan(r.output_produced));
  }
  EXPECT_EQ(a.live_bytes, 0u);
  Fixture g(8, 8);
  auto e = g.make();
  std::array<std::byte, 8> raw{};
  std::array<std::byte, 112> out{};
  auto r = e.process(raw, out, end);
  ASSERT_EQ(r.status, S::need_output);
  ASSERT_EQ(r.input_consumed, 8u);
  out.fill(guard);
  r = e.process(raw, out, 0);
  EXPECT_EQ(r.error.code, C::malformed_stream);
  EXPECT_EQ(r.input_consumed, 0u);
  EXPECT_EQ(r.output_produced, 0u);
  guards(out);
  sticky(e, r);
}
TEST(PositionDistance8mPreparedStream, SafeOwnedDifferentialSeparateLifetimes) {
  for (std::size_t n : {0u, 1u, 7u, 8u, 9u, 31u, 97u, 139u}) {
    std::vector<std::byte> raw(n);
    for (std::size_t i = 0; i < n; ++i)
      raw[i] = static_cast<std::byte>((i * 71) % 5);
    std::vector<std::size_t> chunks(n, 1);
    std::vector<std::byte> safe_wire;
    {
      Fixture f(8, n);
      LzssPositionDistance8mOwnedStreamEncoder safe(f.stream, f.limits,
                                                   f.allocator, 65536);
      auto o = drive(safe, raw, chunks, 1, false, flush);
      ASSERT_EQ(o.last.status, S::end_of_stream);
      safe_wire = std::move(o.wire);
    }
    std::vector<std::byte> prepared_wire;
    {
      Fixture f(8, n);
      auto e = f.make();
      auto o = drive(e, raw, chunks, 17, true);
      ASSERT_EQ(o.last.status, S::end_of_stream);
      prepared_wire = std::move(o.wire);
    }
    EXPECT_EQ(prepared_wire, safe_wire);
    consume(prepared_wire, raw, false,
            safe_wire.capacity() + prepared_wire.capacity() + raw.capacity() +
                chunks.capacity() * sizeof(std::size_t));
  }
}
TEST(PositionDistance8mPreparedStream, MidDrainErrorsPublishOnlyCompleteRawPrefix) {
  // The codec charges each full call view. The separate 64 KiB retained
  // reservation covers this small harness and its saved streams; the large
  // failing output view is charged by process before any additional drain.
  for (unsigned failure = 0; failure < 3; ++failure) {
    std::array<std::byte, 24> raw{};
    auto expected = reference(raw, 8);
    const auto first_size = reference(std::span(raw).first(8), 8).size() - 112;
    std::vector<std::byte> wire;
    {
      Fixture f(8, raw.size());
      f.limits.max_internal_buffered_bytes = 1024 * 1024;
      FaultAllocator a;
      {
        Encoder e(f.stream, f.limits, a, 65536);
        std::vector<std::byte> output(112 + first_size + 7, guard);
        auto r = e.process(raw, output, end);
        ASSERT_EQ(r.status, S::need_output);
        ASSERT_EQ(r.input_consumed, 16u);
        ASSERT_EQ(r.output_produced, output.size());
        wire.assign(output.begin(), output.end());
        ASSERT_TRUE(std::equal(wire.begin(), wire.end(), expected.begin()));
        const auto calls = a.calls;
        std::vector<std::byte> rejected(failure == 2 ? 1024 * 1024 : 32, guard);
        auto input = failure == 1 ? std::span<const std::byte>(rejected).first(1)
                                  : std::span<const std::byte>{};
        r = e.process(input, rejected,
                      failure == 0 ? core::flag_value(core::ProcessFlags::reset_block)
                                   : end);
        ASSERT_EQ(r.status, S::error);
        EXPECT_EQ(r.error.code, failure == 0 ? C::unsupported
                                  : failure == 1 ? C::invalid_argument
                                                 : C::limit_exceeded);
        EXPECT_EQ(r.input_consumed, 0u);
        EXPECT_EQ(r.output_produced, 0u);
        EXPECT_EQ(a.calls, calls);
        guards(rejected);
        sticky(e, r);
      }
      EXPECT_EQ(a.live_bytes, 0u);
    }
    consume(wire, std::span(raw).first(8), true,
            wire.capacity() + expected.capacity() + sizeof(raw) + 65536);
  }
}
} // namespace
