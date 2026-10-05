#include "frame/lzss_position_distance_32m_frame_encoder.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include <algorithm>
#include <array>
#include <bit>
namespace marc::frame::internal {
namespace {
using E = LzssPositionDistance32mFrameEncodeError;
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
  c::LzssFieldContextValidationContext context{};
  c::LzssPositionDistance32mMapResult map{};
  c::LzssFieldContextResult operation_counts{};
  e::LzssPositionDistance32mRangeEncodePlan range_plan{};
  e::LzssPositionDistance32mRangeEncodeResult range{};
  e::ContextualDynamicRangeDescriptor descriptor{};
  LzssPositionDistance32mSerializeResult prefix{};
  TypedContextFrameLayout layout{}, parsed{};
  LzssPositionDistance32mFrameRequirements requirements{};
  std::array<std::size_t, 7> owner_bytes{};
  std::size_t prefix_written{}, local{}, retained{}, complete{};
};
constexpr auto control_bytes =
    sizeof(Working) + sizeof(LzssPositionDistance32mFrameEncodeResult) +
    sizeof(std::array<Region, 15>);
std::size_t helper_bytes() noexcept {
  return std::max(
      {d::lzss_position_distance_32m_indexed_working_bytes(),
       c::lzss_position_distance_32m_map_working_bytes(),
       e::lzss_position_distance_32m_range_encode_working_bytes(),
       lzss_position_distance_32m_prefix_serialize_working_bytes()});
}
template <class T> bool extent(std::span<T> s, std::size_t &bytes) noexcept {
  return core::checked_multiply(s.size(), sizeof(T), bytes);
}
// Nested helper's full charged views/state plus retained remainder equals the
// same top-level full-owner ledger. No logical phase death releases an owner.
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
} // namespace
std::size_t lzss_position_distance_32m_frame_encode_working_bytes() noexcept {
  return control_bytes + helper_bytes();
}
LzssPositionDistance32mFrameEncodeResult
encode_lzss_position_distance_32m_frame(
    std::span<const std::byte> raw,
    const TypedContextFrameValidationContext &context,
    const LzssPositionDistance32mFrameEncodeWorkspace &b,
    std::span<std::byte> output, TypedContextFrameLayout &layout,
    std::size_t &written, std::size_t retained) noexcept {
  LzssPositionDistance32mFrameEncodeResult r{};
  Working w{};
  r.working_state_bytes =
      lzss_position_distance_32m_frame_encode_working_bytes();
  if (!extent(b.tokens, w.owner_bytes[0]) ||
      !extent(b.token_scratch, w.owner_bytes[1]) ||
      !extent(b.index, w.owner_bytes[2]) ||
      !extent(b.operations, w.owner_bytes[3]) ||
      !extent(b.operation_scratch, w.owner_bytes[4])) {
    r.error = E::arithmetic_overflow;
    return r;
  }
  w.owner_bytes[5] = b.frame.size();
  w.owner_bytes[6] = b.payload_scratch.size();
  const std::array regions{
      Region{raw.data(), raw.size()},
      Region{b.tokens.data(), w.owner_bytes[0]},
      Region{b.token_scratch.data(), w.owner_bytes[1]},
      Region{b.index.data(), w.owner_bytes[2]},
      Region{b.operations.data(), w.owner_bytes[3]},
      Region{b.operation_scratch.data(), w.owner_bytes[4]},
      Region{b.frame.data(), b.frame.size()},
      Region{b.payload_scratch.data(), b.payload_scratch.size()},
      Region{output.data(), output.size()},
      Region{&context, sizeof(context)},
      Region{&context.stream, sizeof(context.stream)},
      Region{&context.limits, sizeof(context.limits)},
      Region{&b, sizeof(b)},
      Region{&layout, sizeof(layout)},
      Region{&written, sizeof(written)}};
  for (std::size_t i = 0; i < regions.size(); ++i)
    for (std::size_t j = i + 1; j < regions.size(); ++j) {
      const auto a = core::check_buffer_overlap(
          regions[i].data, regions[i].bytes, regions[j].data, regions[j].bytes);
      if (a != core::BufferOverlap::disjoint) {
        r.error = a == core::BufferOverlap::arithmetic_overflow
                      ? E::arithmetic_overflow
                      : E::overlapping_buffers;
        return r;
      }
    }
  r.aggregate_bytes = r.working_state_bytes;
  for (auto n : w.owner_bytes)
    if (!core::checked_add(r.aggregate_bytes, n, r.aggregate_bytes)) {
      r.error = E::arithmetic_overflow;
      return r;
    }
  for (auto n : {raw.size(), output.size(), retained})
    if (!core::checked_add(r.aggregate_bytes, n, r.aggregate_bytes)) {
      r.error = E::arithmetic_overflow;
      return r;
    }
  if (r.aggregate_bytes > context.limits.max_internal_buffered_bytes) {
    r.error = E::limit_exceeded;
    return r;
  }
  if (validate_lzss_position_distance_32m_stream_semantics(context.stream,
                                                           context.limits) !=
      LzssPositionDistance32mPreflightError::none) {
    r.error = E::invalid_stream;
    return r;
  }
  if (context.output_already_committed % context.stream.frame_size ||
      context.expected_sequence !=
          context.output_already_committed / context.stream.frame_size ||
      context.output_already_committed >= context.stream.original_size ||
      raw.size() !=
          std::min<std::uint64_t>(context.stream.frame_size,
                                  context.stream.original_size -
                                      context.output_already_committed)) {
    r.error = E::invalid_position;
    return r;
  }
  if (b.frame.size() < 80) {
    r.error = E::storage_too_small;
    return r;
  }
  if (!remainder(w, r.aggregate_bytes,
                 {raw.size(), w.owner_bytes[0], w.owner_bytes[1],
                  w.owner_bytes[2],
                  d::lzss_position_distance_32m_indexed_working_bytes()})) {
    r.error = E::arithmetic_overflow;
    return r;
  }
  w.parse = d::tokenize_lzss_position_distance_32m_indexed(
      raw, context.stream.dictionary, context.limits, b.tokens, b.token_scratch,
      b.index, w.token_counts, w.retained, context.output_already_committed);
  if (w.parse.error != d::LzssPositionDistance32mParseError::none) {
    r.error = E::parser_error;
    return r;
  }
  w.context.declared_token_count =
      static_cast<std::uint32_t>(w.parse.tokens_committed);
  w.context.declared_raw_size = static_cast<std::uint32_t>(raw.size());
  w.context.output_already_committed = context.output_already_committed;
  for (const auto &t : b.tokens.first(w.parse.tokens_committed)) {
    std::uint32_t events{}, decisions{};
    if (t.kind == d::LzssTypedTokenKind::literal) {
      events = decisions = 2;
    } else {
      const auto lc = std::bit_width(t.length - 4) - 1u,
                 dc = std::bit_width(t.distance) - 1u;
      events = 3 + (lc != 0) + (dc != 0);
      decisions = 3 + lc + dc;
    }
    if (!core::checked_add(w.context.declared_event_count, events,
                           w.context.declared_event_count) ||
        !core::checked_add(w.context.declared_decision_count, decisions,
                           w.context.declared_decision_count)) {
      r.error = E::arithmetic_overflow;
      return r;
    }
  }
  if (!remainder(w, r.aggregate_bytes,
                 {w.parse.tokens_committed * sizeof(d::LzssTypedToken),
                  w.owner_bytes[3], w.owner_bytes[4],
                  c::lzss_position_distance_32m_map_working_bytes()})) {
    r.error = E::arithmetic_overflow;
    return r;
  }
  w.map = c::map_lzss_position_distance_32m_tokens(
      b.tokens.first(w.parse.tokens_committed), context.stream.dictionary,
      w.context, context.limits, b.operations, b.operation_scratch,
      w.operation_counts, w.retained);
  if (w.map.details.error != c::LzssFieldContextError::none) {
    r.error = E::mapper_error;
    return r;
  }
  if (!remainder(
          w, r.aggregate_bytes,
          {w.map.operations_committed * sizeof(c::ModeledOperation),
           b.frame.size() - 80, b.payload_scratch.size(),
           e::lzss_position_distance_32m_range_encode_working_bytes()})) {
    r.error = E::arithmetic_overflow;
    return r;
  }
  w.range_plan = e::query_lzss_position_distance_32m_range_encode(
      b.operations.first(w.map.operations_committed), context.limits,
      b.frame.size() - 80, b.payload_scratch.size(), w.retained);
  if (w.range_plan.error != e::ContextualDynamicRangeEncodeError::none) {
    r.error = E::range_error;
    return r;
  }
  if (!core::checked_add(std::size_t{80}, w.range_plan.details.payload_size,
                         w.complete)) {
    r.error = E::arithmetic_overflow;
    return r;
  }
  if (output.size() < w.complete) {
    r.error = E::storage_too_small;
    return r;
  }
  w.range = e::encode_lzss_position_distance_32m_range_operations(
      b.operations.first(w.map.operations_committed), context.limits,
      b.frame.subspan(80), b.payload_scratch, w.descriptor, w.retained);
  if (w.range.details.error != e::ContextualDynamicRangeEncodeError::none) {
    r.error = E::range_error;
    return r;
  }
  if (w.range.bytes_committed != w.range_plan.details.payload_size ||
      w.range.details.decision_count != w.context.declared_decision_count ||
      w.range.details.operation_count != w.context.declared_event_count) {
    r.error = E::inconsistent_counts;
    return r;
  }
  w.layout.header.sequence = context.expected_sequence;
  w.layout.header.uncompressed_size = w.context.declared_raw_size;
  w.layout.header.token_count = w.context.declared_token_count;
  w.layout.header.event_count = w.context.declared_event_count;
  w.layout.header.decision_count = w.context.declared_decision_count;
  w.layout.header.payload_size = w.descriptor.payload_size;
  w.layout.header.descriptor_size = 16;
  w.layout.descriptor = w.descriptor;
  w.layout.serialized_size = w.complete;
  if (!remainder(
          w, r.aggregate_bytes,
          {80, lzss_position_distance_32m_prefix_serialize_working_bytes()})) {
    r.error = E::arithmetic_overflow;
    return r;
  }
  w.prefix = serialize_lzss_position_distance_32m_frame_prefix(
      w.layout, context, b.frame.first(80), w.prefix_written, w.retained);
  if (w.prefix.error != LzssPositionDistance32mSerializeError::none) {
    r.error = E::prefix_error;
    return r;
  }
  if (!remainder(w, r.aggregate_bytes,
                 {w.complete,
                  w.parse.tokens_committed * sizeof(d::LzssTypedToken),
                  raw.size(), sizeof(e::LzssPositionDistance32mRangeState)})) {
    r.error = E::arithmetic_overflow;
    return r;
  }
  if (preflight_lzss_position_distance_32m_frame_prefix(
          b.frame.first(w.complete), context, w.parsed, w.requirements,
          w.retained) != LzssPositionDistance32mPreflightError::none) {
    r.error = E::prefix_error;
    return r;
  }
  if (w.parsed.serialized_size != w.complete ||
      w.requirements.aggregate_working_bytes != r.aggregate_bytes ||
      w.prefix_written != 80) {
    r.error = E::inconsistent_counts;
    return r;
  }
  std::copy_n(b.frame.begin(), w.complete, output.begin());
  layout = w.parsed;
  written = w.complete;
  r.bytes_committed = w.complete;
  return r;
}
} // namespace marc::frame::internal
