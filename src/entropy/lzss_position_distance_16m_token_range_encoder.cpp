// Private finite typed-token traversal. First-party reference arithmetic is
// retained independently here; the materialized operation encoder is unchanged.
#include "entropy/lzss_position_distance_16m_token_range_encoder.hpp"
#include "context/lzss_position_distance_16m_field_cursor.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <limits>
namespace marc::entropy::internal {
namespace {
using E = LzssPositionDistance16mTokenRangeError;
namespace c = context::internal;
namespace d = dictionary::internal;
inline constexpr std::uint32_t normalization_threshold = UINT32_C(1) << 24;

class RangeWriter {
public:
  explicit RangeWriter(const std::span<std::byte> output) noexcept
      : output_(output) {}

  [[nodiscard]] bool encode(const std::uint32_t cumulative,
                            const std::uint16_t frequency,
                            const std::uint32_t total) noexcept {
    if (total == 0 || frequency == 0 || cumulative >= total ||
        static_cast<std::uint64_t>(cumulative) + frequency > total ||
        range_ < normalization_threshold) {
      return false;
    }
    range_ /= total;
    if (range_ == 0)
      return false;
    low_ += static_cast<std::uint64_t>(cumulative) * range_;
    range_ *= frequency;
    while (range_ < normalization_threshold) {
      range_ <<= 8;
      if (!shift_low())
        return false;
    }
    return true;
  }

  [[nodiscard]] bool finish() noexcept {
    for (int index = 0; index < 5; ++index) {
      if (!shift_low())
        return false;
    }
    return true;
  }

  [[nodiscard]] std::size_t size() const noexcept { return size_; }

private:
  [[nodiscard]] bool emit(const std::uint8_t value) noexcept {
    if (size_ == std::numeric_limits<std::size_t>::max())
      return false;
    if (!output_.empty()) {
      if (size_ >= output_.size())
        return false;
      output_[size_] = static_cast<std::byte>(value);
    }
    ++size_;
    return true;
  }

  [[nodiscard]] bool shift_low() noexcept {
    const auto low32 = static_cast<std::uint32_t>(low_);
    const auto carry = static_cast<std::uint32_t>(low_ >> 32);
    if (carry > 1)
      return false;
    if (low32 < UINT32_C(0xff000000) || carry != 0) {
      if (!emit(static_cast<std::uint8_t>(cache_ + carry)))
        return false;
      const auto delayed = static_cast<std::uint8_t>(UINT32_C(0xff) + carry);
      for (std::size_t index = 1; index < pending_; ++index) {
        if (!emit(delayed))
          return false;
      }
      cache_ = static_cast<std::uint8_t>(low32 >> 24);
      pending_ = 0;
    }
    if (pending_ == std::numeric_limits<std::size_t>::max())
      return false;
    ++pending_;
    low_ = static_cast<std::uint32_t>(low32 << 8);
    return true;
  }

  std::span<std::byte> output_{};
  std::uint64_t low_{};
  std::uint32_t range_{UINT32_MAX};
  std::uint8_t cache_{};
  std::size_t pending_{1};
  std::size_t size_{};
};

struct Models {
  std::array<std::uint16_t,
             context::internal::lzss_position_distance_16m_frequency_entries>
      frequencies{};
  std::array<std::uint32_t,
             context::internal::lzss_position_distance_16m_context_count>
      totals{};

  Models() noexcept {
    frequencies.fill(1);
    for (std::size_t index = 0; index < totals.size(); ++index) {
      totals[index] =
          context::internal::lzss_position_distance_16m_alphabets[index];
    }
  }

