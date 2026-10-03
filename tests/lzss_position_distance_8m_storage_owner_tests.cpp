#include "frame/lzss_position_distance_8m_frame_decoder.hpp"
#include "frame/lzss_position_distance_8m_storage_owner.hpp"
#include "lzss_position_distance_8m_frame_encode_vectors.hpp"
#include "lzss_position_distance_8m_frame_vectors.hpp"
#include "lzss_position_distance_8m_token_vectors.hpp"
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <iostream>
#include <limits>
#include <vector>
namespace {
using namespace marc;
using namespace frame::internal;
using Code = core::ErrorCode;
struct Allocator final : LzssPositionDistance8mBlockAllocator {
  LzssPositionDistance8mExactAllocator exact;
  struct Record {
    void *ptr{};
    std::size_t bytes{}, id{};
    bool live{};
  };
  std::array<Record, 64> records{};
  std::size_t calls{}, fail{}, over{}, under{}, live_bytes{}, live_blocks{},
      peak{}, deletes{}, next{};
  bool deletion_before_clear{true};
  LzssPositionDistance8mAllocatorControls controls() const noexcept override {
    return {this, sizeof(*this), 256};
  }
  template <class T>
  void record(LzssPositionDistance8mOwnedBlock<T> b) noexcept {
    if (!b.data)
      return;
    records[next++] = {b.data, b.capacity * sizeof(T), calls, true};
    live_bytes += b.capacity * sizeof(T);
    ++live_blocks;
    peak = std::max(peak, live_bytes);
  }
  std::size_t size(std::size_t n) noexcept {
    return calls == over ? n + 1 : calls == under ? n - 1 : n;
  }
  LzssPositionDistance8mOwnedTokens tokens(std::size_t n) noexcept override {
    ++calls;
    if (calls == fail || next == records.size())
      return {};
    auto b = exact.tokens(size(n));
    record(b);
    return b;
  }
  LzssPositionDistance8mOwnedBytes bytes(std::size_t n) noexcept override {
    ++calls;
    if (calls == fail || next == records.size())
      return {};
    auto b = exact.bytes(size(n));
    record(b);
    return b;
  }
  template <class T>
  void drop(LzssPositionDistance8mOwnedBlock<T> &b) noexcept {
    if (!b.data) {
      b = {};
      return;
    }
    const auto ptr = b.data;
    const auto count = b.capacity;
    // This is a real delete[], followed by the accounting/lifetime event.
    delete[] b.data;
    deletion_before_clear =
        deletion_before_clear && b.data == ptr && b.capacity == count;
    for (auto &r : records)
      if (r.ptr == ptr && r.live) {
        r.live = false;
        live_bytes -= r.bytes;
        --live_blocks;
        ++deletes;
        break;
      }
    b = {};
  }
  void release(LzssPositionDistance8mOwnedTokens &b) noexcept override {
    drop(b);
  }
  void release(LzssPositionDistance8mOwnedBytes &b) noexcept override {
    drop(b);
  }
};
struct Fixture {
  Allocator allocator;
  LzssPositionDistance8mStorageOwner owner{allocator};
  std::vector<std::byte> raw;
  std::vector<uint32_t> index;
  core::DecoderLimits limits{};
  TypedContextStreamHeader stream{};
  explicit Fixture(std::size_t n = 32)
      : raw(n, std::byte{65}), index(65536 + n) {
    limits.max_block_size = 8388608;
    limits.max_internal_buffered_bytes = 512u << 20;
    stream.frame_size = static_cast<uint32_t>(n);
    stream.original_size = n;
    stream.dictionary = {8388608, 3, 258, 0};
    stream.range_model_total = 32768;
    stream.context_count = 47;
    stream.dictionary_variant = 11;
    stream.context_algorithm = 1;
    stream.context_variant = 12;
  }
  std::size_t extra() const {
    return sizeof(raw) + sizeof(index) + raw.capacity() - raw.size() +
           4 * (index.capacity() - index.size());
  }
  auto run(std::size_t retained = 0) {
    const TypedContextFrameValidationContext c{stream, limits, 0, 0};
    return owner.encode(raw, c, index, extra() + retained);
  }
};
TEST(PositionDistance8mStorageOwner, ExactAllocatorTypedLifetimes) {
  LzssPositionDistance8mExactAllocator a;
  auto t = a.tokens(3);
  auto b = a.bytes(5);
  ASSERT_NE(t.data, nullptr);
  ASSERT_NE(b.data, nullptr);
  EXPECT_EQ(t.capacity, 3u);
  EXPECT_EQ(b.capacity, 5u);
  EXPECT_EQ(t.data[2].distance, 0u);
  EXPECT_EQ(b.data[4], std::byte{});
  a.release(t);
  a.release(b);
  EXPECT_EQ(t.data, nullptr);
  EXPECT_EQ(b.capacity, 0u);
}
TEST(PositionDistance8mStorageOwner, CompletePrivateFrameAndPendingGuard) {
  Fixture f;
  auto r = f.run();
  ASSERT_EQ(r.error, Code::none);
  EXPECT_EQ(f.allocator.calls, 5u);
  EXPECT_EQ(f.allocator.live_blocks, 5u);
  EXPECT_TRUE(f.owner.pending());
  EXPECT_EQ(r.bytes_validated, f.owner.publication().size());
  std::vector<std::byte> saved(f.owner.publication().begin(),
                               f.owner.publication().end());
  EXPECT_EQ(f.run().error, Code::invalid_argument);
  EXPECT_EQ(f.allocator.calls, 5u);
  EXPECT_TRUE(
      std::equal(saved.begin(), saved.end(), f.owner.publication().begin()));
}
TEST(PositionDistance8mStorageOwner,
     RealFailureAtEachFreshAllocationRollsBack) {
  for (std::size_t at = 1; at <= 5; ++at) {
    Fixture f;
    f.allocator.fail = at;
    EXPECT_EQ(f.run().error, Code::out_of_memory);
    EXPECT_EQ(f.allocator.calls, at);
    EXPECT_EQ(f.allocator.live_blocks, 0u);
    EXPECT_EQ(f.allocator.deletes, at - 1);
    EXPECT_TRUE(f.owner.publication().empty());
    EXPECT_FALSE(f.owner.pending());
    EXPECT_TRUE(f.allocator.deletion_before_clear);
  }
}
TEST(PositionDistance8mStorageOwner,
     EachReplacementFaultPreservesOldPublicationAndOwners) {
  for (std::size_t at = 1; at <= 5; ++at) {
    Fixture f;
    ASSERT_EQ(f.run().error, Code::none);
    f.owner.acknowledge_drained();
    auto layout = f.owner.layout();
    std::vector<std::byte> saved(f.owner.publication().begin(),
                                 f.owner.publication().end());
    const auto oldbytes = f.allocator.live_bytes;
    f.allocator.fail = f.allocator.calls + at;
    auto r = f.run(saved.capacity());
    EXPECT_EQ(r.error, Code::out_of_memory);
    EXPECT_EQ(f.allocator.live_bytes, oldbytes);
    EXPECT_EQ(f.allocator.live_blocks, 5u);
    EXPECT_EQ(f.allocator.deletes, at - 1);
    EXPECT_FALSE(f.owner.pending());
    EXPECT_EQ(std::memcmp(&layout, &f.owner.layout(), sizeof(layout)), 0);
    EXPECT_TRUE(
        std::equal(saved.begin(), saved.end(), f.owner.publication().begin()));
    for (std::size_t i = 0; i < 5; ++i)
      EXPECT_TRUE(f.allocator.records[i].live);
  }
}
TEST(PositionDistance8mStorageOwner,
     EachOvercapacityIsRejectedAndActuallyFreed) {
  for (std::size_t at = 1; at <= 5; ++at) {
    Fixture f;
    f.allocator.over = at;
    EXPECT_EQ(f.run().error, Code::limit_exceeded);
    EXPECT_EQ(f.allocator.calls, at);
    EXPECT_EQ(f.allocator.live_blocks, 0u);
    EXPECT_EQ(f.allocator.deletes, at);
  }
}
TEST(PositionDistance8mStorageOwner, EachUndercapacityCannotReachHelpers) {
  for (std::size_t at = 1; at <= 5; ++at) {
    Fixture f;
    f.allocator.under = at;
    EXPECT_EQ(f.run().error, Code::limit_exceeded);
    EXPECT_EQ(f.allocator.calls, at);
    EXPECT_EQ(f.allocator.live_blocks, 0u);
  }
}
TEST(PositionDistance8mStorageOwner,
     SuccessfulReplacementHoldsBothUntilOldDestruction) {
  Fixture f;
  ASSERT_EQ(f.run().error, Code::none);
  const auto old = f.allocator.live_bytes;
  f.owner.acknowledge_drained();
  auto r = f.run();
  ASSERT_EQ(r.error, Code::none);
  EXPECT_EQ(f.allocator.peak, 2 * old);
  EXPECT_EQ(f.allocator.live_bytes, old);
  EXPECT_EQ(f.allocator.deletes, 5u);
  EXPECT_TRUE(f.allocator.deletion_before_clear);
  for (std::size_t i = 0; i < 5; ++i)
    EXPECT_FALSE(f.allocator.records[i].live);
  for (std::size_t i = 5; i < 10; ++i)
    EXPECT_TRUE(f.allocator.records[i].live);
}
TEST(PositionDistance8mStorageOwner, DestructorReleasesEveryLiveBlock) {
  Allocator a;
  {
    LzssPositionDistance8mStorageOwner owner(a);
    Fixture f;
    const TypedContextFrameValidationContext c{f.stream, f.limits, 0, 0};
    ASSERT_EQ(owner.encode(f.raw, c, f.index, sizeof(Fixture)).error,
              Code::none);
    EXPECT_EQ(a.live_blocks, 5u);
  }
  EXPECT_EQ(a.live_blocks, 0u);
  EXPECT_EQ(a.deletes, 5u);
  EXPECT_TRUE(a.deletion_before_clear);
}
TEST(PositionDistance8mStorageOwner,
     ExactReplacementBudgetAndOneBelowRollsBackPartial) {
  Fixture f;
  ASSERT_EQ(f.run().error, Code::none);
  f.owner.acknowledge_drained();
  const auto peak = f.run().aggregate_bytes;
  f.owner.acknowledge_drained();
  f.limits.max_block_size = 32;
  f.limits.max_internal_buffered_bytes = peak;
  ASSERT_EQ(f.run().error, Code::none);
  f.owner.acknowledge_drained();
  --f.limits.max_internal_buffered_bytes;
  const auto calls = f.allocator.calls;
  EXPECT_EQ(f.run().error, Code::limit_exceeded);
  // Token count/pair may fit; complete frame demand rejects before byte
  // requests.
  EXPECT_LE(f.allocator.calls - calls, 2u);
  EXPECT_EQ(f.allocator.live_blocks, 5u);
}
TEST(PositionDistance8mStorageOwner, PolicyAndOverflowRejectBeforeAllocator) {
  Fixture f;
  f.stream.dictionary_variant = 10;
  EXPECT_EQ(f.run().error, Code::invalid_argument);
  EXPECT_EQ(f.allocator.calls, 0u);
  f.stream.dictionary_variant = 11;
  const TypedContextFrameValidationContext c{f.stream, f.limits, 0, 0};
  EXPECT_EQ(
      f.owner.encode(f.raw, c, f.index, std::numeric_limits<std::size_t>::max())
          .error,
      Code::limit_exceeded);
  EXPECT_EQ(f.allocator.calls, 0u);
  f.limits.max_block_size = 1;
  f.limits.max_internal_buffered_bytes = 1;
  EXPECT_EQ(f.run().error, Code::limit_exceeded);
  EXPECT_EQ(f.allocator.calls, 0u);
}
TEST(PositionDistance8mStorageOwner,
     FiveIndependentCompletePublicationVectors) {
  auto check = [](const auto &raw, const auto &wire) {
    Fixture f(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i)
      f.raw[i] = std::byte(raw[i]);
    ASSERT_EQ(f.run().error, Code::none);
    ASSERT_EQ(f.owner.publication().size(), wire.size());
    for (std::size_t i = 0; i < wire.size(); ++i)
      EXPECT_EQ(f.owner.publication()[i], std::byte(wire[i]));
  };
  using namespace frame_encode_vectors;
  check(repeat_raw, repeat_frame);
  check(pattern_raw, pattern_frame);
  check(tie_raw, tie_frame);
  check(binary_raw, binary_frame);
  std::array<uint8_t, 86> wire{};
  std::copy(frame_vectors::literal_prefix.begin(),
            frame_vectors::literal_prefix.end(), wire.begin());
  std::copy(token_vectors::literal.begin(), token_vectors::literal.end(),
            wire.begin() + 80);
  check(std::array<uint8_t, 1>{65}, wire);
}
TEST(PositionDistance8mStorageOwner, CurrentPublicationAliasRejected) {
  Fixture f;
  ASSERT_EQ(f.run().error, Code::none);
  f.owner.acknowledge_drained();
  const auto calls = f.allocator.calls;
  const TypedContextFrameValidationContext c{f.stream, f.limits, 0, 0};
  EXPECT_EQ(f.owner.encode(f.owner.publication(), c, f.index, f.extra()).error,
            Code::invalid_argument);
  EXPECT_EQ(f.allocator.calls, calls);
}
TEST(PositionDistance8mStorageOwner, RealLargeFramesDefaultAllocatorAndDecode) {
  for (auto n : {1048576u, 8388608u}) {
    LzssPositionDistance8mExactAllocator allocator;
    LzssPositionDistance8mStorageOwner owner(allocator);
    Fixture f(n);
    for (std::size_t i = 0; i < n; ++i)
      f.raw[i] = std::byte(i % 256);
    const TypedContextFrameValidationContext c{f.stream, f.limits, 0, 0};
    const auto external = sizeof(Fixture);
    auto r = owner.encode(f.raw, c, f.index, external);
    ASSERT_EQ(r.error, Code::none);
    const auto t = owner.layout().header.token_count;
    std::vector<dictionary::internal::LzssTypedToken> tokens(t), ts(t);
    std::vector<std::byte> raw(n), scratch(n);
    TypedContextFrameLayout layout{};
    // Charge the actual live owner blocks and both owner/control objects in
    // addition to decode consumers. r includes the encode helper reservation.
    const auto helper =
        sizeof(LzssPositionDistance8mFrameDecodePlan) +
        sizeof(LzssPositionDistance8mFrameDecodeResult) +
        context::internal::lzss_position_distance_8m_token_working_bytes();
    const auto total =
        r.aggregate_bytes + tokens.capacity() * sizeof(tokens[0]) +
        ts.capacity() * sizeof(ts[0]) + raw.capacity() + scratch.capacity() +
        sizeof(tokens) + sizeof(ts) + sizeof(raw) + sizeof(scratch) +
        sizeof(layout) + sizeof(r) + helper;
    const auto local = owner.publication().size() +
                       (tokens.size() + ts.size()) * sizeof(tokens[0]) +
                       raw.size() + scratch.size() + helper;
    ASSERT_LE(local, total);
    auto d = decode_lzss_position_distance_8m_frame(owner.publication(), c,
                                                    tokens, ts, raw, scratch,
                                                    layout, total - local);
    ASSERT_EQ(d.error, LzssPositionDistance8mFrameDecodeError::none);
    EXPECT_EQ(raw, f.raw);
    std::cout << "storage_owner F=" << n << " T=" << t
              << " P=" << owner.layout().header.payload_size
              << " encode=" << r.aggregate_bytes << '\n';
  }
  std::cout << "owner_working="
            << LzssPositionDistance8mStorageOwner::working_bytes() << '\n';
}
} // namespace
