#include "frame/lzss_position_distance_8m_serializer.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>

namespace {
using namespace marc::frame::internal;
void require(bool ok) {
  if (!ok)
    std::abort();
}
template <class T> T read(const std::uint8_t *p, std::size_t offset) {
  T n{};
  for (std::size_t i = 0; i < sizeof(T); ++i)
    n |= T(p[offset + i]) << (8 * i);
  return n;
}
} // namespace
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data,
                                      std::size_t size) {
  if (size < 128)
    return 0;
  marc::core::DecoderLimits limits{};
  limits.max_block_size = 8388608;
  limits.max_internal_buffered_bytes = 512u << 20;
  TypedContextStreamHeader s{};
  s.frame_size = read<std::uint32_t>(data, 16);
  s.original_size = s.frame_size;
  s.dictionary = {8388608, 3, 258, 0};
  s.range_model_total = 32768;
  s.context_count = 47;
  s.dictionary_variant = 11;
  s.context_algorithm = 1;
  s.context_variant = 12;
  // First 80 bytes are independent frame-prefix seeds; trailing bytes mutate
  // stream parameters, capacities, position and retained-owner accounting.
  if (data[80] & 1)
    s.dictionary.window_size = read<std::uint32_t>(data, 84);
  if (data[80] & 2)
    s.dictionary.min_match_length = data[88];
  if (data[80] & 4)
    s.dictionary.max_match_length = read<std::uint16_t>(data, 89);
  if (data[80] & 8)
    s.dictionary.flags = data[91];
  if (data[80] & 16)
    s.dictionary_variant = data[92];
  if (data[80] & 32)
    s.context_variant = data[93];
  if (data[80] & 64)
    limits.max_internal_buffered_bytes = read<std::uint32_t>(data, 94);
  if (data[80] & 128)
    s.original_size = read<std::uint64_t>(data, 98);
  TypedContextFrameLayout x{};
  auto &h = x.header;
  h.flags = read<std::uint16_t>(data, 6);
  h.sequence = read<std::uint64_t>(data, 8);
  h.uncompressed_size = read<std::uint32_t>(data, 16);
  h.token_count = read<std::uint32_t>(data, 20);
  h.event_count = read<std::uint32_t>(data, 24);
  h.decision_count = read<std::uint32_t>(data, 28);
  h.payload_size = read<std::uint32_t>(data, 32);
  h.descriptor_size = read<std::uint32_t>(data, 36);
  h.context_side_data_size = read<std::uint32_t>(data, 40);
  h.checksum_trailer_size = read<std::uint32_t>(data, 44);
  x.descriptor = {read<std::uint32_t>(data, 64), read<std::uint32_t>(data, 68),
                  read<std::uint16_t>(data, 72)};
  x.serialized_size = std::size_t{80} + h.payload_size;
  const auto retained = read<std::uint32_t>(data, 106);
  const auto capacity = std::size_t(data[81]);
  const TypedContextFrameValidationContext context{
      s, limits, read<std::uint64_t>(data, 110),
      read<std::uint64_t>(data, 118)};
  for (bool prefix : {false, true}) {
    std::array<std::byte, 256> first{}, second{};
    first.fill(std::byte{0xa5});
    second = first;
    auto before = first;
    std::size_t n = 77, m = 77;
    const auto q = prefix ? query_lzss_position_distance_8m_prefix_serialize(
                                x, context, capacity, retained)
                          : query_lzss_position_distance_8m_stream_serialize(
                                s, limits, capacity, retained);
    auto call = [&](auto &out, auto &count) {
      auto view = std::span<std::byte>(out).first(capacity);
      return prefix ? serialize_lzss_position_distance_8m_frame_prefix(
                          x, context, view, count, retained)
                    : serialize_lzss_position_distance_8m_stream_header(
                          s, limits, view, count, retained);
    };
    const auto a = call(first, n), b = call(second, m);
    require(a.error == q.error && a.validation_error == q.validation_error);
    require(a.error == b.error && a.validation_error == b.validation_error &&
            a.bytes_committed == b.bytes_committed && n == m &&
            first == second);
    if (a.error != LzssPositionDistance8mSerializeError::none) {
      require(a.bytes_committed == 0 && n == 77 && first == before);
    } else {
      const auto need = prefix ? 80u : 112u;
      require(n == need && a.bytes_committed == need);
      require(
          std::equal(first.begin() + need, first.end(), before.begin() + need));
      if (prefix) {
        TypedContextFrameLayout parsed{};
        LzssPositionDistance8mFrameRequirements r{};
        require(preflight_lzss_position_distance_8m_frame_prefix(
                    std::span(first).first(80), context, parsed, r) ==
                LzssPositionDistance8mPreflightError::none);
        require(parsed.serialized_size == x.serialized_size &&
                parsed.header.token_count == h.token_count &&
                parsed.header.event_count == h.event_count &&
                parsed.header.decision_count == h.decision_count &&
                parsed.header.payload_size == h.payload_size);
      } else {
        TypedContextStreamHeader parsed{};
        std::size_t consumed{};
        require(parse_lzss_position_distance_8m_stream_header(
                    std::span(first).first(112), limits, parsed, consumed) ==
                LzssPositionDistance8mPreflightError::none);
        require(consumed == 112 && parsed.original_size == s.original_size &&
                parsed.frame_size == s.frame_size &&
                parsed.dictionary_variant == 11 &&
                parsed.context_variant == 12);
      }
    }
  }
  return 0;
}
