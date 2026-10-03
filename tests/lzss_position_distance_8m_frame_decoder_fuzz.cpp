#include "frame/lzss_position_distance_8m_frame_decoder.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
namespace {
using namespace marc::frame::internal;
using namespace marc::dictionary::internal;
void check(bool condition) {
  if (!condition)
    std::abort();
}
bool same(const LzssTypedToken &a, const LzssTypedToken &b) {
  return a.kind == b.kind && a.literal == b.literal &&
         a.distance == b.distance && a.length == b.length;
}
bool same(const LzssPositionDistance8mFrameDecodeResult &a,
          const LzssPositionDistance8mFrameDecodeResult &b) {
  const auto &x = a.token_result;
  const auto &y = b.token_result;
  return a.error == b.error && a.preflight_error == b.preflight_error &&
         a.bytes_consumed == b.bytes_consumed &&
         a.raw_produced == b.raw_produced && x.error == y.error &&
         x.token_error == y.token_error && x.token_count == y.token_count &&
         x.token_index == y.token_index && x.raw_size == y.raw_size &&
         x.entropy.error == y.entropy.error &&
         x.entropy.event_count == y.entropy.event_count &&
         x.entropy.decision_count == y.entropy.decision_count &&
         x.entropy.payload_consumed == y.entropy.payload_consumed;
}
} // namespace
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data,
                                      std::size_t size) {
  if (size < 88 || size > 2136)
    return 0;
  const auto word = [&](std::size_t p) {
    return static_cast<std::uint32_t>(data[p]) |
           (static_cast<std::uint32_t>(data[p + 1]) << 8);
  };
  const auto dword = [&](std::size_t p) {
    return word(p) | (word(p + 2) << 16);
  };
  const auto tc = word(0), sc = word(2), rc = word(4), rs = word(6),
             raw = dword(24), count = dword(28), events = dword(32),
             decisions = dword(36);
  if (tc > 66 || sc > 66 || rc > 514 || rs > 514 || raw > 512 || count > 64 ||
      events > 320 || decisions > 2112)
    return 0;
  auto limits = marc::core::DecoderLimits{};
  limits.max_frame_size = 512;
  limits.max_block_size = 512;
  limits.max_compressed_payload_size = 2048;
  limits.max_internal_buffered_bytes = 65536;
  TypedContextStreamHeader stream{};
  stream.frame_size = 512;
  stream.original_size = raw;
  stream.dictionary = {8388608, 3, 258, 0};
  stream.range_model_total = 32768;
  stream.context_count = 47;
  stream.dictionary_variant = 11;
  stream.context_algorithm = 1;
  stream.context_variant = 12;
  const TypedContextFrameValidationContext context{stream, limits, 0, 0};
  const auto input = std::as_bytes(std::span(data + 8, size - 8));
  const LzssTypedToken sentinel{LzssTypedTokenKind::match, 0, 12345, 54321};
  std::array<LzssTypedToken, 66> ta, tb, sa, sb;
  ta.fill(sentinel);
  tb.fill(sentinel);
  sa.fill(sentinel);
  sb.fill(sentinel);
  const auto guard = std::byte{0xa5};
  std::array<std::byte, 514> a, b, ra, rb;
  a.fill(guard);
  b.fill(guard);
  ra.fill(guard);
  rb.fill(guard);
  TypedContextFrameLayout ma{}, mb{};
  ma.serialized_size = 777;
  ma.header.sequence = 88;
  mb = ma;
  std::array<std::byte, sizeof(ma)> before_a{}, before_b{};
  std::memcpy(before_a.data(), &ma, sizeof(ma));
  std::memcpy(before_b.data(), &mb, sizeof(mb));
  const auto x = decode_lzss_position_distance_8m_frame(
      input, context, std::span(ta).first(tc), std::span(sa).first(sc),
      std::span(a).first(rc), std::span(ra).first(rs), ma);
  const auto y = decode_lzss_position_distance_8m_frame(
      input, context, std::span(tb).first(tc), std::span(sb).first(sc),
      std::span(b).first(rc), std::span(rb).first(rs), mb);
  check(same(x, y) && a == b && ra == rb);
  for (std::size_t i = 0; i < ta.size(); ++i) {
    check(same(ta[i], tb[i]) && same(sa[i], sb[i]));
    if (i >= tc)
      check(same(ta[i], sentinel));
    if (i >= sc)
      check(same(sa[i], sentinel));
  }
  if (x.error != LzssPositionDistance8mFrameDecodeError::none) {
    for (auto v : a)
      check(v == guard);
    check(!x.bytes_consumed && !x.raw_produced);
    check(std::memcmp(before_a.data(), &ma, sizeof(ma)) == 0 &&
          std::memcmp(before_b.data(), &mb, sizeof(mb)) == 0);
    return 0;
  }
  check(x.bytes_consumed == input.size() && x.raw_produced == raw &&
        ma.serialized_size == input.size() &&
        mb.serialized_size == input.size() && ma.header.token_count == count &&
        ma.header.uncompressed_size == raw);
  std::array<std::byte, 512> oracle{};
  std::size_t produced{};
  for (std::size_t i = 0; i < count; ++i) {
    const auto &t = ta[i];
    if (t.kind == LzssTypedTokenKind::literal) {
      check(!t.distance && !t.length && produced < raw);
      oracle[produced++] = static_cast<std::byte>(t.literal);
    } else {
      check(t.kind == LzssTypedTokenKind::match && !t.literal && t.distance &&
            t.distance <= produced && t.length >= 3 && t.length <= 258 &&
            t.length <= raw - produced);
      for (std::size_t n = 0; n < t.length; ++n) {
        oracle[produced] = oracle[produced - t.distance];
        ++produced;
      }
    }
  }
  check(produced == raw);
  for (std::size_t i = 0; i < a.size(); ++i)
    check(i < raw ? a[i] == oracle[i] && ra[i] == oracle[i]
                  : a[i] == guard && ra[i] == guard);
  return 0;
}
