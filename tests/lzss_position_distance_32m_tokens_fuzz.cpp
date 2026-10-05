#include "context/lzss_position_distance_32m_compact_tokens.hpp"
#include "core/endian.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>

namespace {
using namespace marc;
using namespace context::internal;
using namespace dictionary::internal;
void check(bool condition) {
  if (!condition)
    std::abort();
}
bool equal(const LzssTypedToken &a, const LzssTypedToken &b) {
  return a.kind == b.kind && a.literal == b.literal &&
         a.distance == b.distance && a.length == b.length;
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data,
                                      std::size_t size) {
  if (size < 8 || size > 2056)
    return 0;
  const auto word = [&](std::size_t i) {
    return static_cast<std::uint32_t>(data[i]) |
           (static_cast<std::uint32_t>(data[i + 1]) << 8);
  };
  const auto raw = word(0), tokens = word(2), events = word(4),
             decisions = word(6);
  if (raw > 512 || tokens > 64)
    return 0;
  core::DecoderLimits limits{};
  limits.max_lz_distance = 33554432;
  limits.max_frame_size = limits.max_block_size = 512;
  limits.max_compressed_payload_size = 2048;
  limits.max_internal_buffered_bytes = 65536;
  const LzssParameters parameters{33554432, 3, 258, 0};
  const LzssFieldContextValidationContext context{tokens, events, decisions,
                                                  raw, 0};
  const entropy::internal::ContextualDynamicRangeDescriptor descriptor{
      decisions, static_cast<std::uint32_t>(size - 8), 49};
  const auto payload = std::as_bytes(std::span(data + 8, size - 8));
  const LzssTypedToken sentinel{LzssTypedTokenKind::match, 0, 12345, 54321};
  std::array<LzssTypedToken, 66> typed, typed_scratch;
  typed.fill(sentinel);
  typed_scratch.fill(sentinel);
  constexpr auto guard = std::byte{0xcc};
  std::array<std::byte, 1539> compact, compact_scratch;
  compact.fill(guard);
  compact_scratch.fill(guard);
  std::size_t committed = 77;
  const auto a = decode_lzss_position_distance_32m_tokens(
      descriptor, payload, parameters, context, limits, typed, typed_scratch);
  const auto b = decode_lzss_position_distance_32m_compact_tokens(
      descriptor, payload, parameters, context, limits, compact,
      compact_scratch, committed);
  check(a.error == b.error && a.token_error == b.token_error &&
        a.token_count == b.token_count && a.raw_size == b.raw_size &&
        a.entropy.error == b.entropy.error);
  if (a.error != LzssContextualRangeDecodeError::none) {
    check(committed == 77);
    for (auto byte : compact)
      check(byte == guard);
    for (const auto &token : typed)
      check(equal(token, sentinel));
    return 0;
  }
  std::array<std::uint8_t, 512> reconstructed{};
  std::size_t cursor{}, produced{};
  for (std::size_t i = 0; i < tokens; ++i) {
    const auto &token = typed[i];
    if (token.kind == LzssTypedTokenKind::literal) {
      check(cursor + 2 <= committed && compact[cursor] == std::byte{0});
      check(compact[cursor + 1] == static_cast<std::byte>(token.literal));
      check(produced < raw);
      reconstructed[produced++] = token.literal;
      cursor += 2;
    } else {
      std::uint32_t distance{}, length{};
      check(cursor + 9 <= committed && compact[cursor] == std::byte{1});
      check(core::load_le(std::span<const std::byte>(compact), cursor + 1,
                          distance));
      check(core::load_le(std::span<const std::byte>(compact), cursor + 5,
                          length));
      check(distance == token.distance && length == token.length && distance &&
            distance <= produced && length <= raw - produced);
      for (std::size_t n = 0; n < length; ++n) {
        reconstructed[produced] = reconstructed[produced - distance];
        ++produced;
      }
      cursor += 9;
    }
  }
  check(cursor == committed && produced == raw);
  for (std::size_t i = committed; i < compact.size(); ++i)
    check(compact[i] == guard);
  for (std::size_t i = tokens; i < typed.size(); ++i)
    check(equal(typed[i], sentinel));
  return 0;
}
