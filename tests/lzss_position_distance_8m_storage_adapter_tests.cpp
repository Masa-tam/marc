#include "frame/lzss_position_distance_8m_frame_decoder.hpp"
#include "frame/lzss_position_distance_8m_storage_adapter.hpp"
#include "lzss_position_distance_8m_frame_encode_vectors.hpp"
#include "lzss_position_distance_8m_frame_vectors.hpp"
#include "lzss_position_distance_8m_token_vectors.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <iostream>
#include <limits>
#include <vector>
namespace {
using namespace marc;
using namespace frame::internal;
using E = LzssPositionDistance8mStorageError;
using Token = dictionary::internal::LzssTypedToken;
using Ledger = LzssPositionDistance8mStorageLedger;
using Admission = LzssPositionDistance8mStorageAdmission;
using Gen = LzssPositionDistance8mGenerationCapacities;
constexpr auto max = std::numeric_limits<std::size_t>::max();
constexpr auto guard = std::byte{0xa5};
template <class T> auto snapshot(const T &t) {
  std::array<std::byte, sizeof(T)> a{};
  std::memcpy(a.data(), &t, sizeof(T));
  return a;
}
template <class T> void unchanged(const T &t, const auto &a) {
  EXPECT_EQ(std::memcmp(&t, a.data(), sizeof(T)), 0);
}
struct Fixture {
  core::DecoderLimits limits{};
  TypedContextStreamHeader stream{};
  std::vector<std::byte> raw;
  std::vector<Token> tokens, scratch;
  std::vector<uint32_t> index;
  LzssPositionDistance8mTokenStorageDemand td{};
  LzssPositionDistance8mFrameStorageDemand fd{};
  explicit Fixture(std::size_t n = 32)
      : raw(n, std::byte{65}), tokens(n + 3), scratch(n + 7),
        index(65536 + n + 11) {
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
    td.tokens = 97;
    fd.frame_bytes = 123;
    fd.layout.header.sequence = 87;
  }
  auto token(std::size_t retained = 0, uint64_t seq = 0, uint64_t prior = 0) {
    const TypedContextFrameValidationContext c{stream, limits, seq, prior};
    return prepare_lzss_position_distance_8m_token_storage(raw, c, index, td,
                                                           retained);
  }
  auto frame(std::size_t retained = 0, uint64_t seq = 0, uint64_t prior = 0) {
    const TypedContextFrameValidationContext c{stream, limits, seq, prior};
    return prepare_lzss_position_distance_8m_frame_storage(
        raw, c, tokens, scratch, index, fd, retained);
  }
  std::size_t current() const {
    return raw.size() + sizeof(Token) * (tokens.size() + scratch.size()) +
           sizeof(uint32_t) * index.size() +
           lzss_position_distance_8m_storage_demand_working_bytes();
  }
};
TEST(PositionDistance8mStorageAdapter, NumericInclusiveAndOneBelow) {
  core::DecoderLimits l{};
  Ledger c{32,
           65568,
           {2, 3, 90, 10, 90},
           {1, 0, 7, 8, 9},
           {4, 5, 100, 20, 100},
           81,
           82,
           83,
           84,
           85,
           7524};
  Admission q{77, 88, 99};
  ASSERT_EQ(admit_lzss_position_distance_8m_storage(l, c, q), E::none);
  const auto total =
      32 + 4 * 65568 + 12 * (2 + 3 + 1 + 4 + 5) + 90 + 10 + 90 + 7 + 8 + 9 +
      100 + 20 + 100 + 81 + 82 + 83 + 84 + 85 + 7524 +
      lzss_position_distance_8m_storage_admission_working_bytes();
  EXPECT_EQ(q.aggregate_bytes, total);
  EXPECT_EQ(q.request_bytes, 12 * (4 + 5) + 220u);
  l.max_block_size = 1;
  l.max_internal_buffered_bytes = total;
  EXPECT_EQ(admit_lzss_position_distance_8m_storage(l, c, q), E::none);
  auto saved = snapshot(q);
  l.max_internal_buffered_bytes = total - 1;
  EXPECT_EQ(admit_lzss_position_distance_8m_storage(l, c, q),
            E::limit_exceeded);
  unchanged(q, saved);
}
TEST(PositionDistance8mStorageAdapter, EveryTypedProductAndSumOverflow) {
  core::DecoderLimits l{};
  Ledger c{};
  Admission q{77, 88, 99};
  const auto saved = snapshot(q);
  for (unsigned n = 0; n < 7; ++n) {
    c = {};
    if (n == 0)
      c.index_entries = max;
    else {
      auto &g = n <= 2 ? c.old : n <= 4 ? c.partial : c.request;
      if (n % 2)
        g.tokens = max;
      else
        g.token_scratch = max;
    }
    EXPECT_EQ(admit_lzss_position_distance_8m_storage(l, c, q),
              E::arithmetic_overflow);
    unchanged(q, saved);
  }
  c = {};
  c.external_bytes = max;
  EXPECT_EQ(admit_lzss_position_distance_8m_storage(l, c, q),
            E::arithmetic_overflow);
  unchanged(q, saved);
  c = {};
  c.request.frame_bytes = max;
  c.request.payload_bytes = 1;
  EXPECT_EQ(admit_lzss_position_distance_8m_storage(l, c, q),
            E::arithmetic_overflow);
  unchanged(q, saved);
}
TEST(PositionDistance8mStorageAdapter, OldAndPartialOwnersRemainCharged) {
  core::DecoderLimits l{};
  l.max_internal_buffered_bytes = 512u << 20;
  Ledger c{};
  c.raw_bytes = 8388608;
  c.index_entries = 65536 + 8388608;
  c.helper_bytes = 7524;
  c.request = {8388608, 8388608, 80 + 67108864, 67108864, 80 + 67108864};
  Admission q{};
  ASSERT_EQ(admit_lzss_position_distance_8m_storage(l, c, q), E::none);
  const auto saved = snapshot(q);
  c.old = c.request;
  EXPECT_EQ(admit_lzss_position_distance_8m_storage(l, c, q),
            E::limit_exceeded);
  unchanged(q, saved);
  c.old = {};
  c.partial = c.request;
  EXPECT_EQ(admit_lzss_position_distance_8m_storage(l, c, q),
            E::limit_exceeded);
  unchanged(q, saved);
}
TEST(PositionDistance8mStorageAdapter, ReconcileActualExtentAndPartialRequest) {
  core::DecoderLimits l{};
  Ledger c{};
  c.old = {4, 5, 6, 7, 8};
  c.partial = {1, 2, 3, 4, 5};
  c.request = {7, 8, 9, 10, 11};
  Gen a{2, 3, 4, 5, 6};
  Admission q{};
  ASSERT_EQ(reconcile_lzss_position_distance_8m_storage(l, c, a, q), E::none);
  EXPECT_EQ(q.request_bytes, 12 * (2 + 3) + 4 + 5 + 6u);
  const auto first = q.aggregate_bytes;
  c.partial.tokens += a.tokens;
  c.partial.token_scratch += a.token_scratch;
  c.partial.frame_bytes += a.frame_bytes;
  c.partial.payload_bytes += a.payload_bytes;
  c.partial.publication_bytes += a.publication_bytes;
  c.request = {1, 1, 1, 1, 1};
  ASSERT_EQ(admit_lzss_position_distance_8m_storage(l, c, q), E::none);
  EXPECT_EQ(q.aggregate_bytes, first + 27);
}
TEST(PositionDistance8mStorageAdapter,
     EveryReportedOvercapacityPreservesReceipt) {
  core::DecoderLimits l{};
  Ledger c{};
  c.request = {2, 2, 2, 2, 2};
  Admission q{77, 88, 99};
  auto saved = snapshot(q);
  for (unsigned n = 0; n < 5; ++n) {
    Gen a = c.request;
    if (n == 0)
      ++a.tokens;
    if (n == 1)
      ++a.token_scratch;
    if (n == 2)
      ++a.frame_bytes;
    if (n == 3)
      ++a.payload_bytes;
    if (n == 4)
      ++a.publication_bytes;
    EXPECT_EQ(reconcile_lzss_position_distance_8m_storage(l, c, a, q),
              E::overcapacity);
    unchanged(q, saved);
  }
}
TEST(PositionDistance8mStorageAdapter, ActualCannotBypassUnadmittedRequest) {
  core::DecoderLimits l{};
  Ledger c{};
  c.request.frame_bytes = 512u << 20;
  Gen a{};
  Admission q{1, 2, 3};
  auto saved = snapshot(q);
  EXPECT_EQ(reconcile_lzss_position_distance_8m_storage(l, c, c.request, q),
            E::overlapping_buffers);
  unchanged(q, saved);
  EXPECT_EQ(reconcile_lzss_position_distance_8m_storage(l, c, a, q),
            E::limit_exceeded);
  unchanged(q, saved);
  l.max_block_size = 0;
  EXPECT_EQ(admit_lzss_position_distance_8m_storage(l, c, q),
            E::invalid_stream);
  unchanged(q, saved);
}
TEST(PositionDistance8mStorageAdapter, TokenCountHasSuccessfulAdmittedDemand) {
  Fixture f;
  ASSERT_EQ(f.token(17), E::none);
  EXPECT_EQ(f.td.tokens, 2u);
  EXPECT_EQ(f.td.token_scratch, 2u);
  EXPECT_EQ(f.td.admitted_bytes, f.td.aggregate_bytes + 48);
  for (std::size_t i = 0; i < f.raw.size(); ++i)
    f.raw[i] = std::byte(i);
  ASSERT_EQ(f.token(), E::none);
  EXPECT_EQ(f.td.tokens, f.raw.size());
  std::fill(f.raw.begin(), f.raw.end(), std::byte{65});
  f.stream.dictionary.max_match_length = 3;
  ASSERT_EQ(f.token(), E::none);
  EXPECT_EQ(f.td.tokens, f.raw.size());
  ASSERT_EQ(f.frame(), E::none);
  EXPECT_EQ(f.fd.counts.declared_event_count, 2 * f.raw.size());
  EXPECT_EQ(f.fd.counts.declared_decision_count, 2 * f.raw.size());
}
TEST(PositionDistance8mStorageAdapter, TokenPairPeakInclusiveAndOneBelow) {
  Fixture f;
  ASSERT_EQ(f.token(), E::none);
  auto saved = snapshot(f.td);
  f.limits.max_block_size = 32;
  f.limits.max_internal_buffered_bytes = f.td.admitted_bytes;
  ASSERT_EQ(f.token(), E::none);
  --f.limits.max_internal_buffered_bytes;
  EXPECT_EQ(f.token(), E::limit_exceeded);
  unchanged(f.td, saved);
}
TEST(PositionDistance8mStorageAdapter,
     TokenPolicyPositionOverflowAndShortIndex) {
  Fixture f;
  auto saved = snapshot(f.td);
  EXPECT_EQ(f.token(0, 1), E::invalid_position);
  unchanged(f.td, saved);
  f.stream.dictionary_variant = 10;
  EXPECT_EQ(f.token(), E::invalid_stream);
  unchanged(f.td, saved);
  f.stream.dictionary_variant = 11;
  f.stream.original_size = f.limits.max_total_output_size + 1;
  EXPECT_EQ(f.token(), E::limit_exceeded);
  unchanged(f.td, saved);
  f.stream.original_size = f.raw.size();
  EXPECT_EQ(f.token(max), E::arithmetic_overflow);
  unchanged(f.td, saved);
  Fixture empty(0);
  empty.stream.frame_size = 1;
  const auto empty_saved = snapshot(empty.td);
  EXPECT_EQ(empty.token(), E::invalid_position);
  unchanged(empty.td, empty_saved);
  f.index.resize(1);
  EXPECT_EQ(f.token(), E::parser_error);
  unchanged(f.td, saved);
}
TEST(PositionDistance8mStorageAdapter,
     TokenMetadataRawAliasRejectedBeforeMutation) {
  Fixture f;
  auto saved = snapshot(f.td);
  const TypedContextFrameValidationContext c{f.stream, f.limits, 0, 0};
  auto raw = std::span<const std::byte>(
      reinterpret_cast<const std::byte *>(&f.td), sizeof(f.td));
  EXPECT_EQ(
      prepare_lzss_position_distance_8m_token_storage(raw, c, f.index, f.td),
      E::overlapping_buffers);
  unchanged(f.td, saved);
}
TEST(PositionDistance8mStorageAdapter, FiveIndependentWireDemandsAndEncoding) {
  auto check = [](const auto &raw, const auto &expected) {
    Fixture f(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i)
      f.raw[i] = std::byte(raw[i]);
    ASSERT_EQ(f.frame(23), E::none);
    EXPECT_EQ(f.fd.frame_bytes, expected.size());
    EXPECT_EQ(f.fd.publication_bytes, expected.size());
    EXPECT_EQ(f.fd.payload_bytes + 80, expected.size());
    EXPECT_EQ(f.fd.admitted_bytes,
              f.current() + 23 + 2 * f.fd.frame_bytes + f.fd.payload_bytes);
    std::vector<std::byte> frame(f.fd.frame_bytes), payload(f.fd.payload_bytes),
        out(frame.size());
    TypedContextFrameLayout layout{};
    std::size_t written{};
    LzssPositionDistance8mTokenFrameWorkspace w{f.tokens, f.scratch, f.index,
                                                frame, payload};
    const TypedContextFrameValidationContext c{f.stream, f.limits, 0, 0};
    ASSERT_EQ(encode_lzss_position_distance_8m_token_frame(f.raw, c, w, out,
                                                           layout, written)
                  .error,
              LzssPositionDistance8mTokenFrameError::none);
    for (std::size_t i = 0; i < out.size(); ++i)
      EXPECT_EQ(out[i], std::byte(expected[i]));
  };
  using namespace frame_encode_vectors;
  check(repeat_raw, repeat_frame);
  check(pattern_raw, pattern_frame);
  check(tie_raw, tie_frame);
  check(binary_raw, binary_frame);
  const std::array<uint8_t, 1> raw{65};
  std::array<uint8_t, 86> literal{};
  std::copy(frame_vectors::literal_prefix.begin(),
            frame_vectors::literal_prefix.end(), literal.begin());
  std::copy(token_vectors::literal.begin(), token_vectors::literal.end(),
            literal.begin() + 80);
  check(raw, literal);
}
TEST(PositionDistance8mStorageAdapter,
     PrefixPolicyIsValidatedBeforeDemandGrant) {
  Fixture f(267);
  auto saved = snapshot(f.fd);
  f.limits.max_expansion_ratio = 1;
  f.limits.expansion_slack = 0;
  EXPECT_EQ(f.frame(), E::prefix_error);
  unchanged(f.fd, saved);
}
TEST(PositionDistance8mStorageAdapter,
     FramePeakIncludesSpareViewsAndRetainedOwners) {
  Fixture f;
  ASSERT_EQ(f.frame(123), E::none);
  EXPECT_EQ(f.fd.aggregate_bytes, f.current() + 123);
  auto saved = snapshot(f.fd);
  f.limits.max_block_size = 32;
  f.limits.max_internal_buffered_bytes = f.fd.admitted_bytes;
  ASSERT_EQ(f.frame(123), E::none);
  --f.limits.max_internal_buffered_bytes;
  EXPECT_EQ(f.frame(123), E::limit_exceeded);
  unchanged(f.fd, saved);
}
TEST(PositionDistance8mStorageAdapter,
     FrameFailedObservationNeverGrantsStorage) {
  Fixture f(1);
  auto saved = snapshot(f.fd);
  f.limits.max_compressed_payload_size = 5;
  EXPECT_EQ(f.frame(), E::count_error);
  unchanged(f.fd, saved);
  f.limits.max_compressed_payload_size = 64u << 20;
  f.tokens.resize(0);
  EXPECT_EQ(f.frame(), E::count_error);
  unchanged(f.fd, saved);
  EXPECT_EQ(f.frame(max), E::arithmetic_overflow);
  unchanged(f.fd, saved);
}
TEST(PositionDistance8mStorageAdapter,
     FrameMetadataAndPrivateAliasesPreserveOutput) {
  Fixture f;
  auto saved = snapshot(f.fd);
  const TypedContextFrameValidationContext c{f.stream, f.limits, 0, 0};
  auto raw = std::span<const std::byte>(
      reinterpret_cast<const std::byte *>(&f.fd), sizeof(f.fd));
  EXPECT_EQ(prepare_lzss_position_distance_8m_frame_storage(
                raw, c, f.tokens, f.scratch, f.index, f.fd),
            E::overlapping_buffers);
  unchanged(f.fd, saved);
  EXPECT_EQ(prepare_lzss_position_distance_8m_frame_storage(
                f.raw, c, f.tokens, f.tokens, f.index, f.fd),
            E::overlapping_buffers);
  unchanged(f.fd, saved);
  EXPECT_EQ(prepare_lzss_position_distance_8m_frame_storage(
                f.raw, c, f.tokens, std::span(f.tokens).last(1), f.index, f.fd),
            E::overlapping_buffers);
  unchanged(f.fd, saved);
}
TEST(PositionDistance8mStorageAdapter, FinalShortFrameAndFrameLocalHistory) {
  Fixture f(5);
  f.stream.frame_size = 17;
  f.stream.original_size = 39;
  ASSERT_EQ(f.token(0, 2, 34), E::none);
  ASSERT_EQ(f.frame(0, 2, 34), E::none);
  EXPECT_EQ(f.fd.layout.header.sequence, 2u);
  EXPECT_EQ(f.fd.counts.declared_raw_size, 5u);
  auto saved = snapshot(f.fd);
  EXPECT_EQ(f.frame(0, 1, 34), E::invalid_position);
  unchanged(f.fd, saved);
}
TEST(PositionDistance8mStorageAdapter, LargeRealPreparedDemandAndDecode) {
  for (auto n : {1048576u, 8388608u}) {
    Fixture f(n);
    for (std::size_t i = 0; i < n; ++i)
      f.raw[i] = std::byte(i % 256);
    const auto controls = sizeof(Fixture) + 7 * sizeof(std::vector<std::byte>) +
                          3 * sizeof(TypedContextFrameLayout) +
                          sizeof(LzssPositionDistance8mTokenFrameWorkspace) +
                          sizeof(TypedContextFrameValidationContext) +
                          sizeof(LzssPositionDistance8mTokenFrameResult) +
                          sizeof(LzssPositionDistance8mFrameDecodeResult) +
                          32 * sizeof(std::size_t);
    const auto tokens =
        sizeof(Token) * (f.tokens.capacity() + f.scratch.capacity());
    const auto index = 4 * f.index.capacity();
    ASSERT_EQ(f.token(controls + tokens), E::none);
    ASSERT_EQ(f.frame(controls), E::none);
    std::vector<std::byte> frame(f.fd.frame_bytes + 3, guard),
        payload(f.fd.payload_bytes + 3, guard), out(frame.size(), guard),
        decoded(n), scratch(n);
    TypedContextFrameLayout layout{}, dl{};
    std::size_t written{};
    LzssPositionDistance8mTokenFrameWorkspace w{f.tokens, f.scratch, f.index,
                                                frame, payload};
    const TypedContextFrameValidationContext c{f.stream, f.limits, 0, 0};
    const auto helper = std::max(
        lzss_position_distance_8m_token_frame_working_bytes(),
        sizeof(LzssPositionDistance8mFrameDecodePlan) +
            sizeof(LzssPositionDistance8mFrameDecodeResult) +
            context::internal::lzss_position_distance_8m_token_working_bytes());
    const auto total = f.raw.capacity() + tokens + index + frame.capacity() +
                       payload.capacity() + out.capacity() +
                       decoded.capacity() + scratch.capacity() + controls +
                       helper;
    ASSERT_LE(total, f.limits.max_internal_buffered_bytes);
    const auto local =
        f.raw.size() + sizeof(Token) * (f.tokens.size() + f.scratch.size()) +
        4 * f.index.size() + frame.size() + payload.size() + out.size() +
        lzss_position_distance_8m_token_frame_working_bytes();
    auto r = encode_lzss_position_distance_8m_token_frame(
        f.raw, c, w, out, layout, written, total - local);
    ASSERT_EQ(r.error, LzssPositionDistance8mTokenFrameError::none);
    EXPECT_EQ(r.aggregate_bytes, total);
    EXPECT_EQ(written, f.fd.frame_bytes);
    const auto dlocal =
        written + sizeof(Token) * (f.tokens.size() + f.scratch.size()) +
        decoded.size() + scratch.size() +
        sizeof(LzssPositionDistance8mFrameDecodePlan) +
        sizeof(LzssPositionDistance8mFrameDecodeResult) +
        context::internal::lzss_position_distance_8m_token_working_bytes();
    auto d = decode_lzss_position_distance_8m_frame(
        std::span(out).first(written), c, f.tokens, f.scratch, decoded, scratch,
        dl, total - dlocal);
    ASSERT_EQ(d.error, LzssPositionDistance8mFrameDecodeError::none);
    EXPECT_EQ(decoded, f.raw);
    std::cout << "storage_adapter F=" << n
              << " T=" << f.fd.counts.declared_token_count
              << " P=" << f.fd.payload_bytes << " full_owners=" << total
              << '\n';
  }
  std::cout << "admission_working="
            << lzss_position_distance_8m_storage_admission_working_bytes()
            << " demand_working="
            << lzss_position_distance_8m_storage_demand_working_bytes() << '\n';
}
} // namespace
