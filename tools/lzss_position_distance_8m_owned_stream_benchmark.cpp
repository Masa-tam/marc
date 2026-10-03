// Private diagnostic only. Independently authored from repository contracts.
// No production target, codec selection, stream format or default is changed.
#include "core/checked_math.hpp"
#include "frame/lzss_position_distance_8m_owned_stream_encoder.hpp"
#include "frame/lzss_position_distance_8m_stream_decoder.hpp"
#include "frame/lzss_position_distance_8m_stream_encoder.hpp"
#include "frame/lzss_position_distance_8m_token_frame_encoder.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using namespace marc;
using namespace frame::internal;
using Token = dictionary::internal::LzssTypedToken;
using Op = context::internal::ModeledOperation;
using Old = LzssPositionDistance8mStreamEncoder;
using New = LzssPositionDistance8mOwnedStreamEncoder;
using Decode = LzssPositionDistance8mStreamDecoder;
using Clock = std::chrono::steady_clock;
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
double seconds(Clock::time_point a, Clock::time_point b) {
  return std::chrono::duration<double>(b - a).count();
}
struct Stats {
  double process{}, lifecycle{};
  std::size_t logical{}, block_peak{}, final_live{}, allocations{}, calls{},
      written{};
};
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
  std::size_t live{}, peak{}, calls{};
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
    return keep(exact.tokens(n));
  }
  auto bytes(std::size_t n) noexcept
      -> LzssPositionDistance8mOwnedBytes override {
    return keep(exact.bytes(n));
  }
  auto indices(std::size_t n) noexcept
      -> LzssPositionDistance8mOwnedIndex override {
    return keep(exact.indices(n));
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
  std::array<Clock::time_point, 8> times;
  std::array<std::span<const std::byte>, 4> inputs;
  std::array<std::span<std::byte>, 4> outputs;
};
struct Driver {
  core::DecoderLimits limits{};
  TypedContextStreamHeader header{};
  std::vector<std::byte> raw, reference, owned, decoded;
  Stats old_stats{}, new_stats{}, decode_stats{};
  std::size_t f{}, t{}, e{}, p{}, frame_count{};
  std::size_t retained() const {
    return add({sizeof(*this), sizeof(Controls), bytes(raw), bytes(reference),
                bytes(owned), bytes(decoded)});
  }
  void prepare(std::size_t n, unsigned pattern, bool double_frame) {
    require(n == mib || n == 8 * mib, "input size");
    require(pattern < 4, "pattern");
    frame_count = double_frame ? 2 : 1;
    f = n;
    limits.max_block_size = 8 * mib;
    limits.max_lz_distance = 8 * mib;
    limits.max_internal_buffered_bytes = ceiling;
    header.frame_size = static_cast<std::uint32_t>(f);
    header.original_size = mul(f, frame_count);
    header.dictionary = {8 * mib, 3, 258, 0};
    header.dictionary_variant = 11;
    header.context_algorithm = 1;
    header.context_variant = 12;
    header.context_count = 47;
    header.range_model_total = 32768;
    admit(add({retained(), mul(f, frame_count)}));
    raw.resize(mul(f, frame_count));
    std::uint32_t random = 1439;
    for (std::size_t i = 0; i < f; ++i) {
      random ^= random << 13;
      random ^= random >> 17;
      random ^= random << 5;
      if (pattern == 0)
        raw[i] = std::byte(i % 256);
      else if (pattern == 1)
        raw[i] = i < mib + 17 ? std::byte(random & 255) : raw[i % (mib + 17)];
      else if (pattern == 2)
        raw[i] = std::byte(random & 255);
      else
        raw[i] = std::byte((i / 65536) % 3 == 0 ? (random & 255) : i % 7);
    }
    if (double_frame)
      std::copy_n(raw.begin(), f, raw.begin() + f);
    {
      Buffers b;
      admit(add({retained(), mul(65536 + f, 4),
                 lzss_position_distance_8m_token_frame_working_bytes(), f}));
      b.index.resize(65536 + f);
      auto count =
          dictionary::internal::query_lzss_position_distance_8m_indexed(
              std::span(raw).first(f), header.dictionary, limits, 0, 0, b.index,
              retained() + bytes(raw) - f);
      require(count.error ==
                  dictionary::internal::LzssPositionDistance8mParseError::
                      output_too_small,
              "count tokens");
      t = count.details.token_count;
      require(t <= f, "token bound");
      admit(add({retained(), b.full(), mul(24, t),
                 lzss_position_distance_8m_token_frame_working_bytes(), f}));
      b.tokens.resize(t);
      b.scratch.resize(t);
      auto w = LzssPositionDistance8mTokenFrameWorkspace{
          b.tokens, b.scratch, b.index, {}, {}};
      auto context = TypedContextFrameValidationContext{header, limits, 0, 0};
      auto plan = query_lzss_position_distance_8m_token_frame_encode(
          std::span(raw).first(f), context, w, 0, retained() + bytes(raw) - f);
      require(plan.error ==
                  LzssPositionDistance8mTokenFrameError::storage_too_small,
              "count payload");
      require(plan.bytes_required >= 80, "prefix");
      p = plan.bytes_required - 80;
      e = plan.counts.declared_event_count;
      require(p <= limits.max_compressed_payload_size && e <= 2 * f,
              "count bounds");
    } // Planning storage really destroyed before any measured phase.
    const auto wire = add({112, mul(frame_count, 80 + p)});
    admit(add({retained(), mul(2, wire), raw.size()}));
    reference.resize(wire);
    owned.resize(wire);
    decoded.resize(raw.size());
    admit(retained());
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
    const auto caps = LzssPositionDistance8mStreamEncodeCapacities{
        f, 80 + p, t, t,          65536 + f,       e,
        e, 80 + p, p, raw.size(), reference.size()};
    auto q = query_lzss_position_distance_8m_stream_encode_workspace(
        limits, caps, retained());
    require(q.error == core::ErrorCode::none, "reference admission");
    admit(q.aggregate_bytes);
    const auto life = Clock::now();
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
      auto external = retained() + b.full() - b.sizes();
      q = query_lzss_position_distance_8m_stream_encode_workspace(
          limits, b.capacities(raw.size(), reference.size()), external);
      require(q.error == core::ErrorCode::none, "reference actual capacities");
      old_stats.logical = q.aggregate_bytes;
      old_stats.block_peak = b.full();
      old_stats.final_live = b.full();
      Old x(header, limits, b.raw, b.publication, b.workspace(), external);
      const auto start = Clock::now();
      old_stats.written = drive(x, raw, reference, old_stats);
      old_stats.process = seconds(start, Clock::now());
    }
    old_stats.lifecycle = seconds(life, Clock::now());
  }
  void new_encode() {
    const auto generation = add({mul(24, t), mul(3, p), 160});
    const auto blocks =
        add({f, mul(65536 + f, 4), mul(frame_count > 1 ? 2 : 1, generation)});
    // Explicit admission of full raw/index and old+new replacement generations.
    new_stats.logical = add({retained(), blocks, New::working_bytes(), 256,
                             raw.size(), owned.size()});
    // Allocator object is already in the conservative Controls reservation;
    // codec contract separately charges it. Preserve that conservative charge.
    new_stats.logical = add({new_stats.logical, sizeof(Allocator)});
    admit(new_stats.logical);
    const auto life = Clock::now();
    {
      Allocator a;
      {
        New x(header, limits, a, retained());
        const auto start = Clock::now();
        new_stats.written = drive(x, raw, owned, new_stats);
        new_stats.process = seconds(start, Clock::now());
        new_stats.block_peak = a.peak;
        new_stats.final_live = a.live;
        new_stats.allocations = a.calls;
        require(a.peak == blocks, "actual owned peak");
        require(a.live == add({f, mul(65536 + f, 4), generation}),
                "actual final owned blocks");
      }
      require(a.live == 0 && std::ranges::all_of(
                                 a.receipts, [](auto &r) { return !r.data; }),
              "destructor receipts");
    }
    new_stats.lifecycle = seconds(life, Clock::now());
  }
  void decode() {
    // Both encoder scopes have ended; all four external vector owners survive.
    auto extra = add({retained(), owned.size(), decoded.size()});
    auto q = query_lzss_position_distance_8m_stream_workspace(limits, 80 + p, t,
                                                              t, f, f, extra);
    require(q.error == core::ErrorCode::none, "decoder admission");
    admit(q.aggregate_bytes);
    const auto life = Clock::now();
    {
      Buffers b;
      b.publication.resize(80 + p);
      b.tokens.resize(t);
      b.scratch.resize(t);
      b.raw.resize(f);
      b.frame.resize(f);
      extra = add({extra, b.full() - b.sizes()});
      q = query_lzss_position_distance_8m_stream_workspace(
          limits, b.publication.size(), b.tokens.size(), b.scratch.size(),
          b.raw.size(), b.frame.size(), extra);
      require(q.error == core::ErrorCode::none, "decoder actual capacities");
      decode_stats.logical = q.aggregate_bytes;
      decode_stats.block_peak = b.full();
      decode_stats.final_live = b.full();
      Decode x(limits, b.publication, b.tokens, b.scratch, b.raw, b.frame,
               extra);
      const auto start = Clock::now();
      decode_stats.written = drive(x, owned, decoded, decode_stats);
      decode_stats.process = seconds(start, Clock::now());
    }
    decode_stats.lifecycle = seconds(life, Clock::now());
    require(raw == decoded, "decoded bytes");
  }
};
void print(const char *name, const Stats &s) {
  std::cout << "\"" << name << "\":{\"process_seconds\":" << s.process
            << ",\"lifecycle_seconds\":" << s.lifecycle
            << ",\"logical_reservation\":" << s.logical
            << ",\"block_peak\":" << s.block_peak
            << ",\"final_live\":" << s.final_live
            << ",\"allocations\":" << s.allocations << ",\"calls\":" << s.calls
            << ",\"written\":" << s.written << "}";
}
} // namespace
int main(int argc, char **argv) {
  try {
    require(argc == 5, "arguments: size pattern double_frame owned_first");
    Driver d;
    d.prepare(std::stoull(argv[1]), static_cast<unsigned>(std::stoul(argv[2])),
              std::stoul(argv[3]) != 0);
    if (std::stoul(argv[4])) {
      d.new_encode();
      d.old_encode();
    } else {
      d.old_encode();
      d.new_encode();
    }
    require(d.reference == d.owned, "wire bytes");
    d.decode();
    std::cout.precision(12);
    std::cout << "{\"raw_bytes\":" << d.raw.size() << ",\"frame_bytes\":" << d.f
              << ",\"frames\":" << d.frame_count
              << ",\"tokens_per_frame\":" << d.t
              << ",\"events_per_frame\":" << d.e
              << ",\"payload_per_frame\":" << d.p
              << ",\"wire_bytes\":" << d.owned.size()
              << ",\"retained_harness\":" << d.retained()
              << ",\"ceiling\":" << ceiling
              << ",\"wire_equal\":true,\"raw_equal\":true,";
    print("reference", d.old_stats);
    std::cout << ",";
    print("owned", d.new_stats);
    std::cout << ",";
    print("decode", d.decode_stats);
    std::cout << "}\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}
