#include "frame/lzss_position_distance_32m_token_frame_encoder.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include <algorithm>
#include <array>
#include <bit>
namespace marc::frame::internal {
namespace {
using E = LzssPositionDistance32mTokenFrameError;
namespace d = dictionary::internal;
namespace c = context::internal;
namespace e = entropy::internal;
struct Region {
  const void *data;
  std::size_t bytes;
};
struct Working {
  d::LzssPositionDistance32mParseResult parse{};
  d::LzssPositionDistance32mParseMetadata token_counts{};
  c::LzssPositionDistance32mTokenCheck token_check{};
  e::LzssPositionDistance32mTokenRangeResult range{};
  e::ContextualDynamicRangeDescriptor descriptor{};
  LzssPositionDistance32mSerializePlan prefix_plan{};
  LzssPositionDistance32mSerializeResult prefix{};
  TypedContextFrameLayout parsed{};
  LzssPositionDistance32mFrameRequirements requirements{};
  std::array<std::size_t, 5> owner_bytes{};
  std::uint64_t produced{};
  std::size_t local{}, retained{}, prefix_written{};
};
std::size_t helper_bytes() noexcept {
  return std::max(
      {d::lzss_position_distance_32m_indexed_working_bytes(),
       e::lzss_position_distance_32m_token_range_working_bytes(),
       lzss_position_distance_32m_prefix_serialize_working_bytes()});
}
constexpr auto control_bytes =
    sizeof(Working) + 2 * sizeof(LzssPositionDistance32mTokenFramePlan) +
    sizeof(LzssPositionDistance32mTokenFrameResult) +
    2 * sizeof(std::array<Region, 13>) +
    sizeof(LzssPositionDistance32mTokenFrameWorkspace) +
    20 * sizeof(std::size_t);
template <class T> bool extent(std::span<T> s, std::size_t &n) noexcept {
  return core::checked_multiply(s.size(), sizeof(T), n);
}
template <std::size_t N> E aliases(const std::array<Region, N> &r) noexcept {
  for (std::size_t i = 0; i < N; ++i)
    for (std::size_t j = i + 1; j < N; ++j) {
      auto a = core::check_buffer_overlap(r[i].data, r[i].bytes, r[j].data,
                                          r[j].bytes);
      if (a != core::BufferOverlap::disjoint)
        return a == core::BufferOverlap::arithmetic_overflow
                   ? E::arithmetic_overflow
                   : E::overlapping_buffers;
    }
  return E::none;
}
bool remainder(Working &w, std::size_t total,
               std::initializer_list<std::size_t> local) noexcept {
  w.local = 0;
  for (auto n : local)
    if (!core::checked_add(w.local, n, w.local))
      return false;
  if (w.local > total)
    return false;
  w.retained = total - w.local;
  return true;
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
} // namespace
std::size_t lzss_position_distance_32m_token_frame_working_bytes() noexcept {
  return control_bytes + helper_bytes();
}
LzssPositionDistance32mTokenFramePlan
query_lzss_position_distance_32m_token_frame_encode(
    std::span<const std::byte> raw,
    const TypedContextFrameValidationContext &context,
    const LzssPositionDistance32mTokenFrameWorkspace &b,
    std::size_t output_capacity, std::size_t retained) noexcept {
  LzssPositionDistance32mTokenFramePlan q{};
  Working w{};
  q.working_state_bytes =
      lzss_position_distance_32m_token_frame_working_bytes();
  if (!extent(b.tokens, w.owner_bytes[0]) ||
      !extent(b.token_scratch, w.owner_bytes[1]) ||
      !extent(b.index, w.owner_bytes[2])) {
    q.error = E::arithmetic_overflow;
    return q;
  }
  w.owner_bytes[3] = b.frame.size();
  w.owner_bytes[4] = b.payload_scratch.size();
  q.error = aliases(std::array{
      Region{raw.data(), raw.size()}, Region{b.tokens.data(), w.owner_bytes[0]},
      Region{b.token_scratch.data(), w.owner_bytes[1]},
      Region{b.index.data(), w.owner_bytes[2]},
      Region{b.frame.data(), b.frame.size()},
      Region{b.payload_scratch.data(), b.payload_scratch.size()},
      Region{&context, sizeof(context)},
      Region{&context.stream, sizeof(context.stream)},
      Region{&context.limits, sizeof(context.limits)}, Region{&b, sizeof(b)}});
  if (q.error != E::none)
    return q;
  q.aggregate_bytes = q.working_state_bytes;
  for (auto n : w.owner_bytes)
    if (!core::checked_add(q.aggregate_bytes, n, q.aggregate_bytes)) {
      q.error = E::arithmetic_overflow;
      return q;
    }
  for (auto n : {raw.size(), output_capacity, retained})
    if (!core::checked_add(q.aggregate_bytes, n, q.aggregate_bytes)) {
      q.error = E::arithmetic_overflow;
      return q;
    }
  if (q.aggregate_bytes > context.limits.max_internal_buffered_bytes) {
    q.error = E::limit_exceeded;
    return q;
  }
  if (validate_lzss_position_distance_32m_stream_semantics(context.stream,
                                                           context.limits) !=
      LzssPositionDistance32mPreflightError::none) {
    q.error = E::invalid_stream;
    return q;
  }
  if (context.output_already_committed % context.stream.frame_size ||
      context.expected_sequence !=
          context.output_already_committed / context.stream.frame_size ||
      context.output_already_committed >= context.stream.original_size ||
      raw.size() !=
          std::min<std::uint64_t>(context.stream.frame_size,
                                  context.stream.original_size -
                                      context.output_already_committed)) {
    q.error = E::invalid_position;
    return q;
  }
  if (!remainder(w, q.aggregate_bytes,
                 {raw.size(), w.owner_bytes[0], w.owner_bytes[1],
                  w.owner_bytes[2],
                  d::lzss_position_distance_32m_indexed_working_bytes()})) {
    q.error = E::arithmetic_overflow;
    return q;
  }
  w.parse = d::tokenize_lzss_position_distance_32m_indexed(
      raw, context.stream.dictionary, context.limits, b.tokens, b.token_scratch,
      b.index, w.token_counts, w.retained, context.output_already_committed);
  if (w.parse.error != d::LzssPositionDistance32mParseError::none) {
    q.error = E::parser_error;
    return q;
  }
  if (w.parse.tokens_committed > UINT32_MAX) {
    q.error = E::arithmetic_overflow;
    return q;
  }
  q.token_count = w.parse.tokens_committed;
  q.counts.declared_token_count = static_cast<std::uint32_t>(q.token_count);
  q.counts.declared_raw_size = static_cast<std::uint32_t>(raw.size());
  q.counts.output_already_committed = context.output_already_committed;
  for (const auto &t : b.tokens.first(q.token_count)) {
    w.token_check = c::validate_lzss_position_distance_32m_token(
        t, context.stream.dictionary, {w.produced, raw.size()}, context.limits);
    if (w.token_check.error != d::LzssTypedTokenError::none) {
      q.error = E::token_error;
      return q;
    }
    w.produced = w.token_check.next_raw_size;
    std::uint32_t events = 2, decisions = 2;
    if (t.kind == d::LzssTypedTokenKind::match) {
      const auto lc = t.length < 5 ? 8u : std::bit_width(t.length - 4) - 1u,
                 dc = std::bit_width(t.distance) - 1u;
      events = 3 + (lc != 0) + (dc != 0);
      decisions = 3 + (lc == 8 ? 1 : lc) + dc;
    }
    if (!core::checked_add(q.counts.declared_event_count, events,
                           q.counts.declared_event_count) ||
        !core::checked_add(q.counts.declared_decision_count, decisions,
                           q.counts.declared_decision_count)) {
      q.error = E::arithmetic_overflow;
      return q;
    }
  }
  if (w.produced != raw.size() || w.token_counts.raw_size != raw.size() ||
      w.token_counts.token_count != q.token_count) {
    q.error = E::inconsistent_counts;
    return q;
  }
  std::size_t token_bytes{};
  if (!core::checked_multiply(q.token_count, sizeof(d::LzssTypedToken),
                              token_bytes)) {
    q.error = E::arithmetic_overflow;
    return q;
  }
  if (!remainder(w, q.aggregate_bytes,
                 {token_bytes,
                  e::lzss_position_distance_32m_token_range_working_bytes()})) {
    q.error = E::arithmetic_overflow;
    return q;
  }
  q.range_plan = e::query_lzss_position_distance_32m_token_range_encode(
      b.tokens.first(q.token_count), context.stream.dictionary, q.counts,
      context.limits, 0, 0, w.retained);
  if (q.range_plan.details.error !=
          e::LzssPositionDistance32mTokenRangeError::payload_output_too_small ||
      q.range_plan.aggregate_bytes != q.aggregate_bytes) {
    q.error = E::range_error;
    return q;
  }
  if (!core::checked_add(std::size_t{80}, q.range_plan.details.payload_size,
                         q.bytes_required)) {
    q.error = E::arithmetic_overflow;
    return q;
  }
  if (b.frame.size() < 80) {
    q.error = E::storage_too_small;
    return q;
  }
  if (!remainder(w, q.aggregate_bytes,
                 {token_bytes, b.frame.size() - 80, b.payload_scratch.size(),
                  e::lzss_position_distance_32m_token_range_working_bytes()})) {
    q.error = E::arithmetic_overflow;
    return q;
  }
  q.range_plan = e::query_lzss_position_distance_32m_token_range_encode(
      b.tokens.first(q.token_count), context.stream.dictionary, q.counts,
      context.limits, b.frame.size() - 80, b.payload_scratch.size(),
      w.retained);
  if (q.range_plan.details.error !=
          e::LzssPositionDistance32mTokenRangeError::none ||
      q.range_plan.aggregate_bytes != q.aggregate_bytes) {
    q.error = E::range_error;
    return q;
  }
  if (q.range_plan.details.payload_size != q.bytes_required - 80 ||
      q.range_plan.details.token_count != q.token_count ||
      q.range_plan.details.raw_size != raw.size() ||
      q.range_plan.details.operation_count != q.counts.declared_event_count ||
      q.range_plan.details.decision_count != q.counts.declared_decision_count) {
    q.error = E::inconsistent_counts;
    return q;
  }
  const auto &desc = q.range_plan.descriptor;
  if (desc.decision_count != q.counts.declared_decision_count ||
      desc.payload_size != q.range_plan.details.payload_size ||
      desc.context_count != 49) {
    q.error = E::inconsistent_counts;
    return q;
  }
  q.layout.header.sequence = context.expected_sequence;
  q.layout.header.uncompressed_size = q.counts.declared_raw_size;
  q.layout.header.token_count = q.counts.declared_token_count;
  q.layout.header.event_count = q.counts.declared_event_count;
  q.layout.header.decision_count = q.counts.declared_decision_count;
  q.layout.header.payload_size = desc.payload_size;
  q.layout.header.descriptor_size = 16;
  q.layout.descriptor.decision_count = q.counts.declared_decision_count;
  q.layout.descriptor.payload_size =
      static_cast<std::uint32_t>(q.range_plan.details.payload_size);
  q.layout.descriptor.context_count = 49;
  q.layout.serialized_size = q.bytes_required;
  if (!remainder(
          w, q.aggregate_bytes,
          {80, lzss_position_distance_32m_prefix_serialize_working_bytes()})) {
    q.error = E::arithmetic_overflow;
    return q;
  }
  w.prefix_plan = query_lzss_position_distance_32m_prefix_serialize(
      q.layout, context, 80, w.retained);
  if (w.prefix_plan.error != LzssPositionDistance32mSerializeError::none ||
      w.prefix_plan.aggregate_bytes != q.aggregate_bytes) {
    q.error = E::prefix_error;
    return q;
  }
  if (output_capacity < q.bytes_required)
    q.error = E::storage_too_small;
  return q;
}
LzssPositionDistance32mTokenFrameResult
encode_lzss_position_distance_32m_token_frame(
    std::span<const std::byte> raw,
    const TypedContextFrameValidationContext &context,
    const LzssPositionDistance32mTokenFrameWorkspace &b,
    std::span<std::byte> output, TypedContextFrameLayout &layout,
    std::size_t &written, std::size_t retained) noexcept {
  LzssPositionDistance32mTokenFrameResult r{};
  std::size_t tb{}, ts{}, ix{};
  if (!extent(b.tokens, tb) || !extent(b.token_scratch, ts) ||
      !extent(b.index, ix)) {
    r.error = E::arithmetic_overflow;
    return r;
  }
  r.error = aliases(std::array{
      Region{raw.data(), raw.size()}, Region{b.tokens.data(), tb},
      Region{b.token_scratch.data(), ts}, Region{b.index.data(), ix},
      Region{b.frame.data(), b.frame.size()},
      Region{b.payload_scratch.data(), b.payload_scratch.size()},
      Region{output.data(), output.size()}, Region{&context, sizeof(context)},
      Region{&context.stream, sizeof(context.stream)},
      Region{&context.limits, sizeof(context.limits)}, Region{&b, sizeof(b)},
      Region{&layout, sizeof(layout)}, Region{&written, sizeof(written)}});
  if (r.error != E::none)
    return r;
  const auto q = query_lzss_position_distance_32m_token_frame_encode(
      raw, context, b, output.size(), retained);
  r.aggregate_bytes = q.aggregate_bytes;
  r.working_state_bytes = q.working_state_bytes;
  r.error = q.error;
  if (r.error != E::none)
    return r;
  Working w{};
  std::size_t token_bytes{};
  if (!core::checked_multiply(q.token_count, sizeof(d::LzssTypedToken),
                              token_bytes)) {
    r.error = E::arithmetic_overflow;
    return r;
  }
  if (!remainder(w, q.aggregate_bytes,
                 {token_bytes, b.frame.size() - 80, b.payload_scratch.size(),
                  e::lzss_position_distance_32m_token_range_working_bytes()})) {
    r.error = E::arithmetic_overflow;
    return r;
  }
  w.range = e::encode_lzss_position_distance_32m_token_range(
      b.tokens.first(q.token_count), context.stream.dictionary, q.counts,
      context.limits, b.frame.subspan(80), b.payload_scratch, w.descriptor,
      w.retained);
  if (w.range.details.error !=
      e::LzssPositionDistance32mTokenRangeError::none) {
    r.error = E::range_error;
    return r;
  }
  if (w.range.bytes_committed != q.range_plan.details.payload_size ||
      w.range.details.token_count != q.token_count ||
      w.range.details.raw_size != raw.size() ||
      w.range.details.decision_count != q.counts.declared_decision_count ||
      w.range.details.operation_count != q.counts.declared_event_count ||
      w.descriptor.decision_count != q.layout.descriptor.decision_count ||
      w.descriptor.payload_size != q.layout.descriptor.payload_size ||
      w.descriptor.context_count != 49) {
    r.error = E::inconsistent_counts;
    return r;
  }
  if (!remainder(
          w, q.aggregate_bytes,
          {80, lzss_position_distance_32m_prefix_serialize_working_bytes()})) {
    r.error = E::arithmetic_overflow;
    return r;
  }
  w.prefix = serialize_lzss_position_distance_32m_frame_prefix(
      q.layout, context, b.frame.first(80), w.prefix_written, w.retained);
  if (w.prefix.error != LzssPositionDistance32mSerializeError::none) {
    r.error = E::prefix_error;
    return r;
  }
  if (!remainder(w, q.aggregate_bytes,
                 {q.bytes_required, token_bytes, raw.size(),
                  sizeof(e::LzssPositionDistance32mRangeState)})) {
    r.error = E::arithmetic_overflow;
    return r;
  }
  if (preflight_lzss_position_distance_32m_frame_prefix(
          b.frame.first(q.bytes_required), context, w.parsed, w.requirements,
          w.retained) != LzssPositionDistance32mPreflightError::none) {
    r.error = E::prefix_error;
    return r;
  }
  if (w.prefix_written != 80 ||
      w.requirements.aggregate_working_bytes != q.aggregate_bytes ||
      !same(w.parsed, q.layout)) {
    r.error = E::inconsistent_counts;
    return r;
  }
  std::copy_n(b.frame.begin(), q.bytes_required, output.begin());
  layout = w.parsed;
  written = q.bytes_required;
  r.bytes_committed = q.bytes_required;
  return r;
}
} // namespace marc::frame::internal
