#include "frame/lzss_position_distance_8m_storage_adapter.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include <algorithm>
#include <array>
namespace marc::frame::internal {
namespace {
using E = LzssPositionDistance8mStorageError;
struct Region {
  const void *data;
  std::size_t bytes;
};
struct Working {
  LzssPositionDistance8mTokenFramePlan frame{};
  dictionary::internal::LzssPositionDistance8mParsePlan parse{};
  LzssPositionDistance8mSerializePlan prefix{};
  LzssPositionDistance8mTokenFrameWorkspace workspace{};
  LzssPositionDistance8mTokenStorageDemand token_demand{};
  LzssPositionDistance8mFrameStorageDemand frame_demand{};
  std::array<std::size_t, 3> extents{};
  std::size_t total{}, local{}, requested{};
};
constexpr auto numeric_controls =
    3 * sizeof(LzssPositionDistance8mStorageLedger) +
    3 * sizeof(LzssPositionDistance8mStorageAdmission) +
    sizeof(LzssPositionDistance8mGenerationCapacities) +
    sizeof(std::array<Region, 4>) + 24 * sizeof(std::size_t);
constexpr auto demand_controls =
    sizeof(Working) + sizeof(LzssPositionDistance8mTokenStorageDemand) +
    sizeof(LzssPositionDistance8mFrameStorageDemand) +
    2 * sizeof(std::array<Region, 9>) +
    sizeof(TypedContextFrameValidationContext) + 16 * sizeof(std::size_t);
template <std::size_t N> E aliases(const std::array<Region, N> &rs) noexcept {
  for (std::size_t i = 0; i < N; ++i)
    for (std::size_t j = i + 1; j < N; ++j) {
      auto a = core::check_buffer_overlap(rs[i].data, rs[i].bytes, rs[j].data,
                                          rs[j].bytes);
      if (a != core::BufferOverlap::disjoint)
        return a == core::BufferOverlap::arithmetic_overflow
                   ? E::arithmetic_overflow
                   : E::overlapping_buffers;
    }
  return E::none;
}
bool generation(const LzssPositionDistance8mGenerationCapacities &c,
                std::size_t &sum) noexcept {
  std::size_t a{}, b{};
  if (!core::checked_multiply(
          c.tokens, sizeof(dictionary::internal::LzssTypedToken), a) ||
      !core::checked_multiply(c.token_scratch,
                              sizeof(dictionary::internal::LzssTypedToken), b))
    return false;
  sum = 0;
  for (auto n : {a, b, c.frame_bytes, c.payload_bytes, c.publication_bytes})
    if (!core::checked_add(sum, n, sum))
      return false;
  return true;
}
E numeric(const core::DecoderLimits &l,
          const LzssPositionDistance8mStorageLedger &c,
          LzssPositionDistance8mStorageAdmission &q) noexcept {
  if (core::validate_limits(l) != core::LimitError::none)
    return E::invalid_stream;
  std::size_t index{}, old{}, partial{}, request{};
  if (!core::checked_multiply(c.index_entries, sizeof(uint32_t), index) ||
      !generation(c.old, old) || !generation(c.partial, partial) ||
      !generation(c.request, request))
    return E::arithmetic_overflow;
  q = {0, request, numeric_controls};
  for (auto n :
       {c.raw_bytes, index, old, partial, request, c.persistent_controls,
        c.call_controls, c.input_bytes, c.output_bytes, c.external_bytes,
        c.helper_bytes, numeric_controls})
    if (!core::checked_add(q.aggregate_bytes, n, q.aggregate_bytes))
      return E::arithmetic_overflow;
  return q.aggregate_bytes > l.max_internal_buffered_bytes ? E::limit_exceeded
                                                           : E::none;
}
E position(std::span<const std::byte> raw,
           const TypedContextFrameValidationContext &c) noexcept {
  auto v =
      validate_lzss_position_distance_8m_stream_semantics(c.stream, c.limits);
  if (v != LzssPositionDistance8mPreflightError::none)
    return v == LzssPositionDistance8mPreflightError::limit_exceeded
               ? E::limit_exceeded
               : E::invalid_stream;
  if (c.output_already_committed % c.stream.frame_size ||
      c.expected_sequence != c.output_already_committed / c.stream.frame_size ||
      c.output_already_committed >= c.stream.original_size ||
      raw.size() != std::min<std::uint64_t>(c.stream.frame_size,
                                            c.stream.original_size -
                                                c.output_already_committed))
    return E::invalid_position;
  return E::none;
}
E start(Working &w, std::size_t raw, std::size_t retained,
        const core::DecoderLimits &l) noexcept {
  w.total = lzss_position_distance_8m_storage_demand_working_bytes();
  for (auto n : {raw, w.extents[0], w.extents[1], w.extents[2], retained})
    if (!core::checked_add(w.total, n, w.total))
      return E::arithmetic_overflow;
  return w.total > l.max_internal_buffered_bytes ? E::limit_exceeded : E::none;
}
bool remainder(Working &w, std::initializer_list<std::size_t> sizes) noexcept {
  w.local = 0;
  for (auto n : sizes)
    if (!core::checked_add(w.local, n, w.local))
      return false;
  return w.local <= w.total;
}
} // namespace
std::size_t
lzss_position_distance_8m_storage_admission_working_bytes() noexcept {
  return numeric_controls;
}
std::size_t lzss_position_distance_8m_storage_demand_working_bytes() noexcept {
  return demand_controls +
         std::max(
             {lzss_position_distance_8m_token_frame_working_bytes(),
              dictionary::internal::
                  lzss_position_distance_8m_indexed_working_bytes(),
              std::size_t{80} +
                  lzss_position_distance_8m_prefix_serialize_working_bytes()});
}
E admit_lzss_position_distance_8m_storage(
    const core::DecoderLimits &l, const LzssPositionDistance8mStorageLedger &c,
    LzssPositionDistance8mStorageAdmission &out) noexcept {
  auto e = aliases(std::array{Region{&l, sizeof(l)}, Region{&c, sizeof(c)},
                              Region{&out, sizeof(out)}});
  if (e != E::none)
    return e;
  LzssPositionDistance8mStorageAdmission q{};
  e = numeric(l, c, q);
  if (e == E::none)
    out = q;
  return e;
}
E reconcile_lzss_position_distance_8m_storage(
    const core::DecoderLimits &l, const LzssPositionDistance8mStorageLedger &c,
    const LzssPositionDistance8mGenerationCapacities &a,
    LzssPositionDistance8mStorageAdmission &out) noexcept {
  auto e =
      aliases(std::array{Region{&l, sizeof(l)}, Region{&c, sizeof(c)},
                         Region{&a, sizeof(a)}, Region{&out, sizeof(out)}});
  if (e != E::none)
    return e;
  LzssPositionDistance8mStorageAdmission q{};
  e = numeric(l, c, q);
  if (e != E::none)
    return e;
  if (a.tokens > c.request.tokens ||
      a.token_scratch > c.request.token_scratch ||
      a.frame_bytes > c.request.frame_bytes ||
      a.payload_bytes > c.request.payload_bytes ||
      a.publication_bytes > c.request.publication_bytes)
    return E::overcapacity;
  auto actual = c;
  actual.request = a;
  e = numeric(l, actual, q);
  if (e == E::none)
    out = q;
  return e;
}
E prepare_lzss_position_distance_8m_token_storage(
    std::span<const std::byte> raw, const TypedContextFrameValidationContext &c,
    std::span<uint32_t> index, LzssPositionDistance8mTokenStorageDemand &out,
    std::size_t retained) noexcept {
  Working w{};
  if (!core::checked_multiply(index.size(), sizeof(uint32_t), w.extents[2]))
    return E::arithmetic_overflow;
  auto e = aliases(std::array{
      Region{raw.data(), raw.size()}, Region{index.data(), w.extents[2]},
      Region{&c, sizeof(c)}, Region{&c.stream, sizeof(c.stream)},
      Region{&c.limits, sizeof(c.limits)}, Region{&out, sizeof(out)}});
  if (e != E::none)
    return e;
  e = start(w, raw.size(), retained, c.limits);
  if (e != E::none)
    return e;
  e = position(raw, c);
  if (e != E::none)
    return e;
  if (!core::checked_add(raw.size(), std::size_t{65536}, w.requested))
    return E::arithmetic_overflow;
  if (index.size() < w.requested)
    return E::parser_error;
  if (!remainder(w, {raw.size(), w.extents[2],
                     dictionary::internal::
                         lzss_position_distance_8m_indexed_working_bytes()}))
    return E::arithmetic_overflow;
  w.parse = dictionary::internal::query_lzss_position_distance_8m_indexed(
      raw, c.stream.dictionary, c.limits, 0, 0, index, w.total - w.local,
      c.output_already_committed);
  if (w.parse.error !=
      dictionary::internal::LzssPositionDistance8mParseError::output_too_small)
    return w.parse.error == dictionary::internal::
                                LzssPositionDistance8mParseError::limit_exceeded
               ? E::limit_exceeded
               : E::parser_error;
  if (w.parse.aggregate_bytes != w.total ||
      w.parse.details.raw_size != raw.size() || !w.parse.details.token_count ||
      w.parse.details.token_count > raw.size())
    return E::count_error;
  if (!core::checked_multiply(w.parse.details.token_count,
                              2 * sizeof(dictionary::internal::LzssTypedToken),
                              w.requested) ||
      !core::checked_add(w.total, w.requested, w.token_demand.admitted_bytes))
    return E::arithmetic_overflow;
  if (w.token_demand.admitted_bytes > c.limits.max_internal_buffered_bytes)
    return E::limit_exceeded;
  w.token_demand.tokens = w.token_demand.token_scratch =
      w.parse.details.token_count;
  w.token_demand.aggregate_bytes = w.total;
  out = w.token_demand;
  return E::none;
}
E prepare_lzss_position_distance_8m_frame_storage(
    std::span<const std::byte> raw, const TypedContextFrameValidationContext &c,
    std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<dictionary::internal::LzssTypedToken> scratch,
    std::span<uint32_t> index, LzssPositionDistance8mFrameStorageDemand &out,
    std::size_t retained) noexcept {
  Working w{};
  if (!core::checked_multiply(tokens.size(), sizeof(tokens[0]), w.extents[0]) ||
      !core::checked_multiply(scratch.size(), sizeof(scratch[0]),
                              w.extents[1]) ||
      !core::checked_multiply(index.size(), sizeof(uint32_t), w.extents[2]))
    return E::arithmetic_overflow;
  auto e = aliases(std::array{
      Region{raw.data(), raw.size()}, Region{tokens.data(), w.extents[0]},
      Region{scratch.data(), w.extents[1]}, Region{index.data(), w.extents[2]},
      Region{&c, sizeof(c)}, Region{&c.stream, sizeof(c.stream)},
      Region{&c.limits, sizeof(c.limits)}, Region{&out, sizeof(out)}});
  if (e != E::none)
    return e;
  e = start(w, raw.size(), retained, c.limits);
  if (e != E::none)
    return e;
  e = position(raw, c);
  if (e != E::none)
    return e;
  w.workspace = {tokens, scratch, index, {}, {}};
  if (!remainder(w, {raw.size(), w.extents[0], w.extents[1], w.extents[2],
                     lzss_position_distance_8m_token_frame_working_bytes()}))
    return E::arithmetic_overflow;
  w.frame = query_lzss_position_distance_8m_token_frame_encode(
      raw, c, w.workspace, 0, w.total - w.local);
  if (w.frame.error != LzssPositionDistance8mTokenFrameError::storage_too_small)
    return E::count_error;
  const auto &r = w.frame.range_plan;
  if (w.frame.aggregate_bytes != w.total || w.frame.bytes_required < 85 ||
      r.details.error !=
          entropy::internal::LzssPositionDistance8mTokenRangeError::
              payload_output_too_small ||
      r.details.payload_size != w.frame.bytes_required - 80 ||
      r.details.payload_size > UINT32_MAX || r.details.raw_size != raw.size() ||
      w.frame.counts.declared_raw_size != raw.size() ||
      w.frame.counts.output_already_committed != c.output_already_committed ||
      r.details.token_count != w.frame.token_count ||
      r.details.token_count != w.frame.counts.declared_token_count ||
      r.details.operation_count != w.frame.counts.declared_event_count ||
      r.details.decision_count != w.frame.counts.declared_decision_count)
    return E::count_error;
  auto &q = w.frame_demand;
  auto &layout = q.layout;
  q.frame_bytes = q.publication_bytes = w.frame.bytes_required;
  q.payload_bytes = r.details.payload_size;
  q.counts = w.frame.counts;
  q.aggregate_bytes = w.total;
  layout.header.sequence = c.expected_sequence;
  layout.header.uncompressed_size = q.counts.declared_raw_size;
  layout.header.token_count = q.counts.declared_token_count;
  layout.header.event_count = q.counts.declared_event_count;
  layout.header.decision_count = q.counts.declared_decision_count;
  layout.header.payload_size = static_cast<uint32_t>(q.payload_bytes);
  layout.header.descriptor_size = 16;
  layout.descriptor.decision_count = q.counts.declared_decision_count;
  layout.descriptor.payload_size = static_cast<uint32_t>(q.payload_bytes);
  layout.descriptor.context_count = 47;
  layout.serialized_size = q.frame_bytes;
  if (!remainder(
          w, {80, lzss_position_distance_8m_prefix_serialize_working_bytes()}))
    return E::arithmetic_overflow;
  w.prefix = query_lzss_position_distance_8m_prefix_serialize(
      layout, c, 80, w.total - w.local);
  if (w.prefix.error != LzssPositionDistance8mSerializeError::none ||
      w.prefix.aggregate_bytes != w.total)
    return E::prefix_error;
  w.requested = 0;
  for (auto n : {q.frame_bytes, q.payload_bytes, q.publication_bytes})
    if (!core::checked_add(w.requested, n, w.requested))
      return E::arithmetic_overflow;
  if (!core::checked_add(w.total, w.requested, q.admitted_bytes))
    return E::arithmetic_overflow;
  if (q.admitted_bytes > c.limits.max_internal_buffered_bytes)
    return E::limit_exceeded;
  out = q;
  return E::none;
}
} // namespace marc::frame::internal
