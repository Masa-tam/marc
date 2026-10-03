#include "frame/lzss_position_distance_8m_frame_decoder.hpp"
#include "frame/lzss_position_distance_8m_frame_encoder.hpp"
#include "frame/lzss_position_distance_8m_prepared_storage_owner.hpp"
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
  LzssPositionDistance8mPreparedStorageOwner owner{allocator};
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
    return sizeof(Fixture) + 1024 + raw.capacity() - raw.size() +
           4 * (index.capacity() - index.size());
  }
  auto run(std::size_t retained = 0) {
    const TypedContextFrameValidationContext c{stream, limits, 0, 0};
    return owner.encode(raw, c, index, extra() + retained);
  }
};
TEST(PositionDistance8mPreparedStorageOwner, ExactAllocatorTypedLifetimes) {
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
TEST(PositionDistance8mPreparedStorageOwner,
     CompletePrivateFrameAndPendingGuard) {
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
TEST(PositionDistance8mPreparedStorageOwner,
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
TEST(PositionDistance8mPreparedStorageOwner,
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
TEST(PositionDistance8mPreparedStorageOwner,
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
TEST(PositionDistance8mPreparedStorageOwner,
     EachUndercapacityCannotReachHelpers) {
  for (std::size_t at = 1; at <= 5; ++at) {
    Fixture f;
    f.allocator.under = at;
    EXPECT_EQ(f.run().error, Code::limit_exceeded);
    EXPECT_EQ(f.allocator.calls, at);
    EXPECT_EQ(f.allocator.live_blocks, 0u);
  }
}
TEST(PositionDistance8mPreparedStorageOwner,
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
TEST(PositionDistance8mPreparedStorageOwner, DestructorReleasesEveryLiveBlock) {
  Allocator a;
  {
    LzssPositionDistance8mPreparedStorageOwner owner(a);
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
TEST(PositionDistance8mPreparedStorageOwner,
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
TEST(PositionDistance8mPreparedStorageOwner,
     PolicyAndOverflowRejectBeforeAllocator) {
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
TEST(PositionDistance8mPreparedStorageOwner,
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
TEST(PositionDistance8mPreparedStorageOwner, CurrentPublicationAliasRejected) {
  Fixture f;
  ASSERT_EQ(f.run().error, Code::none);
  f.owner.acknowledge_drained();
  const auto calls = f.allocator.calls;
  const TypedContextFrameValidationContext c{f.stream, f.limits, 0, 0};
  EXPECT_EQ(f.owner.encode(f.owner.publication(), c, f.index, f.extra()).error,
            Code::invalid_argument);
  EXPECT_EQ(f.allocator.calls, calls);
}
TEST(PositionDistance8mPreparedStorageOwner,
     RealLargeFramesDefaultAllocatorAndDecode) {
  for (auto n : {1048576u, 8388608u}) {
    LzssPositionDistance8mExactAllocator allocator;
    LzssPositionDistance8mPreparedStorageOwner owner(allocator);
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
            << LzssPositionDistance8mPreparedStorageOwner::working_bytes()
            << '\n';
}
struct DifferentialControls {
  TypedContextStreamHeader stream;
  core::DecoderLimits limits;
  TypedContextFrameLayout layout;
  LzssPositionDistance8mOwnerResult owner_result;
  LzssPositionDistance8mFrameEncodeResult encode_result;
  LzssPositionDistance8mFrameDecodeResult decode_result;
  LzssPositionDistance8mFrameEncodeWorkspace workspace;
  std::array<std::vector<std::byte>, 6> byte_owners;
  std::array<std::vector<dictionary::internal::LzssTypedToken>, 2> token_owners;
  std::array<std::vector<context::internal::ModeledOperation>, 2> op_owners;
  std::vector<uint32_t> index;
  std::array<std::size_t, 64> scalars;
};
// Four real scopes. The full input and expected result survive all phases.
// Concrete controls are conservatively retained independently of helper locals.
void differential(std::size_t n, unsigned pattern, bool literals = false,
                  bool subsequent_tail = false, unsigned single_byte = 256) {
  ASSERT_LE(n, 8388608u);
  constexpr std::size_t ceiling = 1024u << 20;
  const auto controls = sizeof(DifferentialControls) + sizeof(Allocator) + 256 +
                        sizeof(LzssPositionDistance8mStorageOwner) +
                        sizeof(LzssPositionDistance8mPreparedStorageOwner);
  ASSERT_LT(controls + n + 4 * (65536 + n) +
                LzssPositionDistance8mPreparedStorageOwner::working_bytes(),
            ceiling);
  std::vector<std::byte> input(n), expected, result;
  core::DecoderLimits limits{};
  limits.max_block_size = 8388608;
  limits.max_internal_buffered_bytes = ceiling;
  TypedContextStreamHeader stream{};
  stream.frame_size = subsequent_tail ? 8388608 : static_cast<uint32_t>(n);
  stream.original_size = subsequent_tail ? 8388608 + n : n;
  stream.dictionary = {8388608, 3, literals ? 3u : 258u, 0};
  stream.range_model_total = 32768;
  stream.context_count = 47;
  stream.dictionary_variant = 11;
  stream.context_algorithm = 1;
  stream.context_variant = 12;
  const TypedContextFrameValidationContext c{stream, limits,
                                             subsequent_tail ? 1u : 0u,
                                             subsequent_tail ? 8388608u : 0u};
  uint32_t rng = 1448;
  for (std::size_t i = 0; i < n; ++i) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    if (pattern == 0)
      input[i] = std::byte(i % 256);
    else if (pattern == 1)
      input[i] = std::byte(rng & 255);
    else if (pattern == 2)
      input[i] = i < 1048593 ? std::byte(rng & 255) : input[i % 1048593];
    else
      input[i] = std::byte((i / 65536) % 3 == 0 ? (rng & 255) : i % 7);
  }
  if (n == 1 && single_byte < 256)
    input[0] = std::byte(single_byte);
  std::size_t t{}, e{}, p{};
  { // Safe owner oracle, independently admitted and really destroyed.
    Allocator a;
    std::vector<uint32_t> index(65536 + n);
    LzssPositionDistance8mStorageOwner owner(a);
    auto r = owner.encode(input, c, index,
                          controls + input.capacity() - input.size() +
                              4 * (index.capacity() - index.size()));
    ASSERT_EQ(r.error, Code::none);
    t = owner.layout().header.token_count;
    e = owner.layout().header.event_count;
    p = owner.layout().header.payload_size;
    ASSERT_LE(r.aggregate_bytes + owner.publication().size(), ceiling);
    expected.resize(owner.publication().size());
    std::copy(owner.publication().begin(), owner.publication().end(),
              expected.begin());
  }
  ASSERT_LE(t, n);
  ASSERT_LE(e, 2 * n);
  ASSERT_LE(p, limits.max_compressed_payload_size);
  const auto outer = controls + input.capacity() + expected.capacity();
  { // Unchanged operation frame, all prospective capacities admitted first.
    const auto work = lzss_position_distance_8m_frame_encode_working_bytes();
    const auto proposed =
        outer + 24 * t + 32 * e + 4 * (65536 + n) + 2 * (80 + p) + p + work;
    ASSERT_LE(proposed, ceiling);
    std::vector<dictionary::internal::LzssTypedToken> tokens(t), ts(t);
    std::vector<context::internal::ModeledOperation> ops(e), os(e);
    std::vector<uint32_t> index(65536 + n);
    std::vector<std::byte> frame(80 + p), payload(p), wire(80 + p);
    const auto total = outer +
                       (tokens.capacity() + ts.capacity()) * sizeof(tokens[0]) +
                       (ops.capacity() + os.capacity()) * sizeof(ops[0]) +
                       4 * index.capacity() + frame.capacity() +
                       payload.capacity() + wire.capacity() + work;
    ASSERT_LE(total, ceiling);
    const auto local =
        input.size() + (tokens.size() + ts.size()) * sizeof(tokens[0]) +
        (ops.size() + os.size()) * sizeof(ops[0]) + 4 * index.size() +
        frame.size() + payload.size() + wire.size() + work;
    ASSERT_LE(local, total);
    LzssPositionDistance8mFrameEncodeWorkspace w{tokens, ts,    index,  ops,
                                                 os,     frame, payload};
    TypedContextFrameLayout layout{};
    std::size_t written{};
    auto r = encode_lzss_position_distance_8m_frame(input, c, w, wire, layout,
                                                    written, total - local);
    ASSERT_EQ(r.error, LzssPositionDistance8mFrameEncodeError::none);
    EXPECT_EQ(r.aggregate_bytes, total);
    EXPECT_EQ(written, expected.size());
    EXPECT_EQ(wire, expected);
  }
  { // New owner, no safe/reference codec owners survive.
    Allocator a;
    const auto prospective =
        outer + 4 * (65536 + n) + 24 * t + 3 * p + 160 +
        LzssPositionDistance8mPreparedStorageOwner::working_bytes();
    ASSERT_LE(prospective + expected.size(), ceiling);
    std::vector<uint32_t> index(65536 + n);
    LzssPositionDistance8mPreparedStorageOwner owner(a);
    auto r = owner.encode(input, c, index,
                          outer - input.size() +
                              4 * (index.capacity() - index.size()));
    ASSERT_EQ(r.error, Code::none);
    ASSERT_LE(r.aggregate_bytes + expected.size(), ceiling);
    result.resize(expected.size());
    std::copy(owner.publication().begin(), owner.publication().end(),
              result.begin());
    EXPECT_EQ(result, expected);
  }
  { // Unchanged consumer, retaining all complete external result owners.
    const auto helper =
        sizeof(LzssPositionDistance8mFrameDecodePlan) +
        sizeof(LzssPositionDistance8mFrameDecodeResult) +
        context::internal::lzss_position_distance_8m_token_working_bytes();
    ASSERT_LE(outer + result.capacity() + 24 * t + 2 * n + helper, ceiling);
    std::vector<dictionary::internal::LzssTypedToken> tokens(t), ts(t);
    std::vector<std::byte> raw(n), scratch(n);
    const auto total = outer + result.capacity() +
                       (tokens.capacity() + ts.capacity()) * sizeof(tokens[0]) +
                       raw.capacity() + scratch.capacity() + helper;
    const auto local = result.size() +
                       (tokens.size() + ts.size()) * sizeof(tokens[0]) +
                       raw.size() + scratch.size() + helper;
    ASSERT_LE(total, ceiling);
    ASSERT_LE(local, total);
    TypedContextFrameLayout layout{};
    auto r = decode_lzss_position_distance_8m_frame(
        result, c, tokens, ts, raw, scratch, layout, total - local);
    ASSERT_EQ(r.error, LzssPositionDistance8mFrameDecodeError::none);
    EXPECT_EQ(raw, input);
  }
}
TEST(PositionDistance8mPreparedStorageOwner, SeparateFourOraclesAndEveryByte) {
  for (unsigned b = 0; b < 256; ++b) {
    differential(1, 1, false, false, b);
  }
  for (auto n : {1u, 5u, 257u, 4097u})
    for (unsigned p = 0; p < 4; ++p)
      differential(n, p);
  differential(32, 0, false, true);
  differential(257, 1, true);
}
TEST(PositionDistance8mPreparedStorageOwner, LargeSeparateScopedOracles) {
  for (auto n : {1048576u, 8388608u}) {
    differential(n, 0);
    differential(n, 3);
    differential(n, 1, true);
  }
  differential(8388608, 2);
}
TEST(PositionDistance8mPreparedStorageOwner,
     EmptyPositionAndOriginalAliasRefusal) {
  Fixture f;
  const TypedContextFrameValidationContext c{f.stream, f.limits, 0, 0};
  EXPECT_EQ(f.owner.encode({}, c, f.index, f.extra()).error,
            Code::invalid_argument);
  const TypedContextFrameValidationContext wrong{f.stream, f.limits, 1, 0};
  EXPECT_EQ(f.owner.encode(f.raw, wrong, f.index, f.extra()).error,
            Code::invalid_argument);
  auto alias = std::span<const std::byte>(
      reinterpret_cast<const std::byte *>(&f.stream), 32);
  EXPECT_EQ(f.owner.encode(alias, c, f.index, f.extra()).error,
            Code::invalid_argument);
  EXPECT_EQ(f.allocator.calls, 0u);
}
} // namespace
