#include "context/lzss_position_distance_8m_tokens.hpp"
#include <array>
#include <cstdlib>
namespace {
using namespace marc::context::internal;
using namespace marc::dictionary::internal;
void check(bool value) {
  if (!value)
    std::abort();
}
bool same(const LzssTypedToken &a, const LzssTypedToken &b) {
  return a.kind == b.kind && a.literal == b.literal &&
         a.distance == b.distance && a.length == b.length;
}
bool same(const LzssContextualRangeDecodeResult &a,
          const LzssContextualRangeDecodeResult &b) {
  return a.error == b.error && a.token_error == b.token_error &&
         a.token_count == b.token_count && a.token_index == b.token_index &&
         a.raw_size == b.raw_size && a.entropy.error == b.entropy.error &&
         a.entropy.event_count == b.entropy.event_count &&
         a.entropy.decision_count == b.entropy.decision_count &&
         a.entropy.payload_consumed == b.entropy.payload_consumed;
}
} // namespace
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data,
                                      std::size_t size) {
  if (size < 10 || size > 2058)
    return 0;
  const auto word = [&](std::size_t i) {
    return static_cast<std::uint32_t>(data[i]) |
           (static_cast<std::uint32_t>(data[i + 1]) << 8);
  };
  const auto raw = word(0), tokens = word(2), events = word(4),
             decisions = word(6);
  if (raw > 512 || tokens > 64 || events > 320 || decisions > 2112 ||
      data[8] > 66 || data[9] > 66)
    return 0;
  const LzssTypedToken sentinel{LzssTypedTokenKind::match, 0, 12345, 54321};
  std::array<LzssTypedToken, 66> a, b, sa, sb;
  a.fill(sentinel);
  b.fill(sentinel);
  sa.fill(sentinel);
  sb.fill(sentinel);
  auto limits = marc::core::DecoderLimits{};
  limits.max_frame_size = 512;
  limits.max_block_size = 512;
  limits.max_compressed_payload_size = 2048;
  limits.max_internal_buffered_bytes = 65536;
  const LzssParameters parameters{8388608, 3, 258, 0};
  const LzssFieldContextValidationContext context{tokens, events, decisions,
                                                  raw, 0};
  const marc::entropy::internal::ContextualDynamicRangeDescriptor descriptor{
      decisions, static_cast<std::uint32_t>(size - 10), 47};
  const auto payload = std::as_bytes(std::span(data + 10, size - 10));
  const auto ra = decode_lzss_position_distance_8m_tokens(
      descriptor, payload, parameters, context, limits,
      std::span(a).first(data[8]), std::span(sa).first(data[9]));
  const auto rb = decode_lzss_position_distance_8m_tokens(
      descriptor, payload, parameters, context, limits,
      std::span(b).first(data[8]), std::span(sb).first(data[9]));
  check(same(ra, rb));
  for (std::size_t i = 0; i < a.size(); ++i) {
    check(same(a[i], b[i]) && same(sa[i], sb[i]));
    if (ra.error != LzssContextualRangeDecodeError::none || i >= tokens)
      check(same(a[i], sentinel));
    if (i >= data[9])
      check(same(sa[i], sentinel));
  }
  if (ra.error == LzssContextualRangeDecodeError::none) {
    check(ra.token_count == tokens && ra.raw_size == raw);
    std::array<std::uint8_t, 512> bytes{};
    std::size_t produced{};
    for (std::size_t i = 0; i < tokens; ++i) {
      const auto &t = a[i];
      if (t.kind == LzssTypedTokenKind::literal) {
        check(!t.distance && !t.length && produced < raw);
        bytes[produced++] = t.literal;
      } else {
        check(t.kind == LzssTypedTokenKind::match && !t.literal && t.distance &&
              t.distance <= produced && t.distance <= 8388608 &&
              t.length >= 3 && t.length <= 258 && t.length <= raw - produced);
        for (std::size_t n = 0; n < t.length; ++n) {
          bytes[produced] = bytes[produced - t.distance];
          ++produced;
        }
      }
    }
    check(produced == raw);
  }
  return 0;
}