  void update(const std::uint16_t context_id,
              const std::uint32_t symbol) noexcept {
    const auto offset =
        context::internal::lzss_position_distance_16m_offsets[context_id];
    ++frequencies[offset + symbol];
    auto &total = totals[context_id];
    ++total;
    if (total != contextual_dynamic_range_model_total_limit)
      return;
    total = 0;
    const auto alphabet =
        context::internal::lzss_position_distance_16m_alphabets[context_id];
    for (std::size_t index = 0; index < alphabet; ++index) {
      auto &frequency = frequencies[offset + index];
      frequency = static_cast<std::uint16_t>(
          (static_cast<std::uint32_t>(frequency) + 1U) / 2U);
      total += frequency;
    }
  }
};

struct Working {
  Models models{};
  RangeWriter writer;
  c::LzssPositionDistance16mFieldState cursor{};
  c::LzssPositionDistance16mRequest pending{};
  c::LzssPositionDistance16mTokenCheck check{};
  LzssPositionDistance16mTokenRangeDetails result{};
  explicit Working(std::span<std::byte> out) : writer(out) {}
};
struct Region {
  const void *data;
  std::size_t bytes;
};
constexpr auto working_bytes =
    sizeof(Working) + 2 * sizeof(LzssPositionDistance16mTokenRangePlan) +
    sizeof(LzssPositionDistance16mTokenRangeResult) +
    sizeof(ContextualDynamicRangeDescriptor) + sizeof(std::array<Region, 9>) +
    sizeof(d::LzssTypedTokenValidationContext) +
    sizeof(std::array<std::size_t, 5>) + 3 * sizeof(std::size_t);
LzssPositionDistance16mTokenRangeDetails
run(std::span<const d::LzssTypedToken> tokens,
    const d::LzssParameters &parameters,
    const c::LzssFieldContextValidationContext &context,
    const core::DecoderLimits &limits, std::span<std::byte> output) noexcept {
  Working w(output);
  auto &r = w.result;
  const auto emit = [&](std::uint32_t value) {
    r.operation_index = r.operation_count;
    w.pending = c::lzss_position_distance_16m_next(w.cursor);
    w.pending.shape.value = value;
    r.field_error =
        c::lzss_position_distance_16m_accept(w.cursor, w.pending.shape);
    if (r.field_error != c::LzssFieldContextError::none) {
      r.error = E::invalid_field;
      return false;
    }
    const auto &op = w.pending.shape;
    if (op.kind == c::ModeledOperationKind::symbol) {
      if (op.context_id >= c::lzss_position_distance_16m_context_count ||
          op.alphabet_size !=
              c::lzss_position_distance_16m_alphabets[op.context_id] ||
          op.value >= op.alphabet_size || op.bit_count) {
        r.error = E::invalid_field;
        return false;
      }
      const auto off = c::lzss_position_distance_16m_offsets[op.context_id];
      std::uint32_t cumulative{};
      for (std::uint32_t i = 0; i < op.value; ++i)
        cumulative += w.models.frequencies[off + i];
      if (!w.writer.encode(cumulative, w.models.frequencies[off + op.value],
                           w.models.totals[op.context_id])) {
        r.error = E::internal_error;
        return false;
      }
      w.models.update(op.context_id, op.value);
    } else {
      if (op.kind != c::ModeledOperationKind::bypass_bits || op.context_id ||
          op.alphabet_size || !op.bit_count || op.bit_count > 24 ||
          (op.value >> op.bit_count)) {
        r.error = E::invalid_field;
        return false;
      }
      for (std::uint8_t bit = 0; bit < op.bit_count; ++bit) {
        const auto v = (op.value >> bit) & 1u;
        const bool adaptive =
            w.pending.field ==
            c::LzssPositionDistance16mField::adaptive_distance_extra;
        const auto id = static_cast<std::uint16_t>(24 + bit);
        const auto off = c::lzss_position_distance_16m_offsets[id];
        const auto cumulative = adaptive && v ? w.models.frequencies[off] : v;
        const auto frequency = adaptive ? w.models.frequencies[off + v] : 1u;
        const auto total = adaptive ? w.models.totals[id] : 2u;
        if (!w.writer.encode(adaptive && !v ? 0 : cumulative,
                             static_cast<std::uint16_t>(frequency), total)) {
          r.error = E::internal_error;
          return false;
        }
        if (adaptive)
          w.models.update(id, v);
      }
    }
    if (!core::checked_add(r.decision_count,
                           op.kind == c::ModeledOperationKind::symbol
                               ? 1u
                               : static_cast<std::uint32_t>(op.bit_count),
                           r.decision_count) ||
        !core::checked_add(r.operation_count, std::size_t{1},
                           r.operation_count)) {
      r.error = E::arithmetic_overflow;
      return false;
    }
    return true;
  };
  for (const auto &token : tokens) {
    r.token_index = r.token_count;
    w.check = c::validate_lzss_position_distance_16m_token(
        token, parameters, {r.raw_size, context.declared_raw_size}, limits);
    if (w.check.error != d::LzssTypedTokenError::none) {
      r.token_error = w.check.error;
      r.error = E::invalid_token;
      return r;
    }
    if (token.kind == d::LzssTypedTokenKind::literal) {
      if (!emit(0) || !emit(token.literal))
        return r;
    } else {
      const auto lc =
          token.length < 5 ? 8u : std::bit_width(token.length - 4) - 1u;
      const auto le = lc == 8 ? token.length - 3
                              : token.length - 4 - (std::uint32_t{1} << lc);
      const auto dc = std::bit_width(token.distance) - 1u;
      if (!emit(1) || !emit(lc) || (lc && !emit(le)) || !emit(dc) ||
          (dc && !emit(token.distance - (std::uint32_t{1} << dc))))
        return r;
    }
    r.raw_size = w.check.next_raw_size;
    if (!core::checked_add(r.token_count, std::size_t{1}, r.token_count)) {
      r.error = E::arithmetic_overflow;
      return r;
    }
  }
  r.token_index = r.token_count;
  r.operation_index = r.operation_count;
  r.field_error = c::lzss_position_distance_16m_finish(w.cursor);
  if (r.field_error != c::LzssFieldContextError::none)
    r.error = E::invalid_field;
  else if (r.token_count != context.declared_token_count)
    r.error = E::token_count_mismatch;
  else if (r.raw_size != context.declared_raw_size)
    r.error = E::raw_size_mismatch;
  else if (r.operation_count != context.declared_event_count)
    r.error = E::event_count_mismatch;
  else if (r.decision_count != context.declared_decision_count)
    r.error = E::decision_count_mismatch;
  if (r.error != E::none)
    return r;
  if (!w.writer.finish()) {
    r.error = E::internal_error;
    return r;
  }
  r.payload_size = w.writer.size();
  return r;
}
} // namespace
std::size_t lzss_position_distance_16m_token_range_working_bytes() noexcept {
  return working_bytes;
}
LzssPositionDistance16mTokenRangePlan
query_lzss_position_distance_16m_token_range_encode(
    std::span<const d::LzssTypedToken> tokens, const d::LzssParameters &p,
    const c::LzssFieldContextValidationContext &context,
    const core::DecoderLimits &limits, std::size_t output_capacity,
    std::size_t scratch_capacity, std::size_t retained) noexcept {
  LzssPositionDistance16mTokenRangePlan q{};
  q.working_state_bytes = working_bytes;
  if (core::validate_limits(limits) != core::LimitError::none) {
    q.details.error = E::limit_exceeded;
    return q;
  }
  std::size_t token_bytes{};
  if (!core::checked_multiply(tokens.size(), sizeof(d::LzssTypedToken),
                              token_bytes)) {
    q.details.error = E::arithmetic_overflow;
    return q;
  }
  q.aggregate_bytes = token_bytes;
  for (auto n : {output_capacity, scratch_capacity, working_bytes, retained})
    if (!core::checked_add(q.aggregate_bytes, n, q.aggregate_bytes)) {
      q.details.error = E::arithmetic_overflow;
      return q;
    }
  if (q.aggregate_bytes > limits.max_internal_buffered_bytes) {
    q.details.error = E::limit_exceeded;
    return q;
  }
  q.details.token_error =
      c::validate_lzss_position_distance_16m_parameters(p, limits);
  if (q.details.token_error != d::LzssTypedTokenError::none) {
    q.details.error =
        q.details.token_error == d::LzssTypedTokenError::limit_exceeded
            ? E::limit_exceeded
            : E::invalid_parameters;
    return q;
  }
  const std::uint64_t f = context.declared_raw_size,
                      t = context.declared_token_count,
                      e = context.declared_event_count,
                      decisions = context.declared_decision_count;
  if (!f || f > 16777216 || !t || t > f || e < 2 * t ||
      e > std::min(2 * f, 5 * t) || decisions < e ||
      decisions > std::min(9 * f, 34 * t)) {
    q.details.error = E::invalid_parameters;
    return q;
  }
  if (tokens.size() != t) {
    q.details.error = E::token_count_mismatch;
    return q;
  }
  std::uint64_t total{};
  if (!core::checked_add(context.output_already_committed, f, total)) {
    q.details.error = E::arithmetic_overflow;
    return q;
  }
  if (f > limits.max_frame_size || f > limits.max_block_size ||
      total > limits.max_total_output_size ||
      2610 > limits.max_entropy_table_entries ||
      32768 > limits.max_range_model_total) {
    q.details.error = E::limit_exceeded;
    return q;
  }
  q.details = run(tokens, p, context, limits, {});
  if (q.details.error != E::none)
    return q;
  if (q.details.payload_size > UINT32_MAX) {
    q.details.error = E::arithmetic_overflow;
    return q;
  }
  if (q.details.payload_size > limits.max_compressed_payload_size) {
    q.details.error = E::limit_exceeded;
    return q;
  }
  if (output_capacity < q.details.payload_size ||
      scratch_capacity < q.details.payload_size) {
    q.details.error = E::payload_output_too_small;
    return q;
  }
  q.descriptor = {q.details.decision_count,
                  static_cast<std::uint32_t>(q.details.payload_size), 48};
  return q;
}
LzssPositionDistance16mTokenRangeResult
encode_lzss_position_distance_16m_token_range(
    std::span<const d::LzssTypedToken> tokens, const d::LzssParameters &p,
    const c::LzssFieldContextValidationContext &context,
    const core::DecoderLimits &limits, std::span<std::byte> output,
    std::span<std::byte> scratch, ContextualDynamicRangeDescriptor &descriptor,
    std::size_t retained) noexcept {
  LzssPositionDistance16mTokenRangeResult result{};
  std::size_t token_bytes{};
  if (!core::checked_multiply(tokens.size(), sizeof(d::LzssTypedToken),
                              token_bytes)) {
    result.details.error = E::arithmetic_overflow;
    return result;
  }
  const std::array regions{Region{tokens.data(), token_bytes},
                           Region{&p, sizeof(p)},
                           Region{&context, sizeof(context)},
                           Region{&limits, sizeof(limits)},
                           Region{output.data(), output.size()},
                           Region{scratch.data(), scratch.size()},
                           Region{&descriptor, sizeof(descriptor)}};
  for (std::size_t i = 0; i < regions.size(); ++i)
    for (std::size_t j = i + 1; j < regions.size(); ++j) {
      const auto alias = core::check_buffer_overlap(
          regions[i].data, regions[i].bytes, regions[j].data, regions[j].bytes);
      if (alias != core::BufferOverlap::disjoint) {
        result.details.error = alias == core::BufferOverlap::arithmetic_overflow
                                   ? E::arithmetic_overflow
                                   : E::overlapping_buffers;
        return result;
      }
    }
  const auto q = query_lzss_position_distance_16m_token_range_encode(
      tokens, p, context, limits, output.size(), scratch.size(), retained);
  result.details = q.details;
  if (result.details.error != E::none)
    return result;
  result.details =
      run(tokens, p, context, limits, scratch.first(q.details.payload_size));
  if (result.details.error != E::none)
    return result;
  if (result.details.token_count != q.details.token_count ||
      result.details.raw_size != q.details.raw_size ||
      result.details.operation_count != q.details.operation_count ||
      result.details.decision_count != q.details.decision_count ||
      result.details.payload_size != q.details.payload_size) {
    result.details.error = E::internal_error;
    return result;
  }
  std::copy_n(scratch.begin(), result.details.payload_size, output.begin());
  descriptor = q.descriptor;
  result.bytes_committed = result.details.payload_size;
  return result;
}
} // namespace marc::entropy::internal
