#include "context/lzss_position_distance_64m_mapper.hpp"
#include "core/checked_math.hpp"
#include "entropy/lzss_position_distance_64m_range_encoder.hpp"
#include "entropy/lzss_position_distance_64m_token_range_encoder.hpp"
#include "lzss_position_distance_64m_token_range_vectors.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <gtest/gtest.h>
#include <iostream>
#include <limits>
#include <vector>
using namespace marc;
using namespace context::internal;
using namespace entropy::internal;
using Token = dictionary::internal::LzssTypedToken;
using Kind = dictionary::internal::LzssTypedTokenKind;
using Error = LzssPositionDistance64mTokenRangeError;
constexpr auto guard = std::byte{0xa5};
std::vector<std::byte> unhex(std::string_view s) {
  auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
  std::vector<std::byte> r;
  for (size_t i = 0; i < s.size(); i += 2)
    r.push_back(std::byte((digit(s[i]) << 4) | digit(s[i + 1])));
  return r;
}
struct Fixture {
  std::vector<Token> tokens;
  std::vector<std::byte> expected, out, scratch;
  dictionary::internal::LzssParameters p{67108864, 3, 258, 0};
  core::DecoderLimits limits{};
  LzssFieldContextValidationContext c{};
  ContextualDynamicRangeDescriptor descriptor{123, 456, 7};
  explicit Fixture(const token_range_vectors::Vector &v) {
    limits.max_block_size = 67108864;
    limits.max_frame_size = 67108864;
    limits.max_lz_distance = 67108864;
    c = {v.t, v.e, v.d, v.f, 0};
    expected = unhex(v.payload);
    out.assign(expected.size() + 7, guard);
    scratch = out;
    if (v.tokens.empty()) {
      if (v.name == "literal_rescale")
        tokens.assign(70000, {Kind::literal, 65, 0, 0});
      else if (v.name == "far3" || v.name == "far258") {
        const auto length = v.name == "far3" ? 3u : 258u;
        const auto distance = 67108864u - length;
        tokens.push_back({Kind::literal, 65, 0, 0});
        auto remaining = distance - 1;
        while (remaining >= 258) {
          tokens.push_back({Kind::match, 0, 1, 258});
          remaining -= 258;
        }
        if (remaining >= 3)
          tokens.push_back({Kind::match, 0, 1, remaining});
        else
          while (remaining--)
            tokens.push_back({Kind::literal, 65, 0, 0});
        tokens.push_back({Kind::match, 0, distance, length});
      } else if (v.name == "match_rescale") {
        tokens.push_back({Kind::literal, 65, 0, 0});
        tokens.insert(tokens.end(), 70000, {Kind::match, 0, 1, 5});
      } else {
        tokens.assign(4096, {Kind::literal, 65, 0, 0});
        for (unsigned i = 0; i <= 12; ++i)
          tokens.push_back({Kind::match, 0, 1u << i, 5});
      }
    } else {
      auto b = unhex(v.tokens);
      for (size_t i = 0; i < b.size(); i += 10) {
        Token t{static_cast<Kind>(b[i]), std::to_integer<uint8_t>(b[i + 1]), 0,
                0};
        for (unsigned j = 0; j < 4; ++j) {
          t.distance |= std::to_integer<uint32_t>(b[i + 2 + j]) << (8 * j);
          t.length |= std::to_integer<uint32_t>(b[i + 6 + j]) << (8 * j);
        }
        tokens.push_back(t);
      }
    }
  }
  auto query(size_t oc, size_t sc, size_t retained = 0) {
    return query_lzss_position_distance_64m_token_range_encode(
        tokens, p, c, limits, oc, sc, retained);
  }
  auto encode(size_t retained = 0) {
    return encode_lzss_position_distance_64m_token_range(
        tokens, p, c, limits, out, scratch, descriptor, retained);
  }
  void failure() {
    auto before = out;
    auto saved = descriptor;
    auto r = encode();
    EXPECT_NE(r.details.error, Error::none);
    EXPECT_EQ(r.bytes_committed, 0u);
    EXPECT_EQ(out, before);
    EXPECT_EQ(std::memcmp(&saved, &descriptor, sizeof(saved)), 0);
  }
};
const auto &named(std::string_view s) {
  return *std::find_if(token_range_vectors::vectors.begin(),
                       token_range_vectors::vectors.end(),
                       [&](auto &v) { return v.name == s; });
}
class MathematicalVectors : public testing::TestWithParam<unsigned> {};
TEST_P(MathematicalVectors, CountBytesReferenceAndTokenConsumer) {
  const auto &v = token_range_vectors::vectors[GetParam()];
  SCOPED_TRACE(v.name);
  Fixture f(v);
  ASSERT_EQ(f.tokens.size(), v.t);
  std::vector<ModeledOperation> ops(v.e), os(v.e);
  std::vector<std::byte> old(f.expected.size()), priv(old.size()), first(f.out);
  std::vector<Token> dt(v.t), ds(v.t);
  // All test-owned backing capacities are simultaneously live. Framework
  // allocations and machine stack/RSS are outside this codec-owner ledger.
  const auto owner = [](const auto &x) { return x.capacity() * sizeof(x[0]); };
  const auto controls =
      sizeof(Fixture) + sizeof(ops) + sizeof(os) + sizeof(old) + sizeof(priv) +
      sizeof(first) + sizeof(dt) + sizeof(ds) +
      sizeof(LzssPositionDistance64mTokenRangePlan) +
      sizeof(LzssPositionDistance64mTokenRangeResult) +
      sizeof(LzssFieldContextResult) +
      sizeof(LzssPositionDistance64mMapResult) +
      sizeof(LzssPositionDistance64mRangeEncodeResult) +
      sizeof(LzssContextualRangeDecodeResult) +
      sizeof(ContextualDynamicRangeDescriptor) + 12 * sizeof(std::size_t);
  const auto helper =
      std::max({lzss_position_distance_64m_token_range_working_bytes(),
                lzss_position_distance_64m_map_working_bytes(),
                lzss_position_distance_64m_range_encode_working_bytes(),
                lzss_position_distance_64m_token_working_bytes()});
  size_t total = 0;
  for (auto n :
       {owner(f.tokens), owner(f.expected), owner(f.out), owner(f.scratch),
        owner(ops), owner(os), owner(old), owner(priv), owner(first), owner(dt),
        owner(ds), controls, helper})
    ASSERT_TRUE(core::checked_add(total, n, total));
  ASSERT_LE(total, f.limits.max_internal_buffered_bytes);
  const auto retained = [&](size_t local, size_t working) {
    return total - local - working;
  };
  const auto token_bytes = f.tokens.size() * sizeof(Token);
  const auto q =
      f.query(0, 0,
              retained(token_bytes,
                       lzss_position_distance_64m_token_range_working_bytes()));
  EXPECT_EQ(q.aggregate_bytes, total);
  ASSERT_EQ(q.details.error, Error::payload_output_too_small);
  EXPECT_EQ(q.details.payload_size, f.expected.size());
  EXPECT_EQ(q.details.decision_count, v.d);
  EXPECT_EQ(q.details.operation_count, v.e);
  EXPECT_EQ(q.details.raw_size, v.f);
  const auto new_retained =
      retained(token_bytes + f.out.size() + f.scratch.size(),
               lzss_position_distance_64m_token_range_working_bytes());
  auto r = f.encode(new_retained);
  ASSERT_EQ(r.details.error, Error::none);
  EXPECT_EQ(r.bytes_committed, f.expected.size());
  EXPECT_EQ(f.descriptor.decision_count, v.d);
  EXPECT_EQ(f.descriptor.payload_size, f.expected.size());
  EXPECT_EQ(f.descriptor.context_count, 50);
  EXPECT_TRUE(std::equal(f.expected.begin(), f.expected.end(), f.out.begin()));
  EXPECT_TRUE(std::all_of(f.out.begin() + f.expected.size(), f.out.end(),
                          [](auto b) { return b == guard; }));
  EXPECT_TRUE(std::all_of(f.scratch.begin() + f.expected.size(),
                          f.scratch.end(), [](auto b) { return b == guard; }));
  LzssFieldContextResult md{};
  auto mapped = map_lzss_position_distance_64m_tokens(
      f.tokens, f.p, f.c, f.limits, ops, os, md,
      retained(token_bytes + 2 * v.e * sizeof(ModeledOperation),
               lzss_position_distance_64m_map_working_bytes()));
  ASSERT_EQ(mapped.details.error, LzssFieldContextError::none);
  ContextualDynamicRangeDescriptor desc{};
  auto prior = encode_lzss_position_distance_64m_range_operations(
      ops, f.limits, old, priv, desc,
      retained(v.e * sizeof(ModeledOperation) + old.size() + priv.size(),
               lzss_position_distance_64m_range_encode_working_bytes()));
  ASSERT_EQ(prior.details.error, ContextualDynamicRangeEncodeError::none);
  EXPECT_EQ(old, f.expected);
  EXPECT_EQ(desc.decision_count, f.descriptor.decision_count);
  EXPECT_EQ(desc.payload_size, f.descriptor.payload_size);
  EXPECT_EQ(desc.context_count, f.descriptor.context_count);
  auto decoded = decode_lzss_position_distance_64m_tokens(
      f.descriptor, std::span(f.out).first(f.expected.size()), f.p, f.c,
      f.limits, dt, ds,
      retained(f.expected.size() + 2 * v.t * sizeof(Token),
               lzss_position_distance_64m_token_working_bytes()));
  ASSERT_EQ(decoded.error, LzssContextualRangeDecodeError::none);
  for (size_t i = 0; i < dt.size(); ++i) {
    EXPECT_EQ(dt[i].kind, f.tokens[i].kind);
    EXPECT_EQ(dt[i].literal, f.tokens[i].literal);
    EXPECT_EQ(dt[i].distance, f.tokens[i].distance);
    EXPECT_EQ(dt[i].length, f.tokens[i].length);
  }
  first = f.out;
  EXPECT_EQ(f.encode(new_retained).details.error, Error::none);
  EXPECT_EQ(first, f.out);
  std::cout << "qualified_owner_bytes=" << v.name << ":" << total << "\n";
}
INSTANTIATE_TEST_SUITE_P(Independent, MathematicalVectors,
                         testing::Range(0u, 29u));
