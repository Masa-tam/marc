#include "frame/lzss_position_distance_32m_serializer.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include "core/endian.hpp"
#include <algorithm>
#include <array>
namespace marc::frame::internal {
namespace {
using E = LzssPositionDistance32mSerializeError;
using V = LzssPositionDistance32mPreflightError;
struct Region {
  const void *data;
  std::size_t bytes;
};
struct StreamWorking {
  std::array<std::byte, 112> bytes{};
  TypedContextStreamHeader parsed{};
  std::size_t consumed{};
};
struct PrefixWorking {
  std::array<std::byte, 80> bytes{};
  TypedContextFrameLayout parsed{};
  LzssPositionDistance32mFrameRequirements requirements{};
};
// Include caller/query/result and the nested parser's concrete named objects.
constexpr auto stream_working =
    sizeof(StreamWorking) + 2 * sizeof(LzssPositionDistance32mSerializePlan) +
    sizeof(LzssPositionDistance32mSerializeResult) +
    sizeof(std::array<Region, 4>) + sizeof(TypedContextStreamHeader) +
    sizeof(std::array<Region, 4>);
constexpr auto prefix_working =
    sizeof(PrefixWorking) + 2 * sizeof(LzssPositionDistance32mSerializePlan) +
    sizeof(LzssPositionDistance32mSerializeResult) +
    sizeof(std::array<Region, 6>) + sizeof(TypedContextFrameLayout) +
    sizeof(LzssPositionDistance32mFrameRequirements) +
    sizeof(core::FrameBounds) + sizeof(std::array<Region, 6>);
E translated(V v) noexcept {
  return v == V::none                  ? E::none
         : v == V::limit_exceeded      ? E::limit_exceeded
         : v == V::arithmetic_overflow ? E::arithmetic_overflow
                                       : E::validation_error;
}
template <std::size_t N>
E aliases(const std::array<Region, N> &regions) noexcept {
  for (std::size_t i = 0; i < N; ++i)
    for (std::size_t j = i + 1; j < N; ++j) {
      auto o = core::check_buffer_overlap(regions[i].data, regions[i].bytes,
                                          regions[j].data, regions[j].bytes);
      if (o != core::BufferOverlap::disjoint)
        return o == core::BufferOverlap::arithmetic_overflow
                   ? E::arithmetic_overflow
                   : E::overlapping_buffers;
    }
  return E::none;
}
template <class T>
bool put(std::span<std::byte> b, std::size_t pos, T value) noexcept {
  return core::store_le(b, pos, value);
}
bool same(const TypedContextStreamHeader &a,
          const TypedContextStreamHeader &b) noexcept {
  return a.frame_size == b.frame_size && a.original_size == b.original_size &&
         a.dictionary.window_size == b.dictionary.window_size &&
         a.dictionary.min_match_length == b.dictionary.min_match_length &&
         a.dictionary.max_match_length == b.dictionary.max_match_length &&
         a.dictionary.flags == b.dictionary.flags &&
         a.range_model_total == b.range_model_total &&
         a.context_count == b.context_count &&
         a.dictionary_variant == b.dictionary_variant &&
         a.context_algorithm == b.context_algorithm &&
         a.context_variant == b.context_variant;
}
bool same(const TypedContextFrameLayout &a,
          const TypedContextFrameLayout &b) noexcept {
  const auto &x = a.header;
  const auto &y = b.header;
  return x.flags == y.flags && x.sequence == y.sequence &&
         x.uncompressed_size == y.uncompressed_size &&
         x.token_count == y.token_count && x.event_count == y.event_count &&
         x.decision_count == y.decision_count &&
         x.payload_size == y.payload_size &&
         x.descriptor_size == y.descriptor_size &&
         x.context_side_data_size == y.context_side_data_size &&
         x.checksum_trailer_size == y.checksum_trailer_size &&
         a.descriptor.decision_count == b.descriptor.decision_count &&
         a.descriptor.payload_size == b.descriptor.payload_size &&
         a.descriptor.context_count == b.descriptor.context_count &&
         a.serialized_size == b.serialized_size;
}
V prepare(StreamWorking &w, const TypedContextStreamHeader &s,
          const core::DecoderLimits &l) noexcept {
  auto v = validate_lzss_position_distance_32m_stream_semantics(s, l);
  if (v != V::none)
    return v;
  auto &b = w.bytes;
  b[0] = std::byte{'M'};
  b[1] = std::byte{'A'};
  b[2] = std::byte{'R'};
  b[3] = std::byte{'C'};
  if (!put(b, 4, std::uint16_t{2}) || !put(b, 6, std::uint16_t{0}) ||
      !put(b, 8, std::uint16_t{64}) || !put(b, 10, std::uint16_t{1}) ||
      !put(b, 12, std::uint16_t{2}) || !put(b, 14, s.dictionary_variant) ||
      !put(b, 16, std::uint16_t{3}) || !put(b, 18, std::uint16_t{2}) ||
      !put(b, 20, s.frame_size) || !put(b, 28, std::uint32_t{16}) ||
      !put(b, 32, std::uint32_t{16}) || !put(b, 40, s.original_size) ||
      !put(b, 48, std::uint32_t{16}) || !put(b, 64, s.dictionary.window_size) ||
      !put(b, 68, s.dictionary.min_match_length) ||
      !put(b, 72, s.dictionary.max_match_length) ||
      !put(b, 76, s.dictionary.flags) || !put(b, 80, s.range_model_total) ||
      !put(b, 84, s.context_count) || !put(b, 96, s.context_algorithm) ||
      !put(b, 98, s.context_variant))
    return V::arithmetic_overflow;
  v = parse_lzss_position_distance_32m_stream_header(b, l, w.parsed,
                                                     w.consumed);
  if (v != V::none)
    return v;
  return w.consumed == 112 && same(w.parsed, s) ? V::none : V::invalid_stream;
}
V prepare(PrefixWorking &w, const TypedContextFrameLayout &layout,
          const TypedContextFrameValidationContext &c) noexcept {
  const auto &h = layout.header;
  const auto &d = layout.descriptor;
  auto &b = w.bytes;
  b[0] = std::byte{'M'};
  b[1] = std::byte{'R'};
  b[2] = std::byte{'F'};
  b[3] = std::byte{'2'};
  if (!put(b, 4, std::uint16_t{64}) || !put(b, 6, h.flags) ||
      !put(b, 8, h.sequence) || !put(b, 16, h.uncompressed_size) ||
      !put(b, 20, h.token_count) || !put(b, 24, h.event_count) ||
      !put(b, 28, h.decision_count) || !put(b, 32, h.payload_size) ||
      !put(b, 36, h.descriptor_size) || !put(b, 40, h.context_side_data_size) ||
      !put(b, 44, h.checksum_trailer_size) || !put(b, 64, d.decision_count) ||
      !put(b, 68, d.payload_size) || !put(b, 72, d.context_count))
    return V::arithmetic_overflow;
  const auto v = preflight_lzss_position_distance_32m_frame_prefix(
      b, c, w.parsed, w.requirements);
  if (v != V::none)
    return v;
  return same(w.parsed, layout) ? V::none : V::contradictory_counts;
}
LzssPositionDistance32mSerializePlan
plan(std::size_t need, std::size_t working, std::size_t capacity,
     std::size_t retained, const core::DecoderLimits &l) noexcept {
  LzssPositionDistance32mSerializePlan q{};
  q.bytes_required = need;
  q.working_state_bytes = working;
  if (!core::checked_add(capacity, working, q.aggregate_bytes) ||
      !core::checked_add(q.aggregate_bytes, retained, q.aggregate_bytes)) {
    q.error = E::arithmetic_overflow;
    return q;
  }
  if (q.aggregate_bytes > l.max_internal_buffered_bytes) {
    q.error = E::limit_exceeded;
    return q;
  }
  if (capacity < need)
    q.error = E::output_too_small;
  return q;
}
} // namespace
std::size_t
lzss_position_distance_32m_stream_serialize_working_bytes() noexcept {
  return stream_working;
}
std::size_t
lzss_position_distance_32m_prefix_serialize_working_bytes() noexcept {
  return prefix_working;
}
LzssPositionDistance32mSerializePlan
query_lzss_position_distance_32m_stream_serialize(
    const TypedContextStreamHeader &s, const core::DecoderLimits &l,
    std::size_t capacity, std::size_t retained) noexcept {
  auto q = plan(112, stream_working, capacity, retained, l);
  if (q.error != E::none)
    return q;
  StreamWorking w{};
  q.validation_error = prepare(w, s, l);
  q.error = translated(q.validation_error);
  return q;
}
LzssPositionDistance32mSerializePlan
query_lzss_position_distance_32m_prefix_serialize(
    const TypedContextFrameLayout &layout,
    const TypedContextFrameValidationContext &c, std::size_t capacity,
    std::size_t retained) noexcept {
  auto q = plan(80, prefix_working, capacity, retained, c.limits);
  if (q.error != E::none)
    return q;
  PrefixWorking w{};
  q.validation_error = prepare(w, layout, c);
  q.error = translated(q.validation_error);
  return q;
}
LzssPositionDistance32mSerializeResult
serialize_lzss_position_distance_32m_stream_header(
    const TypedContextStreamHeader &s, const core::DecoderLimits &l,
    std::span<std::byte> out, std::size_t &written,
    std::size_t retained) noexcept {
  LzssPositionDistance32mSerializeResult r{};
  r.error = aliases(std::array{Region{&s, sizeof(s)}, Region{&l, sizeof(l)},
                               Region{out.data(), out.size()},
                               Region{&written, sizeof(written)}});
  if (r.error != E::none)
    return r;
  const auto q = query_lzss_position_distance_32m_stream_serialize(
      s, l, out.size(), retained);
  r.error = q.error;
  r.validation_error = q.validation_error;
  if (r.error != E::none)
    return r;
  StreamWorking w{};
  r.validation_error = prepare(w, s, l);
  r.error = translated(r.validation_error);
  if (r.error != E::none)
    return r;
  std::ranges::copy(w.bytes, out.begin());
  written = 112;
  r.bytes_committed = 112;
  return r;
}
LzssPositionDistance32mSerializeResult
serialize_lzss_position_distance_32m_frame_prefix(
    const TypedContextFrameLayout &layout,
    const TypedContextFrameValidationContext &c, std::span<std::byte> out,
    std::size_t &written, std::size_t retained) noexcept {
  LzssPositionDistance32mSerializeResult r{};
  r.error = aliases(std::array{
      Region{&layout, sizeof(layout)}, Region{&c, sizeof(c)},
      Region{&c.stream, sizeof(c.stream)}, Region{&c.limits, sizeof(c.limits)},
      Region{out.data(), out.size()}, Region{&written, sizeof(written)}});
  if (r.error != E::none)
    return r;
  const auto q = query_lzss_position_distance_32m_prefix_serialize(
      layout, c, out.size(), retained);
  r.error = q.error;
  r.validation_error = q.validation_error;
  if (r.error != E::none)
    return r;
  PrefixWorking w{};
  r.validation_error = prepare(w, layout, c);
  r.error = translated(r.validation_error);
  if (r.error != E::none)
    return r;
  std::ranges::copy(w.bytes, out.begin());
  written = 80;
  r.bytes_committed = 80;
  return r;
}
} // namespace marc::frame::internal
