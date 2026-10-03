#include "core/checked_math.hpp"
#include "frame/lzss_position_distance_8m_owned_stream_encoder.hpp"
#include "frame/lzss_position_distance_8m_stream_decoder.hpp"
#include "frame/lzss_position_distance_8m_stream_encoder.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>
namespace {
using namespace marc;
using namespace frame::internal;
using Token = dictionary::internal::LzssTypedToken;
using Op = context::internal::ModeledOperation;
using S = core::StreamStatus;
using C = core::ErrorCode;
constexpr auto end = core::flag_value(core::ProcessFlags::end_input);
constexpr auto flush = core::flag_value(core::ProcessFlags::flush);
constexpr auto guard = std::byte{0xa5};
constexpr std::size_t policy = 8u << 20;
void check_at(bool b, int line) {
  if (!b) {
    std::fprintf(stderr, "owned stream oracle line %d\n", line);
    std::abort();
  }
}
#define VERIFY(x) check_at((x), __LINE__)
std::size_t sum(std::initializer_list<std::size_t> values) {
  std::size_t n{};
  for (auto v : values)
    VERIFY(core::checked_add(n, v, n));
  return n;
}
template <class T> std::size_t bytes(const std::vector<T> &v) {
  std::size_t n{};
  VERIFY(core::checked_multiply(v.capacity(), sizeof(T), n));
  return n;
}
struct Result {
  std::array<std::byte, 32768> wire{};
  core::ProcessResult last{};
  std::size_t written{}, consumed{}, calls{};
  bool fired{};
};
struct Harness {
  std::array<std::uint8_t, 8> controls{};
  std::array<std::byte, 256> raw{};
  std::array<Result, 3> results{};
  core::DecoderLimits limits{};
  TypedContextStreamHeader header{};
  std::size_t raw_size{}, source_bytes{}, fault{};
};
struct Driver {
  std::array<std::byte, 67> output{};
  core::ProcessResult result{};
  std::span<const std::byte> input{};
  std::array<std::size_t, 20> scalars{};
};
// Separate named outer controls cover phase queries, constructor copies and
// consumer arrays. Complete object storage is retained in every phase.
constexpr std::size_t outer_controls =
    sizeof(Driver) + sizeof(TypedContextStreamHeader) +
    sizeof(core::DecoderLimits) +
    sizeof(LzssPositionDistance8mStreamEncodeCapacities) +
    sizeof(LzssPositionDistance8mStreamEncodeRequirements) +
    sizeof(LzssPositionDistance8mStreamRequirements) + 64 * sizeof(std::size_t);
static_assert(sizeof(Harness) + outer_controls + 512 * 1024 < policy);
std::size_t retained(const Harness &h) {
  return sum({sizeof(h), outer_controls, h.source_bytes});
}
class Allocator final : public LzssPositionDistance8mStreamAllocator {
public:
  struct Receipt {
    const void *data{};
    std::size_t bytes{};
  };
  std::array<Receipt, 16> receipts{};
  LzssPositionDistance8mExactStreamAllocator exact;
  std::size_t calls{}, fault{}, live{}, peak{};
  bool fired{};
  auto controls() const noexcept
      -> LzssPositionDistance8mAllocatorControls override {
    return {this, sizeof(*this), 256};
  }
  bool failing() noexcept {
    if (++calls == fault) {
      fired = true;
      return true;
    }
    return false;
  }
  template <class T>
  auto record(LzssPositionDistance8mOwnedBlock<T> b) noexcept {
    if (b.data) {
      for (auto &r : receipts)
        if (!r.data) {
          std::size_t n{};
          VERIFY(core::checked_multiply(b.capacity, sizeof(T), n));
          r = {b.data, n};
          live = sum({live, n});
          peak = std::max(peak, live);
          return b;
        }
      std::abort();
    }
    return b;
  }
  auto tokens(std::size_t n) noexcept
      -> LzssPositionDistance8mOwnedTokens override {
    return failing() ? LzssPositionDistance8mOwnedTokens{}
                     : record(exact.tokens(n));
  }
  auto bytes(std::size_t n) noexcept
      -> LzssPositionDistance8mOwnedBytes override {
    return failing() ? LzssPositionDistance8mOwnedBytes{}
                     : record(exact.bytes(n));
  }
  auto indices(std::size_t n) noexcept
      -> LzssPositionDistance8mOwnedIndex override {
    return failing() ? LzssPositionDistance8mOwnedIndex{}
                     : record(exact.indices(n));
  }
  template <class T>
  void destroy(LzssPositionDistance8mOwnedBlock<T> &b) noexcept {
    const auto p = b.data;
    exact.release(b);
    if (p) {
      for (auto &r : receipts)
        if (r.data == p) {
          live -= r.bytes;
          r = {};
          return;
        }
      std::abort();
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
struct Reference {
  std::vector<std::byte> raw, pub, frame, payload;
  std::vector<Token> tokens, scratch;
  std::vector<Op> ops, os;
  std::vector<std::uint32_t> index;
  explicit Reference(std::size_t f)
      : raw(f), pub(18 * f + 85), frame(pub.size()), payload(pub.size()),
        tokens(f), scratch(f), ops(2 * f), os(2 * f), index(65536 + f) {}
  auto caps() const {
    return LzssPositionDistance8mStreamEncodeCapacities{
        raw.size(), pub.size(), tokens.size(), scratch.size(), index.size(),
        ops.size(), os.size(),  frame.size(),  payload.size(), 256,
        64};
  }
  auto workspace() {
    return LzssPositionDistance8mFrameEncodeWorkspace{
        tokens, scratch, index, ops, os, frame, payload};
  }
  std::size_t spares() const {
    return sum({bytes(raw) - raw.size(), bytes(pub) - pub.size(),
                bytes(frame) - frame.size(), bytes(payload) - payload.size(),
                bytes(tokens) - tokens.size() * sizeof(Token),
                bytes(scratch) - scratch.size() * sizeof(Token),
                bytes(ops) - ops.size() * sizeof(Op),
                bytes(os) - os.size() * sizeof(Op),
                bytes(index) - index.size() * 4});
  }
};
void sticky(core::Transform &e, const core::ProcessResult &r) {
  std::array<std::byte, 4> out{guard, guard, guard, guard};
  auto again = e.process(out, out, 0xffffffffu);
  VERIFY(again.status == r.status && again.error.code == r.error.code &&
         again.error.byte_position == r.error.byte_position &&
         again.error.bit_position == r.error.bit_position);
  VERIFY(!again.input_consumed && !again.output_produced);
  VERIFY(std::ranges::all_of(out, [](auto x) { return x == guard; }));
}
void run(core::Transform &e, Harness &h, Result &r, bool split) {
  Driver d{};
  const auto chunk =
      split ? 1 + h.controls[4] % 33 : std::max<std::size_t>(1, h.raw_size);
  const auto capacity = split ? 1 + (h.controls[3] & 31) : 64;
  const bool late = split && (h.controls[1] & 4);
  std::size_t stop = std::min<std::size_t>(chunk, h.raw_size);
  do {
    d.output.fill(guard);
    const auto in = std::span(h.raw)
                        .first(h.raw_size)
                        .subspan(r.consumed, stop - r.consumed);
    const auto cap = split && r.calls % 7 == 0 ? 0 : capacity;
    const auto flags =
        ((h.controls[1] & 8) ? flush : 0) |
        ((!late && stop == h.raw_size) || r.consumed == h.raw_size ? end : 0) |
        ((h.controls[1] & 64) && !r.calls
             ? core::flag_value(core::ProcessFlags::reset_block) |
                   (std::uint32_t(h.controls[7]) << 8)
             : 0);
    r.last = e.process(in, std::span(d.output).first(cap), flags);
    VERIFY(r.last.input_consumed <= in.size() && r.last.output_produced <= cap);
    VERIFY(r.last.status != S::progress || r.last.input_consumed ||
           r.last.output_produced);
    VERIFY(
        std::ranges::all_of(std::span(d.output).subspan(r.last.output_produced),
                            [](auto x) { return x == guard; }));
    VERIFY(sum({r.written, r.last.output_produced}) <= r.wire.size());
    std::copy_n(d.output.begin(), r.last.output_produced,
                r.wire.begin() + r.written);
    r.written += r.last.output_produced;
    r.consumed += r.last.input_consumed;
    if (r.consumed == stop && r.consumed < h.raw_size)
      stop = std::min(h.raw_size, sum({r.consumed, chunk}));
    VERIFY(++r.calls < 65536);
  } while (r.last.status != S::end_of_stream && r.last.status != S::error);
  sticky(e, r.last);
}
void reference(Harness &h) {
  const auto f = h.header.frame_size, storage = 18 * f + 85;
  const LzssPositionDistance8mStreamEncodeCapacities caps{
      f, storage, f, f, 65536 + f, 2 * f, 2 * f, storage, storage, 256, 64};
  const auto before = query_lzss_position_distance_8m_stream_encode_workspace(
      h.limits, caps, sum({retained(h), sizeof(Reference)}));
  VERIFY(before.error == C::none && before.aggregate_bytes <= policy);
  Reference b(f);
  const auto extra = sum({retained(h), sizeof(b), b.spares()});
  VERIFY(query_lzss_position_distance_8m_stream_encode_workspace(
             h.limits, b.caps(), extra)
             .error == C::none);
  LzssPositionDistance8mStreamEncoder e(h.header, h.limits, b.raw, b.pub,
                                        b.workspace(), extra);
  run(e, h, h.results[0], false);
}
void owned(Harness &h, unsigned slot, bool split) {
  Allocator a;
  a.fault = h.fault;
  {
    LzssPositionDistance8mOwnedStreamEncoder e(h.header, h.limits, a,
                                               retained(h));
    run(e, h, h.results[slot], split);
    h.results[slot].fired = a.fired;
    VERIFY(a.peak <= policy);
  }
  VERIFY(a.live == 0 &&
         std::ranges::all_of(a.receipts, [](auto &r) { return !r.data; }));
}
std::size_t le32(const Result &r, std::size_t off) {
  VERIFY(off + 4 <= r.written);
  std::size_t n{};
  for (unsigned i = 0; i < 4; ++i)
    n |= std::to_integer<std::size_t>(r.wire[off + i]) << (8 * i);
  return n;
}
std::size_t committed(const Result &r) {
  if (!r.written)
    return 0;
  VERIFY(r.written >= 112);
  std::size_t at = 112, raw{};
  while (at < r.written) {
    VERIFY(at + 80 <= r.written);
    const auto f = le32(r, at + 16), p = le32(r, at + 32);
    VERIFY(f && f <= 64 && p <= 18 * 64 + 85);
    at = sum({at, 80, p});
    VERIFY(at <= r.written);
    raw = sum({raw, f});
  }
  VERIFY(at == r.written);
  return raw;
}
void consumer(Harness &h, const Result &r) {
  struct Buffers {
    std::array<std::byte, 1240> serial{};
    std::array<std::byte, 64> raw{}, scratch{};
    std::array<Token, 64> tokens{}, ts{};
  };
  const auto extra = sum({retained(h), sizeof(Buffers)});
  // Array capacity is local to the decoder; remove it from retained exactly
  // once, keeping the Buffers object's complete control/array reservation.
  constexpr auto arrays = 1240 + 128 + 128 * sizeof(Token);
  VERIFY(extra >= arrays);
  VERIFY(query_lzss_position_distance_8m_stream_workspace(
             h.limits, 1240, 64, 64, 64, 64, extra - arrays)
             .error == C::none);
  Buffers b;
  LzssPositionDistance8mStreamDecoder d(h.limits, b.serial, b.tokens, b.ts,
                                        b.raw, b.scratch, extra - arrays);
  Driver call{};
  std::size_t offset{}, made{}, steps{};
  do {
    call.output.fill(guard);
    const auto input = std::span(r.wire).first(r.written).subspan(
        offset, std::min<std::size_t>(7, r.written - offset));
    const auto cap = steps % 7 == 0 ? 0u : 3u;
    call.result = d.process(input, std::span(call.output).first(cap),
                            offset + input.size() == r.written ? end : 0);
    VERIFY(call.result.input_consumed <= input.size() &&
           call.result.output_produced <= cap);
    VERIFY(call.result.status != S::progress || call.result.input_consumed ||
           call.result.output_produced);
    VERIFY(std::ranges::all_of(
        std::span(call.output).subspan(call.result.output_produced),
        [](auto x) { return x == guard; }));
    VERIFY(made + call.result.output_produced <= h.raw_size);
    for (std::size_t i = 0; i < call.result.output_produced; ++i)
      VERIFY(call.output[i] == h.raw[made + i]);
    offset += call.result.input_consumed;
    made += call.result.output_produced;
    VERIFY(++steps < 65536);
  } while (call.result.status != S::end_of_stream &&
           call.result.status != S::error);
  const auto expected = committed(r);
  VERIFY(made == expected);
  VERIFY(call.result.status ==
         (r.written >= 112 && expected == h.header.original_size
              ? S::end_of_stream
              : S::error));
  sticky(d, call.result);
}
void same(const Result &a, const Result &b) {
  VERIFY(a.last.status == b.last.status &&
         a.last.error.code == b.last.error.code &&
         a.last.error.byte_position == b.last.error.byte_position &&
         a.last.error.bit_position == b.last.error.bit_position);
  VERIFY(a.written == b.written && a.consumed == b.consumed &&
         a.fired == b.fired);
  VERIFY(
      std::equal(a.wire.begin(), a.wire.begin() + a.written, b.wire.begin()));
}
} // namespace
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data,
                                      std::size_t size) {
  if (size < 8 || size > 264)
    return 0;
  Harness h;
  std::copy_n(data, 8, h.controls.begin());
  h.raw_size = size - 8;
  h.source_bytes = size;
  for (std::size_t i = 0; i < h.raw_size; ++i)
    h.raw[i] = std::byte(data[i + 8]);
  h.limits.max_block_size = 64;
  h.limits.max_internal_buffered_bytes = policy;
  h.header.frame_size = 1 + data[0] % 64;
  h.header.original_size = h.raw_size;
  if (data[1] & 1)
    h.header.original_size += 1 + data[6] % 4;
  if (data[1] & 2)
    h.header.original_size -=
        std::min<std::uint64_t>(h.header.original_size, 1 + data[6] % 4);
  h.header.dictionary = {8388608, 3, (data[1] & 16) ? 3u : 258u, 0};
  h.header.dictionary_variant = (data[1] & 32) ? 0 : 11;
  h.header.context_algorithm = 1;
  h.header.context_variant = 12;
  h.header.context_count = 47;
  h.header.range_model_total = 32768;
  if (data[1] & 128)
    h.limits.max_compressed_payload_size = 1 + data[5];
  h.fault = (data[3] & 128) ? 1 + data[2] % 17 : 0;
  reference(h);
  owned(h, 1, false);
  owned(h, 2, true);
  same(h.results[1], h.results[2]);
  const auto &base = h.results[0];
  const auto &a = h.results[1];
  if (!a.fired)
    same(base, a);
  else {
    VERIFY(a.last.status == S::error && a.last.error.code == C::out_of_memory);
    VERIFY(a.written <= base.written &&
           std::equal(a.wire.begin(), a.wire.begin() + a.written,
                      base.wire.begin()));
    VERIFY(committed(a) <= h.raw_size);
  }
  consumer(h, h.results[1]);
  consumer(h, h.results[2]);
  return 0;
}
