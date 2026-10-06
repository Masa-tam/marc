#include "core/endian.hpp"
#include "frame/lzss_position_distance_64m_frame_decoder.hpp"
#include "frame/lzss_position_distance_64m_frame_encoder.hpp"
#include "frame/lzss_position_distance_64m_token_frame_encoder.hpp"
#include "lzss_position_distance_64m_frame_encode_vectors.hpp"
#include "lzss_position_distance_64m_reference_oracle.hpp"
#include <cstring>
#include <gtest/gtest.h>
#include <iostream>
#include <limits>
#include <random>
#include <vector>
namespace {
using namespace marc;
using namespace frame::internal;
using E = LzssPositionDistance64mTokenFrameError;
using Token = dictionary::internal::LzssTypedToken;
constexpr auto guard = std::byte{0xa5};
struct Fixture {
  std::vector<std::byte> raw, out, frame, payload;
  std::vector<Token> tokens, ts;
  std::vector<std::uint32_t> index;
  core::DecoderLimits limits{};
  TypedContextStreamHeader stream{};
  TypedContextFrameLayout layout{};
  std::size_t written = 77;
  explicit Fixture(std::size_t n = 1)
      : raw(n, std::byte{65}), out(20 * n + 100, guard),
        frame(out.size(), guard), payload(out.size(), guard), tokens(n), ts(n),
        index(1048576 + n) {
    limits.max_block_size = 67108864;
    limits.max_frame_size = 67108864;
    limits.max_lz_distance = 67108864;
    limits.max_internal_buffered_bytes = 512u * 1024u * 1024u;
    limits.max_internal_buffered_bytes = 512u << 20;
    stream.frame_size = static_cast<std::uint32_t>(n);
    stream.original_size = n;
    stream.dictionary = {67108864, 3, 258, 0};
    stream.range_model_total = 32768;
    stream.context_count = 50;
    stream.dictionary_variant = 14;
    stream.context_algorithm = 1;
    stream.context_variant = 15;
    layout.serialized_size = 987;
  }
  LzssPositionDistance64mTokenFrameWorkspace buffers() {
    return {tokens, ts, index, frame, payload};
  }
  LzssPositionDistance64mTokenFrameResult run(std::uint64_t seq = 0,
                                              std::uint64_t committed = 0,
                                              std::size_t retained = 0) {
    auto b = buffers();
    const TypedContextFrameValidationContext c{stream, limits, seq, committed};
    return encode_lzss_position_distance_64m_token_frame(raw, c, b, out, layout,
                                                         written, retained);
  }
  void failed(E expected, std::uint64_t seq = 0, std::uint64_t committed = 0,
              std::size_t retained = 0) {
    const auto before = out;
    std::array<std::byte, sizeof(layout)> saved{};
    std::memcpy(saved.data(), &layout, sizeof(layout));
    const auto old = written;
    auto r = run(seq, committed, retained);
    EXPECT_EQ(r.error, expected);
    EXPECT_EQ(r.bytes_committed, 0u);
    EXPECT_EQ(out, before);
    EXPECT_EQ(written, old);
    EXPECT_EQ(std::memcmp(saved.data(), &layout, sizeof(layout)), 0);
  }
  void success(std::uint64_t seq = 0, std::uint64_t committed = 0) {
    auto r = run(seq, committed);
    ASSERT_EQ(r.error, E::none);
    ASSERT_EQ(written, r.bytes_committed);
    ASSERT_EQ(layout.serialized_size, written);
    EXPECT_EQ(layout.header.sequence, seq);
    EXPECT_EQ(layout.header.uncompressed_size, raw.size());
    EXPECT_TRUE(std::all_of(out.begin() + written, out.end(),
                            [](auto v) { return v == guard; }));
    std::vector<Token> dt(layout.header.token_count), ds(dt.size());
    std::vector<std::byte> decoded(raw.size() + 11, guard),
        scratch(decoded.size(), guard);
    TypedContextFrameLayout dl{};
    const TypedContextFrameValidationContext c{stream, limits, seq, committed};
    auto d = decode_lzss_position_distance_64m_frame(
        std::span(out).first(written), c, dt, ds, decoded, scratch, dl);
    ASSERT_EQ(d.error, LzssPositionDistance64mFrameDecodeError::none);
    EXPECT_EQ(d.raw_produced, raw.size());
    EXPECT_TRUE(std::equal(raw.begin(), raw.end(), decoded.begin()));
    EXPECT_TRUE(std::all_of(decoded.begin() + raw.size(), decoded.end(),
                            [](auto v) { return v == guard; }));
    auto oracle = reference_oracle::parse(raw, stream.dictionary);
    ASSERT_EQ(dt.size(), oracle.size());
    for (std::size_t i = 0; i < dt.size(); ++i)
      EXPECT_TRUE(reference_oracle::equal(dt[i], oracle[i]));
  }
};
TEST(PositionDistance64mTokenFrameEncoder, LiteralIndependentCompleteFrame) {
  Fixture f;
  f.success();
  ASSERT_EQ(f.written, 86u);
  for (std::size_t i = 0; i < 80; ++i)
    EXPECT_EQ(f.out[i], std::byte{frame_vectors::literal_prefix[i]});
  for (std::size_t i = 0; i < 6; ++i)
    EXPECT_EQ(f.out[80 + i], std::byte{token_vectors::literal[i]});
}
TEST(PositionDistance64mTokenFrameEncoder, FourIndependentMathematicalFrames) {
  auto check = [](auto &raw, auto &frame) {
    Fixture f(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i)
      f.raw[i] = std::byte{raw[i]};
    f.success();
    ASSERT_EQ(f.written, frame.size());
    for (std::size_t i = 0; i < frame.size(); ++i)
      EXPECT_EQ(f.out[i], std::byte{frame[i]});
  };
  using namespace frame_encode_vectors;
  check(repeat_raw, repeat_frame);
  check(pattern_raw, pattern_frame);
  check(tie_raw, tie_frame);
  check(binary_raw, binary_frame);
}
TEST(PositionDistance64mTokenFrameEncoder, AllOneByteValues) {
  for (unsigned i = 0; i < 256; ++i) {
    Fixture f;
    f.raw[0] = std::byte(i);
    f.success();
  }
}
TEST(PositionDistance64mTokenFrameEncoder, ShortAndLengthBoundaries) {
  for (auto n : {2, 3, 4, 5, 6, 8, 16, 257, 258, 259, 260, 517}) {
    Fixture f(n);
    f.success();
  }
}
TEST(PositionDistance64mTokenFrameEncoder, BinaryPatterns) {
  Fixture f(512);
  for (std::size_t i = 0; i < f.raw.size(); ++i)
    f.raw[i] = std::byte(i % 256);
  f.success();
}
TEST(PositionDistance64mTokenFrameEncoder, RandomAndRepeatedDifferential) {
  std::mt19937 rng(1430);
  for (unsigned k = 0; k < 32; ++k) {
    Fixture f(1 + rng() % 96);
    for (auto &v : f.raw)
      v = std::byte(rng() % ((k & 1) ? 4 : 256));
    f.success();
  }
}
TEST(PositionDistance64mTokenFrameEncoder, NonzeroSequenceFinalShortFrame) {
  Fixture f(5);
  f.stream.frame_size = 17;
  f.stream.original_size = 22;
  f.success(1, 17);
}
TEST(PositionDistance64mTokenFrameEncoder, DeterministicRepeat) {
  Fixture f(267);
  f.success();
  auto bytes = f.out;
  auto count = f.written;
  f.success();
  EXPECT_EQ(f.out, bytes);
  EXPECT_EQ(f.written, count);
}
TEST(PositionDistance64mTokenFrameEncoder, InvalidPositions) {
  Fixture f;
  f.failed(E::invalid_position, 1);
  f.failed(E::invalid_position, 0, 1);
  f.stream.original_size = 0;
  f.failed(E::invalid_position);
  Fixture empty(0);
  empty.stream.frame_size = 1;
  empty.stream.original_size = 1;
  empty.failed(E::invalid_position);
  Fixture shortframe(2);
  shortframe.stream.frame_size = 3;
  shortframe.stream.original_size = 3;
  shortframe.failed(E::invalid_position);
}
TEST(PositionDistance64mTokenFrameEncoder, InvalidStreamPolicy) {
  Fixture f;
  f.stream.dictionary_variant = 10;
  f.failed(E::invalid_stream);
  f.stream.dictionary_variant = 14;
  f.stream.dictionary.flags = 1;
  f.failed(E::invalid_stream);
}
TEST(PositionDistance64mTokenFrameEncoder, ParserCapacityFailures) {
  Fixture f(5);
  f.tokens.resize(0);
  f.failed(E::parser_error);
  Fixture g(5);
  g.ts.resize(0);
  g.failed(E::parser_error);
  Fixture h;
  h.index.resize(1048576);
  h.failed(E::parser_error);
}
TEST(PositionDistance64mTokenFrameEncoder, RangeCapacityFailures) {
  Fixture f;
  f.frame.resize(85);
  f.failed(E::range_error);
  Fixture g;
  g.payload.resize(5);
  g.failed(E::range_error);
}
TEST(PositionDistance64mTokenFrameEncoder, EveryCallerCapacityShortage) {
  for (unsigned n = 0; n < 86; ++n) {
    Fixture f;
    f.out.resize(n);
    f.failed(E::storage_too_small);
  }
}
TEST(PositionDistance64mTokenFrameEncoder, PrefixStorageAndLatePolicyFailure) {
  Fixture f;
  f.frame.resize(79);
  f.failed(E::storage_too_small);
  Fixture g(32);
  g.limits.max_expansion_ratio = 1;
  g.limits.expansion_slack = 0;
  g.failed(E::prefix_error);
}
TEST(PositionDistance64mTokenFrameEncoder, ExactFullOwnerBudget) {
  Fixture f(32);
  auto r = f.run(0, 0, 123);
  ASSERT_EQ(r.error, E::none);
  const auto expected =
      f.raw.size() + f.out.size() + f.frame.size() + f.payload.size() +
      sizeof(Token) * (f.tokens.size() + f.ts.size()) +
      sizeof(std::uint32_t) * f.index.size() + r.working_state_bytes + 123;
  EXPECT_EQ(r.aggregate_bytes, expected);
  RecordProperty("working_state_bytes", r.working_state_bytes);
  f.limits.max_internal_buffered_bytes = r.aggregate_bytes;
  f.limits.max_block_size = 32;
  f.success();
  EXPECT_EQ(f.run(0, 0, 123).error, E::none);
  --f.limits.max_internal_buffered_bytes;
  f.failed(E::limit_exceeded, 0, 0, 123);
}
TEST(PositionDistance64mTokenFrameEncoder, RetainedOverflow) {
  Fixture f;
  f.failed(E::arithmetic_overflow, 0, 0,
           std::numeric_limits<std::size_t>::max());
}
TEST(PositionDistance64mTokenFrameEncoder, FullWorkspaceAliases) {
  Fixture f;
  auto b = f.buffers();
  b.token_scratch = b.tokens;
  auto old = f.out;
  const TypedContextFrameValidationContext c{f.stream, f.limits, 0, 0};
  auto r = encode_lzss_position_distance_64m_token_frame(f.raw, c, b, f.out,
                                                         f.layout, f.written);
  EXPECT_EQ(r.error, E::overlapping_buffers);
  EXPECT_EQ(f.out, old);
  EXPECT_EQ(f.written, 77u);
  EXPECT_EQ(f.layout.serialized_size, 987u);
  b = f.buffers();
  b.frame = f.out;
  r = encode_lzss_position_distance_64m_token_frame(f.raw, c, b, f.out,
                                                    f.layout, f.written);
  EXPECT_EQ(r.error, E::overlapping_buffers);
  EXPECT_EQ(f.out, old);
}
TEST(PositionDistance64mTokenFrameEncoder, UnusedOutputTailMetadataAlias) {
  Fixture f;
  std::array<std::size_t, 32> owner{};
  owner.fill(77);
  auto before = owner;
  auto b = f.buffers();
  const TypedContextFrameValidationContext c{f.stream, f.limits, 0, 0};
  auto bytes = std::as_writable_bytes(std::span(owner));
  auto r = encode_lzss_position_distance_64m_token_frame(
      f.raw, c, b, bytes, f.layout, owner.back());
  EXPECT_EQ(r.error, E::overlapping_buffers);
  EXPECT_EQ(owner, before);
  EXPECT_EQ(f.layout.serialized_size, 987u);
}
TEST(PositionDistance64mTokenFrameEncoder, MetadataAliasInput) {
  Fixture f;
  auto b = f.buffers();
  const TypedContextFrameValidationContext c{f.stream, f.limits, 0, 0};
  auto old = f.out;
  auto r = encode_lzss_position_distance_64m_token_frame(
      f.raw, c, b, f.out, f.layout, f.layout.serialized_size);
  EXPECT_EQ(r.error, E::overlapping_buffers);
  EXPECT_EQ(f.out, old);
  EXPECT_EQ(f.layout.serialized_size, 987u);
}
TEST(PositionDistance64mTokenFrameEncoder, FullPrivateUnusedCapacitiesCharged) {
  Fixture f;
  auto r = f.run();
  ASSERT_EQ(r.error, E::none);
  f.index.resize(f.index.size() + 100);
  f.limits.max_internal_buffered_bytes = r.aggregate_bytes;
  f.failed(E::limit_exceeded);
}
TEST(PositionDistance64mTokenFrameEncoder, QueryCountsAndPrivateOnlyMutation) {
  for (auto n : {1u, 32u, 267u}) {
    Fixture f(n);
    auto b = f.buffers();
    const TypedContextFrameValidationContext c{f.stream, f.limits, 0, 0};
    auto original = f.out;
    auto q =
        query_lzss_position_distance_64m_token_frame_encode(f.raw, c, b, 0);
    ASSERT_EQ(q.error, E::storage_too_small);
    ASSERT_GE(q.bytes_required, 85u);
    EXPECT_EQ(q.counts.declared_raw_size, n);
    EXPECT_EQ(f.out, original);
    EXPECT_EQ(f.written, 77u);
    EXPECT_EQ(f.layout.serialized_size, 987u);
    auto ok = query_lzss_position_distance_64m_token_frame_encode(f.raw, c, b,
                                                                  f.out.size());
    ASSERT_EQ(ok.error, E::none);
    EXPECT_EQ(q.bytes_required, ok.bytes_required);
    auto r = f.run();
    ASSERT_EQ(r.error, E::none);
    EXPECT_EQ(f.written, ok.bytes_required);
    EXPECT_EQ(f.layout.header.token_count, ok.token_count);
    std::uint16_t contexts{};
    std::uint32_t decisions{}, payload{};
    ASSERT_TRUE(core::load_le(std::span<const std::byte>(f.out), 72, contexts));
    ASSERT_TRUE(
        core::load_le(std::span<const std::byte>(f.out), 64, decisions));
    ASSERT_TRUE(core::load_le(std::span<const std::byte>(f.out), 68, payload));
    EXPECT_EQ(contexts, 50);
    EXPECT_EQ(decisions, ok.counts.declared_decision_count);
    EXPECT_EQ(payload, ok.range_plan.details.payload_size);
  }
}
TEST(PositionDistance64mTokenFrameEncoder, CountBeforeSelectingPayloadStorage) {
  Fixture f(32);
  auto b = f.buffers();
  b.frame = {};
  b.payload_scratch = {};
  const TypedContextFrameValidationContext c{f.stream, f.limits, 0, 0};
  auto q = query_lzss_position_distance_64m_token_frame_encode(f.raw, c, b, 0);
  EXPECT_EQ(q.error, E::storage_too_small);
  EXPECT_GE(q.bytes_required, 85u);
  EXPECT_GT(q.token_count, 0u);
}
TEST(PositionDistance64mTokenFrameEncoder, QueryCheckedOutputCapacityOverflow) {
  Fixture f;
  auto b = f.buffers();
  const TypedContextFrameValidationContext c{f.stream, f.limits, 0, 0};
  EXPECT_EQ(
      query_lzss_position_distance_64m_token_frame_encode(f.raw, c, b, SIZE_MAX)
          .error,
      E::arithmetic_overflow);
}
TEST(PositionDistance64mTokenFrameEncoder,
     ConfigurationAndLayoutPrivateAliases) {
  Fixture f;
  const TypedContextFrameValidationContext c{f.stream, f.limits, 0, 0};
  for (auto bytes : {std::as_writable_bytes(std::span(&f.stream, 1)),
                     std::as_writable_bytes(std::span(&f.limits, 1)),
                     std::as_writable_bytes(std::span(&f.layout, 1))}) {
    auto b = f.buffers();
    b.frame = bytes;
    auto before = std::vector<std::byte>(bytes.begin(), bytes.end());
    auto output = f.out;
    auto r = encode_lzss_position_distance_64m_token_frame(f.raw, c, b, f.out,
                                                           f.layout, f.written);
    EXPECT_EQ(r.error, E::overlapping_buffers);
    EXPECT_EQ(f.out, output);
    EXPECT_TRUE(std::equal(before.begin(), before.end(), bytes.begin()));
    EXPECT_EQ(f.written, 77u);
  }
}
TEST(PositionDistance64mTokenFrameEncoder,
     DifferentialFullFramesAndCompleteOwnerLedger) {
  for (auto n : {1u, 32u, 267u, 1048576u, 4194304u, 8388608u, 67108864u}) {
    Fixture f(1);
    f.raw.assign(n, std::byte{65});
    f.stream.frame_size = n;
    f.stream.original_size = n;
    f.index.resize(1048576 + n);
    auto counted =
        dictionary::internal::query_lzss_position_distance_64m_indexed(
            f.raw, f.stream.dictionary, f.limits, 0, 0, f.index);
    ASSERT_EQ(counted.error,
              dictionary::internal::LzssPositionDistance64mParseError::
                  output_too_small);
    f.tokens.resize(counted.details.token_count);
    f.ts.resize(f.tokens.size());
    auto b = f.buffers();
    b.frame = {};
    b.payload_scratch = {};
    const TypedContextFrameValidationContext c{f.stream, f.limits, 0, 0};
    auto planned =
        query_lzss_position_distance_64m_token_frame_encode(f.raw, c, b, 0);
    ASSERT_EQ(planned.error, E::storage_too_small);
    ASSERT_GE(planned.bytes_required, 85u);
    f.frame.assign(planned.bytes_required + 3, guard);
    f.payload.assign(planned.bytes_required - 80 + 3, guard);
    f.out.assign(planned.bytes_required + 3, guard);
    using Op = context::internal::ModeledOperation;
    std::vector<Op> ops(planned.counts.declared_event_count), os(ops.size());
    std::vector<Token> ot(f.tokens.size()), ots(ot.size()), dt(ot.size()),
        ds(ot.size());
    std::vector<std::uint32_t> oi(f.index.size());
    std::vector<std::byte> old(f.out.size(), guard), of(old.size(), guard),
        op(f.payload.size(), guard), decoded(n), scratch(n);
    TypedContextFrameLayout ol{}, dl{};
    std::size_t ow = 77;
    auto owner = [](const auto &v) { return v.capacity() * sizeof(v[0]); };
    const auto controls =
        sizeof(Fixture) + sizeof(ops) + sizeof(os) + sizeof(ot) + sizeof(ots) +
        sizeof(dt) + sizeof(ds) + sizeof(oi) + sizeof(old) + sizeof(of) +
        sizeof(op) + sizeof(decoded) + sizeof(scratch) + sizeof(ol) +
        sizeof(dl) + sizeof(ow) + sizeof(b) + sizeof(c) + sizeof(planned) +
        sizeof(LzssPositionDistance64mTokenFrameResult) +
        sizeof(LzssPositionDistance64mFrameEncodeResult) +
        sizeof(LzssPositionDistance64mFrameDecodeResult) +
        sizeof(LzssPositionDistance64mFrameEncodeWorkspace) +
        16 * sizeof(std::size_t);
    const auto helper =
        std::max({lzss_position_distance_64m_token_frame_working_bytes(),
                  lzss_position_distance_64m_frame_encode_working_bytes(),
                  (sizeof(LzssPositionDistance64mFrameDecodePlan) +
                   sizeof(LzssPositionDistance64mFrameDecodeResult) +
                   context::internal::
                       lzss_position_distance_64m_token_working_bytes())});
    const auto total = owner(f.raw) + owner(f.tokens) + owner(f.ts) +
                       owner(f.index) + owner(f.out) + owner(f.frame) +
                       owner(f.payload) + owner(ops) + owner(os) + owner(ot) +
                       owner(ots) + owner(dt) + owner(ds) + owner(oi) +
                       owner(old) + owner(of) + owner(op) + owner(decoded) +
                       owner(scratch) + controls + helper;
    if (n == 67108864u) {
      EXPECT_GT(total, f.limits.max_internal_buffered_bytes);
      f.limits.max_internal_buffered_bytes = std::uint64_t{1024} << 20;
    }
    ASSERT_LE(total, f.limits.max_internal_buffered_bytes);
    b = f.buffers();
    const auto local = f.raw.size() + f.out.size() + f.frame.size() +
                       f.payload.size() +
                       sizeof(Token) * (f.tokens.size() + f.ts.size()) +
                       sizeof(uint32_t) * f.index.size() +
                       lzss_position_distance_64m_token_frame_working_bytes();
    auto q = query_lzss_position_distance_64m_token_frame_encode(
        f.raw, c, b, f.out.size(), total - local);
    ASSERT_EQ(q.error, E::none);
    EXPECT_EQ(q.aggregate_bytes, total);
    auto r = encode_lzss_position_distance_64m_token_frame(
        f.raw, c, b, f.out, f.layout, f.written, total - local);
    ASSERT_EQ(r.error, E::none);
    EXPECT_EQ(r.aggregate_bytes, total);
    LzssPositionDistance64mFrameEncodeWorkspace ob{ot, ots, oi, ops,
                                                   os, of,  op};
    const auto oldlocal =
        f.raw.size() + old.size() + of.size() + op.size() +
        sizeof(Token) * (ot.size() + ots.size()) +
        sizeof(uint32_t) * oi.size() + sizeof(Op) * (ops.size() + os.size()) +
        lzss_position_distance_64m_frame_encode_working_bytes();
    auto ref = encode_lzss_position_distance_64m_frame(f.raw, c, ob, old, ol,
                                                       ow, total - oldlocal);
    ASSERT_EQ(ref.error, LzssPositionDistance64mFrameEncodeError::none);
    EXPECT_EQ(ref.aggregate_bytes, total);
    EXPECT_EQ(f.written, ow);
    EXPECT_EQ(f.out, old);
    const auto dlocal =
        f.written + sizeof(Token) * (dt.size() + ds.size()) + decoded.size() +
        scratch.size() +
        (sizeof(LzssPositionDistance64mFrameDecodePlan) +
         sizeof(LzssPositionDistance64mFrameDecodeResult) +
         context::internal::lzss_position_distance_64m_token_working_bytes());
    auto dr = decode_lzss_position_distance_64m_frame(
        std::span(f.out).first(f.written), c, dt, ds, decoded, scratch, dl,
        total - dlocal);
    ASSERT_EQ(dr.error, LzssPositionDistance64mFrameDecodeError::none);
    EXPECT_EQ(decoded, f.raw);
    std::cout << "token_frame_qualified=" << n << ":" << total
              << " working=" << r.working_state_bytes
              << " P=" << q.range_plan.details.payload_size << "\n";
  }
}
TEST(PositionDistance64mTokenFrameEncoder,
     QueryPreservesPrivatePayloadStorage) {
  Fixture f(267);
  auto frame = f.frame, payload = f.payload, output = f.out;
  auto b = f.buffers();
  const TypedContextFrameValidationContext c{f.stream, f.limits, 0, 0};
  auto q = query_lzss_position_distance_64m_token_frame_encode(f.raw, c, b,
                                                               f.out.size());
  ASSERT_EQ(q.error, E::none);
  EXPECT_EQ(f.frame, frame);
  EXPECT_EQ(f.payload, payload);
  EXPECT_EQ(f.out, output);
  EXPECT_EQ(f.written, 77u);
}
TEST(PositionDistance64mTokenFrameEncoder,
     FailureAfterSuccessPreservesCommittedCallerFrame) {
  Fixture f(32);
  ASSERT_EQ(f.run().error, E::none);
  auto saved = f.out;
  const auto written = f.written;
  std::array<std::byte, sizeof(f.layout)> layout{};
  std::memcpy(layout.data(), &f.layout, sizeof(f.layout));
  f.payload.resize(0);
  auto failed = f.run();
  EXPECT_EQ(failed.error, E::range_error);
  EXPECT_EQ(failed.bytes_committed, 0u);
  EXPECT_EQ(f.out, saved);
  EXPECT_EQ(f.written, written);
  EXPECT_EQ(std::memcmp(layout.data(), &f.layout, sizeof(f.layout)), 0);
}
TEST(PositionDistance64mTokenFrameEncoder,
     PayloadAndTotalOutputPolicyFailures) {
  Fixture f;
  f.limits.max_compressed_payload_size = 5;
  f.failed(E::range_error);
  Fixture g;
  g.stream.original_size = g.limits.max_total_output_size + 1;
  g.failed(E::invalid_stream);
}
} // namespace
