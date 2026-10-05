#include "context/lzss_position_distance_64m_mapper.hpp"
#include "context/lzss_position_distance_64m_field_cursor.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include <algorithm>
#include <array>
#include <bit>
namespace marc::context::internal {
namespace {
using E = LzssFieldContextError;
using TE = dictionary::internal::LzssTypedTokenError;
struct Region {
  const void *data;
  std::size_t bytes;
};
struct Working {
  LzssPositionDistance64mFieldState cursor{};
  LzssFieldContextResult result{};
  ModeledOperation pending{};
  LzssPositionDistance64mTokenCheck check{};
};
constexpr std::size_t working_bytes =
    sizeof(Working) + 2 * sizeof(LzssPositionDistance64mMapPlan) +
    sizeof(LzssPositionDistance64mMapResult) + sizeof(std::array<Region, 7>) +
    sizeof(dictionary::internal::LzssTypedTokenValidationContext);
LzssFieldContextResult
run(std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters &parameters,
    const LzssFieldContextValidationContext &context,
    const core::DecoderLimits &limits,
    std::span<ModeledOperation> scratch) noexcept {
  Working w{};
  auto &r = w.result;
  const auto emit = [&](std::uint32_t value) {
    w.pending = lzss_position_distance_64m_next(w.cursor).shape;
    w.pending.value = value;
    const auto error = lzss_position_distance_64m_accept(w.cursor, w.pending);
    if (error != E::none) {
      r.error = error;
      return false;
    }
    const auto decisions = w.pending.kind == ModeledOperationKind::symbol
                               ? 1u
                               : w.pending.bit_count;
    std::uint32_t next{};
    if (!core::checked_add(r.decision_count, decisions, next)) {
      r.error = E::arithmetic_overflow;
      return false;
    }
    if (!scratch.empty()) {
      if (r.operation_count >= scratch.size()) {
        r.error = E::output_too_small;
        return false;
      }
      scratch[r.operation_count] = w.pending;
    }
    r.decision_count = next;
    ++r.operation_count;
    return true;
  };
  for (const auto &token : tokens) {
    r.token_index = r.token_count;
    r.operation_index = r.operation_count;
    w.check = validate_lzss_position_distance_64m_token(
        token, parameters, {r.raw_size, context.declared_raw_size}, limits);
    if (w.check.error != TE::none) {
      r.token_error = w.check.error;
      r.error = E::invalid_token;
      return r;
    }
    if (token.kind == dictionary::internal::LzssTypedTokenKind::literal) {
      if (!emit(0) || !emit(token.literal))
        return r;
    } else {
      const auto lc =
          token.length < 5 ? 8u : std::bit_width(token.length - 4) - 1u;
      const auto length_extra =
          lc == 8 ? token.length - 3
                  : token.length - 4 - (std::uint32_t{1} << lc);
      const auto dc = std::bit_width(token.distance) - 1u;
      if (!emit(1) || !emit(lc))
        return r;
      if (lc && !emit(length_extra))
        return r;
      if (!emit(dc))
        return r;
      if (dc && !emit(token.distance - (std::uint32_t{1} << dc)))
        return r;
    }
    r.raw_size = w.check.next_raw_size;
    ++r.token_count;
  }
  r.operation_index = r.operation_count;
  r.token_index = r.token_count;
  r.error = lzss_position_distance_64m_finish(w.cursor);
  if (r.error != E::none)
    return r;
  if (r.raw_size != context.declared_raw_size)
    r.error = E::raw_size_mismatch;
  else if (r.operation_count != context.declared_event_count)
    r.error = E::event_count_mismatch;
  else if (r.decision_count != context.declared_decision_count)
    r.error = E::decision_count_mismatch;
  return r;
}
} // namespace
std::size_t lzss_position_distance_64m_map_working_bytes() noexcept {
  return working_bytes;
}
LzssPositionDistance64mMapPlan query_lzss_position_distance_64m_map(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters &parameters,
    const LzssFieldContextValidationContext &c,
    const core::DecoderLimits &limits, std::size_t output_capacity,
    std::size_t scratch_capacity, std::size_t retained) noexcept {
  LzssPositionDistance64mMapPlan q{};
  q.working_state_bytes = working_bytes;
  q.details.token_error =
      validate_lzss_position_distance_64m_parameters(parameters, limits);
  if (q.details.token_error != TE::none) {
    q.error = q.details.token_error == TE::limit_exceeded
                  ? E::limit_exceeded
                  : E::invalid_parameters;
    return q;
  }
  const std::uint64_t f = c.declared_raw_size, t = c.declared_token_count,
                      e = c.declared_event_count, d = c.declared_decision_count;
  if (!f || f > 67108864 || !t || t > f || e < 2 * t ||
      e > std::min(2 * f, 5 * t) || d < e || d > std::min(10 * f, 36 * t)) {
    q.error = E::invalid_parameters;
    return q;
  }
  if (tokens.size() != t) {
    q.error = E::token_count_mismatch;
    return q;
  }
  std::uint64_t total{};
  if (!core::checked_add(c.output_already_committed, f, total)) {
    q.error = E::arithmetic_overflow;
    return q;
  }
  if (f > limits.max_frame_size || f > limits.max_block_size ||
      total > limits.max_total_output_size ||
      2632 > limits.max_entropy_table_entries ||
      32768 > limits.max_range_model_total) {
    q.error = E::limit_exceeded;
    return q;
  }
  std::size_t token_bytes{}, out_bytes{}, scratch_bytes{}, aggregate{};
  if (!core::checked_multiply(tokens.size(),
                              sizeof(dictionary::internal::LzssTypedToken),
                              token_bytes) ||
      !core::checked_multiply(output_capacity, sizeof(ModeledOperation),
                              out_bytes) ||
      !core::checked_multiply(scratch_capacity, sizeof(ModeledOperation),
                              scratch_bytes)) {
    q.error = E::arithmetic_overflow;
    return q;
  }
  aggregate = token_bytes;
  for (auto n : {out_bytes, scratch_bytes, working_bytes, retained})
    if (!core::checked_add(aggregate, n, aggregate)) {
      q.error = E::arithmetic_overflow;
      return q;
    }
  q.aggregate_bytes = aggregate;
  if (aggregate > limits.max_internal_buffered_bytes) {
    q.error = E::limit_exceeded;
    return q;
  }
  if (output_capacity < e || scratch_capacity < e) {
    q.error = E::output_too_small;
    return q;
  }
  q.details = run(tokens, parameters, c, limits, {});
  q.error = q.details.error;
  return q;
}
LzssPositionDistance64mMapResult map_lzss_position_distance_64m_tokens(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters &parameters,
    const LzssFieldContextValidationContext &c,
    const core::DecoderLimits &limits, std::span<ModeledOperation> output,
    std::span<ModeledOperation> scratch, LzssFieldContextResult &metadata,
    std::size_t retained) noexcept {
  LzssPositionDistance64mMapResult result{};
  std::size_t token_bytes{}, out_bytes{}, scratch_bytes{};
  if (!core::checked_multiply(tokens.size(),
                              sizeof(dictionary::internal::LzssTypedToken),
                              token_bytes) ||
      !core::checked_multiply(output.size(), sizeof(ModeledOperation),
                              out_bytes) ||
      !core::checked_multiply(scratch.size(), sizeof(ModeledOperation),
                              scratch_bytes)) {
    result.details.error = E::arithmetic_overflow;
    return result;
  }
  const std::array regions{Region{tokens.data(), token_bytes},
                           Region{&parameters, sizeof(parameters)},
                           Region{&c, sizeof(c)},
                           Region{&limits, sizeof(limits)},
                           Region{output.data(), out_bytes},
                           Region{scratch.data(), scratch_bytes},
                           Region{&metadata, sizeof(metadata)}};
  for (std::size_t i = 0; i < regions.size(); ++i)
    for (std::size_t j = i + 1; j < regions.size(); ++j) {
      const auto o = core::check_buffer_overlap(
          regions[i].data, regions[i].bytes, regions[j].data, regions[j].bytes);
      if (o != core::BufferOverlap::disjoint) {
        result.details.error = o == core::BufferOverlap::arithmetic_overflow
                                   ? E::arithmetic_overflow
                                   : E::overlapping_buffers;
        return result;
      }
    }
  const auto q = query_lzss_position_distance_64m_map(
      tokens, parameters, c, limits, output.size(), scratch.size(), retained);
  result.details = q.details;
  if (q.error != E::none) {
    result.details.error = q.error;
    return result;
  }
  result.details = run(tokens, parameters, c, limits, scratch);
  if (result.details.error != E::none)
    return result;
  if (result.details.operation_count != q.details.operation_count ||
      result.details.decision_count != q.details.decision_count ||
      result.details.raw_size != q.details.raw_size) {
    result.details.error = E::invalid_token;
    return result;
  }
  std::copy_n(scratch.begin(), result.details.operation_count, output.begin());
  metadata = result.details;
  result.operations_committed = result.details.operation_count;
  return result;
}
} // namespace marc::context::internal
