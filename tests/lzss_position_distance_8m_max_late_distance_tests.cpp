// Private diagnostic only. Independently authored from repository contracts.
// No production target, codec selection, stream format or default is changed.
#include "context/lzss_position_distance_8m_tokens.hpp"
#include "core/checked_math.hpp"
#include "core/sha256.hpp"
#include "frame/lzss_position_distance_8m_owned_stream_encoder.hpp"
#include "frame/lzss_position_distance_8m_owning_adapter.hpp"
#include "frame/lzss_position_distance_8m_prepared_stream_encoder.hpp"
#include "frame/lzss_position_distance_8m_stream_decoder.hpp"
#include "frame/lzss_position_distance_8m_stream_encoder.hpp"
#include "frame/lzss_position_distance_8m_token_frame_encoder.hpp"
#include "lzss_position_distance_8m_max_fault_seam.hpp"
#include <algorithm>
#include <array>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {
using namespace marc;
using namespace frame::internal;
using Token = dictionary::internal::LzssTypedToken;
using Op = context::internal::ModeledOperation;
using Old = LzssPositionDistance8mStreamEncoder;
using New = LzssPositionDistance8mOwnedStreamEncoder;
using Prepared = LzssPositionDistance8mPreparedStreamEncoder;
using Decode = LzssPositionDistance8mStreamDecoder;
using Adapter = LzssPositionDistance8mOwningAdapter;
constexpr std::size_t mib = 1u << 20;
constexpr std::size_t ceiling = 1024u * mib;
constexpr auto end = core::flag_value(core::ProcessFlags::end_input);
void require(bool b, const char *message) {
  if (!b)
    throw std::runtime_error(message);
}
std::size_t add(std::initializer_list<std::size_t> values) {
  std::size_t n{};
  for (auto v : values)
    require(core::checked_add(n, v, n), "addition");
  return n;
}
std::size_t mul(std::size_t a, std::size_t b) {
  std::size_t n{};
  require(core::checked_multiply(a, b, n), "multiplication");
  return n;
}
template <class T> std::size_t bytes(const std::vector<T> &v) {
  return mul(v.capacity(), sizeof(T));
}
void admit(std::size_t n) { require(n <= ceiling, "diagnostic policy"); }
struct Stats {
  std::size_t logical{}, block_peak{}, final_live{}, allocations{}, calls{},
      written{};
};
struct Plan {
  std::size_t tokens{}, events{}, payload{}, prior{}, planner_allocated{},
      planner_logical{}, literals{}, matches{}, match_bytes{};
};
using Digest = std::array<std::byte, core::sha256_digest_size>;
constexpr std::byte guard{0xa7};
constexpr std::array<std::array<unsigned, 3>, 6> orders{
    {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}}};
