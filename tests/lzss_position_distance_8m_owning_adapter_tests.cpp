#include "core/checked_math.hpp"
#include "core/sha256.hpp"
#include "frame/lzss_position_distance_8m_owning_adapter.hpp"
#include "frame/lzss_position_distance_8m_prepared_stream_encoder.hpp"
#include "frame/lzss_position_distance_8m_stream_decoder.hpp"
#include "frame/lzss_position_distance_8m_stream_encoder.hpp"
#include "lzss_position_distance_8m_late_fault_seam.hpp"
#include "lzss_position_distance_8m_owning_allocator_seam.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string_view>
#include <type_traits>
namespace {
using namespace marc;
using namespace frame::internal;
using Token = dictionary::internal::LzssTypedToken;
using Op = context::internal::ModeledOperation;
using Code = core::ErrorCode;
using Status = core::StreamStatus;
using Encoder = LzssPositionDistance8mPreparedStreamEncoder;
constexpr auto end = core::flag_value(core::ProcessFlags::end_input);
constexpr auto sentinel = std::byte{0xa5};
constexpr std::size_t ceiling = 1048576;
void check(bool b, const char *s) {
  if (!b)
    throw std::runtime_error(s);
}
std::size_t sum(std::initializer_list<std::size_t> values) {
  std::size_t n{};
  for (auto v : values)
    check(core::checked_add(n, v, n), "sum overflow");
  return n;
}
bool same(const Token &a, const Token &b) {
  return a.kind == b.kind && a.literal == b.literal &&
         a.distance == b.distance && a.length == b.length;
}
auto fields(const TypedContextFrameLayout &x) {
  const auto &h = x.header;
  const auto &d = x.descriptor;
  return std::array<std::uint64_t, 14>{h.flags,
                                       h.sequence,
                                       h.uncompressed_size,
                                       h.token_count,
                                       h.event_count,
                                       h.decision_count,
                                       h.payload_size,
                                       h.descriptor_size,
                                       h.context_side_data_size,
                                       h.checksum_trailer_size,
                                       d.decision_count,
                                       d.payload_size,
                                       d.context_count,
                                       x.serialized_size};
}
struct Allocator final : LzssPositionDistance8mStreamAllocator {
  struct Record {
    void *p{};
    std::size_t bytes{}, id{};
    unsigned kind{};
  };
  struct Snapshot {
    Record r{};
    std::array<Token, 64> tokens{};
    std::array<std::byte, 2048> bytes{};
  };
  std::array<Record, 16> records{};
  std::array<Snapshot, 5> snapshots{};
  std::size_t calls{}, deleted{}, live{}, peak{}, base{}, snapshot_at{},
      before_live{}, before_deleted{}, snapshot_count{}, target_bytes{},
      target_peak{};
  bool valid{true}, fault{}, publication_pristine{true}, candidate_dirty{};
  auto controls() const noexcept
      -> LzssPositionDistance8mAllocatorControls override {
    return {this, sizeof(*this),
            32 * sizeof(std::size_t) + 16 * sizeof(void *)};
  }
  void capture() noexcept {
    before_live = live;
    before_deleted = deleted;
    for (auto &r : records)
      if (r.p && r.id > base) {
        if (snapshot_count == 5) {
          valid = false;
          return;
        }
        auto &s = snapshots[snapshot_count++];
        s.r = r;
        if (r.kind == 1 && r.bytes / sizeof(Token) <= 64)
          std::copy_n(static_cast<const Token *>(r.p), r.bytes / sizeof(Token),
                      s.tokens.begin());
        else if (r.kind == 2 && r.bytes <= s.bytes.size())
          std::copy_n(static_cast<const std::byte *>(r.p), r.bytes,
                      s.bytes.begin());
        else
          valid = false;
      }
  }
  template <class T>
  LzssPositionDistance8mOwnedBlock<T> obtain(std::size_t n) noexcept {
    if (calls + 1 == snapshot_at)
      capture();
    ++calls;
    std::size_t bytes{};
    if (!core::checked_multiply(n, sizeof(T), bytes) ||
        bytes > ceiling - live) {
      valid = false;
      return {};
    }
    auto *p = new (std::nothrow) T[n]{};
    if (!p)
      return {};
    for (auto &r : records)
      if (!r.p) {
        constexpr unsigned kind = std::is_same_v<T, Token>       ? 1
                                  : std::is_same_v<T, std::byte> ? 2
                                                                 : 3;
        r = {p, bytes, calls, kind};
        live += bytes;
        peak = std::max(peak, live);
        if (calls >= snapshot_at && calls < snapshot_at + 5) {
          target_bytes += bytes;
          target_peak = live;
        }
        return {p, n};
      }
    delete[] p;
    valid = false;
    return {};
  }
  auto tokens(std::size_t n) noexcept
      -> LzssPositionDistance8mOwnedTokens override {
    return obtain<Token>(n);
  }
  auto bytes(std::size_t n) noexcept
      -> LzssPositionDistance8mOwnedBytes override {
    return obtain<std::byte>(n);
  }
  auto indices(std::size_t n) noexcept
      -> LzssPositionDistance8mOwnedIndex override {
    return obtain<std::uint32_t>(n);
  }
  template <class T>
  void drop(LzssPositionDistance8mOwnedBlock<T> &b) noexcept {
    if (!b.data) {
      b = {};
      return;
    }
    for (auto &r : records)
      if (r.p == b.data) {
        if constexpr (std::is_same_v<T, std::byte>) {
          if (fault && r.id == snapshot_at + 4)
            publication_pristine = publication_pristine &&
                                   std::ranges::all_of(b.view(), [](auto x) {
                                     return x == std::byte{};
                                   });
          if (fault && r.id == snapshot_at + 2)
            candidate_dirty = std::ranges::any_of(
                b.view(), [](auto x) { return x != std::byte{}; });
        }
        auto *p = b.data;
        auto cap = b.capacity;
        const auto bytes = r.bytes;
        delete[] p; // REAL deletion precedes both receipt and block clearing.
        valid = valid && b.data == p && b.capacity == cap;
        live -= bytes;
        r = {};
        ++deleted;
        b = {};
        return;
      }
    valid = false;
  }
  void release(LzssPositionDistance8mOwnedTokens &b) noexcept override {
    drop(b);
  }
  void release(LzssPositionDistance8mOwnedBytes &b) noexcept override {
    drop(b);
  }
  void release(LzssPositionDistance8mOwnedIndex &b) noexcept override {
    drop(b);
  }
  void preserved() const {
    check(valid && live == before_live && deleted == before_deleted + 5,
          "failure receipt ledger");
    check(calls == snapshot_at + 4 && target_peak == before_live + target_bytes,
          "candidate coexistence");
    check(publication_pristine && candidate_dirty,
          "dirty candidate stayed private");
    for (std::size_t i = 0; i < snapshot_count; ++i) {
      const auto &s = snapshots[i];
      check(std::ranges::any_of(records,
                                [&](auto &r) {
                                  return r.p == s.r.p && r.id == s.r.id &&
                                         r.bytes == s.r.bytes;
                                }),
            "old receipt");
      if (s.r.kind == 1) {
        const auto *t = static_cast<const Token *>(s.r.p);
        for (std::size_t j = 0; j < s.r.bytes / sizeof(Token); ++j)
          check(same(t[j], s.tokens[j]), "old token fields");
      } else
        check(std::equal(s.bytes.begin(), s.bytes.begin() + s.r.bytes,
                         static_cast<const std::byte *>(s.r.p)),
              "old full bytes");
    }
  }
  void zero() const {
    check(valid && live == 0 &&
              std::ranges::all_of(records, [](auto &r) { return !r.p; }),
          "zero real receipts");
  }
};
struct Fixture {
  core::DecoderLimits limits{};
  TypedContextStreamHeader header{};
  Allocator allocator, oracle_allocator;
  test::late8m::Controller control{};
  std::array<std::byte, 160> raw{};
  std::array<std::uint32_t, 65600> index{}, op_index{};
  std::array<Token, 64> tokens{}, scratch{}, decode_tokens{}, decode_scratch{};
  std::array<Op, 128> ops{}, op_scratch{};
  std::array<std::byte, 64> op_raw{}, decode_raw{}, decode_raw_scratch{};
  std::array<std::byte, 2048> pub{}, frame{}, payload{}, decode_pub{};
  std::array<std::byte, 4112> oracle{}, output{};
  std::array<std::byte, 176> decoded{};
  std::array<std::size_t, 4> boundaries{};
  std::array<std::size_t, 3> literal_counts{}, match_counts{};
  std::size_t wire{}, logical{}, written{}, consumed{}, steps{};
  Code observed_code{Code::none};
  std::uint64_t observed_position{};
  std::size_t live_after_failure{};
  bool position_available{};
  Fixture() {
    limits.max_frame_size = limits.max_block_size = 64;
    limits.max_compressed_payload_size = 65536;
    limits.max_internal_buffered_bytes = ceiling;
    header.frame_size = 64;
    header.original_size = 160;
    header.dictionary = {8388608, 3, 258, 0};
    header.dictionary_variant = 11;
    header.context_algorithm = 1;
    header.context_variant = 12;
    header.context_count = 47;
    header.range_model_total = 32768;
    for (std::size_t i = 0; i < raw.size(); ++i)
      raw[i] = static_cast<std::byte>((i % 64) % 7);
  }
  static std::size_t extra() {
    return sum({sizeof(Fixture), test::late8m::working_bytes(), sizeof(Encoder),
                sizeof(LzssPositionDistance8mPreparedStorageOwner),
                sizeof(LzssPositionDistance8mOwnedStreamEncoder),
                sizeof(LzssPositionDistance8mStreamDecoder),
                4 * sizeof(core::ProcessResult),
                2 * sizeof(TypedContextFrameLayout), sizeof(core::Sha256),
                sizeof(LzssPositionDistance8mFrameDecodeResult),
                sizeof(LzssPositionDistance8mOwningAdapter),
                2 * sizeof(test::owning8m::Controller),
                2 * sizeof(LzssPositionDistance8mOwningAdapter::Ledger), 4096,
                64, 64 * sizeof(std::size_t), 16 * sizeof(void *)});
  }
  void guards(std::span<const std::byte> s) {
    check(std::ranges::all_of(s, [](auto b) { return b == sentinel; }),
          "output sentinel");
  }
  void make_oracle() {
    literal_counts.fill(0);
    match_counts.fill(0);
    oracle.fill(sentinel);
    const LzssPositionDistance8mStreamEncodeCapacities caps{
        64, 2048, 64, 64, 65600, 128, 128, 2048, 2048, 160, 4096};
    auto q = query_lzss_position_distance_8m_stream_encode_workspace(
        limits, caps, extra());
    check(q.error == Code::none && q.aggregate_bytes <= ceiling,
          "operation prospective admission");
    logical = q.aggregate_bytes;
    {
      LzssPositionDistance8mStreamEncoder e(
          header, limits, op_raw, pub,
          {tokens, scratch, op_index, ops, op_scratch, frame, payload},
          extra());
      auto r = e.process(raw, std::span(oracle).first(4096), end);
      check(r.status == Status::end_of_stream && r.input_consumed == 160,
            "operation oracle");
      wire = r.output_produced;
      guards(std::span(oracle).subspan(wire));
    }
    {
      output.fill(sentinel);
      LzssPositionDistance8mOwnedStreamEncoder e(header, limits,
                                                 oracle_allocator, extra());
      auto r = e.process(raw, std::span(output).first(4096), end);
      check(r.status == Status::end_of_stream && r.input_consumed == 160 &&
                r.output_produced == wire,
            "safe oracle");
      check(std::equal(oracle.begin(), oracle.begin() + wire, output.begin()),
            "safe wire equality");
      guards(std::span(output).subspan(wire));
    }
    oracle_allocator.zero();
    boundaries[0] = 112;
    for (std::size_t j = 0; j < 3; ++j) {
      TypedContextFrameLayout layout{};
      LzssPositionDistance8mFrameRequirements requirements{};
      auto e = preflight_lzss_position_distance_8m_frame_prefix(
          std::span(oracle).subspan(boundaries[j], wire - boundaries[j]),
          {header, limits, j, j * 64}, layout, requirements, extra());
      check(e == LzssPositionDistance8mPreflightError::none,
            "oracle frame boundaries");
      boundaries[j + 1] = sum({boundaries[j], layout.serialized_size});
      TypedContextFrameLayout decoded_layout{};
      const auto decoded_frame = decode_lzss_position_distance_8m_frame(
          std::span(oracle).subspan(boundaries[j], layout.serialized_size),
          {header, limits, j, j * 64}, decode_tokens, decode_scratch,
          decode_raw, decode_raw_scratch, decoded_layout, extra());
      check(decoded_frame.error ==
                    LzssPositionDistance8mFrameDecodeError::none &&
                fields(decoded_layout) == fields(layout),
            "composition decode");
      check(std::equal(decode_raw.begin(),
                       decode_raw.begin() + layout.header.uncompressed_size,
                       raw.begin() + j * 64),
            "composition raw equality");
      for (const auto &token :
           std::span(decode_tokens).first(layout.header.token_count)) {
        if (token.kind == dictionary::internal::LzssTypedTokenKind::literal)
          ++literal_counts[j];
        else if (token.kind ==
                 dictionary::internal::LzssTypedTokenKind::match) {
          ++match_counts[j];
          check(token.distance == 7 && token.length == (j == 2 ? 25u : 57u),
                "hand checked match distance length");
        } else
          check(false, "unknown token composition");
      }
      check(literal_counts[j] == 7 && match_counts[j] == 1 &&
                layout.header.token_count == 8,
            "literal match composition");
    }
    check(boundaries[3] == wire, "wire length");
    decoded.fill(sentinel);
    {
      LzssPositionDistance8mStreamDecoder d(limits, decode_pub, decode_tokens,
                                            decode_scratch, decode_raw,
                                            decode_raw_scratch, extra());
      auto r = d.process(std::span(oracle).first(wire),
                         std::span(decoded).first(160), end);
      check(r.status == Status::end_of_stream && r.input_consumed == wire &&
                r.output_produced == 160,
            "fresh decoder");
    }
    check(std::equal(raw.begin(), raw.end(), decoded.begin()),
          "decoded equality");
    guards(std::span(decoded).subspan(160));
  }
  void trace(bool companion, unsigned mode) {
    check(control.valid, "real delegate/context success");
    check(control.injections == (mode ? 1u : 0u), "one injection");
    check(control.range == (companion ? 0u : 1u), "target range arrival");
    check(control.prefix == ((companion || mode == 1 || mode == 2) ? 0u : 1u),
          "target prefix arrival");
    check(control.reparse ==
              ((companion || (mode >= 1 && mode <= 3)) ? 0u : 1u),
          "target reparse arrival");
    if (mode == 5)
      check(control.parsed_error ==
                LzssPositionDistance8mPreflightError::invalid_magic,
            "actual invalid magic");
  }
  void report(const char *kind, unsigned mode, unsigned target, bool staged,
              std::span<const std::byte> bytes) {
    core::Sha256 hash;
    std::array<std::byte, 32> digest{};
    check(hash.update(bytes) && hash.finalize(digest), "SHA256");
    std::cout << "{\"kind\":\"" << kind << "\",\"mode\":" << mode
              << ",\"target\":" << target
              << ",\"staged\":" << (staged ? "true" : "false")
              << ",\"passed\":true,\"range\":" << control.range
              << ",\"prefix\":" << control.prefix
              << ",\"reparse\":" << control.reparse
              << ",\"injections\":" << control.injections
              << ",\"parsed_error\":"
              << static_cast<unsigned>(control.parsed_error)
              << ",\"literal_counts\":[" << literal_counts[0] << ","
              << literal_counts[1] << "," << literal_counts[2] << "]"
              << ",\"match_counts\":[" << match_counts[0] << ","
              << match_counts[1] << "," << match_counts[2] << "]"
              << ",\"boundaries\":[" << boundaries[0] << "," << boundaries[1]
              << "," << boundaries[2] << "," << boundaries[3] << "]"
              << ",\"fixture_bytes\":" << sizeof(Fixture)
              << ",\"shim_working\":" << test::late8m::working_bytes()
              << ",\"external\":" << extra()
              << ",\"oracle_logical\":" << logical
              << ",\"owner_peak\":" << allocator.peak
              << ",\"target_peak\":" << allocator.target_peak
              << ",\"before_live\":" << allocator.before_live
              << ",\"candidate_bytes\":" << allocator.target_bytes
              << ",\"deleted\":" << allocator.deleted
              << ",\"calls\":" << allocator.calls
              << ",\"error_code\":" << static_cast<unsigned>(observed_code)
              << ",\"position_available\":"
              << (position_available ? "true" : "false")
              << ",\"error_position\":" << observed_position
              << ",\"live_after_failure\":" << live_after_failure
              << ",\"live_after_destroy\":" << allocator.live
              << ",\"snapshot_blocks\":" << allocator.snapshot_count
              << ",\"candidate_dirty\":"
              << (allocator.candidate_dirty ? "true" : "false")
              << ",\"publication_pristine\":"
              << (allocator.publication_pristine ? "true" : "false")
              << ",\"consumed\":" << consumed << ",\"written\":" << bytes.size()
              << ",\"sha256\":\"";
    for (auto b : digest)
      std::cout << std::hex << std::setw(2) << std::setfill('0')
                << std::to_integer<unsigned>(b);
    std::cout << std::dec << "\"}\n";
  }
};
void owner_case(Fixture &f, bool companion, unsigned mode,
                unsigned replacement) {
  f.control = {mode, replacement * 64u, 64};
  f.allocator.fault = mode != 0;
  f.allocator.snapshot_at = 1 + 5 * replacement;
  f.output.fill(sentinel);
  {
    test::late8m::Scope scope(f.control);
    LzssPositionDistance8mPreparedStorageOwner owner(f.allocator);
    if (replacement) {
      auto r =
          owner.encode(std::span(f.raw).first(64), {f.header, f.limits, 0, 0},
                       f.index, Fixture::extra());
      check(r.error == Code::none, "owner initial success");
      owner.acknowledge_drained();
    }
    const auto layout = fields(owner.layout());
    const auto old = owner.publication();
    const auto pending = owner.pending();
    auto r = owner.encode(std::span(f.raw).subspan(replacement * 64, 64),
                          {f.header, f.limits, replacement, replacement * 64},
                          f.index, Fixture::extra());
    f.observed_code = r.error;
    if (mode) {
      check(r.error == Code::limit_exceeded && r.bytes_validated == 0,
            "late owner refusal");
      check(owner.publication().data() == old.data() &&
                owner.publication().size() == old.size() &&
                fields(owner.layout()) == layout && owner.pending() == pending,
            "owner unchanged publication metadata");
      f.allocator.preserved();
      f.live_after_failure = f.allocator.live;
      check(f.allocator.snapshot_count == (replacement ? 5u : 0u),
            "owner old block count");
    } else {
      check(r.error == Code::none && r.aggregate_bytes <= ceiling &&
                owner.pending(),
            "owner control success");
      const auto expected = std::span(f.oracle).subspan(
          f.boundaries[replacement],
          f.boundaries[replacement + 1] - f.boundaries[replacement]);
      check(std::ranges::equal(owner.publication(), expected),
            "owner control wire");
    }
    f.trace(companion, mode);
    f.written = owner.publication().size();
    check(f.written <= 4096, "publication snapshot extent");
    std::copy(owner.publication().begin(), owner.publication().end(),
              f.output.begin());
    f.guards(std::span(f.output).subspan(f.written));
  }
  f.allocator.zero();
  f.report("owner", mode, replacement, false,
           std::span(f.output).first(f.written));
}
void stream_case(Fixture &f, bool companion, unsigned mode, unsigned target,
                 bool staged) {
  test::owning8m::Controller allocator_control;
  test::owning8m::Scope allocator_scope(allocator_control);
  LzssPositionDistance8mOwningAdapter::Ledger actual_ledger{};
  f.control = {mode, target * 64u, target == 2 ? 32u : 64u};
  f.allocator.base = 2;
  f.allocator.snapshot_at = 3 + target * 5;
  f.allocator.fault = mode != 0;
  f.output.fill(sentinel);
  {
    test::late8m::Scope scope(f.control);
    LzssPositionDistance8mOwningAdapter e(
        {f.header, f.limits, Fixture::extra(), 160, 4096});
    auto call = [&](std::span<const std::byte> in, std::size_t capacity,
                    std::uint32_t flags) {
      check(++f.steps < 10000, "call guard");
      auto out = std::span(f.output).subspan(f.written, capacity);
      auto r = e.process(in, out, flags);
      check(r.input_consumed <= in.size() && r.output_produced <= out.size(),
            "process counts");
      check(r.status != Status::progress || r.input_consumed ||
                r.output_produced,
            "zero progress");
      f.consumed += r.input_consumed;
      f.written += r.output_produced;
      f.guards(std::span(f.output).subspan(f.written));
      return r;
    };
    core::ProcessResult r{};
    if (staged) {
      r = call({}, 0, 0);
      check(r.status == Status::need_output, "zero header output");
      while (f.written < 112)
        r = call({}, 1, 0);
      for (unsigned j = 0; j < target; ++j) {
        r = call(std::span(f.raw).subspan(j * 64, 64), 0, 0);
        check(r.status == Status::need_output && r.input_consumed == 64,
              "zero frame output");
        while (f.written < f.boundaries[j + 1])
          r = call({}, 1, 0);
      }
      const auto before = f.written;
      r = call(std::span(f.raw).subspan(target * 64, target == 2 ? 32 : 64),
               4096 - f.written, target == 2 ? end : 0);
      if (mode)
        check(r.output_produced == 0 && f.written == before,
              "staged failed call unchanged");
      else {
        while (r.status != Status::end_of_stream)
          r = call(std::span(f.raw).subspan(f.consumed), 4096 - f.written, end);
      }
    } else
      r = call(f.raw, 4096, end);
    if (mode) {
      check(r.status == Status::error && r.error.code == Code::limit_exceeded &&
                r.error.byte_position == target * 64 &&
                r.error.bit_position == 0,
            "late stream refusal position");
      check(f.consumed == std::min<std::size_t>((target + 1) * 64, 160) &&
                f.written == f.boundaries[target],
            "valid prefix counts");
      const auto ledger = e.ledger();
      check(ledger.live == 262464 + (target ? 394u : 0u) &&
                ledger.blocks == (target ? 7u : 2u),
            "retained adapter generation");
      f.observed_code = r.error.code;
      f.observed_position = r.error.byte_position;
      f.position_available = true;
      f.live_after_failure = e.ledger().live;

      const auto error = r.error;
      for (auto flags : {end, 0xffffffffu}) {
        r = call({}, 4096 - f.written, flags);
        check(r.status == Status::error && r.input_consumed == 0 &&
                  r.output_produced == 0 && r.error.code == error.code &&
                  r.error.byte_position == error.byte_position &&
                  r.error.bit_position == error.bit_position,
              "sticky error");
      }
    } else
      check(r.status == Status::end_of_stream && f.consumed == 160 &&
                f.written == f.wire,
            "stream control complete");
    check(std::equal(f.output.begin(), f.output.begin() + f.written,
                     f.oracle.begin()),
          "exact valid wire prefix");
    f.trace(companion, mode);
    actual_ledger = e.ledger();
    if (!companion)
      check(allocator_control.live == actual_ledger.live &&
                allocator_control.peak == actual_ledger.peak &&
                allocator_control.calls == actual_ledger.calls,
            "adapter independent live ledger");
  }

  check(allocator_control.valid && allocator_control.live == 0 &&
            std::ranges::all_of(allocator_control.records,
                                [](const auto &r) { return !r.p; }),
        "adapter real destruction receipts");
  core::Sha256 hash;
  std::array<std::byte, 32> digest{};
  check(hash.update(std::span(f.output).first(f.written)) &&
            hash.finalize(digest),
        "stream hash");
  std::cout << "{\"kind\":\"adapter-stream\",\"mode\":" << mode
            << ",\"target\":" << target
            << ",\"staged\":" << (staged ? "true" : "false")
            << ",\"passed\":true,\"written\":" << f.written
            << ",\"consumed\":" << f.consumed
            << ",\"error_code\":" << static_cast<unsigned>(f.observed_code)
            << ",\"error_position\":" << f.observed_position
            << ",\"calls\":" << actual_ledger.calls
            << ",\"peak\":" << actual_ledger.peak
            << ",\"live_before_destroy\":" << actual_ledger.live
            << ",\"delegate_live_after_destroy\":" << allocator_control.live
            << ",\"delegate_deleted\":" << allocator_control.deleted
            << ",\"injections\":" << f.control.injections << ",\"sha256\":\"";
  for (auto b : digest)
    std::cout << std::hex << std::setw(2) << std::setfill('0')
              << std::to_integer<unsigned>(b);
  std::cout << std::dec << "\"}\n";
}

using Adapter = LzssPositionDistance8mOwningAdapter;
using Config = LzssPositionDistance8mOwningConfig;
void component_row(const char *name, unsigned n, std::size_t calls = 0) {
  std::cout << "{\"kind\":\"component\",\"name\":\"" << name
            << "\",\"index\":" << n << ",\"calls\":" << calls
            << ",\"passed\":true}\n";
}
unsigned components(bool isolated) {
  unsigned checks{};
  auto f = std::make_unique<Fixture>();
  f->make_oracle();
  Config c{f->header, f->limits, Fixture::extra(), 160, 4096};
  auto q = Adapter::query(c);
  check(q.error == Code::none, "adapter query");
  check(q.raw_bytes == 64 && q.index_entries == 65600 &&
            q.initial_bytes == q.fixed_bytes + 262464,
        "query formula");
  auto run = [&](const char *name, unsigned index, auto action) {
    test::owning8m::Controller control;
    {
      test::owning8m::Scope scope(control);
      action(control);
    }
    check(
        control.valid && !control.live &&
            std::ranges::all_of(control.records, [](auto &r) { return !r.p; }),
        "real zero receipts");
    component_row(name, index, control.calls);
    ++checks;
  };
  for (unsigned k = 0; k < 4; ++k)
    run("query-reject", k, [&](auto &control) {
      auto x = c;
      if (k == 0)
        x.stream.context_count = 46;
      if (k == 1)
        x.limits.max_frame_size = 0;
      if (k == 2)
        x.external = std::numeric_limits<std::size_t>::max();
      if (k == 3)
        x.limits.max_internal_buffered_bytes = q.initial_bytes - 1;
      auto r = Adapter::query(x);
      check(r.error != Code::none, "query reject");
      Adapter a(x);
      f->output.fill(sentinel);
      auto p = a.process({}, std::span(f->output).first(4096), end);
      check(p.status == Status::error && !p.input_consumed &&
                !p.output_produced,
            "constructor sticky");
      check(a.ledger().calls == 0 && control.calls == 0, "no initial callback");
      f->guards(f->output);
    });
  run("initial-exact", 0, [&](auto &) {
    auto x = c;
    x.limits.max_internal_buffered_bytes = q.initial_bytes;
    Adapter a(x);
    check(a.ledger().live == 262464 && a.ledger().calls == 2,
          "exact initial admission");
  });
  run("empty", 0, [&](auto &control) {
    auto x = c;
    x.stream.original_size = 0;
    auto r = Adapter::query(x);
    check(r.error == Code::none && !r.raw_bytes && !r.index_entries,
          "empty query");
    Adapter a(x);
    f->output.fill(sentinel);
    auto p = a.process({}, std::span(f->output).first(4096), end);
    check(p.status == Status::end_of_stream && p.output_produced == 112 &&
              !a.ledger().calls && !control.calls,
          "empty completion");
    f->guards(std::span(f->output).subspan(112));
  });
  for (unsigned k = 0; k < 7; ++k)
    run("boundary", k, [&](auto &control) {
      Adapter a(c);
      f->output.fill(sentinel);
      std::array<std::byte, 161> input{};
      std::span<const std::byte> in{};
      auto out = std::span(f->output).first(4096);
      if (k == 0)
        in = input;
      if (k == 1)
        out = f->output;
      if (k == 2)
        in = std::span(f->output).first(1);
      if (k == 3)
        in = {reinterpret_cast<const std::byte *>(&a), 1};
      if (k == 4)
        out = {reinterpret_cast<std::byte *>(&a), 1};
      if (k >= 5) {
        if (!isolated) {
          in = input;
        } // Ordinary link has no test-controller pointers.
        else {
          check(control.records[0].p, "raw receipt");
          if (k == 5)
            in = {static_cast<const std::byte *>(control.records[0].p), 1};
          else
            out = {static_cast<std::byte *>(control.records[0].p), 1};
        }
      }
      auto p = a.process(in, out, 0);
      check(p.status == Status::error &&
                p.error.code == Code::invalid_argument && !p.input_consumed &&
                !p.output_produced,
            "boundary refused");
      auto again = a.process(input, f->output, 0xffffffffu);
      check(again.error.code == p.error.code &&
                again.error.byte_position == p.error.byte_position &&
                !again.input_consumed && !again.output_produced,
            "wrapper sticky");
      f->guards(f->output);
    });
  for (unsigned k = 0; k < 3; ++k)
    run("flags-size", k, [&](auto &) {
      Adapter a(c);
      f->output.fill(sentinel);
      auto p =
          k == 0 ? a.process({}, std::span(f->output).first(4096),
                             core::flag_value(core::ProcessFlags::reset_block))
          : k == 1 ? a.process(std::span(f->raw).first(159),
                               std::span(f->output).first(4096), end)
                   : a.process(std::span(f->raw).first(160),
                               std::span(f->output).first(4096), end);
      if (k == 0)
        check(p.status == Status::error && p.error.code == Code::unsupported &&
                  !p.output_produced,
              "reset unsupported");
      if (k == 1)
        check(p.status == Status::error &&
                  p.error.code == Code::malformed_stream,
              "short known size");
      if (k == 2) {
        check(p.status == Status::end_of_stream, "known size success");
        p = a.process(std::span(f->raw).first(1), f->output, 0xffffffffu);
        check(p.status == Status::end_of_stream && !p.input_consumed &&
                  !p.output_produced,
              "ended precedence");
      }
    });
  run("excess-size", 0, [&](auto &) {
    auto x = c;
    x.input_capacity = 161;
    Adapter a(x);
    std::array<std::byte, 161> input{};
    std::copy(f->raw.begin(), f->raw.end(), input.begin());
    f->output.fill(sentinel);
    auto p = a.process(input, std::span(f->output).first(4096), end);
    check(p.status == Status::error && p.error.code == Code::malformed_stream,
          "excess known size");
  });
  for (unsigned k = 0; k < 3; ++k)
    run("split", k, [&](auto &) {
      Adapter a(c);
      f->output.fill(sentinel);
      std::size_t consumed{}, written{}, step{};
      auto z = a.process({}, {}, 0);
      check(z.status == Status::need_output, "zero output");
      while (true) {
        check(++step < 10000, "split guard");
        auto count =
            std::min<std::size_t>(f->raw.size() - consumed, k == 0   ? 1
                                                            : k == 1 ? 13
                                                                     : 160);
        auto cap = k == 0 ? 1u : k == 1 ? 17u : 4096u;
        auto p =
            a.process(std::span(f->raw).subspan(consumed, count),
                      std::span(f->output).subspan(
                          written, std::min<std::size_t>(cap, 4096 - written)),
                      (consumed + count == 160 ? end : 0) |
                          core::flag_value(core::ProcessFlags::flush));
        check(p.status != Status::error &&
                  (p.status != Status::progress || p.input_consumed ||
                   p.output_produced),
              "split status");
        consumed += p.input_consumed;
        written += p.output_produced;
        if (p.status == Status::end_of_stream)
          break;
      }
      check(consumed == 160 && written == f->wire &&
                std::equal(f->oracle.begin(), f->oracle.begin() + written,
                           f->output.begin()),
            "split wire equivalence");
      f->guards(std::span(f->output).subspan(written));
    });
  run("one-byte", 0, [&](auto &) {
    auto x = c;
    x.stream.original_size = 1;
    Adapter a(x);
    auto p = a.process(std::span(f->raw).first(1),
                       std::span(f->output).first(4096), end);
    check(p.status == Status::end_of_stream && p.input_consumed == 1,
          "one byte");
  });
  run("accepted-guard-position", 0, [&](auto &) {
    Adapter a(c);
    auto p = a.process(std::span(f->raw).first(1),
                       std::span(f->output).first(113), 0);
    check(p.input_consumed == 1 && p.output_produced == 112 &&
              p.status == Status::progress,
          "prior accepted byte");
    p = a.process({}, f->output, 0);
    check(p.status == Status::error && p.error.code == Code::invalid_argument &&
              p.error.byte_position == 1 && !p.input_consumed &&
              !p.output_produced,
          "guard prior accepted position");
  });
  run("literal-oracles", 0, [&](auto &) {
    for (std::size_t i = 0; i < f->raw.size(); ++i)
      f->raw[i] = static_cast<std::byte>(i % 64);
    f->oracle.fill(sentinel);
    {
      LzssPositionDistance8mStreamEncoder e(
          f->header, f->limits, f->op_raw, f->pub,
          {f->tokens, f->scratch, f->op_index, f->ops, f->op_scratch, f->frame,
           f->payload},
          Fixture::extra());
      auto p = e.process(f->raw, std::span(f->oracle).first(4096), end);
      check(p.status == Status::end_of_stream && p.output_produced == 530,
            "literal operation");
    }
    {
      LzssPositionDistance8mOwnedStreamEncoder e(
          f->header, f->limits, f->oracle_allocator, Fixture::extra());
      auto p = e.process(f->raw, std::span(f->output).first(4096), end);
      check(p.status == Status::end_of_stream && p.output_produced == 530 &&
                std::equal(f->oracle.begin(), f->oracle.begin() + 530,
                           f->output.begin()),
            "literal safe");
    }
    f->oracle_allocator.zero();
    {
      Adapter a(c);
      f->output.fill(sentinel);
      auto p = a.process(f->raw, std::span(f->output).first(4096), end);
      check(p.status == Status::end_of_stream && p.output_produced == 530 &&
                std::equal(f->oracle.begin(), f->oracle.begin() + 530,
                           f->output.begin()),
            "literal adapter");
      f->guards(std::span(f->output).subspan(530));
    }
    {
      LzssPositionDistance8mStreamDecoder d(
          f->limits, f->decode_pub, f->decode_tokens, f->decode_scratch,
          f->decode_raw, f->decode_raw_scratch, Fixture::extra());
      auto p = d.process(std::span(f->output).first(530),
                         std::span(f->decoded).first(160), end);
      check(p.status == Status::end_of_stream && p.output_produced == 160 &&
                std::equal(f->raw.begin(), f->raw.end(), f->decoded.begin()),
            "literal decode");
    }
    for (std::size_t i = 0; i < f->raw.size(); ++i)
      f->raw[i] = static_cast<std::byte>((i % 64) % 7);
    f->make_oracle(); // Restore the match-bearing oracle before fault cases.
  });
  std::size_t discovered{};
  run("discover", 0, [&](auto &control) {
    Adapter a(c);
    auto p = a.process(f->raw, std::span(f->output).first(4096), end);
    check(p.status == Status::end_of_stream && a.ledger().live == 262858 &&
              a.ledger().peak == 263252,
          "discovery ledger");
    discovered = a.ledger().calls;
    if (isolated)
      check(control.calls == discovered && control.live == a.ledger().live &&
                control.peak == a.ledger().peak,
            "independent ledger");
  });
  if (isolated)
    for (unsigned oversize = 0; oversize < 2; ++oversize)
      for (unsigned n = 1; n <= discovered; ++n)
        run(oversize ? "oversize-return" : "allocation-refusal", n,
            [&](auto &control) {
              if (oversize)
                control.oversize_at = n;
              else
                control.fail_at = n;
              Adapter a(c);
              f->output.fill(sentinel);
              auto p = a.process(f->raw, std::span(f->output).first(4096), end);
              check(p.status == Status::error &&
                        p.error.code == Code::out_of_memory &&
                        control.calls == n && control.injections == 1,
                    "actual allocation refusal");
              const auto target = n <= 2 ? 0u : (n - 3) / 5;
              const auto expected = n <= 2 ? 0 : f->boundaries[target];
              check(p.output_produced == expected &&
                        p.input_consumed ==
                            (n <= 2 ? 0
                                    : std::min<std::size_t>(160,
                                                            (target + 1) * 64)),
                    "allocation prefix counts");
              check(std::equal(f->oracle.begin(), f->oracle.begin() + expected,
                               f->output.begin()),
                    "allocation prefix bytes");
              f->guards(std::span(f->output).subspan(expected));
              auto repeat = a.process(
                  {}, std::span(f->output).subspan(expected, 4096 - expected),
                  0xffffffffu);
              check(repeat.status == Status::error &&
                        repeat.error.code == p.error.code &&
                        repeat.error.byte_position == p.error.byte_position &&
                        !repeat.output_produced,
                    "allocation sticky");
        });
  for (unsigned k=0;k<2;++k) run("generation-threshold",k,[&](auto &){
    auto x=c;
    x.limits.max_internal_buffered_bytes=q.fixed_bytes+263252-k;
    Adapter a(x); f->output.fill(sentinel);
    auto p=a.process(f->raw,std::span(f->output).first(4096),end);
    if(!k) check(p.status==Status::end_of_stream && p.output_produced==394 &&
                 a.ledger().peak==263252,"generation exact ceiling");
    else check(p.status==Status::error && p.output_produced==206 &&
               p.input_consumed==128 && a.ledger().live==262858,
               "generation below ceiling privacy");
    f->guards(std::span(f->output).subspan(p.output_produced));
  });
  return checks;
}

} // namespace
int main(int argc, char **argv) {
  try {
    check(argc == 2, "mode required");
    const std::string_view arg(argv[1]);
    check(arg == "--companion" || arg == "--isolated", "mode selector");
    bool companion = arg == "--companion";
    check(sum({Fixture::extra(), 65536}) < ceiling,
          "fixture prospective fixed-owner bound");
    unsigned checks{};
    for (unsigned mode = 0; mode <= (companion ? 0u : 6u); ++mode) {
      for (unsigned replacement = 0; replacement < 2; ++replacement) {
        auto f = std::make_unique<Fixture>();
        f->make_oracle();
        owner_case(*f, companion, mode, replacement);
        ++checks;
      }
      for (unsigned target = 0; target < 3; ++target)
        for (bool staged : {false, true}) {
          auto f = std::make_unique<Fixture>();
          f->make_oracle();
          stream_case(*f, companion, mode, target, staged);
          ++checks;
        }
    }
    check(checks == (companion ? 8u : 56u), "matrix size");
    checks += components(!companion);
    std::cout << "{\"summary\":true,\"passed\":true,\"checks\":" << checks
              << ",\"companion\":" << (companion ? "true" : "false")
              << ",\"timed\":false}\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
