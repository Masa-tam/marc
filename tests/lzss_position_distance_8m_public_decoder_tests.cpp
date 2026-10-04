#include "frame/lzss_position_distance_8m_stream_decoder.hpp"
#include "marc/marc.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace allocation_test {
thread_local unsigned calls{}, fail_at{};
thread_local void *guard{};
thread_local std::size_t bytes{};
} // namespace allocation_test
void *operator new(std::size_t n, const std::nothrow_t &) noexcept {
  using namespace allocation_test;
  ++calls;
  if (fail_at == calls)
    return nullptr;
  try {
    auto p = ::operator new(n);
    if (calls == 1) {
      guard = p;
      bytes = n;
    }
    return p;
  } catch (const std::bad_alloc &) {
    return nullptr;
  }
}
namespace {
using namespace marc;
using namespace frame::internal;
using Config = marc_lzss_position_distance_dynamic_range_8m_decoder_config;
using Req = marc_lzss_position_distance_dynamic_range_8m_decoder_requirements;
using Buffers = marc_lzss_position_distance_dynamic_range_8m_decoder_buffers;
using Token = dictionary::internal::LzssTypedToken;
constexpr std::byte mark{0xa5};
unsigned checks{};
void check(bool b, const char *s) {
  if (!b)
    throw std::runtime_error(s);
}
void row(const std::string &n) {
  ++checks;
  std::cout << "{\"case\":\"" << n << "\",\"passed\":true}\n";
}
Config config() {
  Config c{};
  check(marc_lzss_position_distance_dynamic_range_8m_decoder_config_init(&c) ==
            MARC_STATUS_OK,
        "init");
  c.max_total_output_size = 1048576;
  c.max_frame_size = c.max_block_size = 64;
  c.max_compressed_payload_size = 65536;
  c.max_internal_buffered_bytes = 4 * 1048576;
  c.max_entropy_table_entries = 2599;
  c.max_expansion_ratio = 1024;
  c.expansion_slack = 1048576;
  c.external_retained_bytes = 65536;
  c.input_capacity_bytes = 4096;
  c.output_capacity_bytes = 4096;
  return c;
}
Req requirements(const Config &c) {
  Req q{};
  q.struct_size = sizeof(q);
  q.abi_version = MARC_ABI_VERSION;
  check(
      marc_lzss_position_distance_dynamic_range_8m_decoder_workspace_requirements(
          &c, &q) == MARC_STATUS_OK,
      "query");
  return q;
}
struct Handle {
  marc_transform *p{};
  ~Handle() { marc_transform_destroy(p); }
};
struct Storage {
  Req q;
  std::vector<std::byte> serial, raw, scratch;
  // Aligned allocation is only storage here; the factory begins Token
  // lifetimes.
  std::unique_ptr<std::byte[]> token_owner, scratch_owner;
  Buffers b{};
  explicit Storage(const Config &c, std::size_t tail = 0)
      : q(requirements(c)), serial(q.serialized_bytes + tail, mark),
        raw(q.raw_bytes + tail, mark),
        scratch(q.raw_scratch_bytes + tail, mark),
        token_owner(new std::byte[q.token_bytes + tail * sizeof(Token)]),
        scratch_owner(
            new std::byte[q.token_scratch_bytes + tail * sizeof(Token)]) {
    b = {sizeof(b),
         MARC_ABI_VERSION,
         0,
         0,
         {reinterpret_cast<uint8_t *>(serial.data()), serial.size()},
         {reinterpret_cast<uint8_t *>(token_owner.get()),
          static_cast<std::size_t>(q.token_bytes + tail * sizeof(Token))},
         {reinterpret_cast<uint8_t *>(scratch_owner.get()),
          static_cast<std::size_t>(q.token_scratch_bytes +
                                   tail * sizeof(Token))},
         {reinterpret_cast<uint8_t *>(raw.data()), raw.size()},
         {reinterpret_cast<uint8_t *>(scratch.data()), scratch.size()}};
    check(reinterpret_cast<std::uintptr_t>(b.tokens.data) % q.token_alignment ==
              0,
          "test alignment");
    std::memset(b.tokens.data, 0xa5, b.tokens.size);
    std::memset(b.token_scratch.data, 0xa5, b.token_scratch.size);
  }
};
std::vector<std::byte> encode(const std::vector<std::byte> &raw) {
  marc_lzss_position_distance_dynamic_range_8m_config c{};
  check(marc_lzss_position_distance_dynamic_range_8m_config_init(&c) ==
            MARC_STATUS_OK,
        "encoder init");
  c.original_size = raw.size();
  c.frame_size = 64;
  c.max_frame_size = c.max_block_size = 64;
  c.max_total_output_size = 1048576;
  c.max_compressed_payload_size = 65536;
  c.max_internal_buffered_bytes = 4 * 1048576;
  c.max_entropy_table_entries = 2599;
  c.max_expansion_ratio = 1024;
  c.expansion_slack = 1048576;
  c.input_capacity_bytes = raw.size();
  c.output_capacity_bytes = 4096;
  Handle h;
  check(marc_lzss_position_distance_dynamic_range_8m_create_encoder(&c, &h.p) ==
            MARC_STATUS_OK,
        "encoder create");
  std::vector<std::byte> wire(4096, mark);
  auto r = marc_transform_process(
      h.p, {reinterpret_cast<const uint8_t *>(raw.data()), raw.size()},
      {reinterpret_cast<uint8_t *>(wire.data()), wire.size()},
      MARC_PROCESS_END_INPUT);
  check(r.status == MARC_STATUS_END_OF_STREAM && r.input_consumed == raw.size(),
        "encoder end");
  wire.resize(r.output_produced);
  return wire;
}
core::DecoderLimits limits(const Config &c) {
  core::DecoderLimits l{};
  l.max_total_output_size = c.max_total_output_size;
  l.max_frame_size = c.max_frame_size;
  l.max_block_size = c.max_block_size;
  l.max_compressed_payload_size = c.max_compressed_payload_size;
  l.max_internal_buffered_bytes = c.max_internal_buffered_bytes;
  l.max_lz_distance = c.max_lz_distance;
  l.max_lz_match_length = c.max_lz_match_length;
  l.max_entropy_table_entries = c.max_entropy_table_entries;
  l.max_range_model_total = c.max_range_model_total;
  l.max_expansion_ratio = c.max_expansion_ratio;
  l.expansion_slack = c.expansion_slack;
  l.max_dictionary_serialized_size = l.max_dictionary_entries =
      l.max_huffman_code_length = l.max_blocks_per_frame = 1;
  return l;
}
struct Outcome {
  marc_process_result r{};
  std::vector<std::byte> output, slot;
};
Outcome run(const std::vector<std::byte> &wire, Config c,
            std::size_t step = 4096, std::size_t outstep = 4096,
            bool oracle = false) {
  Storage s(c, 1);
  Handle h;
  std::vector<Token> ot(s.b.tokens.size / sizeof(Token)),
      os(s.b.token_scratch.size / sizeof(Token));
  std::unique_ptr<LzssPositionDistance8mStreamDecoder> private_decoder;
  if (oracle)
    private_decoder = std::make_unique<LzssPositionDistance8mStreamDecoder>(
        limits(c), s.serial, ot, os, s.raw, s.scratch,
        c.external_retained_bytes);
  else
    check(marc_lzss_position_distance_dynamic_range_8m_create_decoder(
              &c, &s.b, &h.p) == MARC_STATUS_OK,
          "decoder create");
  std::vector<std::byte> out(4096, mark);
  std::size_t used{}, made{};
  marc_process_result r{};
  for (unsigned iterations = 0; iterations < 10000; ++iterations) {
    const auto n = std::min(step, wire.size() - used),
               o = std::min(outstep, out.size() - made);
    auto flags =
        used + n == wire.size() ? MARC_PROCESS_END_INPUT : MARC_PROCESS_FLUSH;
    if (oracle) {
      auto x = private_decoder->process(std::span(wire).subspan(used, n),
                                        std::span(out).subspan(made, o), flags);
      r.input_consumed = x.input_consumed;
      r.output_produced = x.output_produced;
      r.status = x.status == core::StreamStatus::error
                     ? MARC_STATUS_INVALID_ARGUMENT +
                           static_cast<marc_status>(x.error.code) - 1
                     : static_cast<marc_status>(x.status) + 1;
      r.error_byte_position = x.error.byte_position;
      r.error_bit_position = x.error.bit_position;
    } else
      r = marc_transform_process(
          h.p, {reinterpret_cast<const uint8_t *>(wire.data() + used), n},
          {reinterpret_cast<uint8_t *>(out.data() + made), o}, flags);
    check(r.input_consumed <= n && r.output_produced <= o, "bounded counts");
    used += r.input_consumed;
    made += r.output_produced;
    if (r.status >= MARC_STATUS_INVALID_ARGUMENT ||
        r.status == MARC_STATUS_END_OF_STREAM) {
      if (!oracle) {
        auto z = marc_transform_process(h.p, {nullptr, 0}, {nullptr, 0}, 0);
        check(z.status == r.status &&
                  z.error_byte_position == r.error_byte_position &&
                  !z.input_consumed && !z.output_produced,
              "sticky");
      }
      check(std::all_of(out.begin() + made, out.end(),
                        [](auto b) { return b == mark; }),
            "downstream tail unchanged");
      out.resize(made);
      return {r, out, s.raw};
    }
    check(r.input_consumed || r.output_produced ||
              r.status != MARC_STATUS_PROGRESS,
          "progress contract");
  }
  throw std::runtime_error("bounded completion");
}
void differential(const std::vector<std::byte> &wire, Config c,
                  const char *name) {
  auto a = run(wire, c), b = run(wire, c, 4096, 4096, true);
  check(a.r.status == b.r.status &&
            a.r.error_byte_position == b.r.error_byte_position &&
            a.r.error_bit_position == b.r.error_bit_position,
        "oracle error equality");
  check(a.output == b.output && a.slot == b.slot,
        "oracle committed output/whole raw slot");
  row(name);
}
void store32(std::vector<std::byte> &b, std::size_t p, std::uint32_t n) {
  for (unsigned i = 0; i < 4; ++i)
    b[p + i] = std::byte(n >> (8 * i));
}
void store64(std::vector<std::byte> &b, std::size_t p, std::uint64_t n) {
  for (unsigned i = 0; i < 8; ++i)
    b[p + i] = std::byte(n >> (8 * i));
}
void acceptance() {
  auto c = config();
  auto q = requirements(c);
  check(q.token_elements == 64 && q.token_bytes == 64 * sizeof(Token) &&
            q.token_alignment == alignof(Token) && q.serialized_bytes == 1237 &&
            q.raw_bytes == 64 &&
            q.admission_scope == MARC_LZSS_POSITION_DISTANCE_8M_CAPACITY_ONLY,
        "sizing");
  row("measured-token-sizing");
  auto expanded = c;
  expanded.input_capacity_bytes += 7;
  expanded.output_capacity_bytes += 11;
  check(requirements(expanded).minimum_aggregate_bytes ==
            q.minimum_aggregate_bytes + 18,
        "full declared IO reservation");
  row("full-declared-io-reservation");
  for (unsigned field = 0; field < 3; ++field) {
    auto invalid = c;
    if (field == 0)
      invalid.max_entropy_table_entries = 2598;
    if (field == 1)
      invalid.max_range_model_total = 32767;
    if (field == 2)
      invalid.max_lz_match_length = 2;
    auto result = q;
    check(
        marc_lzss_position_distance_dynamic_range_8m_decoder_workspace_requirements(
            &invalid, &result) == MARC_STATUS_LIMIT_EXCEEDED &&
            !std::memcmp(&q, &result, sizeof(q)),
        "model/match minimum refusal");
    row("model-minimum-" + std::to_string(field));
  }
  c.max_block_size = 32;
  auto small = requirements(c);
  check(small.raw_bytes == 32 && small.serialized_bytes == 661,
        "block cap sizing");
  row("block-ceiling");
  c = config();
  c.max_compressed_payload_size = 1;
  auto empty = requirements(c);
  check(empty.serialized_bytes == 81, "payload min");
  differential(encode({}), c, "empty-payload-one");
  c = config();
  Storage s(c);
  c.max_internal_buffered_bytes = q.minimum_aggregate_bytes;
  Handle h;
  check(marc_lzss_position_distance_dynamic_range_8m_create_decoder(
            &c, &s.b, &h.p) == MARC_STATUS_OK,
        "exact budget");
  marc_transform_destroy(h.p);
  h.p = nullptr;
  row("exact-budget");
  c.max_internal_buffered_bytes--;
  h.p = reinterpret_cast<marc_transform *>(std::uintptr_t{1});
  check(marc_lzss_position_distance_dynamic_range_8m_create_decoder(
            &c, &s.b, &h.p) == MARC_STATUS_LIMIT_EXCEEDED &&
            !h.p,
        "one below");
  row("one-below-budget");
  c = config();
  Storage larger(c, 1);
  c.max_internal_buffered_bytes = q.minimum_aggregate_bytes;
  check(marc_lzss_position_distance_dynamic_range_8m_create_decoder(
            &c, &larger.b, &h.p) == MARC_STATUS_LIMIT_EXCEEDED &&
            !h.p,
        "retained tails");
  row("tail-budget-refusal");
  c = config();
  auto saved = q;
  q.struct_size--;
  check(
      marc_lzss_position_distance_dynamic_range_8m_decoder_workspace_requirements(
          &c, &q) == MARC_STATUS_INVALID_ARGUMENT &&
          q.struct_size == saved.struct_size - 1,
      "result metadata");
  row("result-metadata");
  q = saved;
  c.reserved = 1;
  check(
      marc_lzss_position_distance_dynamic_range_8m_decoder_workspace_requirements(
          &c, &q) == MARC_STATUS_INVALID_ARGUMENT &&
          !std::memcmp(&q, &saved, sizeof(q)),
      "query invariant");
  row("query-failure-invariant");
  c = config();
  check(
      marc_lzss_position_distance_dynamic_range_8m_decoder_workspace_requirements(
          &c, reinterpret_cast<Req *>(&c)) == MARC_STATUS_INVALID_ARGUMENT,
      "query alias");
  row("query-alias");
  c.external_retained_bytes = std::numeric_limits<std::uint64_t>::max();
  q = saved;
  check(
      marc_lzss_position_distance_dynamic_range_8m_decoder_workspace_requirements(
          &c, &q) == MARC_STATUS_LIMIT_EXCEEDED &&
          !std::memcmp(&q, &saved, sizeof(q)),
      "overflow");
  row("checked-external-overflow");
  c = config();
  for (unsigned failure = 1; failure <= 2; ++failure) {
    Storage t(c);
    allocation_test::calls = 0;
    allocation_test::fail_at = failure;
    auto r = marc_lzss_position_distance_dynamic_range_8m_create_decoder(
        &c, &t.b, &h.p);
    allocation_test::fail_at = 0;
    check(r == MARC_STATUS_OUT_OF_MEMORY && !h.p &&
              std::all_of(t.raw.begin(), t.raw.end(),
                          [](auto b) { return b == mark; }),
          "scalar refusal");
    check(std::all_of(reinterpret_cast<std::byte *>(t.b.tokens.data),
                      reinterpret_cast<std::byte *>(t.b.tokens.data) +
                          t.b.tokens.size,
                      [](auto b) { return b == mark; }),
          "pre-lifetime tokens unchanged");
    row("scalar-refusal-" + std::to_string(failure));
  }
  for (unsigned field = 0; field < 5; ++field) {
    auto b = s.b;
    std::array<marc_buffer *, 5> fields{
        &b.serialized, &b.tokens, &b.token_scratch, &b.raw, &b.raw_scratch};
    fields[field]->size--;
    check(marc_lzss_position_distance_dynamic_range_8m_create_decoder(
              &c, &b, &h.p) == MARC_STATUS_INVALID_ARGUMENT &&
              !h.p,
          "undersized workspace");
    row("undersized-" + std::to_string(field));
  }
  for (unsigned a = 0; a < 5; ++a)
    for (unsigned z = a + 1; z < 5; ++z) {
      auto b = s.b;
      std::array<marc_buffer *, 5> f{&b.serialized, &b.tokens, &b.token_scratch,
                                     &b.raw, &b.raw_scratch};
      f[z]->data = f[a]->data;
      auto previous = h.p =
          reinterpret_cast<marc_transform *>(std::uintptr_t{1});
      check(marc_lzss_position_distance_dynamic_range_8m_create_decoder(
                &c, &b, &h.p) == MARC_STATUS_INVALID_ARGUMENT &&
                h.p == previous,
            "workspace alias retains out");
      h.p = nullptr;
      row("workspace-alias-" + std::to_string(a) + "-" + std::to_string(z));
    }
  for (unsigned field = 1; field <= 2; ++field) {
    auto b = s.b;
    auto &v = field == 1 ? b.tokens : b.token_scratch;
    v.data = v.data + 1;
    check(marc_lzss_position_distance_dynamic_range_8m_create_decoder(
              &c, &b, &h.p) == MARC_STATUS_INVALID_ARGUMENT &&
              !h.p,
          "alignment");
    row("typed-alignment-" + std::to_string(field));
  }
  auto b = s.b;
  b.tokens.size += 1;
  check(marc_lzss_position_distance_dynamic_range_8m_create_decoder(
            &c, &b, &h.p) == MARC_STATUS_INVALID_ARGUMENT &&
            !h.p,
        "divisibility");
  row("typed-divisibility");
  auto out_alias = reinterpret_cast<marc_transform **>(s.raw.data());
  std::array<std::byte, sizeof(void *)> before{};
  std::memcpy(before.data(), s.raw.data(), before.size());
  check(marc_lzss_position_distance_dynamic_range_8m_create_decoder(
            &c, &s.b, out_alias) == MARC_STATUS_INVALID_ARGUMENT &&
            !std::memcmp(before.data(), s.raw.data(), before.size()),
        "output alias unchanged");
  row("output-workspace-alias");
  b = s.b;
  b.reserved = 1;
  check(marc_lzss_position_distance_dynamic_range_8m_create_decoder(
            &c, &b, &h.p) == MARC_STATUS_INVALID_ARGUMENT &&
            !h.p,
        "descriptor reserved");
  row("descriptor-reserved");
  b = s.b;
  b.raw.data = nullptr;
  check(marc_lzss_position_distance_dynamic_range_8m_create_decoder(
            &c, &b, &h.p) == MARC_STATUS_INVALID_ARGUMENT &&
            !h.p,
        "null nonempty workspace");
  row("null-nonempty-workspace");
  b = s.b;
  b.raw.data = reinterpret_cast<uint8_t *>(
      std::numeric_limits<std::uintptr_t>::max() - 1);
  check(marc_lzss_position_distance_dynamic_range_8m_create_decoder(
            &c, &b, &h.p) == MARC_STATUS_INVALID_ARGUMENT,
        "workspace address overflow");
  row("workspace-address-overflow");
  allocation_test::calls = 0;
  b = s.b;
  check(marc_lzss_position_distance_dynamic_range_8m_create_decoder(
            &c, &b, &h.p) == MARC_STATUS_OK,
        "measurement create");
  auto pq = query_lzss_position_distance_8m_stream_workspace(
      limits(c), s.serial.size(), s.b.tokens.size / sizeof(Token),
      s.b.token_scratch.size / sizeof(Token), s.raw.size(), s.scratch.size(),
      0);
  check(pq.error == core::ErrorCode::none && allocation_test::calls == 2,
        "two scalar allocations only");
  std::cout << "{\"sizes\":{\"config\":" << sizeof(Config)
            << ",\"requirements\":" << sizeof(Req)
            << ",\"buffers\":" << sizeof(Buffers)
            << ",\"token\":" << sizeof(Token)
            << ",\"alignment\":" << alignof(Token)
            << ",\"guard\":" << allocation_test::bytes
            << ",\"private_owner\":" << pq.owner_bytes
            << ",\"private_controls\":" << pq.control_bytes
            << ",\"private_helpers\":" << pq.helper_bytes
            << ",\"aggregate\":" << q.minimum_aggregate_bytes << "}}\n";
  row("source-bound-owner-and-query-measurement");
}
void processing() {
  auto c = config();
  std::vector<std::byte> raw(160);
  for (std::size_t i = 0; i < raw.size(); ++i)
    raw[i] = std::byte((i % 64) % 7);
  auto wire = encode(raw);
  auto ok = run(wire, c);
  check(ok.output == raw && ok.r.status == MARC_STATUS_END_OF_STREAM,
        "roundtrip");
  differential(wire, c, "multiframe-oracle");
  for (auto step :
       {std::size_t{1}, std::size_t{7}, std::size_t{111}, std::size_t{4096}})
    for (auto capacity : {std::size_t{1}, std::size_t{17}, std::size_t{4096}}) {
      auto r = run(wire, c, step, capacity);
      check(r.output == raw && r.r.status == MARC_STATUS_END_OF_STREAM,
            "split equality");
      row("split-" + std::to_string(step) + "-" + std::to_string(capacity));
    }
  check(run(encode({}), c, 1, 1).output.empty(), "empty");
  row("empty");
  for (unsigned x = 0; x < 256; ++x) {
    auto v = std::vector<std::byte>{std::byte(x)};
    check(run(encode(v), c, 1, 1).output == v, "every byte");
  }
  row("all-256-one-byte-values");
  std::vector<std::byte> all(256);
  for (unsigned i = 0; i < 256; ++i)
    all[i] = std::byte(i);
  check(run(encode(all), c, 7, 1).output == all, "all bytes");
  row("all-byte-sequence");
  for (auto n : {63, 64, 65, 127, 128, 129}) {
    std::vector<std::byte> v(n, std::byte{0});
    check(run(encode(v), c, 1, 1).output == v, "frame boundary");
    row("frame-boundary-" + std::to_string(n));
  }
  for (std::size_t p :
       {std::size_t{0}, std::size_t{12}, std::size_t{14}, std::size_t{16},
        std::size_t{18}, std::size_t{20}, std::size_t{24}, std::size_t{108},
        std::size_t{112}, std::size_t{112 + 4}, std::size_t{112 + 20},
        std::size_t{112 + 72}}) {
    auto bad = wire;
    bad[p] ^= std::byte{0x80};
    differential(bad, c, ("malformed-field-" + std::to_string(p)).c_str());
    check(run(bad, c).r.status >= MARC_STATUS_INVALID_ARGUMENT,
          "negative error");
  }
  // Every strict prefix truncation of this small fixture, including payload
  // suffixes.
  for (std::size_t n = 0; n < wire.size(); ++n) {
    auto bad = wire;
    bad.resize(n);
    auto a = run(bad, c), b = run(bad, c, 4096, 4096, true);
    check(a.r.status >= MARC_STATUS_INVALID_ARGUMENT &&
              a.r.status == b.r.status && a.output == b.output &&
              a.slot == b.slot,
          "truncation oracle");
  }
  row("all-fixture-truncations");
  auto bad = wire;
  bad[192] = std::byte{0xff};
  differential(bad, c, "invalid-range-payload");
  auto fail = run(bad, c);
  check(fail.r.status == MARC_STATUS_MALFORMED_STREAM && fail.output.empty() &&
            std::all_of(fail.slot.begin(), fail.slot.end(),
                        [](auto b) { return b == mark; }),
        "failed first raw slot unchanged");
  row("first-frame-nonpublication");
  bad = wire;
  store32(bad, 64, 1);
  differential(bad, c, "invalid-lz-distance-under-window-one");
  auto invalid_reference = run(bad, c);
  check(invalid_reference.r.status == MARC_STATUS_MALFORMED_STREAM &&
            invalid_reference.output.empty() &&
            std::all_of(invalid_reference.slot.begin(),
                        invalid_reference.slot.end(),
                        [](auto b) { return b == mark; }),
        "invalid reference nonpublication");
  row("invalid-reference-raw-slot-invariant");
  bad = wire;
  store32(bad, 64, 32);
  store32(bad, 72, 64);
  auto bounded_header = run(bad, c);
  check(bounded_header.output == raw &&
            bounded_header.r.status == MARC_STATUS_END_OF_STREAM,
        "bounded header parameters");
  row("bounded-window-and-match-header-accepted");
  bad = wire;
  store64(bad, 40, 161);
  differential(bad, c, "contradictory-original-size");
  // Locate the second frame using the documented payload-length field, rather
  // than fixing an entropy payload length in the regression.
  std::uint32_t first_payload{};
  for (unsigned i = 0; i < 4; ++i)
    first_payload |=
        std::uint32_t(std::to_integer<unsigned>(wire[112 + 32 + i])) << (8 * i);
  const auto second = 112 + 80 + first_payload;
  check(second + 80 < wire.size(), "second frame location");
  bad = wire;
  bad[second + 80] = std::byte{0xff};
  differential(bad, c, "late-frame-invalid-range");
  auto late = run(bad, c);
  check(late.r.status == MARC_STATUS_MALFORMED_STREAM &&
            late.output.size() == 64 &&
            std::equal(raw.begin(), raw.begin() + 64, late.output.begin()) &&
            std::equal(raw.begin(), raw.begin() + 64, late.slot.begin()) &&
            late.slot.back() == mark,
        "late failure preserves prior whole raw slot");
  row("late-frame-preserves-prior-publication");
  bad = wire;
  bad.push_back(std::byte{0});
  differential(bad, c, "strict-trailing");
  check(run(bad, c).output == raw, "prior final frame committed");
  row("trailing-retains-valid-frames");
  c.max_lz_distance = 1;
  differential(wire, c, "header-window-limit");
  c = config();
  c.max_lz_match_length = 3;
  differential(wire, c, "header-match-limit");
  c = config();
  c.max_expansion_ratio = 1;
  c.expansion_slack = 0;
  differential(wire, c, "frame-expansion-limit");
  c = config();
  for (unsigned workspace = 0; workspace < 5; ++workspace) {
    Storage s(c, 1);
    Handle h;
    check(marc_lzss_position_distance_dynamic_range_8m_create_decoder(
              &c, &s.b, &h.p) == MARC_STATUS_OK,
          "alias create");
    auto b = s.b;
    std::array<marc_buffer, 5> f{b.serialized, b.tokens, b.token_scratch, b.raw,
                                 b.raw_scratch};
    auto last = reinterpret_cast<std::byte *>(f[workspace].data) +
                f[workspace].size - 1;
    auto r = marc_transform_process(h.p, {reinterpret_cast<uint8_t *>(last), 1},
                                    {nullptr, 0}, 0);
    check(r.status == MARC_STATUS_INVALID_ARGUMENT && !r.input_consumed &&
              !r.output_produced,
          "full workspace input alias");
    row("process-tail-alias-" + std::to_string(workspace));
  }
  for (unsigned kind = 0; kind < 5; ++kind) {
    Storage s(c);
    Handle h;
    allocation_test::calls = 0;
    check(marc_lzss_position_distance_dynamic_range_8m_create_decoder(
              &c, &s.b, &h.p) == MARC_STATUS_OK,
          "guard create");
    std::array<std::byte, 4097> in{}, out{};
    marc_const_buffer iv{nullptr, 0};
    marc_buffer ov{nullptr, 0};
    marc_process_flags flags = 0;
    if (kind == 0)
      iv = {reinterpret_cast<uint8_t *>(in.data()), in.size()};
    if (kind == 1)
      ov = {reinterpret_cast<uint8_t *>(out.data()), out.size()};
    if (kind == 2)
      iv = {reinterpret_cast<uint8_t *>(allocation_test::guard) +
                allocation_test::bytes - 1,
            1};
    if (kind == 3)
      ov = {reinterpret_cast<uint8_t *>(h.p), 1};
    if (kind == 4)
      flags = MARC_PROCESS_RESET_BLOCK;
    auto r = marc_transform_process(h.p, iv, ov, flags);
    check(r.status >= MARC_STATUS_INVALID_ARGUMENT && !r.input_consumed &&
              !r.output_produced,
          "guard refusal");
    auto expected = kind < 2   ? MARC_STATUS_LIMIT_EXCEEDED
                    : kind < 4 ? MARC_STATUS_INVALID_ARGUMENT
                               : MARC_STATUS_UNSUPPORTED;
    check(r.status == expected, "guard category");
    row("process-guard-" + std::to_string(kind));
  }
  Storage s(c);
  Handle h;
  check(marc_lzss_position_distance_dynamic_range_8m_create_decoder(
            &c, &s.b, &h.p) == MARC_STATUS_OK,
        "zero output create");
  auto r = marc_transform_process(
      h.p, {reinterpret_cast<uint8_t *>(wire.data()), wire.size()},
      {nullptr, 0}, MARC_PROCESS_END_INPUT);
  check(r.status == MARC_STATUS_NEED_OUTPUT && r.output_produced == 0,
        "zero output need");
  std::array<std::byte, 160> out{};
  auto z = marc_transform_process(
      h.p,
      {reinterpret_cast<const uint8_t *>(wire.data() + r.input_consumed),
       wire.size() - r.input_consumed},
      {reinterpret_cast<uint8_t *>(out.data()), out.size()},
      MARC_PROCESS_END_INPUT);
  check(z.status == MARC_STATUS_END_OF_STREAM &&
            std::equal(raw.begin(), raw.end(), out.begin()),
        "zero capacity resume");
  row("zero-output-resume");
}
} // namespace
int main() {
  try {
    acceptance();
    processing();
    std::cout << "PASS " << checks << " grouped decoder checks\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << "\n";
    return 1;
  }
}