Digest digest(std::span<const std::byte> input) {
  core::Sha256 hash;
  Digest result{};
  require(hash.update(input) && hash.finalize(result), "digest");
  return result;
}
void print_digest(const Digest &d) {
  constexpr char hex[] = "0123456789abcdef";
  std::cout << "\"";
  for (auto b : d) {
    const auto n = std::to_integer<unsigned>(b);
    std::cout << hex[n >> 4] << hex[n & 15];
  }
  std::cout << "\"";
}
struct Buffers {
  std::vector<std::byte> raw, publication, frame, payload;
  std::vector<Token> tokens, scratch;
  std::vector<Op> operations, operation_scratch;
  std::vector<std::uint32_t> index;
  auto workspace() {
    return LzssPositionDistance8mFrameEncodeWorkspace{
        tokens, scratch, index, operations, operation_scratch, frame, payload};
  }
  auto capacities(std::size_t in, std::size_t out) const {
    return LzssPositionDistance8mStreamEncodeCapacities{
        raw.size(),
        publication.size(),
        tokens.size(),
        scratch.size(),
        index.size(),
        operations.size(),
        operation_scratch.size(),
        frame.size(),
        payload.size(),
        in,
        out};
  }
  std::size_t full() const {
    return add({bytes(raw), bytes(publication), bytes(frame), bytes(payload),
                bytes(tokens), bytes(scratch), bytes(operations),
                bytes(operation_scratch), bytes(index)});
  }
  std::size_t sizes() const {
    return add({raw.size(), publication.size(), frame.size(), payload.size(),
                mul(tokens.size() + scratch.size(), sizeof(Token)),
                mul(operations.size() + operation_scratch.size(), sizeof(Op)),
                mul(index.size(), 4)});
  }
};
class Allocator final : public LzssPositionDistance8mStreamAllocator {
public:
  struct Receipt {
    const void *data{};
    std::size_t capacity{};
  };
  std::array<Receipt, 16> receipts{};
  LzssPositionDistance8mExactStreamAllocator exact;
  std::size_t live{}, peak{}, calls{}, fail_at{};
  bool reject() noexcept { return fail_at && calls + 1 == fail_at; }
  LzssPositionDistance8mAllocatorControls controls() const noexcept override {
    return {this, sizeof(*this), 256};
  }
  template <class T> auto keep(LzssPositionDistance8mOwnedBlock<T> b) noexcept {
    ++calls;
    if (b.data) {
      for (auto &r : receipts)
        if (!r.data) {
          r = {b.data, mul(b.capacity, sizeof(T))};
          live = add({live, r.capacity});
          peak = std::max(peak, live);
          return b;
        }
      std::terminate();
    }
    return b;
  }
  auto tokens(std::size_t n) noexcept
      -> LzssPositionDistance8mOwnedTokens override {
    return keep(reject() ? LzssPositionDistance8mOwnedTokens{}
                         : exact.tokens(n));
  }
  auto bytes(std::size_t n) noexcept
      -> LzssPositionDistance8mOwnedBytes override {
    return keep(reject() ? LzssPositionDistance8mOwnedBytes{} : exact.bytes(n));
  }
  auto indices(std::size_t n) noexcept
      -> LzssPositionDistance8mOwnedIndex override {
    return keep(reject() ? LzssPositionDistance8mOwnedIndex{}
                         : exact.indices(n));
  }
  template <class T>
  void destroy(LzssPositionDistance8mOwnedBlock<T> &b) noexcept {
    auto p = b.data;
    exact.release(b); // Real deletion precedes receipt removal.
    if (p) {
      for (auto &r : receipts)
        if (r.data == p) {
          live -= r.capacity;
          r = {};
          return;
        }
      std::terminate();
    }
  }
  void release(LzssPositionDistance8mOwnedTokens &b) noexcept override {
    destroy(b);
  }
  void release(LzssPositionDistance8mOwnedBytes &b) noexcept override {
    destroy(b);
  }
  void release(LzssPositionDistance8mOwnedIndex &b) noexcept override {
    destroy(b);
  }
};
// Named conservative reservations for all phase-local controls. These may
// exceed their physical union. Array storage, owners and call views are extra.
struct Controls {
  Buffers buffers;
  Allocator allocator;
  TypedContextFrameValidationContext context;
  LzssPositionDistance8mStreamEncodeCapacities capacities;
  LzssPositionDistance8mStreamEncodeRequirements encode_query;
  LzssPositionDistance8mStreamRequirements decode_query;
  LzssPositionDistance8mTokenFrameWorkspace token_workspace;
  LzssPositionDistance8mTokenFramePlan token_plan;
  dictionary::internal::LzssPositionDistance8mParsePlan parse_plan;
  core::ProcessResult result;
  std::array<std::size_t, 128> scalars;
  std::array<LzssPositionDistance8mOwningConfig, 8> adapter_configs;
  std::array<LzssPositionDistance8mOwningRequirements, 8> adapter_queries;
  std::array<Adapter::Ledger, 8> adapter_ledgers;
  std::array<test::max8m::Observer, 2> delegate_controls;
  std::array<std::byte, 4096> separate_delegate_working;
  core::Sha256 hash;
  std::array<Digest, 8> digests;
  // Concrete conservative reservation for SHA-256 transform/observer locals.
  std::array<std::uint32_t, 128> hash_controls;
  std::array<std::byte, 128> hash_bytes;
  std::array<std::span<const std::byte>, 4> inputs;
  std::array<std::span<std::byte>, 4> outputs;
};
struct Driver {
  core::DecoderLimits limits{};
  TypedContextStreamHeader header{};
  std::vector<std::byte> raw, reference, owned, prepared, decoded;
  Stats old_stats{}, new_stats{}, prepared_stats{};
  std::size_t adapter_fixed{}, adapter_initial{}, exact_budget{},
      refusal_written{}, refusal_consumed{}, refusal_position{}, refusal_live{},
      refusal_calls{}, delegate_calls{}, delegate_deleted{}, delegate_peak{};
  unsigned refusal_code{}, rejected_configs{};
  bool isolated{};
  std::array<Stats, 3> decode_stats{};
  std::array<Plan, 3> plans{};
  Digest raw_digest{}, wire_digest{};
  std::size_t f{}, t{}, e{}, p{}, frame_count{}, wire{}, block_plan{},
      final_plan{}, prospective_common{}, extra_base{}, extra_test{};
  unsigned marker{};
  std::array<std::size_t, 3> far_positions{}, far_distances{}, far_lengths{};
  std::size_t retained() const {
    return add({extra_test, sizeof(*this), sizeof(Controls), bytes(raw),
                bytes(reference), bytes(owned), bytes(prepared),
                bytes(decoded)});
  }
  auto op_caps() const {
    return LzssPositionDistance8mStreamEncodeCapacities{
        f, add({80, p}), t,   t, add({65536, f}), e, e, add({80, p}),
        p, raw.size(),   wire};
  }
  std::size_t phase_peak(std::size_t common) const {
    auto q = query_lzss_position_distance_8m_stream_encode_workspace(
        limits, op_caps(), common);
    require(q.error == core::ErrorCode::none,
            "prospective operation admission");
    auto peak = q.aggregate_bytes;
    for (auto working : {New::working_bytes(), Prepared::working_bytes()})
      peak = std::max(peak, add({common, block_plan, working, sizeof(Allocator),
                                 256, raw.size(), wire}));
    auto d = query_lzss_position_distance_8m_stream_workspace(
        limits, add({80, p}), t, t, f, f, add({common, wire, raw.size()}));
    require(d.error == core::ErrorCode::none, "prospective decoder admission");
    auto config = LzssPositionDistance8mOwningConfig{header, limits, common,
                                                     raw.size(), wire};
    const auto a = Adapter::query(config);
    require(a.error == core::ErrorCode::none,
            "prospective adapter initial admission");
    const auto adapter_total = add({a.fixed_bytes, block_plan});
    admit(adapter_total);
    return std::max({peak, d.aggregate_bytes, adapter_total});
  }
  void prepare(std::size_t n, unsigned pattern, unsigned kind) {
    require(n == 8 * mib, "maximum frame size");
    require((pattern == 0 || pattern == 5 || pattern == 258) && kind == 2,
            "recipe");
    marker = pattern;
    extra_test = extra_base;
    f = n;
    frame_count = kind == 0 ? 1 : kind == 1 ? 2 : 3;
    limits.max_frame_size = limits.max_block_size = 8 * mib;
    limits.max_lz_distance = 8 * mib;
    limits.max_internal_buffered_bytes = ceiling;
    header.frame_size = static_cast<std::uint32_t>(f);
    header.original_size = add({mul(f, kind ? 2 : 1), kind == 2 ? 32u : 0u});
    header.dictionary = {8 * mib, 3, 258, 0};
    header.dictionary_variant = 11;
    header.context_algorithm = 1;
    header.context_variant = 12;
    header.context_count = 47;
    header.range_model_total = 32768;
    admit(add({retained(), static_cast<std::size_t>(header.original_size)}));
    raw.resize(static_cast<std::size_t>(header.original_size));
    admit(retained());
    for (std::size_t i = 0; i < f; ++i)
      raw[i] = pattern == 0
                   ? std::byte(i % 256)
                   : std::byte((i < pattern || i >= f - pattern) ? 7 : 0);
    if (kind)
      std::copy_n(raw.begin(), f, raw.begin() + f);
    if (kind == 2)
      std::copy_n(raw.begin(), 32, raw.begin() + 2 * f);
    wire = 112;
    std::size_t prior{}, previous{}, generation_peak{};
    for (std::size_t j = 0; j < frame_count; ++j) {
      // Each actual frame is independently planned with its real
      // sequence/prior.
      const auto length = std::min(f, raw.size() - prior);
      const auto input = std::span<const std::byte>(raw).subspan(prior, length);
      Buffers b;
      auto planner_extra = add({retained(), raw.size() - length});
      admit(
          add({planner_extra, mul(add({65536, length}), 4),
               lzss_position_distance_8m_token_frame_working_bytes(), length}));
      b.index.resize(add({65536, length}));
      admit(
          add({planner_extra, b.full(),
               lzss_position_distance_8m_token_frame_working_bytes(), length}));
      auto count =
          dictionary::internal::query_lzss_position_distance_8m_indexed(
              input, header.dictionary, limits, 0, 0, b.index, planner_extra);
      require(count.error ==
                  dictionary::internal::LzssPositionDistance8mParseError::
                      output_too_small,
              "token count");
      const auto tj = count.details.token_count;
      require(tj <= length, "token bound");
      admit(
          add({planner_extra, b.full(), mul(24, tj),
               lzss_position_distance_8m_token_frame_working_bytes(), length}));
      b.tokens.resize(tj);
      b.scratch.resize(tj);
      admit(
          add({planner_extra, b.full(),
               lzss_position_distance_8m_token_frame_working_bytes(), length}));
      auto context =
          TypedContextFrameValidationContext{header, limits, j, prior};
      auto w = LzssPositionDistance8mTokenFrameWorkspace{
          b.tokens, b.scratch, b.index, {}, {}};
      auto plan = query_lzss_position_distance_8m_token_frame_encode(
          input, context, w, 0, add({planner_extra, b.full() - b.sizes()}));
      require(
          plan.error ==
                  LzssPositionDistance8mTokenFrameError::storage_too_small &&
              plan.bytes_required >= 80,
          "payload count");
      plans[j] = {
          tj,
          plan.counts.declared_event_count,
          plan.bytes_required - 80,
          prior,
          b.full(),
          add({planner_extra, b.full(),
               lzss_position_distance_8m_token_frame_working_bytes(), length})};
      admit(add({planner_extra, b.full(),
                 lzss_position_distance_8m_token_frame_working_bytes(),
                 mul(2, length)}));
      std::vector<std::byte> expanded(length);
      plans[j].planner_logical =
          add({planner_extra, b.full(),
               lzss_position_distance_8m_token_frame_working_bytes(), length,
               bytes(expanded)});
      admit(plans[j].planner_logical);
      std::size_t position{};
      bool terminal = false;
      for (const auto &token : b.tokens) {
        const auto checked =
            context::internal::validate_lzss_position_distance_8m_token(
                token, header.dictionary, {position, length}, limits);
        require(checked.error ==
                    dictionary::internal::LzssTypedTokenError::none,
                "every token field");
        if (token.kind == dictionary::internal::LzssTypedTokenKind::literal) {
          ++plans[j].literals;
          expanded[position++] = std::byte(token.literal);
        } else {
          ++plans[j].matches;
          plans[j].match_bytes = add({plans[j].match_bytes, token.length});
          if (marker && length == f && position == f - marker) {
            require(token.distance == f - marker && token.length == marker,
                    "actual terminal distant match");
            require(token.distance > 4 * mib && token.distance >= (1u << 22) &&
                        token.distance < (1u << 23),
                    "actual distance class22");
            far_positions[j] = position;
            far_distances[j] = token.distance;
            far_lengths[j] = token.length;
            terminal = true;
          }
          for (std::size_t k = 0; k < token.length; ++k) {
            expanded[position] = expanded[position - token.distance];
            ++position;
          }
        }
        require(position == checked.next_raw_size,
                "independent expansion position");
      }
      require(position == length && std::ranges::equal(expanded, input),
              "independent full expansion");
      require(!marker || length != f || terminal,
              "distant fixture prerequisite");
      require(plans[j].literals + plans[j].matches == tj &&
                  plans[j].literals + plans[j].match_bytes == length,
              "composition expansion");
      require(plans[j].payload <= limits.max_compressed_payload_size &&
                  plans[j].events <= mul(2, length),
              "plan bounds");
      t = std::max(t, tj);
      e = std::max(e, plans[j].events);
      p = std::max(p, plans[j].payload);
      wire = add({wire, plan.bytes_required});
      const auto generation = add({mul(24, tj), mul(3, plans[j].payload), 160});
      generation_peak = std::max(generation_peak, add({previous, generation}));
      previous = generation;
      prior = add({prior, length});
    } // All planning allocations have really been destroyed.
    const auto initial = add({f, mul(add({65536, f}), 4)});
    block_plan = add({initial, generation_peak});
    final_plan = add({initial, previous});
    extra_test =
        add({extra_base, mul(mul(2, t), sizeof(Token)), mul(3, add({80, p}))});
    prospective_common =
        add({retained(), mul(3, add({wire, 16})), raw.size(), 16});
    admit(
        phase_peak(prospective_common)); // Joint admission BEFORE destinations.
    reference.assign(add({wire, 16}), guard);
    owned.assign(add({wire, 16}), guard);
    prepared.assign(add({wire, 16}), guard);
    decoded.assign(add({raw.size(), 16}), guard);
    admit(phase_peak(
        retained())); // Actual full capacities, no capacity discount.
    raw_digest = digest(raw);
  }
  void check_guard(const std::vector<std::byte> &v, std::size_t n) const {
    require(v.size() == add({n, 16}) &&
                std::ranges::all_of(std::span(v).subspan(n),
                                    [](std::byte b) { return b == guard; }),
            "guard");
  }
  template <class Transform>
  std::size_t drive(Transform &x, std::span<const std::byte> input,
                    std::span<std::byte> output, Stats &s) {
    std::size_t i{}, o{};
    for (std::size_t k = 0; k < 10000; ++k) {
      auto r = x.process(input.subspan(i), output.subspan(o), end);
      require(r.input_consumed <= input.size() - i &&
                  r.output_produced <= output.size() - o,
              "counts");
      i += r.input_consumed;
      o += r.output_produced;
      ++s.calls;
      require(r.status != core::StreamStatus::progress || r.input_consumed ||
                  r.output_produced,
              "zero progress");
      require(r.status != core::StreamStatus::error, "process failed");
      if (r.status == core::StreamStatus::end_of_stream) {
        require(i == input.size() && o == output.size(), "complete sizes");
        auto again = x.process({}, {}, 0xffffffffu);
        require(again.status == core::StreamStatus::end_of_stream &&
                    !again.input_consumed && !again.output_produced,
                "terminal");
        return o;
      }
      require(r.status != core::StreamStatus::need_input || i < input.size(),
              "unexpected starvation");
      require(r.status != core::StreamStatus::need_output || o < output.size(),
              "unexpected output exhaustion");
    }
    throw std::runtime_error("call guard");
  }
  void old_encode() {
    const auto caps = op_caps();
    auto q = query_lzss_position_distance_8m_stream_encode_workspace(
        limits, caps, retained());
    require(q.error == core::ErrorCode::none, "reference admission");
    admit(q.aggregate_bytes);
    {
      Buffers b;
      b.raw.resize(f);
      b.publication.resize(80 + p);
      b.frame.resize(80 + p);
      b.payload.resize(p);
      b.tokens.resize(t);
      b.scratch.resize(t);
      b.operations.resize(e);
      b.operation_scratch.resize(e);
      b.index.resize(65536 + f);
      require(b.full() >= b.sizes(), "workspace spare subtraction");
      auto external = add({retained(), b.full() - b.sizes()});
      q = query_lzss_position_distance_8m_stream_encode_workspace(
          limits, b.capacities(raw.size(), wire), external);
      require(q.error == core::ErrorCode::none, "reference actual capacities");
      admit(q.aggregate_bytes);
      old_stats.logical = q.aggregate_bytes;
      old_stats.block_peak = b.full();
      old_stats.final_live = b.full();
      Old x(header, limits, b.raw, b.publication, b.workspace(), external);
      old_stats.written =
          drive(x, raw, std::span(reference).first(wire), old_stats);
    }
  }
  template <class Path>
  void owner_encode(std::vector<std::byte> &destination, Stats &stats) {
    stats.logical = add({retained(), block_plan, Path::working_bytes(),
                         sizeof(Allocator), 256, raw.size(), wire});
    admit(stats.logical);
    Allocator a;
    {
      Path x(header, limits, a, retained());
      stats.written = drive(x, raw, std::span(destination).first(wire), stats);
      stats.block_peak = a.peak;
      stats.final_live = a.live;
      stats.allocations = a.calls;
      require(a.peak == block_plan && a.live == final_plan,
              "owner actual ledger");
    }
    require(a.live == 0 && std::ranges::all_of(a.receipts,
                                               [](auto &r) { return !r.data; }),
            "real destruction receipts");
  }
  void decode(const std::vector<std::byte> &source, Stats &stats) {
    // ALL encoders are already gone. Every consumer has a fresh workspace.
    auto extra = add({retained(), wire, raw.size()});
    auto q = query_lzss_position_distance_8m_stream_workspace(
        limits, add({80, p}), t, t, f, f, extra);
    require(q.error == core::ErrorCode::none, "decoder admission");
    admit(q.aggregate_bytes);
    {
      Buffers b;
      b.publication.resize(add({80, p}));
      b.tokens.resize(t);
      b.scratch.resize(t);
      b.raw.resize(f);
      b.frame.resize(f);
      extra = add({extra, b.full() - b.sizes()});
      q = query_lzss_position_distance_8m_stream_workspace(
          limits, b.publication.size(), t, t, f, f, extra);
      require(q.error == core::ErrorCode::none, "decoder actual capacities");
      admit(q.aggregate_bytes);
      stats.logical = q.aggregate_bytes;
      stats.block_peak = b.full();
      stats.final_live = b.full();
      Decode x(limits, b.publication, b.tokens, b.scratch, b.raw, b.frame,
               extra);
      stats.written = drive(x, std::span(source).first(wire),
                            std::span(decoded).first(raw.size()), stats);
    }
    require(std::ranges::equal(raw, std::span(decoded).first(raw.size())),
            "raw bytes");
    require(digest(std::span(decoded).first(raw.size())) == raw_digest,
            "raw digest");
    check_guard(source, wire);
    check_guard(decoded, raw.size());
  }

