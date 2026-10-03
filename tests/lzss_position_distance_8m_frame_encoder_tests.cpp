#include "frame/lzss_position_distance_8m_frame_decoder.hpp"
#include "frame/lzss_position_distance_8m_frame_encoder.hpp"
#include "lzss_position_distance_8m_frame_encode_vectors.hpp"
#include "lzss_position_distance_8m_frame_vectors.hpp"
#include "lzss_position_distance_8m_reference_oracle.hpp"
#include "lzss_position_distance_8m_token_vectors.hpp"
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
#include <random>
#include <vector>
namespace {
using namespace marc;
using namespace frame::internal;
using E = LzssPositionDistance8mFrameEncodeError;
using Token = dictionary::internal::LzssTypedToken;
using Op = context::internal::ModeledOperation;
constexpr auto guard = std::byte{0xa5};
struct Fixture {
  std::vector<std::byte> raw, out, frame, payload;
  std::vector<Token> tokens, ts;
  std::vector<Op> ops, os;
  std::vector<std::uint32_t> index;
  core::DecoderLimits limits{};
  TypedContextStreamHeader stream{};
  TypedContextFrameLayout layout{};
  std::size_t written = 77;
  explicit Fixture(std::size_t n = 1)
      : raw(n, std::byte{65}), out(18 * n + 100, guard),
        frame(out.size(), guard), payload(out.size(), guard), tokens(n), ts(n),
        ops(2 * n), os(2 * n), index(65536 + n) {
    limits.max_block_size = 8388608;
    limits.max_internal_buffered_bytes = 512u << 20;
    stream.frame_size = static_cast<std::uint32_t>(n);
    stream.original_size = n;
    stream.dictionary = {8388608, 3, 258, 0};
    stream.range_model_total = 32768;
    stream.context_count = 47;
    stream.dictionary_variant = 11;
    stream.context_algorithm = 1;
    stream.context_variant = 12;
    layout.serialized_size = 987;
  }
  LzssPositionDistance8mFrameEncodeWorkspace buffers() {
    return {tokens, ts, index, ops, os, frame, payload};
  }
  LzssPositionDistance8mFrameEncodeResult run(std::uint64_t seq = 0,
                                              std::uint64_t committed = 0,
                                              std::size_t retained = 0) {
    auto b = buffers();
    const TypedContextFrameValidationContext c{stream, limits, seq, committed};
    return encode_lzss_position_distance_8m_frame(raw, c, b, out, layout,
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
    auto d = decode_lzss_position_distance_8m_frame(
        std::span(out).first(written), c, dt, ds, decoded, scratch, dl);
    ASSERT_EQ(d.error, LzssPositionDistance8mFrameDecodeError::none);
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
TEST(PositionDistance8mFrameEncoder, LiteralIndependentCompleteFrame) {
  Fixture f;
  f.success();
  ASSERT_EQ(f.written, 86u);
  for (std::size_t i = 0; i < 80; ++i)
    EXPECT_EQ(f.out[i], std::byte{frame_vectors::literal_prefix[i]});
  for (std::size_t i = 0; i < 6; ++i)
    EXPECT_EQ(f.out[80 + i], std::byte{token_vectors::literal[i]});
}
TEST(PositionDistance8mFrameEncoder, FourIndependentMathematicalFrames) {
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
TEST(PositionDistance8mFrameEncoder, AllOneByteValues) {
  for (unsigned i = 0; i < 256; ++i) {
    Fixture f;
    f.raw[0] = std::byte(i);
    f.success();
  }
}
TEST(PositionDistance8mFrameEncoder, ShortAndLengthBoundaries) {
  for (auto n : {2, 3, 4, 5, 6, 8, 16, 257, 258, 259, 260, 517}) {
    Fixture f(n);
    f.success();
  }
}
TEST(PositionDistance8mFrameEncoder, BinaryPatterns) {
  Fixture f(512);
  for (std::size_t i = 0; i < f.raw.size(); ++i)
    f.raw[i] = std::byte(i % 256);
  f.success();
}
TEST(PositionDistance8mFrameEncoder, RandomAndRepeatedDifferential) {
  std::mt19937 rng(1430);
  for (unsigned k = 0; k < 32; ++k) {
    Fixture f(1 + rng() % 96);
    for (auto &v : f.raw)
      v = std::byte(rng() % ((k & 1) ? 4 : 256));
    f.success();
  }
}
TEST(PositionDistance8mFrameEncoder, NonzeroSequenceFinalShortFrame) {
  Fixture f(5);
  f.stream.frame_size = 17;
  f.stream.original_size = 22;
  f.success(1, 17);
}
TEST(PositionDistance8mFrameEncoder, DeterministicRepeat) {
  Fixture f(267);
  f.success();
  auto bytes = f.out;
  auto count = f.written;
  f.success();
  EXPECT_EQ(f.out, bytes);
  EXPECT_EQ(f.written, count);
}
TEST(PositionDistance8mFrameEncoder, InvalidPositions) {
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
TEST(PositionDistance8mFrameEncoder, InvalidStreamPolicy) {
  Fixture f;
  f.stream.dictionary_variant = 10;
  f.failed(E::invalid_stream);
  f.stream.dictionary_variant = 11;
  f.stream.dictionary.flags = 1;
  f.failed(E::invalid_stream);
}
TEST(PositionDistance8mFrameEncoder, ParserCapacityFailures) {
  Fixture f(5);
  f.tokens.resize(0);
  f.failed(E::parser_error);
  Fixture g(5);
  g.ts.resize(0);
  g.failed(E::parser_error);
  Fixture h;
  h.index.resize(65536);
  h.failed(E::parser_error);
}
TEST(PositionDistance8mFrameEncoder, MapperCapacityFailures) {
  Fixture f;
  f.ops.resize(1);
  f.failed(E::mapper_error);
  Fixture g;
  g.os.resize(1);
  g.failed(E::mapper_error);
}
TEST(PositionDistance8mFrameEncoder, RangeCapacityFailures) {
  Fixture f;
  f.frame.resize(85);
  f.failed(E::range_error);
  Fixture g;
  g.payload.resize(5);
  g.failed(E::range_error);
}
TEST(PositionDistance8mFrameEncoder, EveryCallerCapacityShortage) {
  for (unsigned n = 0; n < 86; ++n) {
    Fixture f;
    f.out.resize(n);
    f.failed(E::storage_too_small);
  }
}
TEST(PositionDistance8mFrameEncoder, PrefixStorageAndLatePolicyFailure) {
  Fixture f;
  f.frame.resize(79);
  f.failed(E::storage_too_small);
  Fixture g(32);
  g.limits.max_expansion_ratio = 1;
  g.limits.expansion_slack = 0;
  g.failed(E::prefix_error);
}
TEST(PositionDistance8mFrameEncoder, ExactFullOwnerBudget) {
  Fixture f(32);
  auto r = f.run(0, 0, 123);
  ASSERT_EQ(r.error, E::none);
  const auto expected =
      f.raw.size() + f.out.size() + f.frame.size() + f.payload.size() +
      sizeof(Token) * (f.tokens.size() + f.ts.size()) +
      sizeof(Op) * (f.ops.size() + f.os.size()) +
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
TEST(PositionDistance8mFrameEncoder, RetainedOverflow) {
  Fixture f;
  f.failed(E::arithmetic_overflow, 0, 0,
           std::numeric_limits<std::size_t>::max());
}
TEST(PositionDistance8mFrameEncoder, FullWorkspaceAliases) {
  Fixture f;
  auto b = f.buffers();
  b.token_scratch = b.tokens;
  auto old = f.out;
  const TypedContextFrameValidationContext c{f.stream, f.limits, 0, 0};
  auto r = encode_lzss_position_distance_8m_frame(f.raw, c, b, f.out, f.layout,
                                                  f.written);
  EXPECT_EQ(r.error, E::overlapping_buffers);
  EXPECT_EQ(f.out, old);
  EXPECT_EQ(f.written, 77u);
  EXPECT_EQ(f.layout.serialized_size, 987u);
  b = f.buffers();
  b.frame = f.out;
  r = encode_lzss_position_distance_8m_frame(f.raw, c, b, f.out, f.layout,
                                             f.written);
  EXPECT_EQ(r.error, E::overlapping_buffers);
  EXPECT_EQ(f.out, old);
}
TEST(PositionDistance8mFrameEncoder, UnusedOutputTailMetadataAlias) {
  Fixture f;
  std::array<std::size_t, 32> owner{};
  owner.fill(77);
  auto before = owner;
  auto b = f.buffers();
  const TypedContextFrameValidationContext c{f.stream, f.limits, 0, 0};
  auto bytes = std::as_writable_bytes(std::span(owner));
  auto r = encode_lzss_position_distance_8m_frame(f.raw, c, b, bytes, f.layout,
                                                  owner.back());
  EXPECT_EQ(r.error, E::overlapping_buffers);
  EXPECT_EQ(owner, before);
  EXPECT_EQ(f.layout.serialized_size, 987u);
}
TEST(PositionDistance8mFrameEncoder, MetadataAliasInput) {
  Fixture f;
  auto b = f.buffers();
  const TypedContextFrameValidationContext c{f.stream, f.limits, 0, 0};
  auto old = f.out;
  auto r = encode_lzss_position_distance_8m_frame(f.raw, c, b, f.out, f.layout,
                                                  f.layout.serialized_size);
  EXPECT_EQ(r.error, E::overlapping_buffers);
  EXPECT_EQ(f.out, old);
  EXPECT_EQ(f.layout.serialized_size, 987u);
}
TEST(PositionDistance8mFrameEncoder, FullPrivateUnusedCapacitiesCharged) {
  Fixture f;
  auto r = f.run();
  ASSERT_EQ(r.error, E::none);
  f.index.resize(f.index.size() + 100);
  f.limits.max_internal_buffered_bytes = r.aggregate_bytes;
  f.failed(E::limit_exceeded);
}
} // namespace
