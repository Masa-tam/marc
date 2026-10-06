#include "core/status.hpp"
#include "frame/lzss_position_distance_64m_frame_decoder.hpp"
#include "frame/lzss_position_distance_64m_owned_stream_encoder.hpp"
#include "frame/lzss_position_distance_64m_token_frame_encoder.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <vector>
namespace {
using namespace marc;
using namespace frame::internal;
using Code = core::ErrorCode;
using Status = core::StreamStatus;
using Token = dictionary::internal::LzssTypedToken;
constexpr auto guard = std::byte{0xa5};
struct Allocator final : LzssPositionDistance64mStreamAllocator {
  struct Record {
    const void *data{};
    std::size_t bytes{};
  };
  std::array<Record, 12> records{};
  LzssPositionDistance64mExactStreamAllocator exact;
  std::size_t calls{}, fail_at{}, live{}, peak{}, releases{};
  LzssPositionDistance64mAllocatorControls controls() const noexcept override {
    return {this, sizeof(*this), 128};
  }
  void remember(const void *p, std::size_t bytes) noexcept {
    if (!p)
      return;
    for (auto &r : records)
      if (!r.data) {
        r = {p, bytes};
        live += bytes;
        peak = std::max(peak, live);
        return;
      }
    std::abort();
  }
  void forget(const void *p) noexcept {
    if (!p)
      return;
    for (auto &r : records)
      if (r.data == p) {
        live -= r.bytes;
        r = {};
        ++releases;
        return;
      }
    std::abort();
  }
  LzssPositionDistance64mOwnedBytes bytes(std::size_t n) noexcept override {
    if (++calls == fail_at)
      return {};
    auto b = exact.bytes(n);
    remember(b.data, b.capacity);
    return b;
  }
  LzssPositionDistance64mOwnedIndex indices(std::size_t n) noexcept override {
    if (++calls == fail_at)
      return {};
    auto b = exact.indices(n);
    remember(b.data, b.capacity * 4);
    return b;
  }
  void release(LzssPositionDistance64mOwnedBytes &b) noexcept override {
    auto p = b.data;
    exact.release(b);
    forget(p);
  }
  void release(LzssPositionDistance64mOwnedIndex &b) noexcept override {
    auto p = b.data;
    exact.release(b);
    forget(p);
  }
};
core::DecoderLimits limits() {
  core::DecoderLimits l{};
  l.max_block_size = 67108864;
  l.max_frame_size = l.max_lz_distance = 67108864;
  l.max_internal_buffered_bytes = 512u << 20;
  return l;
}
TypedContextStreamHeader stream(std::size_t size, std::size_t frame = 32) {
  TypedContextStreamHeader s{};
  s.original_size = size;
  s.frame_size = static_cast<std::uint32_t>(frame);
  s.dictionary = {67108864, 3, 258, 0};
  s.dictionary_variant = 14;
  s.context_algorithm = 1;
  s.context_variant = 15;
  s.context_count = 50;
  s.range_model_total = 32768;
  return s;
}
struct OwnerFixture {
  Allocator a;
  LzssPositionDistance64mCompactOwner owner{a};
  std::vector<std::byte> raw = std::vector<std::byte>(32, std::byte{65});
  std::vector<std::uint32_t> index = std::vector<std::uint32_t>(1048576 + 32);
  core::DecoderLimits l = limits();
  TypedContextStreamHeader s = stream(32);
  LzssPositionDistance64mOwnerResult encode(std::size_t extra = 0) {
    const TypedContextFrameValidationContext c{s, l, 0, 0};
    return owner.encode(raw, c, index, extra);
  }
  void failure(Code code) {
    const auto data = owner.publication().data();
    const auto previous = std::vector<std::byte>(owner.publication().begin(),
                                                 owner.publication().end());
    const auto pending = owner.pending();
    const auto layout = owner.layout();
    const auto live = a.live;
    auto r = encode();
    EXPECT_EQ(r.error, code);
    EXPECT_EQ(r.bytes_validated, 0u);
    EXPECT_EQ(owner.publication().data(), data);
    EXPECT_TRUE(std::ranges::equal(owner.publication(), previous));
    EXPECT_EQ(owner.pending(), pending);
    EXPECT_EQ(std::memcmp(&layout, &owner.layout(), sizeof(layout)), 0);
    EXPECT_EQ(a.live, live);
  }
};
std::vector<std::byte> expected(std::span<const std::byte> raw,
                                std::size_t frame) {
  auto s = stream(raw.size(), frame);
  auto l = limits();
  std::vector<std::byte> output(112);
  std::size_t written{};
  EXPECT_EQ(
      serialize_lzss_position_distance_64m_stream_header(s, l, output, written)
          .error,
      LzssPositionDistance64mSerializeError::none);
  std::vector<std::uint32_t> index(1048576 + frame);
  for (std::size_t pos = 0; pos < raw.size(); pos += frame) {
    const auto part = raw.subspan(pos, std::min(frame, raw.size() - pos));
    std::vector<Token> t(part.size()), ts(t.size());
    std::vector<std::byte> buffer(20 * part.size() + 100), priv(buffer.size()),
        payload(buffer.size());
    const LzssPositionDistance64mTokenFrameWorkspace b{t, ts, index, priv,
                                                       payload};
    TypedContextFrameLayout layout{};
    const TypedContextFrameValidationContext c{s, l, pos / frame, pos};
    const auto r = encode_lzss_position_distance_64m_token_frame(
        part, c, b, buffer, layout, written);
    EXPECT_EQ(r.error, LzssPositionDistance64mTokenFrameError::none);
    output.insert(output.end(), buffer.begin(), buffer.begin() + written);
  }
  return output;
}
std::vector<std::byte> encode_stream(std::span<const std::byte> raw,
                                     std::size_t frame, std::size_t input_chunk,
                                     std::size_t output_chunk,
                                     bool flush = false) {
  Allocator a;
  std::vector<std::byte> encoded;
  auto l = limits();
  auto s = stream(raw.size(), frame);
  {
    LzssPositionDistance64mOwnedStreamEncoder encoder(s, l, a);
    std::size_t position = 0;
    std::vector<std::byte> buffer(output_chunk, guard);
    unsigned calls = 0;
    while (++calls < 100000) {
      const auto n = std::min(input_chunk, raw.size() - position);
      const bool last = position + n == raw.size();
      const auto flags =
          (last ? core::flag_value(core::ProcessFlags::end_input) : 0) |
          (flush ? core::flag_value(core::ProcessFlags::flush) : 0);
      const auto r = encoder.process(raw.subspan(position, n), buffer, flags);
      EXPECT_TRUE(core::is_valid(r, n, buffer.size()));
      EXPECT_NE(r.status, Status::error);
      position += r.input_consumed;
      encoded.insert(encoded.end(), buffer.begin(),
                     buffer.begin() + r.output_produced);
      if (r.status == Status::error)
        break;
      if (r.status == Status::end_of_stream) {
        EXPECT_EQ(position, raw.size());
        EXPECT_EQ(encoder.process({}, buffer, 0).status, Status::end_of_stream);
        break;
      }
    }
    EXPECT_LT(calls, 100000u);
    std::size_t live_blocks = 0;
    for (auto receipt : a.records)
      live_blocks += receipt.data != nullptr;
    EXPECT_EQ(live_blocks, raw.empty() ? 0u : 3u);
    EXPECT_GE(
        a.live,
        std::min(frame, raw.size()) +
            (raw.empty() ? 0 : (1048576 + std::min(frame, raw.size())) * 4));
  }
  EXPECT_EQ(a.live, 0u);
  return encoded;
}
TEST(CompactOwner, CompleteByteAgreementAndActualTemporaryRelease) {
  OwnerFixture f;
  auto r = f.encode();
  ASSERT_EQ(r.error, Code::none);
  EXPECT_EQ(f.a.calls, 4u);
  EXPECT_EQ(f.a.releases, 3u);
  EXPECT_EQ(f.a.live, f.owner.publication_capacity());
  EXPECT_TRUE(f.owner.pending());
  auto wire = expected(f.raw, 32);
  EXPECT_TRUE(
      std::ranges::equal(f.owner.publication(), std::span(wire).subspan(112)));
}
TEST(CompactOwner, EveryCandidateAllocationFailurePreservesPriorPublication) {
  for (unsigned i = 1; i <= 4; ++i) {
    OwnerFixture f;
    ASSERT_EQ(f.encode().error, Code::none);
    f.a.fail_at = f.a.calls + i;
    std::fill(f.raw.begin(), f.raw.end(), std::byte{66});
    f.failure(Code::out_of_memory);
  }
}
TEST(CompactOwner, RetainedPublicationChargedUntilDestruction) {
  OwnerFixture f;
  auto first = f.encode(111);
  ASSERT_EQ(first.error, Code::none);
  const auto old = f.owner.publication_capacity();
  const auto before = f.a.releases;
  std::fill(f.raw.begin(), f.raw.end(), std::byte{66});
  auto second = f.encode(111);
  ASSERT_EQ(second.error, Code::none);
  EXPECT_EQ(second.aggregate_bytes, first.aggregate_bytes + old);
  EXPECT_EQ(f.a.live, f.owner.publication_capacity());
  EXPECT_EQ(f.a.releases, before + 4);
}
TEST(CompactOwner, ExactWholeLedgerAndOneBelow) {
  OwnerFixture f;
  const auto r = f.encode(111);
  ASSERT_EQ(r.error, Code::none);
  const auto expected_bytes =
      LzssPositionDistance64mCompactOwner::working_bytes() + f.raw.size() +
      f.index.size() * 4 + sizeof(f.a) + 128 + 111 + 2 * f.raw.size() +
      3 * r.bytes_validated - 80;
  EXPECT_EQ(r.aggregate_bytes, expected_bytes);
  f.l.max_block_size = 32;
  const auto second_budget = r.aggregate_bytes + f.owner.publication_capacity();
  f.l.max_internal_buffered_bytes = second_budget;
  EXPECT_EQ(f.encode(111).error, Code::none);
  --f.l.max_internal_buffered_bytes;
  const auto ptr = f.owner.publication().data();
  const auto before = std::vector<std::byte>(f.owner.publication().begin(),
                                             f.owner.publication().end());
  EXPECT_EQ(f.encode(111).error, Code::limit_exceeded);
  EXPECT_EQ(f.owner.publication().data(), ptr);
  EXPECT_TRUE(std::ranges::equal(f.owner.publication(), before));
}
TEST(CompactOwner, PolicyFailureAfterPriorSuccess) {
  OwnerFixture f;
  ASSERT_EQ(f.encode().error, Code::none);
  f.l.max_expansion_ratio = 1;
  f.l.expansion_slack = 0;
  f.failure(Code::limit_exceeded);
}
TEST(CompactOwner, AdmissionRefusalBeforeAllocation) {
  OwnerFixture f;
  f.l.max_internal_buffered_bytes = 1;
  f.failure(Code::invalid_argument);
  EXPECT_EQ(f.a.calls, 0u);
  f.l = limits();
  const auto calls = f.a.calls;
  EXPECT_EQ(f.encode(SIZE_MAX).error, Code::limit_exceeded);
  EXPECT_EQ(f.a.calls, calls);
}
TEST(CompactOwner, DrainedStatePreservedOnFailure) {
  OwnerFixture f;
  ASSERT_EQ(f.encode().error, Code::none);
  f.owner.acknowledge_drained();
  EXPECT_FALSE(f.owner.pending());
  f.a.fail_at = f.a.calls + 3;
  f.failure(Code::out_of_memory);
}
TEST(CompactOwner, CallerAndPriorOwnerOverlapRefused) {
  OwnerFixture f;
  ASSERT_EQ(f.encode().error, Code::none);
  const TypedContextFrameValidationContext c{f.s, f.l, 0, 0};
  const auto before = std::vector<std::byte>(f.owner.publication().begin(),
                                             f.owner.publication().end());
  EXPECT_EQ(f.owner.encode(f.owner.publication(), c, f.index).error,
            Code::invalid_argument);
  EXPECT_TRUE(std::ranges::equal(f.owner.publication(), before));
}
TEST(OwnedStream, EmptyAndSingleByteAllSchedules) {
  for (unsigned v = 0; v < 256; ++v) {
    std::array raw{std::byte{static_cast<unsigned char>(v)}};
    EXPECT_EQ(encode_stream(raw, 32, 1, 1), expected(raw, 32));
  }
  EXPECT_EQ(encode_stream({}, 32, 1, 1), expected({}, 32));
}
TEST(OwnedStream, ChunkSplitsFlushAndFrameBoundaries) {
  for (std::size_t n : {31, 32, 33, 64, 65}) {
    std::vector<std::byte> raw(n);
    for (std::size_t i = 0; i < n; ++i)
      raw[i] = std::byte{static_cast<unsigned char>(i % 7)};
    const auto wire = expected(raw, 32);
    for (std::size_t in : {1, 7, 31, 64})
      for (std::size_t out : {1, 17, 112, 257})
        EXPECT_EQ(encode_stream(raw, 32, in, out, true), wire);
  }
}
TEST(OwnedStream, ConstructorAllocationFailuresDoNotPublish) {
  for (unsigned i = 1; i <= 2; ++i) {
    Allocator a;
    a.fail_at = i;
    std::array<std::byte, 256> output{};
    output.fill(guard);
    {
      LzssPositionDistance64mOwnedStreamEncoder e(stream(1), limits(), a);
      auto r = e.process({}, output, 0);
      EXPECT_EQ(r.status, Status::error);
      EXPECT_EQ(r.error.code, Code::out_of_memory);
      EXPECT_EQ(r.input_consumed, 0u);
      EXPECT_EQ(r.output_produced, 0u);
      EXPECT_TRUE(
          std::ranges::all_of(output, [](auto b) { return b == guard; }));
    }
    EXPECT_EQ(a.live, 0u);
  }
}
TEST(OwnedStream, FailedSecondFrameNeverDrains) {
  Allocator a;
  a.fail_at = 7;
  std::vector<std::byte> input(33, std::byte{65}), out(4096, guard);
  const auto first = expected(std::span(input).first(32), 32);
  auto s = stream(33);
  LzssPositionDistance64mOwnedStreamEncoder e(s, limits(), a);
  auto r =
      e.process(input, out, core::flag_value(core::ProcessFlags::end_input));
  EXPECT_EQ(r.status, Status::error);
  EXPECT_EQ(r.error.code, Code::out_of_memory);
  EXPECT_EQ(r.output_produced, first.size());
  EXPECT_EQ(r.input_consumed, 33u);
  EXPECT_TRUE(std::equal(first.begin() + 112, first.end(), out.begin() + 112));
  EXPECT_TRUE(std::ranges::all_of(std::span(out).subspan(r.output_produced),
                                  [](auto b) { return b == guard; }));
  auto next = e.process({}, out, 0);
  EXPECT_EQ(next.status, Status::error);
  EXPECT_EQ(next.input_consumed, 0u);
  EXPECT_EQ(next.output_produced, 0u);
}
TEST(OwnedStream, TruncationTrailingInputAndUnsupportedFlags) {
  for (unsigned which = 0; which < 3; ++which) {
    Allocator a;
    LzssPositionDistance64mOwnedStreamEncoder e(stream(2), limits(), a);
    std::array raw{std::byte{65}, std::byte{66}, std::byte{67}};
    std::array<std::byte, 512> out{};
    const auto r =
        e.process(std::span(raw).first(which == 0 ? 1 : 3), out,
                  which == 2 ? core::flag_value(core::ProcessFlags::reset_block)
                             : core::flag_value(core::ProcessFlags::end_input));
    EXPECT_EQ(r.status, Status::error);
    EXPECT_EQ(r.error.code,
              which == 2 ? Code::unsupported : Code::malformed_stream);
  }
}
TEST(OwnedStream, ZeroOutputAndEndInputSuffix) {
  std::vector<std::byte> raw(65, std::byte{65});
  EXPECT_EQ(encode_stream(raw, 32, 65, 1), expected(raw, 32));
  Allocator a;
  LzssPositionDistance64mOwnedStreamEncoder e(stream(1), limits(), a);
  std::array input{std::byte{65}};
  auto r =
      e.process(input, {}, core::flag_value(core::ProcessFlags::end_input));
  EXPECT_EQ(r.status, Status::need_output);
  EXPECT_EQ(r.input_consumed, 0u);
  EXPECT_EQ(r.output_produced, 0u);
}
TEST(OwnedStream, FullCallAndConfigurationAliases) {
  Allocator a;
  auto s = stream(1);
  auto l = limits();
  LzssPositionDistance64mOwnedStreamEncoder e(s, l, a);
  std::array<std::byte, 256> out{};
  const auto before = out;
  auto r = e.process(std::span(out).first(1), out, 0);
  EXPECT_EQ(r.status, Status::error);
  EXPECT_EQ(r.error.code, Code::invalid_argument);
  EXPECT_EQ(out, before);
}
TEST(CompactOwner, SizeAndPayloadPoliciesReportLimitBeforePublication) {
  OwnerFixture f;
  ASSERT_EQ(f.encode().error, Code::none);
  f.l.max_block_size = 16;
  f.failure(Code::limit_exceeded);
  f.l = limits();
  f.l.max_frame_size = 16;
  f.failure(Code::limit_exceeded);
  f.l = limits();
  f.l.max_compressed_payload_size = 5;
  f.failure(Code::limit_exceeded);
}
TEST(CompactOwner, EveryFirstCandidateRefusalPublishesNothing) {
  for (unsigned i = 1; i <= 4; ++i) {
    OwnerFixture f;
    f.a.fail_at = i;
    f.failure(Code::out_of_memory);
    EXPECT_TRUE(f.owner.publication().empty());
    EXPECT_FALSE(f.owner.pending());
    EXPECT_EQ(f.owner.publication_capacity(), 0u);
    EXPECT_EQ(f.a.live, 0u);
  }
}
TEST(OwnedStream, EverySecondFrameCandidateRefusalPublishesOnlyFirst) {
  for (unsigned i = 1; i <= 4; ++i) {
    Allocator a;
    // Two constructor allocations, four first-frame allocations, then i.
    a.fail_at = 6 + i;
    std::vector<std::byte> input(64, std::byte{65}), out(4096, guard);
    const auto first = expected(std::span(input).first(32), 32);
    {
      LzssPositionDistance64mOwnedStreamEncoder e(stream(64), limits(), a);
      const auto r = e.process(input, out,
          core::flag_value(core::ProcessFlags::end_input));
      EXPECT_EQ(r.status, Status::error);
      EXPECT_EQ(r.error.code, Code::out_of_memory);
      EXPECT_EQ(r.input_consumed, input.size());
      ASSERT_EQ(r.output_produced, first.size());
      EXPECT_TRUE(std::equal(first.begin() + 112, first.end(), out.begin() + 112));
      EXPECT_TRUE(std::ranges::all_of(std::span(out).subspan(r.output_produced),
                                      [](auto b) { return b == guard; }));
      const auto next = e.process(input, out, 0);
      EXPECT_EQ(next.status, Status::error);
      EXPECT_EQ(next.error.code, r.error.code);
      EXPECT_EQ(next.input_consumed, 0u);
      EXPECT_EQ(next.output_produced, 0u);
    }
    EXPECT_EQ(a.live, 0u);
  }
}
TEST(OwnedStream, WaitsForExplicitEndAfterAllKnownBytes) {
  Allocator a;
  std::array raw{std::byte{65}};
  std::array<std::byte, 512> output{};
  LzssPositionDistance64mOwnedStreamEncoder e(stream(1), limits(), a);
  const auto r = e.process(raw, output, 0);
  ASSERT_NE(r.status, Status::error);
  EXPECT_NE(r.status, Status::end_of_stream);
  const auto idle = e.process({}, output, 0);
  EXPECT_EQ(idle.status, Status::need_input);
  EXPECT_EQ(idle.input_consumed, 0u);
  EXPECT_EQ(idle.output_produced, 0u);
  EXPECT_EQ(e.process({}, output,
      core::flag_value(core::ProcessFlags::end_input)).status,
      Status::end_of_stream);
}
TEST(OwnedStream, BinaryEveryInputSplitAndOutputOneByte) {
  std::array<std::byte, 19> raw{};
  for (std::size_t i = 0; i < raw.size(); ++i)
    raw[i] = std::byte(static_cast<unsigned char>(i * 79));
  const auto wire = expected(raw, 7);
  for (std::size_t in = 1; in <= raw.size(); ++in)
    EXPECT_EQ(encode_stream(raw, 7, in, 1, true), wire);
}
} // namespace