  void adapter_encode() {
    auto c = LzssPositionDistance8mOwningConfig{header, limits, retained(),
                                                raw.size(), wire};
    const auto q = Adapter::query(c);
    require(q.error == core::ErrorCode::none, "oracle adapter query");
    adapter_fixed = q.fixed_bytes;
    adapter_initial = q.initial_bytes;
    exact_budget = add({q.fixed_bytes, block_plan});
    admit(exact_budget);
    prepared_stats.logical = exact_budget;
    {
      Adapter x(c);
      prepared_stats.written =
          drive(x, raw, std::span(prepared).first(wire), prepared_stats);
      auto ledger = x.ledger();
      require(ledger.peak == block_plan && ledger.live == final_plan,
              "oracle adapter capacity");
      prepared_stats.block_peak = ledger.peak;
      prepared_stats.final_live = ledger.live;
      prepared_stats.allocations = ledger.calls;
    }
    require(std::ranges::equal(reference, prepared), "oracle adapter wire");
    check_guard(prepared, wire);
  }
  void run(unsigned order) {
    require(order == 0, "fixed untimed order");
    for (auto path : orders[order]) {
      if (path == 0)
        old_encode();
      else if (path == 1)
        owner_encode<New>(owned, new_stats);
      else
        adapter_encode();
    }
    require(std::ranges::equal(reference, owned) &&
                std::ranges::equal(reference, prepared),
            "wire bytes");
    wire_digest = digest(std::span(reference).first(wire));
    require(digest(std::span(owned).first(wire)) == wire_digest &&
                digest(std::span(prepared).first(wire)) == wire_digest,
            "wire digest");
    decode(reference, decode_stats[0]);
    decode(owned, decode_stats[1]);
    decode(prepared, decode_stats[2]);
  }
};
void print(const char *name, const Stats &s) {
  std::cout << "\"" << name << "\":{\"logical_reservation\":" << s.logical
            << ",\"allocation_peak\":" << s.block_peak
            << ",\"final_live\":" << s.final_live
            << ",\"allocations\":" << s.allocations << ",\"calls\":" << s.calls
            << ",\"written\":" << s.written << "}";
}

