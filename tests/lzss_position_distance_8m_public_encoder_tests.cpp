#include "frame/lzss_position_distance_8m_stream_decoder.hpp"
#include "frame/lzss_position_distance_8m_stream_encoder.hpp"
#include "lzss_position_distance_8m_late_fault_seam.hpp"
#include "lzss_position_distance_8m_owning_allocator_seam.hpp"
#include "marc/marc.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
// Standalone test replacement of the scalar nothrow allocation entry only.
// The ordinary throwing allocation/deletion remain the real runtime pair.
namespace public_nothrow_test {
thread_local std::size_t calls{}, fail_at{}, injections{};
thread_local void *first_pointer{};
thread_local std::size_t first_bytes{};
} // namespace public_nothrow_test
void *operator new(std::size_t n, const std::nothrow_t &) noexcept {
  using namespace public_nothrow_test;
  if (fail_at && ++calls == fail_at) {
    ++injections;
    return nullptr;
  }
  try {
    auto *p = ::operator new(n);
    if (fail_at && calls == 1) {
      first_pointer = p;
      first_bytes = n;
    }
    return p;
  } catch (const std::bad_alloc &) {
    return nullptr;
  }
}
namespace {
using namespace marc;
using namespace frame::internal;
using Config = marc_lzss_position_distance_dynamic_range_8m_config;
using Resources = marc_lzss_position_distance_dynamic_range_8m_resources;
using Token = dictionary::internal::LzssTypedToken;
using Op = context::internal::ModeledOperation;
constexpr std::byte sentinel{0xa5};
bool isolated{};
unsigned checks{};
void check(bool yes, const char *what) {
  if (!yes)
    throw std::runtime_error(what);
}
void row(const std::string &name) {
  ++checks;
  std::cout << "{\"case\":\"" << name << "\",\"passed\":true}\n";
}
Config config(std::size_t n = 160) {
  Config c{};
  check(marc_lzss_position_distance_dynamic_range_8m_config_init(&c) ==
            MARC_STATUS_OK,
        "init");
  c.original_size = n;
  c.frame_size = 64;
  c.max_frame_size = c.max_block_size = 64;
  c.max_total_output_size = 1048576;
  c.max_compressed_payload_size = 65536;
  c.max_internal_buffered_bytes = 4 * 1048576;
  c.max_entropy_table_entries = 2599;
  c.max_expansion_ratio = 1024;
  c.expansion_slack = 1048576;
  c.input_capacity_bytes = 160;
  c.output_capacity_bytes = 4096;
  return c;
}
struct Handle {
  marc_transform *p{};
  ~Handle() { marc_transform_destroy(p); }
};
struct Fixture {
  Config c{config()};
  std::array<std::byte, 160> raw{};
  std::array<std::byte, 64> op_raw{}, dec_raw{}, dec_scratch{};
  std::array<std::uint32_t, 65600> index{};
  std::array<Token, 64> tokens{}, scratch{}, dec_tokens{}, dec_token_scratch{};
  std::array<Op, 128> ops{}, op_scratch{};
  std::array<std::byte, 2048> publication{}, frame{}, payload{}, serialized{};
  std::array<std::byte, 4112> oracle{}, output{};
  std::array<std::byte, 176> decoded{};
  std::array<std::size_t, 4> boundaries{};
  std::size_t wire{};
  Fixture() {
    for (std::size_t i = 0; i < raw.size(); ++i)
      raw[i] = std::byte((i % 64) % 7);
    // All retained test owners and seam/control reserves are charged.
    c.external_retained_bytes =
        sizeof(Fixture) + test::late8m::working_bytes() +
        4 * sizeof(test::owning8m::Controller) +
        sizeof(LzssPositionDistance8mStreamEncoder) +
        sizeof(LzssPositionDistance8mStreamDecoder) + 65536;
  }
  core::DecoderLimits limits() const {
    core::DecoderLimits l{};
    l.max_frame_size = l.max_block_size = 64;
    l.max_compressed_payload_size = 65536;
    l.max_internal_buffered_bytes = c.max_internal_buffered_bytes;
    return l;
  }
  TypedContextStreamHeader header() const {
    return {64, 160, {8388608, 3, 258, 0}, 32768, 47, 11, 1, 12};
  }
  void guards(std::span<const std::byte> s) {
    check(std::ranges::all_of(s, [](auto x) { return x == sentinel; }),
          "output guard");
  }
  void prepare() {
    oracle.fill(sentinel);
    const LzssPositionDistance8mStreamEncodeCapacities caps{
        64, 2048, 64, 64, 65600, 128, 128, 2048, 2048, 160, 4096};
    auto q = query_lzss_position_distance_8m_stream_encode_workspace(
        limits(), caps, c.external_retained_bytes);
    check(q.error == core::ErrorCode::none, "oracle admission");
    LzssPositionDistance8mStreamEncoder e(
        header(), limits(), op_raw, publication,
        {tokens, scratch, index, ops, op_scratch, frame, payload},
        c.external_retained_bytes);
    auto r =
        e.process(raw, std::span(oracle).first(4096), MARC_PROCESS_END_INPUT);
    check(r.status == core::StreamStatus::end_of_stream &&
              r.input_consumed == 160,
          "oracle end");
    wire = r.output_produced;
    boundaries[0] = 112;
    guards(std::span(oracle).subspan(wire));
    for (std::size_t j = 0; j < 3; ++j) {
      TypedContextFrameLayout layout{};
      LzssPositionDistance8mFrameRequirements needed{};
      auto error = preflight_lzss_position_distance_8m_frame_prefix(
          std::span(oracle).subspan(boundaries[j], wire - boundaries[j]),
          {header(), limits(), j, j * 64}, layout, needed,
          c.external_retained_bytes);
      check(error == LzssPositionDistance8mPreflightError::none,
            "oracle prefix");
      check(layout.header.token_count == 8, "match fixture token count");
      boundaries[j + 1] = boundaries[j] + layout.serialized_size;
    }
    check(boundaries[3] == wire, "oracle boundaries");
    decoded.fill(sentinel);
    LzssPositionDistance8mStreamDecoder d(
        limits(), serialized, dec_tokens, dec_token_scratch, dec_raw,
        dec_scratch, c.external_retained_bytes);
    auto x = d.process(std::span(oracle).first(wire),
                       std::span(decoded).first(160), MARC_PROCESS_END_INPUT);
    check(x.status == core::StreamStatus::end_of_stream &&
              x.output_produced == 160,
          "oracle decode");
    check(std::equal(raw.begin(), raw.end(), decoded.begin()), "raw equality");
    guards(std::span(decoded).subspan(160));
  }
  marc_process_result run(marc_transform *h, bool staged, std::size_t n = 160) {
    output.fill(sentinel);
    std::size_t used{}, made{};
    if (staged) {
      auto z = marc_transform_process(h, {nullptr, 0}, {nullptr, 0}, 0);
      check(z.status == MARC_STATUS_NEED_OUTPUT && !z.input_consumed &&
                !z.output_produced,
            "zero output");
    }
    for (std::size_t step = 0; step < 20000; ++step) {
      auto in = staged ? std::min<std::size_t>(1, n - used) : n - used;
      auto out = staged ? std::size_t{1} : 4096 - made;
      auto r = marc_transform_process(
          h, {reinterpret_cast<const uint8_t *>(raw.data() + used), in},
          {reinterpret_cast<uint8_t *>(output.data() + made), out},
          used + in == n ? MARC_PROCESS_END_INPUT : MARC_PROCESS_FLUSH);
      check(r.input_consumed <= in && r.output_produced <= out, "counts");
      check(r.status != MARC_STATUS_PROGRESS || r.input_consumed ||
                r.output_produced,
            "progress");
      used += r.input_consumed;
      made += r.output_produced;
      if (r.status >= 100 || r.status == MARC_STATUS_END_OF_STREAM) {
        check(std::equal(output.begin(), output.begin() + made, oracle.begin()),
              "committed prefix equality");
        guards(std::span(output).subspan(made));
        if (r.status == MARC_STATUS_END_OF_STREAM)
          check(used == n && made == wire, "complete wire equality");
        auto sticky = marc_transform_process(
            h, {reinterpret_cast<const uint8_t *>(raw.data()), 160},
            {reinterpret_cast<uint8_t *>(output.data() + made), 1}, UINT32_MAX);
        check(sticky.status == r.status &&
                  sticky.error_byte_position == r.error_byte_position &&
                  sticky.error_bit_position == r.error_bit_position &&
                  !sticky.input_consumed && !sticky.output_produced,
              "sticky");
        guards(std::span(output).subspan(made));
        r.reserved[0] =
            static_cast<uint8_t>(used); // test-local consumed total (<256)
        // Total publication is checked independently by expected boundary.
        r.output_produced = made;
        return r;
      }
    }
    check(false, "finite steps");
    return {};
  }
};
void metadata() {
  Config c{};
  check(marc_lzss_position_distance_dynamic_range_8m_config_init(nullptr) ==
            MARC_STATUS_INVALID_ARGUMENT,
        "null init");
  check(marc_lzss_position_distance_dynamic_range_8m_config_init(&c) ==
            MARC_STATUS_OK,
        "template init");
  Resources out{};
  std::memset(&out, 0xa5, sizeof(out));
  auto old = out;
  check(!c.max_internal_buffered_bytes && !c.max_total_output_size &&
            !c.max_entropy_table_entries,
        "explicit template");
  check(marc_lzss_position_distance_dynamic_range_8m_resource_requirements(
            &c, &out) == MARC_STATUS_INVALID_ARGUMENT,
        "incomplete query");
  check(std::memcmp(&out, &old, sizeof(out)) == 0, "query preserved");
  row("init-template");
  c = config();
  auto bytes = c;
  auto *alias = reinterpret_cast<Resources *>(&c);
  check(marc_lzss_position_distance_dynamic_range_8m_resource_requirements(
            &c, alias) == MARC_STATUS_INVALID_ARGUMENT,
        "query alias");
  check(std::memcmp(&c, &bytes, sizeof(c)) == 0, "alias unchanged");
  check(marc_lzss_position_distance_dynamic_range_8m_create_encoder(
            &c, reinterpret_cast<marc_transform **>(&c)) ==
            MARC_STATUS_INVALID_ARGUMENT,
        "create alias");
  check(std::memcmp(&c, &bytes, sizeof(c)) == 0, "create alias unchanged");
  row("metadata-alias");
  for (unsigned mode = 0; mode < 10; ++mode) {
    c = config();
    if (mode == 0)
      c.struct_size--;
    if (mode == 1)
      c.abi_version++;
    if (mode == 2)
      c.reserved = 1;
    if (mode == 3)
      c.reserved2 = 1;
    if (mode == 4)
      c.encoder_strategy = 0;
    if (mode == 5)
      c.encoder_strategy = 2;
    if (mode == 6)
      c.frame_size = 0;
    if (mode == 7)
      c.frame_size = 8388609;
    if (mode == 8)
      c.max_internal_buffered_bytes = 0;
    if (mode == 9)
      c.max_expansion_ratio = 0;
    out = old;
    Handle h;
    h.p = reinterpret_cast<marc_transform *>(1);
    check(marc_lzss_position_distance_dynamic_range_8m_resource_requirements(
              &c, &out) == MARC_STATUS_INVALID_ARGUMENT,
          "invalid metadata");
    check(std::memcmp(&out, &old, sizeof(out)) == 0, "invalid preserves query");
    check(marc_lzss_position_distance_dynamic_range_8m_create_encoder(
              &c, &h.p) == MARC_STATUS_INVALID_ARGUMENT &&
              !h.p,
          "invalid create null");
    row("invalid-config-" + std::to_string(mode));
  }
  for (unsigned mode = 0; mode < 10; ++mode) {
    c = config();
    if (mode == 0)
      c.external_retained_bytes = UINT64_MAX;
    if (mode == 1)
      c.input_capacity_bytes = UINT64_MAX;
    if (mode == 2)
      c.output_capacity_bytes = UINT64_MAX;
    if (mode == 3)
      c.original_size = UINT64_MAX;
    if (mode == 4)
      c.max_lz_distance = 8388607;
    if (mode == 5)
      c.max_entropy_table_entries = 2598;
    if (mode == 6)
      c.max_range_model_total = 32767;
    if (mode == 7)
      c.max_lz_match_length = 257;
    if (mode == 8)
      c.max_frame_size = 63;
    if (mode == 9)
      c.max_total_output_size = 159;
    out = old;
    Handle h;
    check(marc_lzss_position_distance_dynamic_range_8m_resource_requirements(
              &c, &out) == MARC_STATUS_LIMIT_EXCEEDED,
          "overflow/semantic limit");
    check(std::memcmp(&out, &old, sizeof(out)) == 0, "limit preserves query");
    check(marc_lzss_position_distance_dynamic_range_8m_create_encoder(
              &c, &h.p) == MARC_STATUS_LIMIT_EXCEEDED &&
              !h.p,
          "limit null");
    row("limit-config-" + std::to_string(mode));
  }
  c = config();
  check(marc_lzss_position_distance_dynamic_range_8m_resource_requirements(
            &c, &out) == MARC_STATUS_OK,
        "resource query");
  check(out.admission_scope == MARC_LZSS_POSITION_DISTANCE_8M_INITIAL_ONLY &&
            out.initial_raw_bytes == 64 && out.initial_index_entries == 65600 &&
            out.initial_bytes == out.fixed_bytes + 64 + 4 * 65600,
        "initial arithmetic");
  auto q = out;
  auto bigger = c;
  bigger.input_capacity_bytes += 13;
  bigger.output_capacity_bytes += 17;
  check(marc_lzss_position_distance_dynamic_range_8m_resource_requirements(
            &bigger, &out) == MARC_STATUS_OK,
        "tail query");
  check(out.fixed_bytes == q.fixed_bytes + 60 &&
            out.external_charge_bytes == q.external_charge_bytes + 30,
        "duplicated capacities");
  bigger = c;
  bigger.external_retained_bytes += 19;
  check(marc_lzss_position_distance_dynamic_range_8m_resource_requirements(
            &bigger, &out) == MARC_STATUS_OK &&
            out.fixed_bytes == q.fixed_bytes + 19,
        "external charge");
  std::cout << "{\"resources\":true,\"fixed\":" << q.fixed_bytes
            << ",\"initial\":" << q.initial_bytes
            << ",\"external\":" << q.external_charge_bytes << "}\n";
  row("resource-arithmetic");
  for (unsigned empty = 0; empty < 2; ++empty) {
    c = config(empty ? 0 : 160);
    check(marc_lzss_position_distance_dynamic_range_8m_resource_requirements(
              &c, &out) == MARC_STATUS_OK,
          "initial query");
    if (empty)
      check(!out.initial_raw_bytes && !out.initial_index_entries &&
                out.initial_bytes == out.fixed_bytes,
            "empty initial");
    c.max_internal_buffered_bytes = out.initial_bytes;
    Handle h;
    check(marc_lzss_position_distance_dynamic_range_8m_create_encoder(
              &c, &h.p) == MARC_STATUS_OK,
          "exact initial create");
    marc_transform_destroy(h.p);
    h.p = nullptr;
    --c.max_internal_buffered_bytes;
    out = old;
    check(marc_lzss_position_distance_dynamic_range_8m_resource_requirements(
              &c, &out) == MARC_STATUS_LIMIT_EXCEEDED,
          "initial minus one query");
    check(std::memcmp(&out, &old, sizeof(out)) == 0, "initial minus preserves");
    check(marc_lzss_position_distance_dynamic_range_8m_create_encoder(
              &c, &h.p) == MARC_STATUS_LIMIT_EXCEEDED &&
              !h.p,
          "initial minus create");
    row(empty ? "empty-initial-budget" : "nonempty-initial-budget");
  }
}
void boundaries() {
  for (unsigned mode = 0; mode < 9; ++mode) {
    auto c = config();
    Handle h;
    public_nothrow_test::calls = 0;
    public_nothrow_test::fail_at =
        3; // captures first successful factory allocation
    check(marc_lzss_position_distance_dynamic_range_8m_create_encoder(
              &c, &h.p) == MARC_STATUS_OK,
          "boundary create");
    public_nothrow_test::fail_at = 0;
    std::array<uint8_t, 4200> buffer{};
    marc_const_buffer in{buffer.data(), 1};
    marc_buffer out{buffer.data() + 200, 1};
    if (mode == 0)
      in.size = 161;
    if (mode == 1)
      out.size = 4097;
    if (mode == 2)
      out = {buffer.data(), 1};
    if (mode == 3)
      in = {reinterpret_cast<uint8_t *>(h.p), sizeof(void *)};
    if (mode == 4)
      out = {reinterpret_cast<uint8_t *>(h.p), sizeof(void *)};
    if (mode == 7)
      in = {static_cast<uint8_t *>(public_nothrow_test::first_pointer) +
                public_nothrow_test::first_bytes - 1,
            1};
    if (mode == 8)
      out = {static_cast<uint8_t *>(public_nothrow_test::first_pointer) +
                 public_nothrow_test::first_bytes - 1,
             1};
    const auto flags = mode == 5   ? MARC_PROCESS_RESET_BLOCK
                       : mode == 6 ? UINT32_MAX
                                   : 0;
    auto r = marc_transform_process(h.p, in, out, flags);
    check(r.status == ((mode == 5 || mode == 6)
                           ? MARC_STATUS_UNSUPPORTED
                           : MARC_STATUS_INVALID_ARGUMENT) &&
              !r.input_consumed && !r.output_produced,
          "boundary refusal");
    auto t = marc_transform_process(h.p, {buffer.data(), 161},
                                    {buffer.data() + 200, 1}, UINT32_MAX);
    check(t.status == r.status && !t.input_consumed && !t.output_produced,
          "boundary sticky");
    row("boundary-" + std::to_string(mode));
  }
}
void streams() {
  Fixture f;
  f.prepare();
  for (bool staged : {false, true}) {
    test::owning8m::Controller a;
    test::owning8m::Scope observe(a);
    Handle h;
    check(marc_lzss_position_distance_dynamic_range_8m_create_encoder(
              &f.c, &h.p) == MARC_STATUS_OK,
          "stream create");
    auto r = f.run(h.p, staged);
    check(r.status == MARC_STATUS_END_OF_STREAM, "public end");
    marc_transform_destroy(h.p);
    h.p = nullptr;
    check(a.valid && !a.live, "positive deletion");
    row(staged ? "one-byte-wire-decode" : "whole-wire-decode");
  }
  for (unsigned mode = 0; mode < 2; ++mode) {
    Handle h;
    auto c = f.c;
    if (mode)
      c.input_capacity_bytes = 161;
    check(marc_lzss_position_distance_dynamic_range_8m_create_encoder(
              &c, &h.p) == MARC_STATUS_OK,
          "size create");
    marc_process_result r{};
    if (mode) {
      std::array<uint8_t, 161> extra{};
      std::memcpy(extra.data(), f.raw.data(), 160);
      f.output.fill(sentinel);
      r = marc_transform_process(
          h.p, {extra.data(), extra.size()},
          {reinterpret_cast<uint8_t *>(f.output.data()), 4096},
          MARC_PROCESS_END_INPUT);
      check(r.input_consumed == 160 && r.output_produced == f.wire &&
                std::equal(f.output.begin(), f.output.begin() + f.wire,
                           f.oracle.begin()),
            "extra preserves complete valid stream");
      f.guards(std::span(f.output).subspan(f.wire));
    } else {
      r = f.run(h.p, false, 159);
    }
    check(r.status == MARC_STATUS_MALFORMED_STREAM, "known size failure");
    row(mode ? "known-size-extra" : "known-size-early-end");
  }
  // A generation must be admitted after creation. No frame can be exposed.
  auto c = f.c;
  Resources q{};
  check(marc_lzss_position_distance_dynamic_range_8m_resource_requirements(
            &c, &q) == MARC_STATUS_OK,
        "deferred query");
  c.max_internal_buffered_bytes = q.initial_bytes;
  Handle h;
  check(marc_lzss_position_distance_dynamic_range_8m_create_encoder(&c, &h.p) ==
            MARC_STATUS_OK,
        "deferred create");
  auto r = f.run(h.p, false);
  check(r.status == MARC_STATUS_LIMIT_EXCEEDED && r.output_produced == 112,
        "deferred privacy");
  row("initial-only-deferred-refusal");
  {
    auto small = f.c;
    small.max_block_size = 63;
    Handle limited;
    check(marc_lzss_position_distance_dynamic_range_8m_create_encoder(
              &small, &limited.p) == MARC_STATUS_OK,
          "block initial admission");
    auto refusal = f.run(limited.p, false);
    check(refusal.status == MARC_STATUS_LIMIT_EXCEEDED &&
              refusal.output_produced == 112 &&
              refusal.error_byte_position == 0,
          "block failed-frame privacy");
    row("block-limit-before-frame");
  }
  // Empty and every byte value test the public encoder without assuming a
  // compression benefit; fresh private decoders independently check output.
  for (unsigned value = 0; value < 257; ++value) {
    auto z = config(value == 256 ? 0 : 1);
    z.external_retained_bytes = f.c.external_retained_bytes;
    Handle one;
    check(marc_lzss_position_distance_dynamic_range_8m_create_encoder(
              &z, &one.p) == MARC_STATUS_OK,
          "byte create");
    f.output.fill(sentinel);
    uint8_t b = static_cast<uint8_t>(value);
    auto x = marc_transform_process(
        one.p, {&b, value == 256 ? 0u : 1u},
        {reinterpret_cast<uint8_t *>(f.output.data()), 4096},
        MARC_PROCESS_END_INPUT);
    check(x.status == MARC_STATUS_END_OF_STREAM, "byte end");
    f.decoded.fill(sentinel);
    LzssPositionDistance8mStreamDecoder d(
        f.limits(), f.serialized, f.dec_tokens, f.dec_token_scratch, f.dec_raw,
        f.dec_scratch, f.c.external_retained_bytes);
    auto y = d.process(std::span(f.output).first(x.output_produced),
                       std::span(f.decoded).first(1), MARC_PROCESS_END_INPUT);
    check(y.status == core::StreamStatus::end_of_stream &&
              y.output_produced == (value == 256 ? 0u : 1u),
          "byte decode");
    if (value != 256)
      check(f.decoded[0] == std::byte(b), "byte equality");
    f.guards(std::span(f.decoded).subspan(y.output_produced));
  }
  row("all-256-one-byte-and-empty");
}
void faults() {
  Fixture f;
  f.prepare();
  for (std::size_t at : {std::size_t{1}, std::size_t{2}}) {
    test::owning8m::Controller a;
    test::owning8m::Scope receipts(a);
    public_nothrow_test::calls = public_nothrow_test::injections = 0;
    public_nothrow_test::fail_at = at;
    Handle h;
    const auto result =
        marc_lzss_position_distance_dynamic_range_8m_create_encoder(&f.c, &h.p);
    public_nothrow_test::fail_at = 0;
    check(result == MARC_STATUS_OUT_OF_MEMORY && !h.p &&
              public_nothrow_test::injections == 1 &&
              public_nothrow_test::calls == at,
          "public scalar allocation failure");
    check(a.valid && !a.live && a.calls == (at == 1 ? 0u : 2u),
          "factory failure cleanup");
    row("factory-scalar-refusal-" + std::to_string(at));
  }
  // Independently discover the complete allocation count, then refuse each
  // actual delegated allocation. Destruction must clear every real receipt.
  test::owning8m::Controller baseline;
  {
    test::owning8m::Scope s(baseline);
    Handle h;
    check(marc_lzss_position_distance_dynamic_range_8m_create_encoder(
              &f.c, &h.p) == MARC_STATUS_OK,
          "allocation baseline create");
    check(f.run(h.p, false).status == MARC_STATUS_END_OF_STREAM,
          "allocation baseline end");
  }
  check(baseline.valid && !baseline.live && baseline.calls == 17,
        "allocation discovery");
  row("delegate-discovery-17");
  for (std::size_t at = 1; at <= baseline.calls; ++at) {
    test::owning8m::Controller a;
    a.fail_at = at;
    test::owning8m::Scope s(a);
    {
      Handle h;
      auto status = marc_lzss_position_distance_dynamic_range_8m_create_encoder(
          &f.c, &h.p);
      if (at <= 2)
        check(status == MARC_STATUS_OUT_OF_MEMORY && !h.p,
              "initial real delegate failure");
      else {
        check(status == MARC_STATUS_OK, "generation create");
        auto r = f.run(h.p, false);
        check(r.status == MARC_STATUS_OUT_OF_MEMORY &&
                  r.error_byte_position == ((at - 3) / 5) * 64,
              "generation delegate failure");
        check(r.output_produced == f.boundaries[(at - 3) / 5],
              "failed frame private");
      }
    }
    check(a.valid && !a.live && a.injections == 1,
          "all failed allocations really release");
    row("delegate-refusal-" + std::to_string(at));
  }
  for (std::size_t target = 0; target < 3; ++target)
    for (unsigned mode = 0; mode <= 6; ++mode) {
      test::late8m::Controller late{};
      late.mode = mode;
      late.prior = target * 64;
      late.raw = target == 2 ? 32 : 64;
      test::owning8m::Controller a;
      test::late8m::Scope seam(late);
      test::owning8m::Scope receipts(a);
      {
        Handle h;
        check(marc_lzss_position_distance_dynamic_range_8m_create_encoder(
                  &f.c, &h.p) == MARC_STATUS_OK,
              "late create");
        auto r = f.run(h.p, mode % 2 != 0);
        check(late.valid && late.injections == (mode ? 1u : 0u) &&
                  late.range == 1,
              "real late range");
        if (mode)
          check(r.status == MARC_STATUS_LIMIT_EXCEEDED &&
                    r.error_byte_position == target * 64 &&
                    r.output_produced == f.boundaries[target],
                "late privacy");
        else
          check(r.status == MARC_STATUS_END_OF_STREAM, "mode zero");
      }
      check(a.valid && !a.live, "late real deletion");
      row("late-" + std::to_string(target) + "-" + std::to_string(mode));
    }
  // Measure actual receipts prospectively under a large explicit budget, then
  // enforce the exact peak and one below, without transferring any grant.
  Resources q{};
  check(marc_lzss_position_distance_dynamic_range_8m_resource_requirements(
            &f.c, &q) == MARC_STATUS_OK,
        "peak query");
  for (unsigned below = 0; below < 2; ++below) {
    auto c = f.c;
    c.max_internal_buffered_bytes = q.fixed_bytes + baseline.peak - below;
    test::owning8m::Controller a;
    test::owning8m::Scope s(a);
    {
      Handle h;
      check(marc_lzss_position_distance_dynamic_range_8m_create_encoder(
                &c, &h.p) == MARC_STATUS_OK,
            "peak create");
      auto r = f.run(h.p, false);
      check(r.status == (below ? MARC_STATUS_LIMIT_EXCEEDED
                               : MARC_STATUS_END_OF_STREAM),
            "peak budget");
      if (below)
        check(r.error_byte_position == 64 &&
                  r.output_produced == f.boundaries[1],
              "replacement prospective privacy");
    }
    check(a.valid && !a.live, "peak real release");
    row(below ? "complete-peak-minus-one" : "complete-peak-exact");
  }
  std::cout << "{\"allocation\":true,\"calls\":" << baseline.calls
            << ",\"block_peak\":" << baseline.peak
            << ",\"fixed\":" << q.fixed_bytes
            << ",\"complete_peak\":" << q.fixed_bytes + baseline.peak << "}\n";
}
} // namespace
int main(int argc, char **argv) {
  try {
    isolated = argc == 2 && std::string(argv[1]) == "--isolated";
    metadata();
    boundaries();
    streams();
    if (isolated)
      faults();
    std::cout << "{\"passed\":true,\"checks\":" << checks
              << ",\"isolated\":" << (isolated ? "true" : "false")
              << ",\"timed\":false}\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
  }
}
