#include "frame/lzss_position_distance_64m_compact_prefix.hpp"
#include "core/buffer_overlap.hpp"
#include "core/endian.hpp"
#include <algorithm>
#include <array>
#include <utility>
namespace marc::frame::internal {
namespace {
using Error = LzssPositionDistance64mPreflightError;
struct Region {
  const void *data;
  std::size_t size;
};
template <std::size_t W, std::size_t R>
Error output_preflight(const std::array<Region, W> &writes,
                       const std::array<Region, R> &reads) noexcept {
  for (const auto &a : writes) {
    for (const auto &b : reads) {
      auto o = core::check_buffer_overlap(a.data, a.size, b.data, b.size);
      if (o != core::BufferOverlap::disjoint)
        return o == core::BufferOverlap::arithmetic_overflow
                   ? Error::arithmetic_overflow
                   : Error::overlapping_output;
    }
  }
  for (std::size_t i = 0; i < W; ++i)
    for (std::size_t j = i + 1; j < W; ++j)
      if (core::check_buffer_overlap(writes[i].data, writes[i].size,
                                     writes[j].data, writes[j].size) !=
          core::BufferOverlap::disjoint)
        return Error::overlapping_output;
  return Error::none;
}
bool zero(std::span<const std::byte> s) noexcept {
  return std::ranges::all_of(s, [](std::byte b) { return b == std::byte{}; });
}
template <class T>
T read(std::span<const std::byte> s, std::size_t offset) noexcept {
  T v{};
  static_cast<void>(core::load_le(s, offset, v));
  return v;
}
bool magic(std::span<const std::byte> s, std::byte a, std::byte b, std::byte c,
           std::byte d) noexcept {
  return s[0] == a && s[1] == b && s[2] == c && s[3] == d;
}
} // namespace
Error preflight_lzss_position_distance_64m_compact_encode_prefix(
    std::span<const std::byte> input,
    const TypedContextFrameValidationContext &c,
    TypedContextFrameLayout &output,
    LzssPositionDistance64mFrameRequirements &requirements,
    std::size_t compact_capacity, std::size_t retained) noexcept {
  const auto alias = output_preflight(
      std::array{Region{&output, sizeof(output)},
                 Region{&requirements, sizeof(requirements)}},
      std::array{Region{input.data(), input.size()}, Region{&c, sizeof(c)},
                 Region{&c.stream, sizeof(c.stream)},
                 Region{&c.limits, sizeof(c.limits)}});
  if (alias != Error::none)
    return alias;
  auto error =
      validate_lzss_position_distance_64m_stream_semantics(c.stream, c.limits);
  if (error != Error::none)
    return error;
  if (input.size() < 64)
    return Error::truncated_frame_header;
  const auto b = input.first(64);
  if (!magic(b, std::byte{'M'}, std::byte{'R'}, std::byte{'F'}, std::byte{'2'}))
    return Error::invalid_magic;
  if (read<std::uint16_t>(b, 4) != 64)
    return Error::invalid_header_size;
  if (read<std::uint16_t>(b, 6) || read<std::uint32_t>(b, 40) ||
      read<std::uint32_t>(b, 44))
    return Error::unsupported_feature;
  if (!zero(b.subspan(48, 16)))
    return Error::nonzero_reserved;
  if (input.size() < 80)
    return Error::truncated_descriptor;
  auto d = input.subspan(64, 16);
  if (read<std::uint16_t>(d, 10))
    return Error::unsupported_feature;
  if (!zero(d.subspan(12, 4)))
    return Error::nonzero_reserved;
  TypedContextFrameLayout result{};
  auto &f = result.header;
  f.sequence = read<std::uint64_t>(b, 8);
  f.uncompressed_size = read<std::uint32_t>(b, 16);
  f.token_count = read<std::uint32_t>(b, 20);
  f.event_count = read<std::uint32_t>(b, 24);
  f.decision_count = read<std::uint32_t>(b, 28);
  f.payload_size = read<std::uint32_t>(b, 32);
  f.descriptor_size = read<std::uint32_t>(b, 36);
  result.descriptor = {read<std::uint32_t>(d, 0), read<std::uint32_t>(d, 4),
                       read<std::uint16_t>(d, 8)};
  if (c.output_already_committed % c.stream.frame_size ||
      c.expected_sequence != c.output_already_committed / c.stream.frame_size ||
      f.sequence != c.expected_sequence)
    return Error::unexpected_sequence;
  if (c.output_already_committed >= c.stream.original_size ||
      f.uncompressed_size !=
          std::min<std::uint64_t>(c.stream.frame_size,
                                  c.stream.original_size -
                                      c.output_already_committed))
    return Error::unexpected_frame_size;
  const std::uint64_t raw = f.uncompressed_size, t = f.token_count,
                      e = f.event_count, n = f.decision_count,
                      p = f.payload_size;
  // Stream capacity already bounds these values; widen before products.
  if (!t || t > raw || e < 2 * t || e > std::min(2 * raw, 5 * t) || n < e ||
      n > std::min(10 * raw, 36 * t) || p < 5 ||
      p > std::min(2 * n + 5, 20 * raw + 5) || f.descriptor_size != 16)
    return Error::contradictory_counts;
  if (result.descriptor.decision_count != n ||
      result.descriptor.payload_size != p ||
      result.descriptor.context_count != 50)
    return Error::invalid_descriptor;
  if (compact_capacity < 2 * t)
    return Error::contradictory_counts;
  std::uint64_t serialized{}, tokens{}, working{}, model{}, aggregate{};
  if (!core::checked_add(p, std::uint64_t{80}, serialized) ||
      !core::checked_add(std::uint64_t{0},
                         static_cast<std::uint64_t>(compact_capacity),
                         tokens) ||
      !core::checked_add(serialized, tokens, working) ||
      !core::checked_add(working, raw, working) ||
      !core::checked_add(
          std::uint64_t{
              sizeof(entropy::internal::LzssPositionDistance64mRangeState)},
          static_cast<std::uint64_t>(retained), model) ||
      !core::checked_add(working, model, aggregate) ||
      !std::in_range<std::size_t>(aggregate) ||
      !std::in_range<std::size_t>(serialized))
    return Error::arithmetic_overflow;
  const core::FrameBounds bounds{raw,
                                 0,
                                 p,
                                 raw,
                                 0,
                                 c.stream.dictionary.window_size,
                                 c.stream.dictionary.max_match_length,
                                 0,
                                 2632,
                                 32768,
                                 model,
                                 working,
                                 1};
  const auto limit =
      core::validate_frame_bounds(c.limits, bounds, c.output_already_committed);
  if (limit != core::LimitError::none)
    return limit == core::LimitError::arithmetic_overflow
               ? Error::arithmetic_overflow
               : Error::limit_exceeded;
  result.serialized_size = static_cast<std::size_t>(serialized);
  const LzssPositionDistance64mFrameRequirements r{
      result.serialized_size, static_cast<std::size_t>(t),
      static_cast<std::size_t>(raw), static_cast<std::size_t>(aggregate)};
  output = result;
  requirements = r;
  return Error::none;
}
namespace {
using E = LzssPositionDistance64mSerializeError;
using V = LzssPositionDistance64mPreflightError;
struct PrefixWorking {
  std::array<std::byte, 80> bytes{};
  TypedContextFrameLayout parsed{};
  LzssPositionDistance64mFrameRequirements requirements{};
};
constexpr auto controls =
    sizeof(PrefixWorking) + 2 * sizeof(LzssPositionDistance64mSerializePlan) +
    sizeof(LzssPositionDistance64mSerializeResult) +
    2 * sizeof(std::array<Region, 6>) + sizeof(TypedContextFrameLayout) +
    sizeof(LzssPositionDistance64mFrameRequirements) +
    sizeof(core::FrameBounds) + 20 * sizeof(std::size_t);
template <class T>
bool put(std::span<std::byte> b, std::size_t p, T v) noexcept {
  return core::store_le(b, p, v);
}
E translated(V v) noexcept {
  return v == V::none                  ? E::none
         : v == V::limit_exceeded      ? E::limit_exceeded
         : v == V::arithmetic_overflow ? E::arithmetic_overflow
                                       : E::validation_error;
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
V prepare(PrefixWorking &w, const TypedContextFrameLayout &layout,
          const TypedContextFrameValidationContext &c,
          std::size_t compact_capacity) noexcept {
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
  const auto v = preflight_lzss_position_distance_64m_compact_encode_prefix(
      b, c, w.parsed, w.requirements, compact_capacity);
  if (v != V::none)
    return v;
  return same(w.parsed, layout) ? V::none : V::contradictory_counts;
}
} // namespace
std::size_t lzss_position_distance_64m_compact_prefix_working_bytes() noexcept {
  return controls;
}
LzssPositionDistance64mSerializePlan
query_lzss_position_distance_64m_compact_prefix_serialize(
    const TypedContextFrameLayout &layout,
    const TypedContextFrameValidationContext &c, std::size_t capacity,
    std::size_t compact_capacity, std::size_t retained) noexcept {
  LzssPositionDistance64mSerializePlan q{};
  q.bytes_required = 80;
  q.working_state_bytes = controls;
  q.aggregate_bytes = controls;
  for (auto n : {capacity, compact_capacity, retained})
    if (!core::checked_add(q.aggregate_bytes, n, q.aggregate_bytes)) {
      q.error = E::arithmetic_overflow;
      return q;
    }
  if (q.aggregate_bytes > c.limits.max_internal_buffered_bytes) {
    q.error = E::limit_exceeded;
    return q;
  }
  if (capacity < 80) {
    q.error = E::output_too_small;
    return q;
  }
  PrefixWorking w{};
  q.validation_error = prepare(w, layout, c, compact_capacity);
  q.error = translated(q.validation_error);
  return q;
}
LzssPositionDistance64mSerializeResult
serialize_lzss_position_distance_64m_compact_prefix(
    const TypedContextFrameLayout &layout,
    const TypedContextFrameValidationContext &c, std::span<std::byte> output,
    std::size_t &written, std::size_t compact_capacity,
    std::size_t retained) noexcept {
  LzssPositionDistance64mSerializeResult r{};
  const auto reads = std::array{
      Region{&layout, sizeof(layout)}, Region{&c, sizeof(c)},
      Region{&c.stream, sizeof(c.stream)}, Region{&c.limits, sizeof(c.limits)}};
  r.validation_error =
      output_preflight(std::array{Region{output.data(), output.size()},
                                  Region{&written, sizeof(written)}},
                       reads);
  if (r.validation_error != V::none) {
    r.error = r.validation_error == V::arithmetic_overflow
                  ? E::arithmetic_overflow
                  : E::overlapping_buffers;
    return r;
  }
  const auto q = query_lzss_position_distance_64m_compact_prefix_serialize(
      layout, c, output.size(), compact_capacity, retained);
  r.error = q.error;
  r.validation_error = q.validation_error;
  if (r.error != E::none)
    return r;
  PrefixWorking w{};
  r.validation_error = prepare(w, layout, c, compact_capacity);
  r.error = translated(r.validation_error);
  if (r.error != E::none)
    return r;
  std::ranges::copy(w.bytes, output.begin());
  written = 80;
  r.bytes_committed = 80;
  return r;
}
} // namespace marc::frame::internal
