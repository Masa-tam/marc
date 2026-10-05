#include "frame/lzss_position_distance_32m_compact_frame_encoder.hpp"
#include "frame/lzss_position_distance_32m_frame_decoder.hpp"
#include "frame/lzss_position_distance_32m_frame_encoder.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdlib>
#include <cstring>
#include <vector>
namespace {
using namespace marc::frame::internal;
using Token = marc::dictionary::internal::LzssTypedToken;
using Op = marc::context::internal::ModeledOperation;
void check(bool v) {
  if (!v)
    std::abort();
}
void range_records(std::span<const std::byte> bytes, std::size_t retained) {
  using namespace marc;
  using namespace entropy::internal;
  std::array<Token, 128> tokens{};
  std::size_t pos = 0, count = 0;
  std::uint64_t raw = 0;
  std::uint32_t events = 0, decisions = 0;
  bool syntax = true;
  while (pos < bytes.size()) {
    const auto tag = std::to_integer<unsigned>(bytes[pos]);
    const std::size_t size = tag == 0 ? 2 : 9;
    if (tag > 1 || bytes.size() - pos < size) {
      syntax = false;
      break;
    }
    auto &t = tokens[count++];
    if (!tag) {
      t.literal = std::to_integer<unsigned char>(bytes[pos + 1]);
      ++raw;
      events += 2;
      decisions += 2;
    } else {
      t.kind = dictionary::internal::LzssTypedTokenKind::match;
      for (unsigned i = 0; i < 4; ++i) {
        t.distance |= std::to_integer<std::uint32_t>(bytes[pos + 1 + i])
                      << (8 * i);
        t.length |= std::to_integer<std::uint32_t>(bytes[pos + 5 + i])
                    << (8 * i);
      }
      raw += t.length;
      const auto lc = t.length < 5 ? 8u : std::bit_width(t.length - 4) - 1u;
      const auto dc = t.distance ? std::bit_width(t.distance) - 1u : 0u;
      events += 3 + (lc != 0) + (dc != 0);
      decisions += 3 + (lc == 8 ? 1 : lc) + dc;
    }
    pos += size;
  }
  core::DecoderLimits limits{};
  limits.max_frame_size = 33554432;
  limits.max_lz_distance = 33554432;
  limits.max_block_size = 33554432;
  limits.max_frame_size = 33554432;
  limits.max_lz_distance = 33554432;
  dictionary::internal::LzssParameters p{33554432, 3, 258, 0};
  context::internal::LzssFieldContextValidationContext c{
      static_cast<std::uint32_t>(count), events, decisions,
      static_cast<std::uint32_t>(std::min<std::uint64_t>(raw, 33554433)), 0};
  std::array<std::byte, 2400> output{}, scratch{}, typed{}, ts{};
  output.fill(std::byte{0xa5});
  typed = output;
  ContextualDynamicRangeDescriptor descriptor{123, 456, 7},
      previous = descriptor, td = descriptor;
  const auto controls = sizeof(tokens) + sizeof(p) + sizeof(c) +
                        sizeof(limits) + sizeof(output) + sizeof(scratch) +
                        sizeof(typed) + sizeof(ts) + sizeof(descriptor) +
                        sizeof(previous) + sizeof(td) +
                        4 * sizeof(LzssPositionDistance32mTokenRangeResult) +
                        16 * sizeof(std::size_t);
  const auto total =
      retained + controls + bytes.size() +
      std::max(lzss_position_distance_32m_compact_range_working_bytes(),
               lzss_position_distance_32m_token_range_working_bytes());
  const auto r = encode_lzss_position_distance_32m_compact_range(
      bytes, p, c, limits, output, scratch, descriptor,
      total - bytes.size() - output.size() - scratch.size() -
          lzss_position_distance_32m_compact_range_working_bytes());
  if (r.details.error != LzssPositionDistance32mTokenRangeError::none) {
    check(!r.bytes_committed &&
          std::memcmp(&descriptor, &previous, sizeof(previous)) == 0);
    for (auto b : output)
      check(b == std::byte{0xa5});
  }
  if (syntax) {
    const auto old = encode_lzss_position_distance_32m_token_range(
        std::span(tokens).first(count), p, c, limits, typed, ts, td,
        total - count * sizeof(Token) - typed.size() - ts.size() -
            lzss_position_distance_32m_token_range_working_bytes());
    check(old.details.error == r.details.error &&
          old.bytes_committed == r.bytes_committed && output == typed);
  } else
    check(r.details.error != LzssPositionDistance32mTokenRangeError::none);
}
} // namespace
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data,
                                      std::size_t size) {
  if (size < 9 || size > 136)
    return 0;
  const auto n = size - 8;
  std::array<std::byte, 128> raw{};
  for (std::size_t i = 0; i < n; ++i)
    raw[i] = std::byte{data[i + 8]};
  std::array<std::byte, 256> compact{};
  std::vector<std::uint32_t> index(1048576 + n);
  std::array<std::byte, 2400> frame{}, payload{}, first{}, second{};
  first.fill(std::byte{0xa5});
  second = first;
  auto capacity = [&](unsigned flag, unsigned control, std::size_t full) {
    return (data[0] & flag) ? std::size_t(data[control]) % full : full;
  };
  const LzssPositionDistance32mCompactFrameWorkspace b{
      std::span(compact).first(capacity(1, 1, compact.size())), index,
      std::span(frame).first(capacity(4, 5, frame.size())),
      std::span(payload).first(capacity(4, 6, payload.size()))};
  marc::core::DecoderLimits limits{};
  limits.max_frame_size = 33554432;
  limits.max_lz_distance = 33554432;
  limits.max_block_size = 128;
  limits.max_internal_buffered_bytes = 128u << 20;
  if (data[0] & 64)
    limits.max_internal_buffered_bytes = std::size_t(data[7]) * 4096;
  TypedContextStreamHeader s{};
  s.frame_size = static_cast<std::uint32_t>(n);
  s.original_size = n;
  s.dictionary = {33554432, 3, 258, 0};
  s.range_model_total = 32768;
  s.context_count = 49;
  s.dictionary_variant = 13;
  s.context_algorithm = 1;
  s.context_variant = 14;
  if (data[0] & 32)
    s.dictionary.min_match_length = data[1];
  const TypedContextFrameValidationContext c{s, limits,
                                             (data[0] & 16) ? data[2] : 0u, 0};
  const auto retained = (data[0] & 128) ? std::size_t(data[3]) * 1024 : 0;
  const auto outcap = capacity(8, 7, first.size());
  const auto charged = n + b.compact.size() +
                       index.size() * sizeof(std::uint32_t) + b.frame.size() +
                       b.payload_scratch.size() + outcap;
  const auto harness = sizeof(raw) + sizeof(compact) + sizeof(frame) +
                       sizeof(payload) + sizeof(first) + sizeof(second) +
                       index.capacity() * sizeof(std::uint32_t) +
                       sizeof(index) + sizeof(b) + sizeof(limits) + sizeof(s) +
                       sizeof(c) + 2 * sizeof(TypedContextFrameLayout) +
                       sizeof(LzssPositionDistance32mCompactFramePlan) +
                       2 * sizeof(LzssPositionDistance32mCompactFrameResult) +
                       256 * sizeof(std::byte) + 256 * sizeof(Token) +
                       32 * sizeof(std::size_t);
  const auto honest_retained = harness - charged + retained;
  TypedContextFrameLayout a{}, z{};
  a.serialized_size = z.serialized_size = 777;
  std::array<std::byte, sizeof(a)> before{};
  std::memcpy(before.data(), &a, sizeof(a));
  std::size_t written = 77, other = 77;
  auto r = encode_lzss_position_distance_32m_compact_frame(
      std::span(raw).first(n), c, b, std::span(first).first(outcap), a, written,
      honest_retained);
  auto t = encode_lzss_position_distance_32m_compact_frame(
      std::span(raw).first(n), c, b, std::span(second).first(outcap), z, other,
      honest_retained);
  check(r.error == t.error && r.bytes_committed == t.bytes_committed &&
        r.aggregate_bytes == t.aggregate_bytes && first == second &&
        written == other);
  if (r.error != LzssPositionDistance32mCompactFrameError::none) {
    check(r.bytes_committed == 0 && written == 77 &&
          std::memcmp(before.data(), &a, sizeof(a)) == 0);
    check(std::all_of(first.begin(), first.end(),
                      [](auto v) { return v == std::byte{0xa5}; }));
  } else {
    check(written == r.bytes_committed && a.serialized_size == written);
    check(std::all_of(first.begin() + written, first.end(),
                      [](auto v) { return v == std::byte{0xa5}; }));
    std::array<Token, 128> rt{}, rs{};
    std::array<Op, 256> ro{}, ros{};
    std::vector<std::uint32_t> ri(1048576 + n);
    std::array<std::byte, 2400> rf{}, rp{}, reference{};
    auto reference_limits = limits;
    reference_limits.max_internal_buffered_bytes = 128u << 20;
    const TypedContextFrameValidationContext rc{s, reference_limits,
                                                c.expected_sequence, 0};
    const LzssPositionDistance32mFrameEncodeWorkspace rb{rt,  rs, ri, ro,
                                                         ros, rf, rp};
    TypedContextFrameLayout rl{};
    std::size_t rw = 0;
    const auto rr = encode_lzss_position_distance_32m_frame(
        std::span(raw).first(n), rc, rb, reference, rl, rw,
        harness + sizeof(rt) + sizeof(rs) + sizeof(ro) + sizeof(ros) +
            sizeof(ri) + sizeof(rf) + sizeof(rp) + sizeof(reference) +
            sizeof(reference_limits) + sizeof(rc) + sizeof(rb) + sizeof(rl) +
            sizeof(rw));
    check(rr.error == LzssPositionDistance32mFrameEncodeError::none &&
          rw == written &&
          std::equal(reference.begin(), reference.begin() + rw, first.begin()));
    std::array<Token, 128> dt{}, ds{};
    std::array<std::byte, 128> decoded{}, scratch{};
    TypedContextFrameLayout parsed{};
    // The reference index and both reference/private owners are still live.
    // Use the explicit sufficient consumer policy, with conservative retained
    // controls, rather than hiding them behind the mutated encoder budget.
    const auto decode_retained =
        harness + ri.capacity() * sizeof(std::uint32_t) + sizeof(rt) +
        sizeof(rs) + sizeof(ro) + sizeof(ros) + sizeof(rf) + sizeof(rp) +
        sizeof(reference) + 4096;
    auto d = decode_lzss_position_distance_32m_frame(
        std::span(first).first(written), rc, dt, ds, decoded, scratch, parsed,
        decode_retained);
    check(d.error == LzssPositionDistance32mFrameDecodeError::none &&
          d.raw_produced == n &&
          std::equal(raw.begin(), raw.begin() + n, decoded.begin()));
  }
  range_records(std::span<const std::byte>(raw).first(n), harness + 4096);
  return 0;
}