using Code = core::ErrorCode;
using Status = core::StreamStatus;
using Observer = test::max8m::Observer;
using Owner = LzssPositionDistance8mPreparedStorageOwner;
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
struct Direct final : LzssPositionDistance8mStreamAllocator {
  Observer &o;
  LzssPositionDistance8mExactStreamAllocator real;
  explicit Direct(Observer &v) : o(v) {}
  LzssPositionDistance8mAllocatorControls controls() const noexcept override {
    return {this, sizeof(*this), 4096};
  }
  template <class T> auto obtain(std::size_t n) noexcept {
    o.arrived();
    auto b = [&] {
      if constexpr (std::is_same_v<T, Token>)
        return real.tokens(n);
      else if constexpr (std::is_same_v<T, std::byte>)
        return real.bytes(n);
      else
        return real.indices(n);
    }();
    std::size_t count{};
    if (!core::checked_multiply(b.capacity, sizeof(T), count))
      std::terminate();
    if constexpr (std::is_same_v<T, std::byte>)
      std::fill_n(b.data, b.capacity, guard);
    o.obtained(b.data, b.capacity, count,
               std::is_same_v<T, Token>       ? 1
               : std::is_same_v<T, std::byte> ? 2
                                              : 3);
    return b;
  }
  template <class T>
  void drop(LzssPositionDistance8mOwnedBlock<T> &b) noexcept {
    auto *p = b.data;
    o.before_release(p, b.capacity,
                     std::is_same_v<T, Token>       ? 1
                     : std::is_same_v<T, std::byte> ? 2
                                                    : 3);
    real.release(b);
    o.valid = o.valid && !b.data && !b.capacity;
    o.released(p);
  }
  LzssPositionDistance8mOwnedTokens tokens(std::size_t n) noexcept override {
    return obtain<Token>(n);
  }
  LzssPositionDistance8mOwnedBytes bytes(std::size_t n) noexcept override {
    return obtain<std::byte>(n);
  }
  LzssPositionDistance8mOwnedIndex indices(std::size_t n) noexcept override {
    return obtain<std::uint32_t>(n);
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
};
std::size_t generation(const Plan &p) {
  return add({mul(mul(2, p.tokens), sizeof(Token)), mul(3, p.payload), 160});
}
std::size_t boundary(const Driver &d, unsigned target) {
  std::size_t n = 112;
  for (unsigned j = 0; j < target; ++j)
    n = add({n, 80, d.plans[j].payload});
  return n;
}
void reserve(Driver &d, Observer &o) {
  admit(add({d.retained(), Owner::working_bytes(), d.block_plan}));
  o.reserve(d.t, add({80, d.p}));
  require(o.snapshot_bytes() ==
              add({mul(mul(2, d.t), sizeof(Token)), mul(3, add({80, d.p}))}),
          "actual full snapshot capacities");
  admit(d.phase_peak(d.retained()));
}
test::max8m::Late selection(const Driver &d, unsigned mode, unsigned target) {
  require(target < 3 && (mode == 0 || mode == 1 || mode == 6),
          "explicit selection");
  const auto prior = mul(target, d.f);
  const auto length = std::min(d.f, d.raw.size() - prior);
  require(d.header.frame_size == d.f && prior <= d.header.original_size &&
              length == (target == 2 ? 32 : d.f),
          "selection context");
  test::max8m::Late c;
  c.mode = mode;
  c.prior = prior;
  c.sequence = target;
  c.raw = length;
  c.frame = d.f;
  return c;
}
void trace(const test::max8m::Late &c, bool isolated, unsigned mode) {
  require(c.valid && c.range == (isolated ? 1u : 0u) &&
              c.prefix == (isolated && mode != 1 ? 1u : 0u) &&
              c.reparse == (isolated && mode != 1 ? 1u : 0u) &&
              c.injections == (mode ? 1u : 0u),
          "exact successful stage and injection");
  if (isolated) {
    require(c.range_bytes == c.payload && c.range_bytes > 0,
            "original successful range");
    if (mode != 1)
      require(c.prefix_bytes == 80 && c.parsed_sequence == c.sequence,
              "original successful prefix/reparse");
  }
}
struct Result {
  std::size_t consumed{}, written{}, calls{}, live{}, peak{}, blocks{},
      position{};
  Code error{};
  bool position_available{};
};
void row(const Driver &d, const Observer &o, const test::max8m::Late &c,
         const Result &r, const char *kind, unsigned target, bool staged,
         bool isolated) {
  std::cout << "{\"passed\":true,\"untimed\":true,\"kind\":\"" << kind
            << "\",\"marker\":" << d.marker << ",\"mode\":" << c.mode
            << ",\"target\":" << target
            << ",\"staged\":" << (staged ? "true" : "false")
            << ",\"isolated\":" << (isolated ? "true" : "false")
            << ",\"range\":" << c.range << ",\"prefix\":" << c.prefix
            << ",\"reparse\":" << c.reparse
            << ",\"injections\":" << c.injections
            << ",\"range_bytes\":" << c.range_bytes
            << ",\"prefix_bytes\":" << c.prefix_bytes
            << ",\"original_sequence\":" << c.parsed_sequence
            << ",\"calls\":" << o.calls << ",\"deleted\":" << o.deleted
            << ",\"live_after_destroy\":" << o.live << ",\"peak\":" << o.peak
            << ",\"before_live\":" << o.before_live
            << ",\"candidate_bytes\":" << o.candidate_bytes
            << ",\"snapshot_count\":" << o.snapshot_count
            << ",\"snapshot_bytes\":" << o.snapshot_bytes()
            << ",\"pristine\":" << (o.pristine ? "true" : "false")
            << ",\"dirty\":" << (o.dirty ? "true" : "false")
            << ",\"consumed\":" << r.consumed << ",\"written\":" << r.written
            << ",\"code\":" << static_cast<unsigned>(r.error)
            << ",\"position_available\":"
            << (r.position_available ? "true" : "false")
            << ",\"position\":" << r.position << ",\"retained_live\":" << r.live
            << ",\"retained_peak\":" << r.peak
            << ",\"retained_blocks\":" << r.blocks
            << ",\"process_calls\":" << r.calls
            << ",\"common\":" << d.retained() << ",\"wire_sha256\":";
  print_digest(d.wire_digest);
  std::cout << ",\"output_sha256\":";
  print_digest(digest(std::span(d.prepared).first(r.written)));
  std::cout << "}\n" << std::flush;
}
void owner_case(Driver &d, bool isolated, unsigned mode, unsigned target) {
  Observer o;
  reserve(d, o);
  o.fault = mode != 0;
  o.snapshot_at = add({1, mul(5, target)});
  auto c = selection(d, mode, target);
  Result result;
  const auto index_count = add({65536, d.f});
  admit(add({d.retained(), mul(index_count, sizeof(std::uint32_t)), d.f,
             mul(2, generation(d.plans[0])), Owner::working_bytes(),
             sizeof(Direct), 4096}));
  std::vector<std::uint32_t> index(index_count);
  const auto extra = add({d.retained(), mul(index.capacity() - index.size(),
                                            sizeof(std::uint32_t))});
  std::fill(d.prepared.begin(), d.prepared.end(), guard);
  {
    Direct allocator(o);
    test::max8m::LateScope scope(c);
    Owner x(allocator);
    if (target) {
      auto first = x.encode(std::span(d.raw).first(d.f),
                            {d.header, d.limits, 0, 0}, index, extra);
      require(first.error == Code::none, "initial owner generation");
      x.acknowledge_drained();
    }
    auto old = x.publication();
    const auto layout = fields(x.layout());
    const auto pending = x.pending();
    auto r =
        x.encode(std::span(d.raw).subspan(mul(target, d.f), d.f),
                 {d.header, d.limits, target, mul(target, d.f)}, index, extra);
    result.error = r.error;
    if (mode) {
      require(r.error == Code::limit_exceeded && !r.bytes_validated,
              "late owner refusal");
      require(x.publication().data() == old.data() &&
                  x.publication().size() == old.size() &&
                  fields(x.layout()) == layout && x.pending() == pending,
              "all old owner publication/layout state");
      require(o.preserved() && o.snapshot_count == (target ? 5u : 0u),
              "all old contents and candidate releases");
    } else {
      require(r.error == Code::none && r.aggregate_bytes <= ceiling &&
                  x.pending(),
              "positive owner");
      require(
          std::ranges::equal(x.publication(),
                             std::span(d.reference)
                                 .subspan(boundary(d, target),
                                          add({80, d.plans[target].payload}))),
          "owner oracle wire");
    }
    result.live = o.live;
    result.peak = o.peak;
    result.written = x.publication().size();
    std::copy(x.publication().begin(), x.publication().end(),
              d.prepared.begin());
    trace(c, isolated, mode);
  }
  require(o.zero(), "owner real releases");
  require(std::ranges::all_of(std::span(d.prepared).subspan(result.written),
                              [](auto b) { return b == guard; }),
          "owner entire output suffix");
  row(d, o, c, result, "owner", target, false, isolated);
}
void adapter_case(Driver &d, bool isolated, unsigned mode, unsigned target,
                  bool staged) {
  Observer o;
  reserve(d, o);
  o.base = 2;
  o.snapshot_at = add({3, mul(5, target)});
  o.fault = mode != 0;
  auto c = selection(d, mode, target);
  Result result;
  std::fill(d.prepared.begin(), d.prepared.end(), guard);
  auto config = LzssPositionDistance8mOwningConfig{
      d.header, d.limits, d.retained(), d.raw.size(), d.wire};
  auto q = Adapter::query(config);
  require(q.error == Code::none, "complete adapter query");
  admit(add({q.fixed_bytes, d.block_plan}));
  {
    test::max8m::ObserveScope observation(o);
    test::max8m::LateScope scope(c);
    Adapter x(config);
    auto call = [&](std::span<const std::byte> in, std::size_t capacity,
                    std::uint32_t flags) {
      require(++result.calls < 10000 && capacity <= d.wire - result.written,
              "bounded selected splits");
      auto r = x.process(
          in, std::span(d.prepared).subspan(result.written, capacity), flags);
      require(r.input_consumed <= in.size() && r.output_produced <= capacity,
              "call counts");
      require(r.status != Status::progress || r.input_consumed ||
                  r.output_produced,
              "nonzero progress");
      result.consumed = add({result.consumed, r.input_consumed});
      result.written = add({result.written, r.output_produced});
      require(std::ranges::all_of(std::span(d.prepared).subspan(result.written),
                                  [](auto b) { return b == guard; }),
              "entire sentinel suffix");
      return r;
    };
    core::ProcessResult r;
    if (staged) {
      r = call({}, 0, 0);
      require(r.status == Status::need_output, "zero header output");
      r = call({}, 1, 0);
      require(r.output_produced == 1, "one header byte");
      while (result.written < 112)
        r = call({}, std::min<std::size_t>(4096, 112 - result.written), 0);
      for (unsigned j = 0; j < target; ++j) {
        r = call(std::span(d.raw).subspan(mul(j, d.f), d.f), 0, 0);
        require(r.status == Status::need_output && r.input_consumed == d.f,
                "zero frame output");
        r = call({}, 1, 0);
        require(r.output_produced == 1, "one frame byte");
        while (result.written < boundary(d, j + 1))
          r = call(
              {},
              std::min<std::size_t>(4096, boundary(d, j + 1) - result.written),
              0);
      }
      auto before = result.written;
      r = call(std::span(d.raw).subspan(mul(target, d.f), c.raw),
               d.wire - result.written, target == 2 ? end : 0);
      if (mode)
        require(!r.output_produced && result.written == before,
                "staged failed call zero output");
      else
        while (r.status != Status::end_of_stream) {
          require(r.status != Status::error, "positive staged state");
          r = call(std::span(d.raw).subspan(result.consumed),
                   d.wire - result.written, end);
        }
    } else
      r = call(d.raw, d.wire, end);
    auto ledger = x.ledger();
    result.live = ledger.live;
    result.peak = ledger.peak;
    result.blocks = ledger.blocks;
    if (mode) {
      require(r.status == Status::error &&
                  r.error.code == Code::limit_exceeded &&
                  r.error.byte_position == mul(target, d.f) &&
                  r.error.bit_position == 0,
              "late error exact code/position");
      require(result.consumed == add({mul(target, d.f), c.raw}) &&
                  result.written == boundary(d, target),
              "valid prefix/accepted input");
      require(o.preserved() && o.snapshot_count == (target ? 5u : 0u),
              "adapter complete old block snapshots");
      require(ledger.live ==
                  add({d.f, mul(add({65536, d.f}), sizeof(std::uint32_t)),
                       target ? generation(d.plans[target - 1]) : 0}),
              "old generation retained ledger");
      result.error = r.error.code;
      result.position = r.error.byte_position;
      result.position_available = true;
      for (auto flags : {end, 0xffffffffu}) {
        auto again = call({}, d.wire - result.written, flags);
        require(again.status == Status::error && !again.input_consumed &&
                    !again.output_produced &&
                    again.error.code == r.error.code &&
                    again.error.byte_position == r.error.byte_position &&
                    again.error.bit_position == r.error.bit_position,
                "sticky exact error");
      }
    } else {
      require(r.status == Status::end_of_stream &&
                  result.consumed == d.raw.size() && result.written == d.wire,
              "positive complete stream");
      auto again = call({}, 0, 0xffffffffu);
      require(again.status == Status::end_of_stream && !again.input_consumed &&
                  !again.output_produced,
              "sticky ended");
    }
    require(std::equal(d.reference.begin(),
                       d.reference.begin() + result.written,
                       d.prepared.begin()),
            "exact ordinary valid prefix");
    if (isolated)
      require(o.live == ledger.live && o.peak == ledger.peak &&
                  o.calls == ledger.calls,
              "independent actual allocator ledger");
    trace(c, isolated, mode);
  }
  require(o.zero(), "adapter real releases");
  row(d, o, c, result, "adapter", target, staged, isolated);
}
void prerequisite(const Driver &d) {
  std::cout
      << "{\"prerequisite\":true,\"passed\":true,\"untimed\":true,\"marker\":"
      << d.marker << ",\"raw_bytes\":" << d.raw.size()
      << ",\"wire_bytes\":" << d.wire << ",\"common\":" << d.retained()
      << ",\"extra_test\":" << d.extra_test
      << ",\"block_peak\":" << d.block_plan
      << ",\"final_live\":" << d.final_plan
      << ",\"operation_logical\":" << d.old_stats.logical
      << ",\"adapter_logical\":" << d.exact_budget << ",\"raw_sha256\":";
  print_digest(d.raw_digest);
  std::cout << ",\"wire_sha256\":";
  print_digest(d.wire_digest);
  std::cout << ",\"plans\":[";
  for (unsigned j = 0; j < 3; ++j) {
    if (j)
      std::cout << ",";
    auto &p = d.plans[j];
    std::cout << "{\"tokens\":" << p.tokens << ",\"events\":" << p.events
              << ",\"payload\":" << p.payload << ",\"literals\":" << p.literals
              << ",\"matches\":" << p.matches
              << ",\"match_bytes\":" << p.match_bytes
              << ",\"far_position\":" << d.far_positions[j]
              << ",\"far_distance\":" << d.far_distances[j]
              << ",\"far_length\":" << d.far_lengths[j]
              << ",\"planner_logical\":" << p.planner_logical << "}";
  }
  std::cout << "]}\n" << std::flush;
}
} // namespace
int main(int argc, char **argv) {
  try {
    require(argc == 2, "link role");
    std::string_view role(argv[1]);
    require(role == "ordinary" || role == "isolated", "role");
    bool isolated = role == "isolated";
    unsigned checks{}, faults{};
    for (unsigned marker : {0u, 5u, 258u}) {
      Driver d;
      d.extra_base = add({sizeof(Observer), sizeof(Direct),
                          test::max8m::working_bytes(), 65536});
      d.prepare(8 * mib, marker, 2);
      d.run(0);
      prerequisite(d);
      if (marker == 0) {
        for (unsigned mode : {0u, 1u, 6u}) {
          if (!isolated && mode)
            continue;
          for (unsigned target = 0; target < 2; ++target) {
            owner_case(d, isolated, mode, target);
            ++checks;
            if (mode)
              ++faults;
          }
          for (unsigned target = 0; target < 3; ++target)
            for (bool staged : {false, true}) {
              adapter_case(d, isolated, mode, target, staged);
              ++checks;
              if (mode)
                ++faults;
            }
        }
      } else {
        adapter_case(d, isolated, 0, 1, false);
        ++checks;
        if (marker == 258) {
          adapter_case(d, isolated, 0, 1, true);
          ++checks;
          if (isolated)
            for (bool staged : {false, true}) {
              adapter_case(d, isolated, 6, 1, staged);
              ++checks;
              ++faults;
            }
        }
      }
    }
    require(checks == (isolated ? 29u : 11u) && faults == (isolated ? 18u : 0u),
            "complete planned matrix");
    std::cout << "{\"summary\":true,\"passed\":true,\"checks\":" << checks
              << ",\"faults\":" << faults << ",\"untimed\":true}\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}
