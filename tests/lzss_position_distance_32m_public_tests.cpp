#include "lzss_position_distance_32m_public_wire_vectors.hpp"
#include "marc/marc.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
#include <vector>
namespace {
using Config = marc_lzss_position_distance_dynamic_range_32m_config;
using Resources = marc_lzss_position_distance_dynamic_range_32m_resources;
using Decode = marc_lzss_position_distance_dynamic_range_32m_decoder_config;
using Requirements =
    marc_lzss_position_distance_dynamic_range_32m_decoder_requirements;
using Buffers = marc_lzss_position_distance_dynamic_range_32m_decoder_buffers;
struct Handle {
  marc_transform *p{};
  ~Handle() { marc_transform_destroy(p); }
};
Config config(std::size_t n) {
  Config c{};
  EXPECT_EQ(marc_lzss_position_distance_dynamic_range_32m_config_init(&c),
            MARC_STATUS_OK);
  c.original_size = n;
  c.frame_size = 32;
  c.max_frame_size = c.max_block_size = 32;
  c.max_total_output_size = 1048576;
  c.max_compressed_payload_size = 65536;
  c.max_internal_buffered_bytes = 16 * 1048576;
  c.max_entropy_table_entries = 2621;
  c.max_expansion_ratio = 1048576;
  c.expansion_slack = 1048576;
  c.input_capacity_bytes = c.output_capacity_bytes = 4096;
  // Complete test fixture owners, controls and caller backing tails.
  c.external_retained_bytes = 65536;
  return c;
}
Decode decode_config() {
  Decode d{};
  EXPECT_EQ(
      marc_lzss_position_distance_dynamic_range_32m_decoder_config_init(&d),
      MARC_STATUS_OK);
  auto c = config(0);
  d.max_frame_size = c.max_frame_size;
  d.max_block_size = c.max_block_size;
  d.max_total_output_size = c.max_total_output_size;
  d.max_compressed_payload_size = c.max_compressed_payload_size;
  d.max_internal_buffered_bytes = c.max_internal_buffered_bytes;
  d.max_entropy_table_entries = c.max_entropy_table_entries;
  d.max_expansion_ratio = c.max_expansion_ratio;
  d.expansion_slack = c.expansion_slack;
  d.input_capacity_bytes = c.input_capacity_bytes;
  d.output_capacity_bytes = c.output_capacity_bytes;
  d.external_retained_bytes = c.external_retained_bytes;
  return d;
}
std::vector<std::uint8_t> encode(const std::vector<std::uint8_t> &raw,
                                 std::size_t in_chunk, std::size_t out_chunk) {
  auto c = config(raw.size());
  Handle h;
  EXPECT_EQ(
      marc_lzss_position_distance_dynamic_range_32m_create_encoder(&c, &h.p),
      MARC_STATUS_OK);
  if (!h.p)
    return {};
  std::vector<std::uint8_t> result;
  result.reserve(4096);
  std::array<std::uint8_t, 4096> out{};
  std::size_t pos{};
  for (unsigned call = 0; call < 20000; ++call) {
    auto n = std::min(in_chunk, raw.size() - pos);
    out.fill(0xa5);
    auto r = marc_transform_process(
        h.p, {raw.empty() ? nullptr : raw.data() + pos, n},
        {out.data(), out_chunk},
        (pos + n == raw.size() ? MARC_PROCESS_END_INPUT : 0) |
            MARC_PROCESS_FLUSH);
    EXPECT_LE(r.input_consumed, n);
    EXPECT_LE(r.output_produced, out_chunk);
    EXPECT_TRUE(std::all_of(out.begin() + r.output_produced, out.end(),
                            [](auto b) { return b == 0xa5; }));
    result.insert(result.end(), out.begin(), out.begin() + r.output_produced);
    pos += r.input_consumed;
    if (r.status == MARC_STATUS_END_OF_STREAM) {
      EXPECT_EQ(pos, raw.size());
      auto sticky =
          marc_transform_process(h.p, {nullptr, 0}, {nullptr, 0}, UINT32_MAX);
      EXPECT_EQ(sticky.status, MARC_STATUS_END_OF_STREAM);
      return result;
    }
    if (r.status >= 100) {
      ADD_FAILURE() << "encoder status " << r.status;
      return {};
    }
    EXPECT_TRUE(r.input_consumed || r.output_produced ||
                r.status == MARC_STATUS_NEED_INPUT ||
                r.status == MARC_STATUS_NEED_OUTPUT);
  }
  ADD_FAILURE() << "encoder call guard";
  return {};
}
struct DecoderFixture {
  Decode c{decode_config()};
  Requirements q{sizeof(q), MARC_ABI_VERSION};
  std::vector<std::uint8_t> serial, tokens, scratch, raw, raw_scratch;
  Buffers b{};
  Handle h;
  DecoderFixture() {
    EXPECT_EQ(
        marc_lzss_position_distance_dynamic_range_32m_decoder_workspace_requirements(
            &c, &q),
        MARC_STATUS_OK);
    serial.resize(q.serialized_bytes);
    tokens.resize(q.token_bytes);
    scratch.resize(q.token_scratch_bytes);
    raw.resize(q.raw_bytes);
    raw_scratch.resize(q.raw_scratch_bytes);
    b = {sizeof(b),
         MARC_ABI_VERSION,
         0,
         0,
         {serial.data(), serial.size()},
         {tokens.data(), tokens.size()},
         {scratch.data(), scratch.size()},
         {raw.data(), raw.size()},
         {raw_scratch.data(), raw_scratch.size()}};
  }
  marc_status start() {
    return marc_lzss_position_distance_dynamic_range_32m_create_decoder(&c, &b,
                                                                        &h.p);
  }
};
std::vector<std::uint8_t> decode(const std::vector<std::uint8_t> &wire,
                                 std::size_t in_chunk, std::size_t out_chunk) {
  DecoderFixture f;
  EXPECT_EQ(f.start(), MARC_STATUS_OK);
  if (!f.h.p)
    return {};
  std::array<std::uint8_t, 4096> out{};
  std::vector<std::uint8_t> result;
  result.reserve(4096);
  std::size_t pos{};
  for (unsigned call = 0; call < 20000; ++call) {
    auto n = std::min(in_chunk, wire.size() - pos);
    out.fill(0xa5);
    auto r = marc_transform_process(
        f.h.p, {wire.data() + pos, n}, {out.data(), out_chunk},
        pos + n == wire.size() ? MARC_PROCESS_END_INPUT : 0);
    EXPECT_LE(r.input_consumed, n);
    EXPECT_LE(r.output_produced, out_chunk);
    EXPECT_TRUE(std::all_of(out.begin() + r.output_produced, out.end(),
                            [](auto b) { return b == 0xa5; }));
    result.insert(result.end(), out.begin(), out.begin() + r.output_produced);
    pos += r.input_consumed;
    if (r.status == MARC_STATUS_END_OF_STREAM) {
      EXPECT_EQ(pos, wire.size());
      return result;
    }
    if (r.status >= 100) {
      ADD_FAILURE() << "decoder status " << r.status;
      return {};
    }
  }
  ADD_FAILURE() << "decoder call guard";
  return {};
}
TEST(Public32m, InitializersLeaveExistingProfileAndUnspecifiedLimitsAlone) {
  Config c{};
  ASSERT_EQ(marc_lzss_position_distance_dynamic_range_32m_config_init(&c),
            MARC_STATUS_OK);
  EXPECT_EQ(c.frame_size, 33554432u);
  EXPECT_EQ(c.max_internal_buffered_bytes, 0u);
  EXPECT_EQ(c.encoder_strategy, MARC_LZSS_POSITION_DISTANCE_32M_COMPACT_OWNING);
  marc_lzss_position_distance_dynamic_range_8m_config old{};
  ASSERT_EQ(marc_lzss_position_distance_dynamic_range_8m_config_init(&old),
            MARC_STATUS_OK);
  EXPECT_EQ(old.frame_size, 8388608u);
  EXPECT_EQ(old.max_internal_buffered_bytes, 0u);
}
TEST(Public32m, EmptyAndEveryOneByteRoundTrip) {
  EXPECT_EQ(decode(encode({}, 1, 1), 1, 1), std::vector<std::uint8_t>{});
  for (unsigned b = 0; b < 256; ++b) {
    std::vector<std::uint8_t> raw{static_cast<std::uint8_t>(b)};
    EXPECT_EQ(decode(encode(raw, 1, 1), 1, 1), raw);
  }
}
TEST(Public32m, DeterministicFullCallAndChunkSchedules) {
  for (std::size_t size : {31u, 32u, 33u, 64u, 65u}) {
    std::vector<std::uint8_t> raw(size);
    for (std::size_t i = 0; i < size; ++i)
      raw[i] = static_cast<std::uint8_t>((i * 13) % 17);
    auto expected = encode(raw, 4096, 4096);
    ASSERT_GE(expected.size(), 112u);
    EXPECT_EQ(expected[14], 13u);
    EXPECT_EQ(expected[98], 14u);
    for (auto in : {1u, 7u, 32u})
      for (auto out : {1u, 17u, 257u}) {
        auto wire = encode(raw, in, out);
        EXPECT_EQ(wire, expected);
        EXPECT_EQ(decode(wire, in, out), raw);
      }
  }
}
TEST(Public32m, EncoderInitialBudgetExactOneBelowAndFailuresPreserveQuery) {
  auto c = config(65);
  Resources q{};
  ASSERT_EQ(marc_lzss_position_distance_dynamic_range_32m_resource_requirements(
                &c, &q),
            MARC_STATUS_OK);
  EXPECT_EQ(q.initial_raw_bytes, 32u);
  EXPECT_EQ(q.initial_index_entries, 1048608u);
  EXPECT_EQ(q.admission_scope, MARC_LZSS_POSITION_DISTANCE_32M_INITIAL_ONLY);
  c.max_internal_buffered_bytes = q.initial_bytes;
  Handle h;
  ASSERT_EQ(
      marc_lzss_position_distance_dynamic_range_32m_create_encoder(&c, &h.p),
      MARC_STATUS_OK);
  c.max_internal_buffered_bytes--;
  auto prior = q;
  EXPECT_EQ(marc_lzss_position_distance_dynamic_range_32m_resource_requirements(
                &c, &q),
            MARC_STATUS_LIMIT_EXCEEDED);
  EXPECT_EQ(std::memcmp(&q, &prior, sizeof(q)), 0);
  Handle refused;
  EXPECT_EQ(marc_lzss_position_distance_dynamic_range_32m_create_encoder(
                &c, &refused.p),
            MARC_STATUS_LIMIT_EXCEEDED);
  EXPECT_EQ(refused.p, nullptr);
}
TEST(Public32m, DecoderBudgetExactOneBelowAndOversizedPayloadIsBounded) {
  DecoderFixture f;
  f.c.max_internal_buffered_bytes = f.q.minimum_aggregate_bytes;
  ASSERT_EQ(f.start(), MARC_STATUS_OK);
  auto d = f.c;
  d.max_internal_buffered_bytes--;
  auto q = f.q;
  auto before = q;
  EXPECT_EQ(
      marc_lzss_position_distance_dynamic_range_32m_decoder_workspace_requirements(
          &d, &q),
      MARC_STATUS_LIMIT_EXCEEDED);
  EXPECT_EQ(std::memcmp(&q, &before, sizeof(q)), 0);
  auto maximum = decode_config();
  maximum.max_frame_size = maximum.max_block_size = 33554432;
  maximum.max_total_output_size = 33554432;
  maximum.expansion_slack = 33554432;
  maximum.max_compressed_payload_size = 67108864;
  maximum.max_internal_buffered_bytes = UINT64_C(1) << 30;
  q = {sizeof(q), MARC_ABI_VERSION};
  ASSERT_EQ(
      marc_lzss_position_distance_dynamic_range_32m_decoder_workspace_requirements(
          &maximum, &q),
      MARC_STATUS_OK);
  EXPECT_EQ(q.serialized_bytes, 67108944u);
  EXPECT_EQ(q.token_bytes, 100663296u);
  EXPECT_GT(q.minimum_aggregate_bytes, 335544400u);
}
TEST(Public32m, ReservedFieldsAliasesAlignmentAndCapacityAreRejected) {
  auto c = config(1);
  Resources q{};
  auto prior = q;
  c.reserved = 1;
  EXPECT_EQ(marc_lzss_position_distance_dynamic_range_32m_resource_requirements(
                &c, &q),
            MARC_STATUS_INVALID_ARGUMENT);
  EXPECT_EQ(std::memcmp(&q, &prior, sizeof(q)), 0);
  c = config(1);
  c.external_retained_bytes = UINT64_MAX;
  EXPECT_EQ(marc_lzss_position_distance_dynamic_range_32m_resource_requirements(
                &c, &q),
            MARC_STATUS_LIMIT_EXCEEDED);
  DecoderFixture f;
  f.b.token_scratch = f.b.tokens;
  EXPECT_EQ(f.start(), MARC_STATUS_INVALID_ARGUMENT);
  f.b.token_scratch = {f.scratch.data() + 1, f.scratch.size() - 1};
  EXPECT_EQ(f.start(), MARC_STATUS_INVALID_ARGUMENT);
}
TEST(Public32m, LateSecondFrameCorruptionPublishesOnlyFirstFrame) {
  std::vector<std::uint8_t> input(65, 65);
  auto wire = encode(input, 4096, 4096);
  ASSERT_GT(wire.size(), 192u);
  std::uint32_t first_payload{};
  for (unsigned i = 0; i < 4; ++i)
    first_payload |= std::uint32_t(wire[144 + i]) << (8 * i);
  auto second = 192 + first_payload;
  ASSERT_LT(second + 80, wire.size());
  wire[second + 80] ^= 1;
  DecoderFixture f;
  ASSERT_EQ(f.start(), MARC_STATUS_OK);
  std::array<std::uint8_t, 4096> out{};
  out.fill(0xa5);
  auto r =
      marc_transform_process(f.h.p, {wire.data(), wire.size()},
                             {out.data(), out.size()}, MARC_PROCESS_END_INPUT);
  EXPECT_GE(r.status, 100u);
  EXPECT_EQ(r.output_produced, 32u);
  EXPECT_TRUE(std::equal(out.begin(), out.begin() + 32, input.begin()));
  EXPECT_TRUE(std::all_of(out.begin() + 32, out.end(),
                          [](auto b) { return b == 0xa5; }));
  auto sticky =
      marc_transform_process(f.h.p, {nullptr, 0}, {out.data(), out.size()}, 0);
  EXPECT_EQ(sticky.status, r.status);
  EXPECT_EQ(sticky.output_produced, 0u);
}
TEST(Public32m, CompactByteBuffersHaveNoTypedAlignmentOrElementMultiple) {
  DecoderFixture f;
  std::vector<std::uint8_t> shifted(f.q.token_bytes + 1, 0xa5);
  f.b.tokens = {shifted.data() + 1, static_cast<std::size_t>(f.q.token_bytes)};
  EXPECT_EQ(f.q.token_alignment, 1u);
  EXPECT_EQ(f.q.record_capacity_bytes, 96u);
  ASSERT_EQ(f.start(), MARC_STATUS_OK);
  const auto wire = encode({0x7f}, 1, 1);
  std::array<std::uint8_t, 16> out{};
  out.fill(0xa5);
  const auto r =
      marc_transform_process(f.h.p, {wire.data(), wire.size()},
                             {out.data(), out.size()}, MARC_PROCESS_END_INPUT);
  EXPECT_EQ(r.status, MARC_STATUS_END_OF_STREAM);
  EXPECT_EQ(r.output_produced, 1u);
  EXPECT_EQ(out[0], 0x7f);
  EXPECT_EQ(shifted[0], 0xa5);
  marc_transform_destroy(f.h.p);
  f.h.p = nullptr; // backing outlives the borrowed handle.
}
TEST(Public32m, GenericWireLengthsThreeAndFourRetainDecoderAdmission) {
  using namespace public_wire_vectors;
  const std::vector<std::uint8_t> three(length3.begin(), length3.end());
  const std::vector<std::uint8_t> four(length4.begin(), length4.end());
  EXPECT_EQ(decode(three, 1, 1), std::vector<std::uint8_t>(4, 65));
  EXPECT_EQ(decode(four, 1, 1), std::vector<std::uint8_t>(5, 65));
}
TEST(Public32m, FullActualRecordTailIsChargedBeforeHandlePublication) {
  DecoderFixture f;
  std::vector<std::uint8_t> storage(f.q.token_bytes + 17, 0xa5);
  f.b.tokens = {storage.data(), storage.size()};
  f.c.max_internal_buffered_bytes = f.q.minimum_aggregate_bytes;
  EXPECT_EQ(f.start(), MARC_STATUS_LIMIT_EXCEEDED);
  EXPECT_EQ(f.h.p, nullptr);
  EXPECT_TRUE(std::all_of(storage.begin(), storage.end(),
                          [](auto b) { return b == 0xa5; }));
  f.c.max_internal_buffered_bytes += 16;
  EXPECT_EQ(f.start(), MARC_STATUS_LIMIT_EXCEEDED);
  EXPECT_EQ(f.h.p, nullptr);
  ++f.c.max_internal_buffered_bytes;
  ASSERT_EQ(f.start(), MARC_STATUS_OK);
  marc_transform_destroy(f.h.p);
  f.h.p = nullptr;
}
} // namespace
