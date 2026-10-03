#include "frame/lzss_position_distance_8m_frame_decoder.hpp"
#include "frame/lzss_position_distance_8m_frame_encoder.hpp"
#include "frame/lzss_position_distance_8m_token_frame_encoder.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <iostream>
#include <vector>

namespace {
using namespace marc;
using namespace frame::internal;
using Token = dictionary::internal::LzssTypedToken;
using Op = context::internal::ModeledOperation;
using Error = LzssPositionDistance8mTokenFrameError;
constexpr std::size_t mib = 1u << 20;
constexpr auto guard = std::byte{0xa5};

struct Case {
  std::size_t size;
  unsigned pattern;
  bool literals;
  bool final_short;
};

struct Owner {
  std::vector<Token> tokens, scratch;
  std::vector<std::uint32_t> index;
  std::vector<std::byte> frame, payload, output;
  TypedContextFrameLayout layout{};
  std::size_t written{};
  LzssPositionDistance8mTokenFrameWorkspace workspace() {
    return {tokens, scratch, index, frame, payload};
  }
};

template <class T> std::size_t capacity(const std::vector<T> &v) {
  // All qualification capacities are bounded by the pre-admitted case ceiling.
  return v.capacity() * sizeof(T);
}
std::size_t buffers(const Owner &o) {
  return capacity(o.tokens) + capacity(o.scratch) + capacity(o.index) +
         capacity(o.frame) + capacity(o.payload) + capacity(o.output);
}
std::size_t local(const Owner &o, std::size_t raw) {
  return raw + o.tokens.size() * sizeof(Token) +
         o.scratch.size() * sizeof(Token) + o.index.size() * sizeof(uint32_t) +
         o.frame.size() + o.payload.size() + o.output.size() +
         lzss_position_distance_8m_token_frame_working_bytes();
}

class LargeTokenFrame : public ::testing::TestWithParam<Case> {};
TEST_P(LargeTokenFrame, SeparateReferenceScopesAndResourceTransactions) {
  const auto test = GetParam();
  ASSERT_LE(test.size, 8 * mib);
  // Admit bounded raw/index/token requests first and exact operation/payload
  // requests after counting, before allocating them. No default changes.
  const auto reference_ceiling = 1024 * mib;
  ASSERT_LT(30 * test.size + 8 * mib, reference_ceiling);
  std::vector<std::byte> raw(test.size), expected;
  std::uint32_t random = 1439;
  const auto period = mib + 17;
  for (std::size_t i = 0; i < raw.size(); ++i) {
    random ^= random << 13;
    random ^= random >> 17;
    random ^= random << 5;
    if (test.pattern == 0)
      raw[i] = std::byte(i % 256);
    else if (test.pattern == 1)
      raw[i] = i < period ? std::byte(random & 255) : raw[i % period];
    else if (test.pattern == 2)
      raw[i] = std::byte(random & 255);
    else
      raw[i] = std::byte((i / 65536) % 3 == 0 ? (random & 255) : i % 7);
  }
  core::DecoderLimits limits{};
  limits.max_block_size = 8 * mib;
  limits.max_internal_buffered_bytes = reference_ceiling;
  TypedContextStreamHeader stream{};
  stream.frame_size =
      static_cast<uint32_t>(test.final_short ? 4 * mib : test.size);
  const auto prior = test.final_short ? 8 * mib : 0;
  const auto sequence = test.final_short ? 2u : 0u;
  stream.original_size = prior + test.size;
  stream.dictionary = {8 * mib, 3, test.literals ? 3u : 258u, 0};
  stream.range_model_total = 32768;
  stream.context_count = 47;
  stream.dictionary_variant = 11;
  stream.context_algorithm = 1;
  stream.context_variant = 12;
  const TypedContextFrameValidationContext context{stream, limits, sequence,
                                                   prior};
  std::size_t token_count{}, event_count{}, decision_count{}, payload_size{},
      reference_total{};
  // Reserve concrete fixture controls, plans/results/workspaces and scalar
  // bookkeeping conservatively across phases; code/framework/RSS are excluded.
  const auto controls =
      sizeof(Owner) + sizeof(raw) + sizeof(expected) + sizeof(stream) +
      sizeof(limits) + sizeof(context) + sizeof(test) +
      2 * sizeof(std::vector<Op>) + 2 * sizeof(std::vector<std::byte>) +
      sizeof(LzssPositionDistance8mTokenFramePlan) +
      sizeof(LzssPositionDistance8mTokenFrameResult) +
      sizeof(LzssPositionDistance8mFrameEncodeWorkspace) +
      sizeof(LzssPositionDistance8mFrameEncodeResult) +
      sizeof(LzssPositionDistance8mTokenFrameWorkspace) +
      sizeof(dictionary::internal::LzssPositionDistance8mParsePlan) +
      sizeof(LzssPositionDistance8mFrameDecodeResult) +
      3 * sizeof(TypedContextFrameLayout) + 64 * sizeof(std::size_t);
  {
    Owner ref;
    ref.index.resize(65536 + raw.size());
    const auto counted =
        dictionary::internal::query_lzss_position_distance_8m_indexed(
            raw, stream.dictionary, limits, 0, 0, ref.index,
            capacity(raw) + controls);
    ASSERT_EQ(counted.error,
              dictionary::internal::LzssPositionDistance8mParseError::
                  output_too_small);
    token_count = counted.details.token_count;
    ASSERT_LE(token_count, raw.size());
    ASSERT_LE(capacity(raw) + capacity(ref.index) + controls +
                  2 * token_count * sizeof(Token) +
                  lzss_position_distance_8m_token_frame_working_bytes(),
              reference_ceiling);
    ref.tokens.resize(token_count);
    ref.scratch.resize(token_count);
    auto w = ref.workspace();
    const auto planned = query_lzss_position_distance_8m_token_frame_encode(
        raw, context, w, 0,
        controls + capacity(raw) + buffers(ref) - local(ref, raw.size()) +
            lzss_position_distance_8m_token_frame_working_bytes());
    ASSERT_EQ(planned.error, Error::storage_too_small);
    event_count = planned.counts.declared_event_count;
    decision_count = planned.counts.declared_decision_count;
    payload_size = planned.bytes_required - 80;
    ASSERT_LE(token_count, raw.size());
    ASSERT_LE(event_count, 2 * raw.size());
    ASSERT_LE(payload_size, limits.max_compressed_payload_size);
    const auto proposed_reference =
        capacity(raw) + buffers(ref) + controls +
        3 * (planned.bytes_required + 3) + payload_size + 3 +
        2 * event_count * sizeof(Op) +
        lzss_position_distance_8m_frame_encode_working_bytes();
    ASSERT_LE(proposed_reference, reference_ceiling);
    ref.frame.resize(planned.bytes_required + 3, guard);
    ref.output.resize(ref.frame.size(), guard);
    ref.payload.resize(payload_size + 3, guard);
    expected.resize(ref.output.size(), guard);
    std::vector<Op> operations(event_count), operation_scratch(event_count);
    reference_total = capacity(raw) + capacity(expected) + buffers(ref) +
                      capacity(operations) + capacity(operation_scratch) +
                      controls +
                      lzss_position_distance_8m_frame_encode_working_bytes();
    ASSERT_LE(reference_total, reference_ceiling);
    const auto own =
        raw.size() + ref.output.size() + ref.frame.size() + ref.payload.size() +
        sizeof(Token) * (ref.tokens.size() + ref.scratch.size()) +
        sizeof(uint32_t) * ref.index.size() +
        sizeof(Op) * (operations.size() + operation_scratch.size()) +
        lzss_position_distance_8m_frame_encode_working_bytes();
    LzssPositionDistance8mFrameEncodeWorkspace workspace{
        ref.tokens,        ref.scratch, ref.index,  operations,
        operation_scratch, ref.frame,   ref.payload};
    const auto result = encode_lzss_position_distance_8m_frame(
        raw, context, workspace, ref.output, ref.layout, ref.written,
        reference_total - own);
    ASSERT_EQ(result.error, LzssPositionDistance8mFrameEncodeError::none);
    ASSERT_EQ(result.aggregate_bytes, reference_total);
    ASSERT_EQ(ref.written, payload_size + 80);
    expected = ref.output;
  } // Reference operation/token/index/frame owners actually destroyed here.

  limits.max_internal_buffered_bytes = 512 * mib;
  const auto proposed_new =
      capacity(raw) + capacity(expected) + controls +
      2 * token_count * sizeof(Token) + 4 * (65536 + raw.size()) +
      2 * expected.size() + payload_size + 3 +
      lzss_position_distance_8m_token_frame_working_bytes();
  ASSERT_LE(proposed_new, limits.max_internal_buffered_bytes);
  Owner owner;
  owner.tokens.resize(token_count);
  owner.scratch.resize(token_count);
  owner.index.resize(65536 + raw.size());
  owner.frame.resize(expected.size(), guard);
  owner.output.resize(expected.size(), guard);
  owner.payload.resize(payload_size + 3, guard);
  auto workspace = owner.workspace();
  const auto total = capacity(raw) + capacity(expected) + buffers(owner) +
                     controls +
                     lzss_position_distance_8m_token_frame_working_bytes();
  ASSERT_LE(total, limits.max_internal_buffered_bytes);
  const auto retained = total - local(owner, raw.size());
  auto result = encode_lzss_position_distance_8m_token_frame(
      raw, context, workspace, owner.output, owner.layout, owner.written,
      retained);
  ASSERT_EQ(result.error, Error::none);
  EXPECT_EQ(result.aggregate_bytes, total);
  EXPECT_EQ(owner.output, expected);
  EXPECT_EQ(owner.written, payload_size + 80);
  EXPECT_EQ(owner.layout.header.sequence, sequence);
  if (test.literals)
    EXPECT_EQ(token_count, raw.size());
  if (test.pattern == 1) {
    ASSERT_TRUE(std::any_of(owner.tokens.begin(), owner.tokens.end(),
                            [](const Token &t) { return t.distance > mib; }));
  }

  std::array<std::byte, sizeof(owner.layout)> saved{};
  std::memcpy(saved.data(), &owner.layout, sizeof(owner.layout));
  const auto written = owner.written;
  auto unchanged = [&] {
    EXPECT_EQ(result.bytes_committed, 0u);
    EXPECT_EQ(owner.output, expected);
    EXPECT_EQ(owner.written, written);
    EXPECT_EQ(std::memcmp(saved.data(), &owner.layout, sizeof(owner.layout)),
              0);
  };
  // Exact inclusive budget succeeds; one byte below rejects before mutation.
  limits.max_block_size = raw.size();
  limits.max_internal_buffered_bytes = total;
  auto plan = query_lzss_position_distance_8m_token_frame_encode(
      raw, context, workspace, owner.output.size(), retained);
  ASSERT_EQ(plan.error, Error::none);
  EXPECT_EQ(plan.aggregate_bytes, total);
  limits.max_internal_buffered_bytes = total - 1;
  result = encode_lzss_position_distance_8m_token_frame(
      raw, context, workspace, owner.output, owner.layout, owner.written,
      retained);
  EXPECT_EQ(result.error, Error::limit_exceeded);
  unchanged();
  limits.max_internal_buffered_bytes = 512 * mib;
  workspace.frame = std::span(owner.output).last(3);
  result = encode_lzss_position_distance_8m_token_frame(
      raw, context, workspace, owner.output, owner.layout, owner.written,
      retained + owner.frame.size() - 3);
  EXPECT_EQ(result.error, Error::overlapping_buffers);
  unchanged();
  workspace = owner.workspace();
  limits.max_block_size = raw.size() - 1;
  result = encode_lzss_position_distance_8m_token_frame(
      raw, context, workspace, owner.output, owner.layout, owner.written,
      retained);
  EXPECT_EQ(result.error, Error::parser_error);
  unchanged();
  limits.max_block_size = raw.size();
  // Count the complete capacities still owned, even when only a short output
  // view is supplied. The shortened view is not permission to drop its owner.
  result = encode_lzss_position_distance_8m_token_frame(
      raw, context, workspace, std::span(owner.output).first(written - 1),
      owner.layout, owner.written,
      retained + owner.output.size() - written + 1);
  EXPECT_EQ(result.error, Error::storage_too_small);
  unchanged();
  workspace.payload_scratch = {};
  result = encode_lzss_position_distance_8m_token_frame(
      raw, context, workspace, owner.output, owner.layout, owner.written,
      retained + owner.payload.size());
  EXPECT_EQ(result.error, Error::range_error);
  unchanged();
  workspace = owner.workspace();
  limits.max_compressed_payload_size = payload_size - 1;
  result = encode_lzss_position_distance_8m_token_frame(
      raw, context, workspace, owner.output, owner.layout, owner.written,
      retained);
  EXPECT_NE(result.error, Error::none);
  unchanged();
  limits.max_compressed_payload_size = 64 * mib;
  limits.max_total_output_size = prior + raw.size() - 1;
  result = encode_lzss_position_distance_8m_token_frame(
      raw, context, workspace, owner.output, owner.layout, owner.written,
      retained);
  EXPECT_NE(result.error, Error::none);
  unchanged();
  limits.max_total_output_size = 1ull << 40;

  // Reuse private tokens for decoding after encode: no replacement token owner.
  ASSERT_LE(
      total + 2 * raw.size() + sizeof(LzssPositionDistance8mFrameDecodePlan) +
          sizeof(LzssPositionDistance8mFrameDecodeResult) +
          context::internal::lzss_position_distance_8m_token_working_bytes(),
      limits.max_internal_buffered_bytes);
  std::vector<std::byte> decoded(raw.size(), guard), scratch(raw.size(), guard);
  TypedContextFrameLayout decoded_layout{};
  const auto decode_working =
      sizeof(LzssPositionDistance8mFrameDecodePlan) +
      sizeof(LzssPositionDistance8mFrameDecodeResult) +
      context::internal::lzss_position_distance_8m_token_working_bytes();
  const auto decode_total = capacity(raw) + capacity(expected) +
                            buffers(owner) + capacity(decoded) +
                            capacity(scratch) + controls + decode_working;
  ASSERT_LE(decode_total, limits.max_internal_buffered_bytes);
  const auto decode_local =
      written + sizeof(Token) * (owner.tokens.size() + owner.scratch.size()) +
      decoded.size() + scratch.size() + decode_working;
  const auto d = decode_lzss_position_distance_8m_frame(
      std::span(owner.output).first(written), context, owner.tokens,
      owner.scratch, decoded, scratch, decoded_layout,
      decode_total - decode_local);
  ASSERT_EQ(d.error, LzssPositionDistance8mFrameDecodeError::none);
  EXPECT_EQ(decoded, raw);
  std::cout << "large_token_frame F=" << raw.size()
            << " pattern=" << test.pattern << " literals=" << test.literals
            << " T=" << token_count << " E=" << event_count
            << " D=" << decision_count << " P=" << payload_size
            << " reference=" << reference_total << " encode=" << total
            << " decode=" << decode_total << '\n';
}
INSTANTIATE_TEST_SUITE_P(DiverseFiniteFrames, LargeTokenFrame,
                         ::testing::Values(Case{mib - 1, 0, false, false},
                                           Case{mib + 1, 3, false, true},
                                           Case{4 * mib - 1, 0, false, false},
                                           Case{4 * mib + 1, 3, false, false},
                                           Case{8 * mib, 0, false, false},
                                           Case{8 * mib, 1, false, false},
                                           Case{mib, 2, false, false},
                                           Case{mib, 0, true, false},
                                           Case{8 * mib, 0, true, false}));
} // namespace
