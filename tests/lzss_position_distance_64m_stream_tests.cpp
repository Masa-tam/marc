#include "frame/lzss_position_distance_64m_stream_decoder.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using namespace marc;
using namespace frame::internal;
using S = core::StreamStatus;
using Code = core::ErrorCode;
constexpr auto End = core::flag_value(core::ProcessFlags::end_input);
constexpr auto Flush = core::flag_value(core::ProcessFlags::flush);
constexpr std::byte guard{0xa5};
void check(bool b,
           const std::source_location at = std::source_location::current()) {
  if (!b)
    throw std::runtime_error("check failed at line " +
                             std::to_string(at.line()));
}
std::uint32_t read32(std::istream &s) {
  std::uint32_t v = 0;
  for (unsigned i = 0; i < 4; ++i) {
    const auto c = s.get();
    check(c != EOF);
    v |= static_cast<std::uint32_t>(c) << (8 * i);
  }
  return v;
}
struct Case {
  std::string name;
  bool accepted{};
  std::uint32_t prefix{}, raw{}, tokens{}, serial{};
  std::vector<std::byte> wire;
};
struct Buffers {
  std::vector<std::byte> serial, raw, scratch_raw;
  std::vector<std::byte> tokens, scratch_tokens;
  explicit Buffers(const Case &c)
      : serial(c.serial + 3), raw(c.raw + 7), scratch_raw(c.raw + 11),
        tokens(std::min(std::size_t{3} * c.raw, std::size_t{9} * c.tokens) + 3),
        scratch_tokens(tokens.size() + 2) {}
};
struct Result {
  std::vector<std::byte> raw;
  core::ProcessResult last{};
  std::size_t consumed{}, calls{};
};
core::DecoderLimits limits(const Case &c) {
  core::DecoderLimits l{};
  l.max_frame_size = l.max_lz_distance = 67108864;
  l.max_block_size = std::max(c.raw, 1u);
  l.max_internal_buffered_bytes = std::uint64_t{512} << 20;
  return l;
}
Result decode(const Case &c, std::span<const std::byte> wire,
              std::size_t input_chunk, std::size_t output_chunk,
              std::size_t split, bool delayed = false, bool random = false,
              bool starvation = false) {
  Buffers b(c);
  auto l = limits(c);
  const auto q = query_lzss_position_distance_64m_stream_workspace(
      l, b.serial.size(), b.tokens.size(), b.scratch_tokens.size(),
      b.raw.size(), b.scratch_raw.size(), 123);
  check(q.error == Code::none);
  check(q.aggregate_bytes ==
        b.serial.size() + (b.tokens.size() + b.scratch_tokens.size()) +
            b.raw.size() + b.scratch_raw.size() + q.owner_bytes +
            q.control_bytes + q.helper_bytes + 123);
  l.max_internal_buffered_bytes = q.aggregate_bytes;
  LzssPositionDistance64mStreamDecoder d(
      l, b.serial, b.tokens, b.scratch_tokens, b.raw, b.scratch_raw, 123);
  const auto initial = d.process({}, {}, 0);
  check(initial.status == S::need_input && !initial.input_consumed &&
        !initial.output_produced);
  const auto flushed = d.process({}, {}, Flush);
  check(flushed.status == S::need_input && !flushed.input_consumed &&
        !flushed.output_produced);
  Result r;
  std::vector<std::byte> out(std::max(output_chunk, std::size_t{1}), guard);
  std::uint32_t rng = 1478;
  const auto call_limit = 4 * (wire.size() + std::size_t{2} * c.raw + 1);
  for (std::size_t calls = 0; calls < call_limit; ++calls) {
    const auto available = wire.size() - r.consumed;
    auto n = std::min(available, input_chunk);
    if (r.consumed < split)
      n = std::min(n, split - r.consumed);
    auto cap = output_chunk;
    if (random) {
      rng = rng * 1664525u + 1013904223u;
      n = std::min(n, static_cast<std::size_t>(1 + rng % 113));
      rng = rng * 1664525u + 1013904223u;
      cap = std::min(cap, static_cast<std::size_t>(1 + rng % 79));
    }
    if (starvation && calls % 13 == 0)
      cap = 0;
    const auto flags =
        (!delayed && n == available ? End : 0u) | (calls % 7 == 0 ? Flush : 0u);
    std::fill(out.begin(), out.end(), guard);
    auto result = d.process(wire.subspan(r.consumed, n),
                            std::span(out).first(cap), flags);
    check(result.input_consumed <= n && result.output_produced <= cap);
    check(result.status != S::progress || result.input_consumed ||
          result.output_produced);
    for (std::size_t i = result.output_produced; i < out.size(); ++i)
      check(out[i] == guard);
    r.consumed += result.input_consumed;
    r.raw.insert(r.raw.end(), out.begin(),
                 out.begin() + result.output_produced);
    ++r.calls;
    r.last = result;
    if (result.status == S::end_of_stream || result.status == S::error)
      break;
    if (delayed && r.consumed == wire.size() && !result.output_produced &&
        result.status == S::need_input) {
      std::fill(out.begin(), out.end(), guard);
      result = d.process({}, out, End);
      r.raw.insert(r.raw.end(), out.begin(),
                   out.begin() + result.output_produced);
      ++r.calls;
      r.last = result;
      if (result.status == S::error || result.status == S::end_of_stream)
        break;
    }
  }
  check(r.last.status == S::end_of_stream || r.last.status == S::error);
  const auto repeat =
      d.process(wire, out, core::flag_value(core::ProcessFlags::reset_block));
  check(repeat.status == r.last.status && !repeat.input_consumed &&
        !repeat.output_produced);
  if (r.last.status == S::error)
    check(repeat.error.code == r.last.error.code &&
          repeat.error.byte_position == r.last.error.byte_position);
  return r;
}
void verify(const Case &c, const Result &r) {
  check(r.last.status == (c.accepted ? S::end_of_stream : S::error));
  const auto expected =
      c.accepted
          ? (c.name == "empty"
                 ? 0u
                 : (c.name == "two-small" || c.name == "two-full" ? 2 * c.raw
                                                                  : c.raw))
          : c.prefix;
  check(r.raw.size() == expected);
  if (c.accepted)
    check(r.consumed == c.wire.size());
  for (std::size_t i = 0; i < r.raw.size(); ++i) {
    const bool two = c.name == "two-small" || c.name == "two-full";
    const auto v = two && i >= c.raw ? 66
                   : (c.name == "overlap" || c.name == "two-small" ||
                      c.name == "late-canonical") &&
                           i >= 8 && i % 2 == 0
                       ? 66
                       : 65;
    check(r.raw[i] == static_cast<std::byte>(v));
  }
}
void full_capacity(const Case &c) {
  constexpr std::size_t F = 67108864;
  for (bool corrupt : {false, true}) {
    std::vector<std::byte> serial((std::size_t{128} << 20) + 80, guard),
        tokens(3 * F, guard), scratch_tokens(3 * F, guard), raw(F, guard),
        scratch_raw(F, guard);
    auto wire = c.wire;
    if (corrupt)
      wire.back() ^= std::byte{1};
    std::array<std::byte, 65536> output{};
    const auto retained = c.wire.capacity() + wire.capacity() + output.size();
    auto l = limits(c);
    l.max_internal_buffered_bytes = std::uint64_t{1024} << 20;
    const auto q = query_lzss_position_distance_64m_stream_workspace(
        l, serial.size(), tokens.size(), scratch_tokens.size(), raw.size(),
        scratch_raw.size(), retained);
    const auto backing = serial.size() + tokens.size() + scratch_tokens.size() +
                         raw.size() + scratch_raw.size();
    check(q.error == Code::none && backing == 671088720);
    auto old_policy = l;
    old_policy.max_internal_buffered_bytes = std::uint64_t{512} << 20;
    check(query_lzss_position_distance_64m_stream_workspace(
              old_policy, serial.size(), tokens.size(), scratch_tokens.size(),
              raw.size(), scratch_raw.size(), retained)
              .error == Code::limit_exceeded);

    check(q.aggregate_bytes == backing + q.owner_bytes + q.control_bytes +
                                   q.helper_bytes + retained);
    l.max_internal_buffered_bytes = q.aggregate_bytes;
    LzssPositionDistance64mStreamDecoder d(l, serial, tokens, scratch_tokens,
                                           raw, scratch_raw, retained);
    std::size_t consumed{}, produced{};
    core::ProcessResult result{};
    for (std::size_t calls = 0; calls < 10000; ++calls) {
      output.fill(guard);
      result = d.process(std::span(wire).subspan(consumed), output, End);
      check(result.input_consumed <= wire.size() - consumed &&
            result.output_produced <= output.size());
      consumed += result.input_consumed;
      for (std::size_t i = 0; i < result.output_produced; ++i)
        check(output[i] == (produced + i < F ? std::byte{65} : std::byte{66}));
      for (std::size_t i = result.output_produced; i < output.size(); ++i)
        check(output[i] == guard);
      produced += result.output_produced;
      if (result.status == S::error || result.status == S::end_of_stream)
        break;
    }
    check(result.status == (corrupt ? S::error : S::end_of_stream));
    check(produced == (corrupt ? F : 2 * F) && consumed == wire.size());
    check(std::ranges::all_of(raw, [&](auto b) {
      return b == (corrupt ? std::byte{65} : std::byte{66});
    }));
    --l.max_internal_buffered_bytes;
    LzssPositionDistance64mStreamDecoder below(
        l, serial, tokens, scratch_tokens, raw, scratch_raw, retained);
    output.fill(guard);
    const auto refused = below.process(wire, output, End);
    check(refused.status == S::error &&
          refused.error.code == Code::limit_exceeded &&
          !refused.input_consumed && !refused.output_produced);
    for (auto b : output)
      check(b == guard);
    std::cout << "stream full-capacity corrupt=" << corrupt
              << " backing=" << backing << " controls="
              << q.owner_bytes + q.control_bytes + q.helper_bytes
              << " retained=" << retained << " total=" << q.aggregate_bytes
              << '\n';
  }
}
void run(const Case &c) {
  if (c.name == "two-full")
    full_capacity(c);
  std::size_t schedules = 0;
  auto test = [&](std::size_t ic, std::size_t oc, std::size_t split,
                  bool delayed = false, bool random = false,
                  bool starve = false) {
    const auto r = decode(c, c.wire, ic, oc, split, delayed, random, starve);
    verify(c, r);
    ++schedules;
  };
  test(65536, 65536, c.wire.size());
  test(65536, 65536, c.wire.size(), true);
  test(113, 79, c.wire.size(), false, true, true);
  if (c.raw <= 512) {
    test(1, 1, c.wire.size());
    for (std::size_t split = 0; split <= c.wire.size(); ++split)
      test(c.wire.size(), 1, split);
  } else {
    test(1, 65536, 112);
    test(65536, 1, 112);
  }
  if (c.name == "literal" || c.name == "empty") {
    for (std::size_t n = 0; n < c.wire.size(); ++n) {
      auto r = decode(c, std::span(c.wire).first(n), 17, 1, n);
      check(r.last.status == S::error && r.raw.empty());
      ++schedules;
    }
    auto extra = c.wire;
    extra.push_back(std::byte{});
    const auto r = decode(c, extra, extra.size(), 65536, extra.size());
    check(r.last.status == S::error);
    ++schedules;
  }
  Buffers b(c);
  auto l = limits(c);
  const auto q = query_lzss_position_distance_64m_stream_workspace(
      l, b.serial.size(), b.tokens.size(), b.scratch_tokens.size(),
      b.raw.size(), b.scratch_raw.size(), 123);
  --(l.max_internal_buffered_bytes = q.aggregate_bytes);
  LzssPositionDistance64mStreamDecoder refused(
      l, b.serial, b.tokens, b.scratch_tokens, b.raw, b.scratch_raw, 123);
  std::vector<std::byte> out(7, guard);
  const auto f = refused.process(c.wire, out, End);
  check(f.status == S::error && !f.input_consumed && !f.output_produced);
  for (auto v : out)
    check(v == guard);
  l = limits(c);
  LzssPositionDistance64mStreamDecoder aliases(l, b.serial, b.tokens, b.tokens,
                                               b.raw, b.scratch_raw);
  const auto alias = aliases.process(c.wire, out, End);
  check(alias.status == S::error &&
        alias.error.code == Code::invalid_argument && !alias.output_produced);
  LzssPositionDistance64mStreamDecoder flags(
      l, b.serial, b.tokens, b.scratch_tokens, b.raw, b.scratch_raw);
  const auto unsupported = flags.process(
      c.wire, out, core::flag_value(core::ProcessFlags::reset_block));
  check(unsupported.status == S::error &&
        unsupported.error.code == Code::unsupported &&
        !unsupported.input_consumed && !unsupported.output_produced);
  const auto overflow = query_lzss_position_distance_64m_stream_workspace(
      l, b.serial.size(), b.tokens.size(), b.scratch_tokens.size(),
      b.raw.size(), b.scratch_raw.size(),
      std::numeric_limits<std::size_t>::max());
  check(overflow.error == Code::limit_exceeded);
  std::cout << c.name << " schedules=" << schedules
            << " ledger=" << q.aggregate_bytes << " frame=" << c.raw << '\n';
}
} // namespace
int main(int argc, char **argv) {
  try {
    check(argc == 2);
    std::ifstream source(argv[1], std::ios::binary);
    check(source.good());
    char magic[8]{};
    source.read(magic, 8);
    check(std::memcmp(magic, "M64S0001", 8) == 0);
    check(read32(source) == 16);
    for (unsigned i = 0; i < 16; ++i) {
      const auto n = read32(source);
      check(n < 32);
      Case c;
      c.name.resize(n);
      source.read(c.name.data(), n);
      c.accepted = read32(source) != 0;
      c.prefix = read32(source);
      c.raw = read32(source);
      c.tokens = read32(source);
      c.serial = read32(source);
      const auto size = read32(source);
      check(size < 1048576 && c.raw <= 67108864 && c.tokens <= 260114);
      c.wire.resize(size);
      source.read(reinterpret_cast<char *>(c.wire.data()), size);
      check(source.good());
      run(c);
    }
    check(source.get() == EOF);
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