TEST(TokenRange, InvalidLateTokensAreTransactional) {
  for (unsigned mutation = 0; mutation < 8; ++mutation) {
    Fixture f(named("short_matches"));
    auto &t = f.tokens.back();
    switch (mutation) {
    case 0:
      t.kind = static_cast<Kind>(255);
      break;
    case 1:
      t.literal = 1;
      break;
    case 2:
      t.distance = 0;
      break;
    case 3:
      t.distance = 67108865;
      break;
    case 4:
      t.distance = 1000;
      break;
    case 5:
      t.length = 2;
      break;
    case 6:
      t.length = 259;
      break;
    case 7:
      f.p.max_match_length = 257;
      break;
    }
    f.failure();
  }
  Fixture f(named("all_literals"));
  f.tokens.back().length = 1;
  f.failure();
}
TEST(TokenRange, LargeDistanceClassesWithValidHistory) {
  for (unsigned dc = 13; dc <= 25; ++dc) {
    Fixture f(named("literal_A"));
    const auto distance = (1u << dc) + 3;
    uint32_t raw = 1;
    while (raw < distance) {
      const auto len = std::min(258u, distance - raw);
      if (len < 3) {
        f.tokens.push_back({Kind::literal, 65, 0, 0});
        ++raw;
      } else {
        f.tokens.push_back({Kind::match, 0, 1, len});
        raw += len;
      }
    }
    f.tokens.push_back({Kind::match, 0, distance, 3});
    raw += 3;
    uint32_t events = 0, decisions = 0;
    for (auto t : f.tokens) {
      if (t.kind == Kind::literal) {
        events += 2;
        decisions += 2;
      } else {
        const auto lc = t.length < 5 ? 8u : std::bit_width(t.length - 4) - 1u;
        const auto cls = std::bit_width(t.distance) - 1u;
        events += 3 + (lc != 0) + (cls != 0);
        decisions += 3 + (lc == 8 ? 1 : lc) + cls;
      }
    }
    f.c = {static_cast<uint32_t>(f.tokens.size()), events, decisions, raw, 0};
    std::vector<ModeledOperation> ops(events), os(events);
    LzssFieldContextResult md{};
    ASSERT_EQ(map_lzss_position_distance_64m_tokens(f.tokens, f.p, f.c,
                                                    f.limits, ops, os, md)
                  .details.error,
              LzssFieldContextError::none);
    const auto q = f.query(0, 0);
    ASSERT_EQ(q.details.error, Error::payload_output_too_small);
    f.out.assign(q.details.payload_size + 7, guard);
    f.scratch = f.out;
    ContextualDynamicRangeDescriptor desc{};
    std::vector<std::byte> old(q.details.payload_size), scratch(old.size());
    ASSERT_EQ(encode_lzss_position_distance_64m_range_operations(
                  ops, f.limits, old, scratch, desc)
                  .details.error,
              ContextualDynamicRangeEncodeError::none);
    ASSERT_EQ(f.encode().details.error, Error::none);
    EXPECT_TRUE(std::equal(old.begin(), old.end(), f.out.begin()));
    EXPECT_EQ(desc.decision_count, f.descriptor.decision_count);
    std::vector<Token> dt(f.tokens.size()), ds(dt.size());
    ASSERT_EQ(decode_lzss_position_distance_64m_tokens(
                  f.descriptor, std::span(f.out).first(old.size()), f.p, f.c,
                  f.limits, dt, ds)
                  .error,
              LzssContextualRangeDecodeError::none);
    EXPECT_EQ(dt.back().distance, distance);
    EXPECT_EQ(dt.back().length, 3u);
  }
}
TEST(TokenRange, InvalidParametersAndDeclarations) {
  for (unsigned mutation = 0; mutation < 11; ++mutation) {
    Fixture f(named("short_matches"));
    switch (mutation) {
    case 0:
      f.p.window_size = 0;
      break;
    case 1:
      f.p.window_size = 67108865;
      break;
    case 2:
      f.p.min_match_length = 2;
      break;
    case 3:
      f.p.max_match_length = 259;
      break;
    case 4:
      f.p.flags = 1;
      break;
    case 5:
      ++f.c.declared_token_count;
      break;
    case 6:
      --f.c.declared_raw_size;
      break;
    case 7:
      ++f.c.declared_raw_size;
      break;
    case 8:
      ++f.c.declared_event_count;
      break;
    case 9:
      ++f.c.declared_decision_count;
      break;
    case 10:
      f.c.declared_raw_size = 0;
      break;
    }
    f.failure();
  }
}
TEST(TokenRange, ShortMatchConfiguredMaximumFour) {
  Fixture f(named("literal_A"));
  f.tokens.push_back({Kind::match, 0, 1, 4});
  f.p.max_match_length = 4;
  f.c = {2, 6, 6, 5, 0};
  f.out.assign(32, guard);
  f.scratch = f.out;
  EXPECT_EQ(f.encode().details.error, Error::none);
}
TEST(TokenRange, ConfiguredLimits) {
  for (unsigned mutation = 0; mutation < 8; ++mutation) {
    Fixture f(named("short_matches"));
    switch (mutation) {
    case 0:
      f.limits.max_frame_size = f.c.declared_raw_size - 1;
      break;
    case 1:
      f.limits.max_block_size = f.c.declared_raw_size - 1;
      break;
    case 2:
      f.limits.max_total_output_size = f.c.declared_raw_size - 1;
      break;
    case 3:
      f.limits.max_entropy_table_entries = 2631;
      break;
    case 4:
      f.limits.max_range_model_total = 32767;
      break;
    case 5:
      f.limits.max_compressed_payload_size = f.expected.size() - 1;
      break;
    case 6:
      f.c.output_already_committed = UINT64_MAX;
      break;
    case 7:
      f.limits.max_internal_buffered_bytes = 0;
      break;
    }
    f.failure();
  }
}
TEST(TokenRange, ExactFullCapacityBudgetAndRetainedOwners) {
  Fixture f(named("short_matches"));
  f.limits.max_block_size = 1024;
  auto q = f.query(f.out.size(), f.scratch.size(), 1234);
  ASSERT_EQ(q.details.error, Error::none);
  EXPECT_EQ(q.aggregate_bytes, f.tokens.size() * sizeof(Token) + f.out.size() +
                                   f.scratch.size() + q.working_state_bytes +
                                   1234);
  f.limits.max_internal_buffered_bytes = q.aggregate_bytes;
  EXPECT_EQ(f.encode(1234).details.error, Error::none);
  --f.limits.max_internal_buffered_bytes;
  auto before = f.out;
  auto saved = f.descriptor;
  auto r = f.encode(1234);
  EXPECT_EQ(r.details.error, Error::limit_exceeded);
  EXPECT_EQ(f.out, before);
  EXPECT_EQ(std::memcmp(&saved, &f.descriptor, sizeof(saved)), 0);
  std::cout << "token_range_sizes=" << sizeof(Token) << ","
            << sizeof(LzssPositionDistance64mTokenRangePlan) << ","
            << sizeof(LzssPositionDistance64mTokenRangeResult) << ","
            << q.working_state_bytes << "\n";
}
TEST(TokenRange, CheckedCapacityAndPriorOutputOverflow) {
  Fixture f(named("literal_A"));
  constexpr auto maximum = std::numeric_limits<size_t>::max();
  EXPECT_EQ(f.query(maximum, 0).details.error, Error::arithmetic_overflow);
  EXPECT_EQ(f.query(0, maximum).details.error, Error::arithmetic_overflow);
  EXPECT_EQ(f.query(0, 0, maximum).details.error, Error::arithmetic_overflow);
  f.c.output_already_committed = UINT64_MAX;
  EXPECT_EQ(f.query(16, 16).details.error, Error::arithmetic_overflow);
}
TEST(TokenRange, ShortPayloadCapacitiesAreTransactional) {
  for (bool scratch : {false, true}) {
    Fixture f(named("all_literals"));
    (scratch ? f.scratch : f.out).resize(f.expected.size() - 1);
    f.failure();
  }
}
TEST(TokenRange, RejectsWholeExtentOverlapIncludingUnusedTails) {
  Fixture f(named("literal_A"));
  auto saved = f.descriptor;
  auto before = f.out;
  auto r = encode_lzss_position_distance_64m_token_range(
      f.tokens, f.p, f.c, f.limits, f.out, std::span(f.out).last(1),
      f.descriptor);
  EXPECT_EQ(r.details.error, Error::overlapping_buffers);
  EXPECT_EQ(r.bytes_committed, 0u);
  EXPECT_EQ(f.out, before);
  EXPECT_EQ(std::memcmp(&saved, &f.descriptor, sizeof(saved)), 0);
  auto tb = std::as_writable_bytes(std::span(f.tokens));
  auto original = f.tokens.front();
  r = encode_lzss_position_distance_64m_token_range(
      f.tokens, f.p, f.c, f.limits, tb, f.scratch, f.descriptor);
  EXPECT_EQ(r.details.error, Error::overlapping_buffers);
  EXPECT_EQ(f.tokens.front().literal, original.literal);
}
TEST(TokenRange, RejectsConfigurationAndDescriptorAliases) {
  Fixture f(named("literal_A"));
  auto saved = f.descriptor;
  for (auto bytes : {std::as_writable_bytes(std::span(&f.p, 1)),
                     std::as_writable_bytes(std::span(&f.c, 1)),
                     std::as_writable_bytes(std::span(&f.limits, 1)),
                     std::as_writable_bytes(std::span(&f.descriptor, 1))}) {
    auto old = std::vector<std::byte>(bytes.begin(), bytes.end());
    auto r = encode_lzss_position_distance_64m_token_range(
        f.tokens, f.p, f.c, f.limits, bytes, f.scratch, f.descriptor);
    EXPECT_EQ(r.details.error, Error::overlapping_buffers);
    EXPECT_TRUE(std::equal(old.begin(), old.end(), bytes.begin()));
  }
  EXPECT_EQ(std::memcmp(&saved, &f.descriptor, sizeof(saved)), 0);
}

TEST(TokenRange, ExactWindowDistanceUnreachableWithinResetFrame) {
  Fixture f(named("literal_A"));
  f.tokens.push_back({Kind::match, 0, 67108864, 3});
  f.c = {2, 7, 32, 67108864, 0};
  f.out.assign(64, guard);
  f.scratch = f.out;
  const auto before = f.out;
  const auto descriptor = f.descriptor;
  const auto r = f.encode();
  EXPECT_EQ(r.details.error, Error::invalid_token);
  EXPECT_EQ(r.details.token_index, 1u);
  EXPECT_EQ(r.bytes_committed, 0u);
  EXPECT_EQ(f.out, before);
  EXPECT_EQ(std::memcmp(&descriptor, &f.descriptor, sizeof(descriptor)), 0);
}
